import re
import gzip

from Bio import SeqIO
from typing import List, Dict, Tuple, Set, Any, Union

class Mapping(object):
    def __init__(self, s1, e1, s2=None, e2=None, len1=None, len2=None, idy=None, ref=None, contig=None,
                 cigar=None, ns_pos=None, sv_type=None):
        self.s1, self.e1, self.s2, self.e2, self.len1, self.len2, self.idy, self.ref, self.contig = s1, e1, s2, e2, len1, len2, idy, ref, contig
        self.cigar = cigar
        self.ns_pos = ns_pos
        self.sv_type = sv_type

    @classmethod
    def from_line(cls, line):
        # line from coords file,e.g.
        # 4324128  4496883  |   112426   285180  |   172755   172756  |  99.9900  | gi|48994873|gb|U00096.2|  NODE_333_length_285180_cov_221082  | cs:Z::172755
        line = line.split()
        assert line[2] == line[5] == line[8] == line[10] == line[13] == '|', line
        ref = line[11]
        contig = line[12]
        s1, e1, s2, e2, len1, len2 = [int(line[i]) for i in [0, 1, 3, 4, 6, 7]]
        idy = float(line[9])
        cigar = line[14]
        return Mapping(s1, e1, s2, e2, len1, len2, idy, ref, contig, cigar)

    def __str__(self):
        return ' '.join(
            str(x) for x in [self.s1, self.e1, '|', self.s2, self.e2, '|', self.len1, self.len2, '|',
                             self.idy, '|', self.ref, self.contig])

    def coords_str(self):
        return ' '.join(
            str(x) for x in [self.s1, self.e1, '|', self.s2, self.e2, '|', self.len1, self.len2, '|',
                             self.idy, '|', self.ref, self.contig, '|', self.cigar])

    def short_str(self):
        return ' '.join(str(x) for x in [self.s1, self.e1, '|', self.s2, self.e2, '|', self.len1, self.len2])

    def icarus_report_str(self, ambiguity='', is_best='True'):
        return '\t'.join(str(x) for x in
                         [self.s1, self.e1, self.s2, self.e2, self.ref, self.contig, self.idy, ambiguity,
                          is_best])

    def clone(self):
        return Mapping(self.s1, self.e1, self.s2, self.e2, self.len1, self.len2, self.idy, self.ref,
                       self.contig, self.cigar)

    def start(self):
        """Return start on contig (always <= end)"""
        return min(self.s2, self.e2)

    def end(self):
        """Return end on contig (always >= start)"""
        return max(self.s2, self.e2)

    def pos_strand(self):
        """Returns True for positive strand and False for negative"""
        return self.s2 < self.e2


class MinimapParser():

    @staticmethod
    def parse_minimap(lines: List[str]) -> List[str]:
        cigar_pattern = re.compile(r'(\d+[M=XIDNSH])')
        out = []
        for line in lines:
            fs = line.split('\t')
            if len(fs) < 10:
                continue
            contig, align_start, align_end, strand, ref_name, ref_start = \
                fs[0], fs[2], fs[3], fs[4], fs[5], fs[7]
            align_start, align_end, ref_start = map(int, (align_start, align_end, ref_start))
            align_start += 1
            ref_start += 1
            if fs[-1].startswith('cs'):
                cs = fs[-1].strip()
                cigar = fs[-2]
            else:
                cs = ''
                cigar = fs[-1]
            cigar = cigar.split(':')[-1]

            strand_direction = 1
            if strand == '-':
                align_start, align_end = align_end, align_start
                strand_direction = -1
            align_len = 0
            ref_len = 0
            matched_bases, bases_in_mapping = map(int, (fs[9], fs[10]))
            operations = cigar_pattern.findall(cigar)

            for op in operations:
                n_bases, operation = int(op[:-1]), op[-1]
                if operation == 'S' or operation == 'H':
                    align_start += n_bases
                elif operation == 'M' or operation == '=' or operation == 'X':
                    align_len += n_bases
                    ref_len += n_bases
                elif operation == 'D':
                    ref_len += n_bases
                elif operation == 'I':
                    align_len += n_bases

            align_end = align_start + (align_len - 1) * strand_direction
            ref_end = ref_start + ref_len - 1

            idy = '%.2f' % (matched_bases * 100.0 / bases_in_mapping)
            if ref_name != "*":
                if float(idy) >= 0.90:
                    align = Mapping(s1=ref_start, e1=ref_end, s2=align_start, e2=align_end, len1=ref_len,
                                    len2=align_len, idy=idy, ref=ref_name, contig=contig, cigar=cs)
                    out.append(align.coords_str())
        return out

    def parse_input_nodes(self, fixed_lines: List[str]) -> Set[str]:
        ref_nodes = set()
        for line in fixed_lines:
            line = line.split()
            ref_node, node = line[11], line[12]
            ref_nodes.add(ref_node)
        return ref_nodes

    def find_positions(self, ref_nodes: Set[str], reference_fpath: str) -> Dict[str, int]:
        cnt = 0
        ref_nodes_positions = {}
        fasta_sequences = SeqIO.parse(gzip.open(reference_fpath, 'rt'), 'fasta')
        for seq in fasta_sequences:
            name = seq.name
            if name in ref_nodes:
                ref_nodes_positions[name] = cnt
            cnt += 1
        return ref_nodes_positions

    def find_subsegments(self, sorted_positions: List[Tuple[str, int]], thresh: int=3) -> List[List[Tuple[str, int]]]:
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

    def expand_subsegments(self, subsegments: List[List[Tuple[str, int]]], reference_fpath: str,
                           expand_len: int=8) -> List[Tuple[str, str]]:
        cnt = 0
        starts = set()
        ends = set()
        merged = True
        while merged:
            merged = False
            for i in range(len(subsegments) - 1):
                if subsegments[i + 1][0][1] - subsegments[i][-1][1] <= 2 * expand_len:
                    subsegments = subsegments[:i] + [subsegments[i] + subsegments[i + 1]] + subsegments[i + 2:]
                    merged = True
                    break
        for sub in subsegments:
            starts.add(sub[0][1])
            ends.add(sub[-1][1])
        new_seqs = []
        l, r = None, None
        fasta_sequences = SeqIO.parse(gzip.open(reference_fpath, 'rt'), 'fasta')
        for seq in fasta_sequences:
            if cnt + expand_len in starts:
                l = seq.name
            elif cnt - expand_len in ends:
                r = seq.name
                new_seqs.append((l, r))
            cnt += 1
        return new_seqs

    def cut_expanded(self, new_seqs: List[Tuple[str, str]], reference_fpath: str) -> List[List[Union[str, List[Any]]]]:
        starts, ends = set(), set()
        for start, end in new_seqs:
            starts.add(start)
            ends.add(end)
        fasta_sequences = SeqIO.parse(gzip.open(reference_fpath, 'rt'), 'fasta')
        cur_seq = None
        cutted = []
        for seq in fasta_sequences:
            if seq.name in starts:
                cur_seq = [seq.name.replace('|', '_').replace('/', '_'), []]
            if cur_seq is not None:
                cur_seq[1].append(seq)
            if seq.name in ends:
                cutted.append(cur_seq)
                cur_seq = None
        return cutted

    def expand_and_cut(self, fixed_lines: List[str], reference_fpath: str) -> List[List[Union[str, List[Any]]]]:
        ref_nodes = self.parse_input_nodes(fixed_lines)
        ref_nodes_positions = self.find_positions(ref_nodes, reference_fpath)
        sorted_positions = list(sorted(ref_nodes_positions.items(), key=lambda x: x[1]))
        subsegments = self.find_subsegments(sorted_positions)
        new_seqs = self.expand_subsegments(subsegments, reference_fpath)
        res = self.cut_expanded(new_seqs, reference_fpath)
        return res