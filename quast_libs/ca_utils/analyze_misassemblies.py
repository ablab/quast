############################################################################
# Copyright (c) 2015-2022 Saint Petersburg State University
# Copyright (c) 2011-2015 Saint Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################

from __future__ import with_statement
from __future__ import division

from quast_libs import qconfig
from quast_libs.ca_utils.misc import is_same_reference, get_ref_by_chromosome, parse_cs_tag

from quast_libs.log import get_logger
logger = get_logger(qconfig.LOGGER_DEFAULT_NAME)
from quast_libs.qutils import correct_name


class Misassembly:
    LOCAL = 0
    INVERSION = 1
    RELOCATION = 2
    TRANSLOCATION = 3
    INTERSPECTRANSLOCATION = 4  # for metaquast, if translocation occurs between chromosomes of different references
    SCAFFOLD_GAP = 5
    LOCAL_SCAFFOLD_GAP = 6
    FRAGMENTED = 7
    POTENTIALLY_MIS_CONTIGS = 8
    POSSIBLE_MISASSEMBLIES = 9
    MATCHED_SV = 10
    POTENTIAL_MGE = 11
    # special group for separating contig/scaffold misassemblies
    SCF_INVERSION = 12
    SCF_RELOCATION = 13
    SCF_TRANSLOCATION = 14
    SCF_INTERSPECTRANSLOCATION = 15


class StructuralVariations(object):
    __slots__ = ("inversions", "relocations", "translocations")

    def __init__(self):
        self.inversions = []
        self.relocations = []
        self.translocations = []

    def get_count(self):
        return len(self.inversions) + len(self.relocations) + len(self.translocations)


class Mapping(object):
    __slots__ = ("s1", "e1", "s2", "e2", "len1", "len2", "idy", "ref", "contig", "cigar", "ns_pos", "sv_type")

    def __init__(self, s1, e1, s2=None, e2=None, len1=None, len2=None, idy=None, ref=None, contig=None, cigar=None, ns_pos=None, sv_type=None):
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
        return ' '.join(str(x) for x in [self.s1, self.e1, '|', self.s2, self.e2, '|', self.len1, self.len2, '|',
                                         self.idy, '|', self.ref, self.contig])

    def coords_str(self):
        return ' '.join(str(x) for x in [self.s1, self.e1, '|', self.s2, self.e2, '|', self.len1, self.len2, '|',
                                         self.idy, '|', self.ref, self.contig, '|', self.cigar])

    def short_str(self):
        return ' '.join(str(x) for x in [self.s1, self.e1, '|', self.s2, self.e2, '|', self.len1, self.len2])

    def icarus_report_str(self, ambiguity='', is_best='True'):
        return '\t'.join(str(x) for x in [self.s1, self.e1, self.s2, self.e2, self.ref, self.contig, self.idy, ambiguity, is_best])

    def clone(self):
        return Mapping(self.s1, self.e1, self.s2, self.e2, self.len1, self.len2, self.idy, self.ref, self.contig, self.cigar)

    def start(self):
        """Return start on contig (always <= end)"""
        return min(self.s2, self.e2)

    def end(self):
        """Return end on contig (always >= start)"""
        return max(self.s2, self.e2)

    def pos_strand(self):
        """Returns True for positive strand and False for negative"""
        return self.s2 < self.e2


class IndelsInfo(object):
    __slots__ = ("mismatches", "insertions", "deletions", "indels_list")

    def __init__(self):
        self.mismatches = 0
        self.insertions = 0
        self.deletions = 0
        self.indels_list = []

    def __add__(self, other):
        self.mismatches += other.mismatches
        self.insertions += other.insertions
        self.deletions += other.deletions
        self.indels_list += other.indels_list
        return self


def distance_between_alignments(align1, align2, cyclic_ref_len=None):
    # returns distance (in reference) between two alignments
    distance_align1_align2 = align2.s1 - align1.e1 - 1
    distance_align2_align1 = align1.s1 - align2.e1 - 1
    if align1.pos_strand() and align2.pos_strand():            # alignment 1 should be earlier in reference
        distance = distance_align1_align2
    elif not align1.pos_strand() and not align2.pos_strand():  # alignment 2 should be earlier in reference
        distance = distance_align2_align1
    else:
        if align2.s1 > align1.s1:
            distance = distance_align1_align2
        else:
            distance = distance_align2_align1

    cyclic_moment = False
    if cyclic_ref_len is not None:
        cyclic_distance = distance
        if align1.e1 < align2.e1 and (cyclic_ref_len + distance_align2_align1) < qconfig.extensive_misassembly_threshold:
            cyclic_distance = cyclic_ref_len + distance_align2_align1
        elif align1.e1 >= align2.e1 and (cyclic_ref_len + distance_align1_align2) < qconfig.extensive_misassembly_threshold:
            cyclic_distance = cyclic_ref_len + distance_align1_align2
        if abs(cyclic_distance) < abs(distance):
            distance = cyclic_distance
            cyclic_moment = True
    return distance, cyclic_moment


def cyclic_back_ends_overlap(align1, align2):
    # returns overlap (in reference) between two alignments considered as having "cyclic moment"
    distance_align1_align2 = align2.s1 - align1.e1 - 1
    distance_align2_align1 = align1.s1 - align2.e1 - 1
    if align2.s1 > align1.s1:  # align1 is closer to ref start while align2 is closer to ref end
        overlap = max(0, -distance_align1_align2)  # negative distance means overlap
    else:  # otherwise
        overlap = max(0, -distance_align2_align1)  # negative distance means overlap
    return overlap


def __get_border_gaps(align1, align2, ref_lens):
    gap1 = ref_lens[align1.ref] - align1.e1 if align1.pos_strand() else align1.s1 - 1
    gap2 = align2.s1 - 1 if align2.pos_strand() else ref_lens[align2.ref] - align2.e1
    return [gap1, gap2]


def is_fragmented_ref_fake_translocation(align1, align2, ref_lens):
    # Check whether translocation is caused by fragmented reference and thus should be marked Fake misassembly
    # Return inconsistency value if translocation is fake or None if translocation is real
    # !!! it is assumed that align1.ref != align2.ref
    # assert align1.ref != align2.ref, "Internal QUAST bug: is_fragmented_ref_fake_translocation() " \
    #                                 "should be called only if align1.ref != align2.ref"
    if align1.ref == align2.ref:
        return False

    if qconfig.check_for_fragmented_ref:
        if qconfig.is_combined_ref and not is_same_reference(align1.ref, align2.ref):
            return False
        if all([d <= qconfig.fragmented_max_indent for d in __get_border_gaps(align1, align2, ref_lens)]):
            return True
    return False


def is_misassembly(align1, align2, contig_seq, ref_lens, is_cyclic=False, region_struct_variations=None, is_fake_translocation=False, is_cyclic_contig=False):
    #Calculate inconsistency between distances on the reference and on the contig
    distance_on_contig = align2.start() - align1.end() - 1
    if is_cyclic_contig:
        distance_on_contig += len(contig_seq)
    cyclic_ref_lens = ref_lens if is_cyclic else None
    if cyclic_ref_lens is not None and align1.ref == align2.ref:
        distance_on_reference, cyclic_moment = distance_between_alignments(align1, align2, cyclic_ref_lens[align1.ref])
    else:
        distance_on_reference, cyclic_moment = distance_between_alignments(align1, align2)

    misassembly_internal_overlap = 0
    if distance_on_contig < 0:
        if distance_on_reference >= 0:
            misassembly_internal_overlap = (-distance_on_contig)
        elif (-distance_on_reference) < (-distance_on_contig):
            misassembly_internal_overlap = (distance_on_reference - distance_on_contig)

    strand1 = (align1.s2 <= align1.e2)
    strand2 = (align2.s2 <= align2.e2)
    inconsistency = distance_on_reference - distance_on_contig
    aux_data = {"inconsistency": inconsistency, "distance_on_contig": distance_on_contig,
                "misassembly_internal_overlap": misassembly_internal_overlap, "cyclic_moment": cyclic_moment,
                "is_sv": False, "is_scaffold_gap": False}

    if contig_seq and check_is_scaffold_gap(inconsistency, contig_seq, align1, align2):
        aux_data["is_scaffold_gap"] = True
        return False, aux_data
    if region_struct_variations and check_sv(align1, align2, inconsistency, region_struct_variations):
        aux_data['is_sv'] = True
        return False, aux_data
    if is_fake_translocation:
        aux_data["inconsistency"] = sum(__get_border_gaps(align1, align2, ref_lens))
        return False, aux_data
    # we should check special case when two "cyclic" alignments have also overlap on back ends
    # (it indicates a local or an extensive misassembly)
    if cyclic_moment and cyclic_back_ends_overlap(align1, align2) > abs(inconsistency):
        aux_data["inconsistency"] = -cyclic_back_ends_overlap(align1, align2)  # overlap is a negative inconsistency
    if align1.ref != align2.ref or abs(aux_data["inconsistency"]) > qconfig.extensive_misassembly_threshold \
            or strand1 != strand2:
        return True, aux_data
    return False, aux_data  # regular local misassembly


def check_sv(align1, align2, inconsistency, region_struct_variations):
    max_error_sv = 100 # qconfig.smgap / 4  # min(2 * qconfig.smgap, max(qconfig.smgap, inconsistency * 0.05))
    max_error_trivial_del = 250

    max_gap = qconfig.extensive_misassembly_threshold // 4

    def __match_ci(pos, sv):  # check whether pos matches confidence interval of sv
        max_error = max_error_trivial_del if sv.sv_type == 'QuastDEL' else max_error_sv
        return sv.s1 - max_error <= pos <= sv.e1 + max_error

    def __check_translocation(align1, align2, sv):
        if sv[0].ref == align1.ref and sv[1].ref == align2.ref and \
                __match_ci(align1.e1, sv[0]) and __match_ci(align2.s1, sv[1]):
            return True

    def __check_inversion(align, sv):
        if __match_ci(align.s1, sv[0]) and sv[0].s1 <= align.e1 <= sv[1].e1:
            return True
        if __match_ci(align.e1, sv[1]) and sv[0].s1 <= align.s1 <= sv[1].e1:
            return True

    if align1.ref != align2.ref:  # translocation
        for sv in region_struct_variations.translocations:
            if __check_translocation(align1, align2, sv) or __check_translocation(align2, align1, sv):
                return True
    elif (align1.s2 < align1.e2) != (align2.s2 < align2.e2) and abs(inconsistency) < qconfig.extensive_misassembly_threshold:
        for sv in region_struct_variations.inversions:
            if align1.ref == sv[0].ref and (__check_inversion(align1, sv) or __check_inversion(align2, sv)):
                return True
    else:
        variations = region_struct_variations.relocations
        if align2.s1 < align1.s1:
            sv_start, sv_end = align2.s1, align1.e1
        else:
            sv_start, sv_end = align1.e1, align2.s1
        for index, sv in enumerate(variations):
            if sv[0].ref == align1.ref and __match_ci(sv_start, sv[0]):
                if __match_ci(sv_end, sv[1]):
                    return True
                # unite large deletion (relocations only)
                if sv[0].sv_type == 'QuastDEL':
                    prev_end = sv[1].e1
                    index_variation = index + 1
                    while index_variation < len(variations) and \
                                            variations[index_variation][0].s1 - prev_end <= max_gap and \
                                            variations[index_variation][0].ref == align1.ref:
                        sv = variations[index_variation]
                        if __match_ci(sv_end, sv[1]):
                            return True
                        prev_end = sv[1].e1
                        index_variation += 1
    return False


def find_all_sv(bed_fpath):
    if not bed_fpath:
        return None

    region_struct_variations = StructuralVariations()
    with open(bed_fpath) as f:
        for line in f:
            fs = line.split('\t')
            if not line.startswith('#'):
                try:
                    align1 = Mapping(s1=int(fs[1]), e1=int(fs[2]), ref=correct_name(fs[0]), sv_type=fs[6])
                    align2 = Mapping(s1=int(fs[4]), e1=int(fs[5]), ref=correct_name(fs[3]), sv_type=fs[6])
                    if align1.ref != align2.ref:
                        region_struct_variations.translocations.append((align1, align2))
                    elif 'INV' in fs[6]:
                        region_struct_variations.inversions.append((align1, align2))
                    elif 'DEL' in fs[6] or 'INS' in fs[6] or 'BND' in fs[6]:
                        region_struct_variations.relocations.append((align1, align2))
                    else:
                        pass # not supported yet
                except ValueError:
                    pass  # incorrect line format
    return region_struct_variations


def detect_potential_mge(misassemblies):
    is_potential_mge = []

    while len(is_potential_mge) < len(misassemblies):
        idx = len(is_potential_mge)
        if not misassemblies[idx] or idx + 1 >= len(misassemblies) or not misassemblies[idx + 1]:
            is_potential_mge.append(False)
            continue
        align1, start_in_ref, ms_type, mge_len = misassemblies[idx][0]
        align2, end_in_ref, ms_type2 = misassemblies[idx + 1][1]
        if ms_type == ms_type2 and mge_len <= qconfig.extensive_misassembly_threshold and align1.ref == align2.ref and \
                        (align1.s2 < align1.e2) == (align2.s2 < align2.e2):
            start_in_contig, end_in_contig = align1.end(), align2.start()
            distance_on_contig = end_in_contig - start_in_contig - 1
            distance_on_reference, cyclic_moment = distance_between_alignments(align1, align2)
            inconsistency = distance_on_reference - distance_on_contig
            if abs(inconsistency) <= qconfig.extensive_misassembly_threshold:
                is_potential_mge.append(True)
                is_potential_mge.append(True)
                continue
        is_potential_mge.append(False)
    return is_potential_mge


def check_is_scaffold_gap(inconsistency, contig_seq, align1, align2):
    if abs(inconsistency) <= qconfig.scaffolds_gap_threshold and align1.ref == align2.ref and \
            align1.pos_strand() == align2.pos_strand() and align1.pos_strand() == (align1.s1 < align2.s1) and \
            is_gap_filled_ns(contig_seq, align1, align2):
        return True
    return False


def exclude_internal_overlaps(align1, align2, i=None):
    # returns size of align1.len2 decrease (or 0 if not changed). It is important for cur_aligned_len calculation
    def __shift_cigar(align, new_start=None, new_end=None):
        new_cigar = 'cs:Z:'
        ctg_pos = align.s2
        strand_direction = 1 if align.s2 < align.e2 else -1
        diff_len = 0
        if not align.cigar:
            return diff_len

        for op in parse_cs_tag(align.cigar):
            if op.startswith('*'):
                if (new_start and ctg_pos >= new_start) or \
                        (new_end and ctg_pos <= new_end):
                    new_cigar += op
                ctg_pos += 1 * strand_direction
            else:
                if op.startswith(':'):
                    n_bases = int(op[1:])
                else:
                    n_bases = len(op) - 1
                corr_n_bases = n_bases
                if new_end and (ctg_pos + n_bases * strand_direction > new_end or ctg_pos > new_end):
                    corr_n_bases = new_end - ctg_pos + (n_bases if strand_direction == -1 else 1)
                elif new_start and (ctg_pos < new_start or ctg_pos + n_bases * strand_direction < new_start):
                    corr_n_bases = ctg_pos + (n_bases if strand_direction == 1 else 1) - new_start

                if corr_n_bases < 1:
                    if not op.startswith('-'):
                        ctg_pos += n_bases * strand_direction
                    if op.startswith('-'):
                        diff_len -= n_bases
                    if op.startswith('+'):
                        diff_len += n_bases
                    continue
                if op.startswith('+'):
                    ctg_pos += n_bases * strand_direction
                    diff_len += (n_bases - corr_n_bases)
                    if new_start:
                        new_cigar += '+' + op[1 + (corr_n_bases - n_bases):]
                    elif new_end:
                        new_cigar += op[:corr_n_bases + 1]
                elif op.startswith('-'):
                    diff_len -= (n_bases - corr_n_bases)
                    if new_start:
                        new_cigar += '-' + op[1 + (corr_n_bases - n_bases):]
                    elif new_end:
                        new_cigar += op[:corr_n_bases + 1]
                elif op.startswith(':'):
                    ctg_pos += n_bases * strand_direction
                    new_cigar += ':' + str(corr_n_bases)
        align.cigar = new_cigar
        return diff_len

    def __shift_start(align, new_start, diff_len):
        align_modification = '%s' % align.short_str()
        if align.s2 < align.e2:
            align.s1 += (new_start - align.s2) - diff_len
            align.s2 = new_start
            align.len2 = align.e2 - align.s2 + 1
        else:
            align.e1 -= (new_start - align.e2) - diff_len
            align.e2 = new_start
            align.len2 = align.s2 - align.e2 + 1
        align.len1 = align.e1 - align.s1 + 1
        align_modification += ' --> %s\n' % align.short_str()
        return align_modification

    def __shift_end(align, new_end, diff_len):
        align_modification = '%s' % align.short_str()
        if align.s2 < align.e2:
            align.e1 -= (align.e2 - new_end) - diff_len
            align.e2 = new_end
            align.len2 = align.e2 - align.s2 + 1
        else:
            align.s1 += (align.s2 - new_end) - diff_len
            align.s2 = new_end
            align.len2 = align.s2 - align.e2 + 1
        align.len1 = align.e1 - align.s1 + 1
        align_modification += ' --> %s\n' % align.short_str()
        return align_modification

    distance_on_contig = align2.start() - align1.end() - 1
    if distance_on_contig >= 0:  # no overlap
        return 0, None
    prev_len2 = align1.len2
    overlap_msg = '\t\t\tExcluding internal overlap of size %d between Alignment %d and %d: ' % \
                  (-distance_on_contig, i+1, i+2) if i is not None else ''

    # left only one of two copies (remove overlap from shorter alignment)
    if align1.len2 >= align2.len2:
        diff_len = __shift_cigar(align2, new_start=align1.end() + 1)  # excluded insertions and deletions
        overlap_msg += __shift_start(align2, align1.end() + 1, diff_len)
    else:
        diff_len = __shift_cigar(align1, new_end=align2.start() - 1)
        overlap_msg += __shift_end(align1, align2.start() - 1, diff_len)
    return prev_len2 - align1.len2, overlap_msg


def count_ns_and_not_ns_between_aligns(contig_seq, align1, align2):
    gap_in_contig = contig_seq[align1.end(): align2.start() - 1]
    ns_count = gap_in_contig.count('N')
    return ns_count, len(gap_in_contig) - ns_count


def is_gap_filled_ns(contig_seq, align1, align2):
    gap_in_contig = contig_seq[align1.end(): align2.start() - 1]
    return 'N' * qconfig.Ns_break_threshold in gap_in_contig


def process_misassembled_contig(sorted_aligns, is_cyclic, aligned_lengths, region_misassemblies, ref_lens, ref_aligns,
                                ref_features, contig_seq, misassemblies_by_ref, istranslocations_by_ref, region_struct_variations,
                                ca_output):
    misassembly_internal_overlap = 0
    prev_align = sorted_aligns[0]
    cur_aligned_length = prev_align.len2
    is_misassembled = False
    contig_is_printed = False
    indels_info = IndelsInfo()
    cnt_misassemblies = 0

    misassemblies = []
    misassembly_info = []
    for i in range(len(sorted_aligns) - 1):
        next_align = sorted_aligns[i + 1]

        is_fake_translocation = is_fragmented_ref_fake_translocation(prev_align, next_align, ref_lens)
        internal_overlap, overlap_msg = exclude_internal_overlaps(prev_align, next_align, i)
        is_extensive_misassembly, aux_data = is_misassembly(prev_align, next_align, contig_seq, ref_lens,
                                                            is_cyclic, region_struct_variations, is_fake_translocation)
        misassembly_type = ''
        if is_extensive_misassembly: # it is not a Fake translocation, because is_extensive_misassembly is True
            prev_ref, next_ref = get_ref_by_chromosome(prev_align.ref), get_ref_by_chromosome(next_align.ref)
            if prev_align.ref != next_align.ref:  # if chromosomes from different references
                if qconfig.is_combined_ref and prev_ref != next_ref:
                    misassembly_type = 'interspecies translocation'
                else:
                    misassembly_type = 'translocation'
            elif abs(aux_data["inconsistency"]) > qconfig.extensive_misassembly_threshold:
                misassembly_type = 'relocation'
            else: #if strand1 != strand2:
                misassembly_type = 'inversion'
            if next_align.s1 > prev_align.e1:
                start_in_ref, end_in_ref = prev_align.e1, next_align.s1
            else:
                start_in_ref, end_in_ref = next_align.s1, prev_align.e1
            misassemblies.append([(prev_align, start_in_ref, misassembly_type, next_align.len2), (next_align, end_in_ref, misassembly_type)])
        else:
            misassemblies.append([])
        misassembly_info.append((internal_overlap, overlap_msg, is_extensive_misassembly, aux_data, misassembly_type))
        prev_align = next_align
    is_potential_mge = None
    if qconfig.large_genome:
        is_potential_mge = detect_potential_mge(misassemblies)

    prev_align = sorted_aligns[0]
    contig_aligned_lengths = []
    for i in range(len(sorted_aligns) - 1):
        next_align = sorted_aligns[i + 1]
        internal_overlap, overlap_msg, is_extensive_misassembly, aux_data, misassembly_type = misassembly_info[i]
        if overlap_msg:
            cur_aligned_length -= internal_overlap
            ca_output.stdout_f.write(overlap_msg)

        inconsistency = aux_data["inconsistency"]
        distance_on_contig = aux_data["distance_on_contig"]
        misassembly_internal_overlap += aux_data["misassembly_internal_overlap"]
        cyclic_moment = aux_data["cyclic_moment"]
        ca_output.icarus_out_f.write(prev_align.icarus_report_str() + '\n')
        ca_output.stdout_f.write('\t\t\tReal Alignment %d: %s\n' % (i+1, str(prev_align)))

        ref_aligns.setdefault(prev_align.ref, []).append(prev_align)
        ca_output.coords_filtered_f.write(prev_align.coords_str() + '\n')
        prev_ref, next_ref = get_ref_by_chromosome(prev_align.ref), get_ref_by_chromosome(next_align.ref)
        if aux_data["is_sv"]:
            ca_output.stdout_f.write('\t\t\t  Not a misassembly (structural variation of the genome) between these two alignments\n')
            ca_output.icarus_out_f.write('fake: not a misassembly (structural variation of the genome)\n')
            region_misassemblies.append(Misassembly.MATCHED_SV)
        elif aux_data["is_scaffold_gap"]:
            if abs(inconsistency) > qconfig.extensive_misassembly_threshold:
                scaff_gap_type = ' (extensive)'
                region_misassemblies.append(Misassembly.SCAFFOLD_GAP)
                misassemblies_by_ref[prev_ref].append(Misassembly.SCAFFOLD_GAP)
                ca_output.icarus_out_f.write('fake: scaffold gap size wrong estimation' + scaff_gap_type + '\n')
            else:
                scaff_gap_type = ' (local)'
                region_misassemblies.append(Misassembly.LOCAL_SCAFFOLD_GAP)
                misassemblies_by_ref[prev_ref].append(Misassembly.LOCAL_SCAFFOLD_GAP)
                ca_output.icarus_out_f.write('fake: scaffold gap size wrong estimation' + scaff_gap_type + '\n')
            ca_output.stdout_f.write('\t\t\t  Scaffold gap between these two alignments, ')
            ca_output.stdout_f.write('gap lengths difference (reference vs assembly) = ' + str(inconsistency) + scaff_gap_type + '\n')
        elif is_extensive_misassembly and is_potential_mge and is_potential_mge[i]:
            ca_output.stdout_f.write(
                '\t\t\t  Not a misassembly (possible transposable element) between these two alignments\n')
            ca_output.icarus_out_f.write('fake: not a misassembly (possible transposable element)\n')
            region_misassemblies.append(Misassembly.POTENTIAL_MGE)
        elif is_extensive_misassembly:
            is_misassembled = True
            cnt_misassemblies += 1
            contig_aligned_lengths.append(cur_aligned_length)
            cur_aligned_length = 0
            if not contig_is_printed:
                ca_output.misassembly_f.write(prev_align.contig + '\n')
                contig_is_printed = True
            ca_output.misassembly_f.write('Extensive misassembly (')
            ca_output.stdout_f.write('\t\t\t  Extensive misassembly (')
            msg = ''
            if misassembly_type == 'interspecies translocation':
                misassembly_id = Misassembly.INTERSPECTRANSLOCATION
                istranslocations_by_ref[prev_ref][next_ref] += 1
                istranslocations_by_ref[next_ref][prev_ref] += 1
            elif misassembly_type == 'translocation':
                misassembly_id = Misassembly.TRANSLOCATION
            elif misassembly_type == 'relocation':
                misassembly_id = Misassembly.RELOCATION
                msg = ', inconsistency = ' + str(inconsistency) + \
                      (' [linear representation of circular genome]' if cyclic_moment else '')
            else: #if strand1 != strand2:
                misassembly_id = Misassembly.INVERSION
            region_misassemblies.append(misassembly_id)
            misassemblies_by_ref[prev_ref].append(misassembly_id)
            if misassembly_id == Misassembly.INTERSPECTRANSLOCATION:  # special case
                misassemblies_by_ref[next_ref].append(misassembly_id)
            if is_gap_filled_ns(contig_seq, prev_align, next_align):
                misassembly_type += ', scaffold gap is present'
                region_misassemblies.append(misassembly_id + (Misassembly.SCF_INVERSION - Misassembly.INVERSION))
            ca_output.stdout_f.write(misassembly_type + msg)
            ca_output.misassembly_f.write(misassembly_type + msg)
            ca_output.icarus_out_f.write(misassembly_type + msg)
            ca_output.stdout_f.write(') between these two alignments\n')
            ca_output.misassembly_f.write(') between %s %s and %s %s' % (prev_align.s2, prev_align.e2, next_align.s2, next_align.e2) + '\n')
            ca_output.icarus_out_f.write('\n')
            ref_features.setdefault(prev_align.ref, {})[prev_align.e1] = 'M'
            ref_features.setdefault(next_align.ref, {})[next_align.e1] = 'M'
        else:
            reason_msg = "" + (" [linear representation of circular genome]" if cyclic_moment else "") + \
                         (" [fragmentation of reference genome]" if prev_align.ref != next_align.ref else "")
            if inconsistency == 0 and cyclic_moment:
                ca_output.stdout_f.write('\t\t\t  Not a misassembly' + reason_msg + ' between these two alignments\n')
                ca_output.icarus_out_f.write('fake: not a misassembly' + reason_msg + '\n')
            elif inconsistency == 0 and prev_align.ref != next_align.ref:  # is_fragmented_ref_fake_translocation is True, because is_extensive_misassembly is False
                ca_output.stdout_f.write('\t\t\t  Not a misassembly' + reason_msg + ' between these two alignments\n')
                region_misassemblies.append(Misassembly.FRAGMENTED)
                misassemblies_by_ref[prev_ref].append(Misassembly.FRAGMENTED)
                ca_output.icarus_out_f.write('fake: not a misassembly' + reason_msg + '\n')
            elif abs(inconsistency) < qconfig.local_misassembly_min_length and \
                            count_ns_and_not_ns_between_aligns(contig_seq, prev_align, next_align)[1] <= max(qconfig.min_alignment, qconfig.local_misassembly_min_length - 1):
                ns_number, not_ns_number = count_ns_and_not_ns_between_aligns(contig_seq, prev_align, next_align)

                if inconsistency == 0:
                    ca_output.stdout_f.write(('\t\t\t  Stretch of %d mismatches between these two alignments (number of Ns: %d)' %
                                              (not_ns_number, ns_number)) + reason_msg + '\n')
                    indels_info.mismatches += not_ns_number
                    ca_output.icarus_out_f.write('indel: stretch of mismatches' + reason_msg + '\n')
                else:
                    indel_length = abs(inconsistency)
                    indel_class = 'Indel (<= 5bp)' if indel_length <= qconfig.SHORT_INDEL_THRESHOLD else 'Indel (> 5bp)'
                    indel_type = 'insertion' if inconsistency < 0 else 'deletion'
                    mismatches = max(0, not_ns_number - indel_length)
                    ca_output.stdout_f.write(('\t\t\t  %s between these two alignments: %s of length %d; %d mismatches (number of Ns: %d)')
                                                 % (indel_class, indel_type, indel_length, mismatches, ns_number) + reason_msg + '\n')
                    indels_info.indels_list.append(indel_length)
                    if indel_type == 'insertion':
                        indels_info.insertions += indel_length
                    else:
                        indels_info.deletions += indel_length
                    indels_info.mismatches += mismatches
                    ca_output.icarus_out_f.write('indel: ' + indel_class.lower() + reason_msg + '\n')
            else:
                if qconfig.strict_NA:
                    contig_aligned_lengths.append(cur_aligned_length)
                    cur_aligned_length = 0

                if distance_on_contig < 0:
                    #There is an overlap between the two alignments, a local misassembly
                    ca_output.stdout_f.write('\t\t\t  Overlap between these two alignments (local misassembly).')
                elif distance_on_contig > 0:
                    #There is a small gap between the two alignments, a local misassembly
                    ca_output.stdout_f.write('\t\t\t  Gap between these two alignments (local misassembly).')
                elif inconsistency < 0:
                    ca_output.stdout_f.write('\t\t\t  Overlap between these two alignments (local misassembly).')
                else:
                    ca_output.stdout_f.write('\t\t\t  Gap between these two alignments (local misassembly).')
                ca_output.stdout_f.write(' Inconsistency = ' + str(inconsistency) + reason_msg + '\n')
                ca_output.icarus_out_f.write('local misassembly' + reason_msg + '\n')
                region_misassemblies.append(Misassembly.LOCAL)
                misassemblies_by_ref[prev_ref].append(Misassembly.LOCAL)

        prev_align = next_align
        cur_aligned_length += prev_align.len2 - (-distance_on_contig if distance_on_contig < 0 else 0)

    #Record the very last alignment
    i = len(sorted_aligns) - 1
    ca_output.stdout_f.write('\t\t\tReal Alignment %d: %s' % (i + 1, str(next_align)) + '\n')
    ca_output.icarus_out_f.write(next_align.icarus_report_str() + '\n')
    ref_aligns.setdefault(next_align.ref, []).append(next_align)
    ca_output.coords_filtered_f.write(next_align.coords_str() + '\n')

    contig_aligned_lengths.append(cur_aligned_length)
    contig_aligned_length = sum(contig_aligned_lengths)

    # if contig covers more than 95% of cyclic chromosome/plasmid consider it as cyclic and do not split the first and the last aligned blocks
    if is_cyclic and len(contig_aligned_lengths) > 1 and sorted_aligns[-1].ref == sorted_aligns[0].ref and contig_aligned_length >= 0.95 * ref_lens[sorted_aligns[0].ref]:
        is_extensive_misassembly, aux_data = is_misassembly(sorted_aligns[-1], sorted_aligns[0], contig_seq, ref_lens,
                                                            is_cyclic, is_cyclic_contig=True, region_struct_variations=region_struct_variations)
        if not is_extensive_misassembly and not aux_data["is_scaffold_gap"] and not aux_data["is_sv"]:
            inconsistency = abs(aux_data["inconsistency"])
            if not qconfig.strict_NA or inconsistency < qconfig.local_misassembly_min_length:
                contig_aligned_lengths[0] += contig_aligned_lengths[-1]
                contig_aligned_lengths = contig_aligned_lengths[:-1]

    aligned_lengths.extend(contig_aligned_lengths)
    assert contig_aligned_length <= len(contig_seq), "Internal QUAST bug: contig aligned length is greater than " \
                                                     "contig length (contig: %s, len: %d, aligned: %d)!" % \
                                                     (sorted_aligns[0].contig, contig_aligned_length, len(contig_seq))

    return is_misassembled, misassembly_internal_overlap, indels_info, cnt_misassemblies, contig_aligned_length

