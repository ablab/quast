import subprocess
import gzip
import os
import shutil
import sys

from quast_libs import qconfig
from quast_libs.ca_utils.misc import minimap_fpath, get_path_to_program, mash_fpath

from Bio import SeqIO
from quast_libs.viralquast.quast_ranger import QuastRanger
from quast_libs.viralquast.minimap_parser import MinimapParser
from typing import List

class ReferenceFinder:
    def __init__(self, logger, ref_path):
        self.ref_path = ref_path
        self.logger = logger
        if qconfig.output_dirpath is None:
            self.output_dir = 'viralquast_results'
        else:
            self.output_dir = qconfig.output_dirpath
        try:
            if not os.path.exists(self.output_dir):
                os.mkdir(self.output_dir)
            else:
                shutil.rmtree(self.output_dir)
                os.mkdir(self.output_dir)
        except Exception as ex:
            self.logger.error('No access to output directory, exiting...')
            sys.exit(1)

    def find_reference(self, *args):
        pass


class MashReferenceFinder(ReferenceFinder):

    def __init__(self, *args):
        super().__init__(*args)
        if qconfig.mash_path is None:
            self.mash_path = mash_fpath()
        else:
            # mash_path = 'external_tools/mash/mash-Linux64-v2.3/mash'
            self.mash_path = qconfig.mash_path
        if self.mash_path is None:

            raise Exception('Can not find mash, try to run "sudo apt update & sudo apt install mash" command in terminal,'
                            'or install it manually from https://github.com/marbl/Mash')

    def check_reference(self, contigs_fpath: str, mash_reference_fpath: str) -> bool:
        res = subprocess.check_output([self.mash_path, 'dist', mash_reference_fpath, contigs_fpath], universal_newlines=True)
        res = res.split('\n')[:-1]
        name = res[0].split()[0].split('/')[-1].split('.fasta')[0]
        fasta_sequences = SeqIO.parse(gzip.open(self.ref_path, 'rt'), 'fasta')
        for seq in fasta_sequences:
            name_ = seq.name.replace('|', '_').replace('/', '_')
            if seq.name == name or name_ == name:
                return True
        return False

    def find_reference(self, contigs_fpath: str, mash_reference_fpath: str):
        save_all_reports = qconfig.save_all_reports
        if not self.check_reference(contigs_fpath, mash_reference_fpath):
            raise Exception('Looks like references do not match, exiting')

        mash_output = subprocess.check_output([self.mash_path, 'dist', mash_reference_fpath, contigs_fpath], universal_newlines=True)
        mash_output = mash_output.split('\n')[:-1]
        mash_output = list(sorted(mash_output, key=lambda x: float(x.split('\t')[2])))
        names = []
        for line in mash_output[:30]:
            name = line.split()[0].split('/')[-1].split('.fasta')[0]
            names.append(name)
        samples_paths = self.cut_samples(names)
        ranger = QuastRanger()
        quast_files_names = ranger.run_quast(contigs_fpath, samples_paths)
        mash_output = ranger.parse_reports(quast_files_names)
        shutil.move('{}/cutted_tmp/{}.fasta'.format(self.output_dir, mash_output[0]),
                    '{}/cutted_result.fasta'.format(self.output_dir))
        shutil.move('{}/quast_all_reports/{}'.format(self.output_dir, mash_output[0]),
                    '{}/quast_best_results'.format(self.output_dir))
        shutil.rmtree('{}/cutted_tmp'.format(self.output_dir))
        if save_all_reports is False:
            shutil.rmtree('{}/quast_all_reports'.format(self.output_dir))


    def cut_samples(self, names: List[str]) -> List[str]:
        fasta_sequences = SeqIO.parse(gzip.open(self.ref_path, 'rt'), 'fasta')
        seqs = {n: [] for n in names}
        seqs_descrs = {}
        for seq in fasta_sequences:
            name = seq.name.replace('|', '_').replace('/', '_')
            try:
                descr = seq.description.split(' (')[1].split(') ')[0]
            except:
                continue
            if name in names or seq.name in names:
                if 'mixed' in seq.description or len(seq.seq) > 15000:
                    names.remove(name)
                    continue
                if descr not in seqs_descrs:
                    seqs_descrs[descr] = name
            if descr in seqs_descrs:
                seqs[seqs_descrs[descr]].append((name, seq))
        try:
            os.mkdir('{}/cutted_tmp'.format(self.output_dir))
        except:
            shutil.rmtree('{}/cutted_tmp'.format(self.output_dir))
            os.mkdir('{}/cutted_tmp'.format(self.output_dir))
        cutted_filenames_paths = []
        for name in names[:10]:
            seqs_ = map(lambda x: x[1], sorted(seqs[name], key=lambda x:x[0]))
            fixed_name = name.replace('|', '_')
            SeqIO.write(seqs_, '{}/cutted_tmp/{}.fasta'.format(self.output_dir, fixed_name), 'fasta')
            cutted_filenames_paths.append('{}/cutted_tmp/{}.fasta'.format(self.output_dir, fixed_name))
        return cutted_filenames_paths


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
        expanded = MinimapParser().expand_and_cut(fixed_output, self.ref_path)

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
        ref_nodes, scores, d_back, allignings = self.parse_input_nodes(minimap_results)
        allignings = self.fix_allignings(allignings)
        ref_nodes_positions, back_mapping, descrs, lengths = self.find_positions(ref_nodes, self.ref_path)
        sorted_positions = list(sorted(ref_nodes_positions.items(), key=lambda x: x[1]))
        subsegments = self.find_subsegments(sorted_positions)
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
        for seqs, _ in sorted_best_seqs[:5]:
            seqs = map(lambda x: x[0], sorted(seqs, key=lambda x: x[1]))
            names.append(list(seqs)[0])

        samples_paths = self.cut_samples(names)

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

    def parse_input_nodes(self, minimap_results):
        nodes_to_reference = {}
        reference_to_nodes = {}
        scores = {}
        allignings = {}
        ref_nodes = set()
        for sample in minimap_results:
            for line in sample:
                line = line.split()
                ref_node, node = line[11], line[12]
                if node not in nodes_to_reference:
                    nodes_to_reference[node] = [ref_node]
                else:
                    nodes_to_reference[node].append(ref_node)
                if ref_node not in reference_to_nodes:
                    reference_to_nodes[ref_node] = {node}
                else:
                    reference_to_nodes[ref_node].add(node)
                scores[(ref_node, node)] = int(line[6]) * float(line[9]) / 100  # - (1 - float(line[9]) / 100) * 10000
                if ref_node not in allignings:
                    allignings[ref_node] = [tuple(sorted((int(line[0]), int(line[1]))))]
                else:
                    allignings[ref_node] += [tuple(sorted((int(line[0]), int(line[1]))))]
                ref_nodes.add(ref_node)
        return ref_nodes, scores, reference_to_nodes, allignings

    def fix_allignings(self, allignings):
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

    def find_positions(self, ref_nodes, reference_fpath):
        cnt = 0
        ref_nodes_positions = {}
        back_mapping = {}
        fasta_sequences = SeqIO.parse(gzip.open(reference_fpath, 'rt'), 'fasta')
        descrs = {}
        lengths = {}
        for seq in fasta_sequences:
            name = seq.name
            if name in ref_nodes:
                try:
                    description = seq.description.split(' (')[1].split(') ')[0]
                    descrs[name] = description
                    lengths[name] = len(seq.seq)
                except:
                    descrs[name] = None
                ref_nodes_positions[name] = cnt
                back_mapping[cnt] = name
            cnt += 1
        return ref_nodes_positions, back_mapping, descrs, lengths

    def find_subsegments(self, sorted_positions, thresh=3):
        subsegments = []
        i = 0
        while i < len(sorted_positions) - 1:
            subsegment = []
            while i + 1 < len(sorted_positions) and sorted_positions[i + 1][1] - sorted_positions[i][1] < thresh:
                subsegment.append(sorted_positions[i])
                i += 1
            if len(subsegment) > 3:
                subsegment.append(sorted_positions[i])
                subsegments.append(subsegment)
            i += 1
        return subsegments

    def find_seqs(self, subsegments,
                  # reference_to_nodes, scores,
                  descrs, allignings, lengths):
        best_seqs = []
        for subsegment in subsegments:
            descrs_to_segments = {}
            for subsegment in subsegment:
                descr = descrs[subsegment[0]]
                if descr in descrs_to_segments:
                    descrs_to_segments[descr].append(subsegment)
                else:
                    descrs_to_segments[descr] = [subsegment]
            for _, subsegments in descrs_to_segments.items():
                score_ = 0
                for subsegment in subsegments:
                    score_ += sum(map(lambda x: x[1] - x[0], allignings[subsegment[0]]))
                total_len = sum(map(lambda x: lengths[x[0]], subsegments))
                cov = score_ / total_len
                best_seqs.append((subsegments, (score_, cov)))
        return best_seqs

    def cut_samples(self, names):
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
        for name in names[:10]:
            seqs_ = map(lambda x: x[1], sorted(seqs[name], key=lambda x: x[0]))
            fixed_name = name.replace('|', '_')
            SeqIO.write(seqs_, '{}/cutted_tmp/{}.fasta'.format(self.output_dir, fixed_name), 'fasta')
            ret.append('{}/cutted_tmp/{}.fasta'.format(self.output_dir, fixed_name))
        return ret
