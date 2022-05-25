import subprocess
import gzip
import os
import shutil

from quast_libs import qconfig
from quast_libs.ca_utils.misc import minimap_fpath

from Bio import SeqIO
from quast_libs.viralquast.quast_ranger import QuastRanger
from quast_libs.viralquast.minimap_parser import MinimapParser
from quast_libs.viralquast.reference_finder import ReferenceFinder
from typing import List, Tuple, Dict, Set


class MinimapReferenceFinder(ReferenceFinder):

    def __init__(self, *args):
        super().__init__(*args)
        if qconfig.minimap_path is None:
            self.minimap_path = minimap_fpath()
        else:
            self.minimap_path = qconfig.minimap_path
        if self.minimap_path is None:
            raise Exception('Can not find minimap, exiting')

    def find_reference(self, contigs_fpath: str):
        save_all_reports: bool = qconfig.save_all_reports
        fixed_output = self.run_minimap(contigs_fpath, self.ref_path)
        minimap_parser = MinimapParser()
        expanded = minimap_parser.expand_and_cut(fixed_output, self.ref_path)

        if not os.path.exists('{}/cutted_tmp'.format(self.output_dir)):
            os.mkdir('{}/cutted_tmp'.format(self.output_dir))
        files = []
        for name, seqs in expanded:
            with open('{}/cutted_tmp/{}'.format(self.output_dir, name), 'w') as f:
                SeqIO.write(seqs, f, 'fasta')
                files.append('{}/cutted_tmp/{}'.format(self.output_dir, name))

        minimap_results = []
        for file in files:
            minimap_results.append(self.run_minimap(contigs_fpath, file))

        ref_nodes, allignings = self.parse_input_nodes(minimap_results)
        allignings = self.fix_allignings(allignings)
        ref_nodes_positions, descrs, lengths = self.find_positions(ref_nodes, self.ref_path)
        sorted_positions = list(sorted(ref_nodes_positions.items(), key=lambda x: x[1]))
        subsegments = minimap_parser.find_subsegments(sorted_positions)
        best_seqs = self.find_seqs(subsegments, descrs, allignings, lengths)

        sorted_best_seqs = list(sorted(best_seqs, key=lambda x: -x[1][0]))[:50]

        i = 0
        while i + 1 < len(sorted_best_seqs) and sorted_best_seqs[i][1][0] - 100 < sorted_best_seqs[i + 1][1][0]:
            i += 1
        if i >= 5:
            sorted_best_seqs = sorted_best_seqs[:i]
            sorted_best_seqs = list(sorted(sorted_best_seqs, key=lambda x: -x[1][1]))
        else:
            sorted_best_seqs = sorted_best_seqs[:5]

        if len(best_seqs) == 0:
            exit(0)
        names = []
        for seqs, _ in sorted_best_seqs[:20]:
            seqs = map(lambda x: x[0], sorted(seqs, key=lambda x: x[1]))
            names.append(list(seqs)[0])
        if len(names) == 0:
            print('len names 0')
            exit(0)
        samples_paths = self.cut_samples(names)
        print(samples_paths)

        ranger = QuastRanger()
        names = ranger.run_quast(contigs_fpath, samples_paths)
        res = ranger.parse_reports(names)

        shutil.move('{}/cutted_tmp/{}.fasta'.format(self.output_dir, res[0]),
                    '{}/cutted_result.fasta'.format(self.output_dir))
        shutil.move('{}/quast_all_reports/{}'.format(self.output_dir, res[0]),
                    '{}/quast_best_results'.format(self.output_dir))
        shutil.rmtree('{}/cutted_tmp'.format(self.output_dir))
        if save_all_reports is False:
            shutil.rmtree('{}/quast_all_reports'.format(self.output_dir))

    def run_minimap(self, contigs_fpath: str, reference_fpath: str) -> List[str]:
        threads = qconfig.max_threads if qconfig.max_threads is not None else 1
        minimap_res = subprocess.check_output(
            [self.minimap_path, '-c', '-x', 'asm10', '-B5', '-O4,16', '-r', '85', '-N',
             '100', '-s', '65', '-z', '100', '--mask-level', '0.9', '--min-occ', '200',
             '-g', '2500', '--score-N', '2', '--cs', '-t', str(threads),
             reference_fpath, contigs_fpath], universal_newlines=True)
        minimap_res = minimap_res.split('\n')
        fixed = MinimapParser.parse_minimap(minimap_res)
        return fixed

    def parse_input_nodes(self, minimap_results: List[List[str]]) -> Tuple[Set[str], Dict[str, List[Tuple[int, int]]]]:
        allignings = {}
        ref_nodes = set()
        for sample in minimap_results:
            for line in sample:
                line = line.split()
                ref_node, node = line[11], line[12]
                if ref_node not in allignings:
                    allignings[ref_node] = [tuple(sorted((int(line[0]), int(line[1]))))]
                else:
                    allignings[ref_node] += [tuple(sorted((int(line[0]), int(line[1]))))]
                ref_nodes.add(ref_node)
        return ref_nodes, allignings

    def fix_allignings(self, allignings: Dict[str, List[Tuple[int, int]]]) -> Dict[str, List[Tuple[int, int]]]:
        allignings_fixed = {}
        for k in allignings.keys():
            l = list(sorted(allignings[k], key=lambda x: x[0]))
            cur = l[0]
            i = 1
            ans = []
            while i < len(l):
                while i < len(l) and cur[1] >= l[i][0]:
                    cur = (cur[0], max(l[i][1], cur[1]))
                    i += 1
                ans.append(cur)
                if i >= len(l):
                    break
                cur = l[i]
                i += 1
            if len(ans) == 0 or ans[-1] != cur:
                ans.append(cur)
            allignings_fixed[k] = ans
        return allignings_fixed

    def find_positions(self, ref_nodes: Set[str], reference_fpath: str) -> Tuple[Dict[str, int], Dict[str, str], Dict[str, int]]:
        cnt = 0
        ref_nodes_positions = {}
        fasta_sequences = SeqIO.parse(gzip.open(reference_fpath, 'rt'), 'fasta')
        descrs = {}
        lengths = {}
        for seq in fasta_sequences:
            name = seq.name
            if name in ref_nodes:
                try:
                    description = seq.description.split(' (')[1].split(') ')[0]
                    lengths[name] = len(seq.seq)
                    descrs[name] = description
                    ref_nodes_positions[name] = cnt
                except:
                    pass
            cnt += 1
        return ref_nodes_positions, descrs, lengths

    def find_seqs(self, subsegments: List[List[Tuple[str, int]]], descrs: Dict[str, str],
                  allignings: Dict[str, List[Tuple[int, int]]],
                  lengths: Dict[str, int]) -> List[Tuple[List[Tuple[str, int]], Tuple[int, float]]]:
        best_seqs = []
        for subsegment in subsegments:
            descrs_to_segments = {}
            for sub in subsegment:
                descr = descrs[sub[0]]
                if descr in descrs_to_segments:
                    descrs_to_segments[descr].append(sub)
                else:
                    descrs_to_segments[descr] = [sub]
            for _, subs in descrs_to_segments.items():
                score_ = 0
                for sub in subs:
                    score_ += sum(map(lambda x: x[1] - x[0], allignings[sub[0]]))
                total_len = sum(map(lambda x: lengths[x[0]], subs))
                cov = score_ / total_len
                best_seqs.append((subs, (score_, cov)))
        return best_seqs

    def cut_samples(self, names: List[str]) -> List[str]:
        fasta_sequences = SeqIO.parse(gzip.open(self.ref_path, 'rt'), 'fasta')
        seqs = {n: [] for n in names}
        seqs_descrs = {}
        for seq in fasta_sequences:
            name = seq.name
            try:
                descr = seq.description.split(' (')[1].split(') ')[0]
            except:
                continue
            if name in names:
                if 'mixed' in seq.description or len(seq.seq) > 15000:
                    names.remove(name)
                    continue
                if descr not in seqs_descrs:
                    seqs_descrs[descr] = name
            if descr in seqs_descrs:
                seqs[seqs_descrs[descr]].append((name, seq))
        if not os.path.exists('{}/cutted_tmp'.format(self.output_dir)):
            os.mkdir('{}/cutted_tmp'.format(self.output_dir))
        else:
            shutil.rmtree('{}/cutted_tmp'.format(self.output_dir))
            os.mkdir('{}/cutted_tmp'.format(self.output_dir))
        ret = []
        cnt = 0
        for name in names:
            if cnt == 5:
                break
            seqs_ = list(map(lambda x: x[1], sorted(seqs[name], key=lambda x: x[0])))
            if len(seqs_) == 0:
                continue
            fixed_name = name.replace('|', '_').replace('/', '_')
            SeqIO.write(seqs_, '{}/cutted_tmp/{}.fasta'.format(self.output_dir, fixed_name), 'fasta')
            ret.append('{}/cutted_tmp/{}.fasta'.format(self.output_dir, fixed_name))
            cnt += 1
        return ret
