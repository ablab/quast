############################################################################
# Copyright (c) 2015-2016 Saint Petersburg State University
# Copyright (c) 2011-2015 Saint Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################

from libs import qconfig
from libs.ca_utils.analyze_misassemblies import is_misassembly, exclude_internal_overlaps


class ScoredAlignSet(object):
    def __init__(self, score, indexes, uncovered):
        self.score = score
        self.indexes = indexes
        self.uncovered = uncovered


class PSA(object):  # PSA stands for Possibly Solid Alignment
    def __init__(self, align):
        self.align = align
        self.unique_start = align.start()
        self.unique_end = align.end()

    def is_solid(self, min_unique_len):
        return self.unique_end - self.unique_start + 1 > min_unique_len

    # intersect PSA with align, which is guaranteed to be inside of after this PSA
    # return True if switch to the next PSA is needed and False otherwise
    def intersect_and_go_next(self, align, solids, min_unique_len):
        if self.unique_end - align.end() > min_unique_len:  # if enough len on the right side
            if self.is_solid(min_unique_len):
                solids.append(self.align)
                return True
        self.unique_end = min(self.unique_end, align.start() - 1)
        return not self.is_solid(min_unique_len)  # if self is not solid anymore we can switch to the next PSA


class SolidRegion(object):
    def __init__(self, align):
        self.start = align.start()
        self.end = align.end()

    def include(self, align):
        return self.start <= align.start() and align.end() <= self.end


def get_best_aligns_set(sorted_aligns, ctg_len, planta_out_f, seq, cyclic_ref_lens=None, region_struct_variations=None):
    critical_number_of_aligns = 200  # use additional optimizations for large number of alignments

    penalties = dict()
    penalties['extensive'] = max(50, int(round(min(qconfig.extensive_misassembly_threshold / 4.0, ctg_len * 0.05)))) - 1
    penalties['local'] = max(2, int(round(min(qconfig.MAX_INDEL_LENGTH / 2.0, ctg_len * 0.01)))) - 1
    penalties['scaffold'] = 5
    # trying to optimise the algorithm if the number of possible alignments is large
    if len(sorted_aligns) > critical_number_of_aligns:
        print >> planta_out_f, '\t\t\tSkipping redundant alignments which can\'t be in the best set of alignments A PRIORI'

        # FIRST STEP: find solid aligns (which are present in the best selection for sure)
        # they should have unique (not covered by other aligns) region of length > 2 * extensive_penalty
        min_unique_len = 2 * penalties['extensive']

        possible_solids = [PSA(align) for align in sorted_aligns if align.len2 > min_unique_len]
        solids = []
        try:
            cur_PSA = possible_solids.pop()
            for align in reversed(sorted_aligns):
                if align != cur_PSA.align and cur_PSA.intersect_and_go_next(align, solids, min_unique_len):
                    next_PSA = possible_solids.pop()
                    while next_PSA.intersect_and_go_next(cur_PSA.align, solids, min_unique_len):
                        next_PSA = possible_solids.pop()
                    while align != next_PSA.align and next_PSA.intersect_and_go_next(align, solids, min_unique_len):
                        next_PSA = possible_solids.pop()
                    cur_PSA = next_PSA
        except IndexError:  # possible_solids is empty
            pass

        # SECOND STEP: remove all aligns which are inside solid ones
        if len(solids):
            solid_regions = []  # intersection of all solid aligns
            cur_region = SolidRegion(solids[0])
            for align in solids[1:]:
                if align.end() + 1 < cur_region.start:
                    solid_regions.append(cur_region)
                    cur_region = SolidRegion(align)
                else:  # shift start of the current region
                    cur_region.start = align.start()
            solid_regions.append(cur_region)

            filtered_aligns = solids
            idx = 0
            try:
                cur_region = solid_regions.pop()
                for idx, align in enumerate(sorted_aligns):
                    while not cur_region.include(align):
                        if align.start() > cur_region.end:
                            cur_region = solid_regions.pop()
                            continue
                        filtered_aligns.append(align)
                        break
                    else:
                        print >> planta_out_f, '\t\tSkipping redundant alignment %s' % (str(align))
            except IndexError:  # solid_regions is empty
                filtered_aligns += sorted_aligns[idx:]

            sorted_aligns = sorted(filtered_aligns, key=lambda x: x.end())

    all_scored_sets = [ScoredAlignSet(0, [], ctg_len)]
    max_score = 0
    best_set = []

    for idx, align in enumerate(sorted_aligns):
        cur_max_score = 0
        new_scored_set = None
        sets_to_remove = []
        for scored_set in all_scored_sets:
            if (scored_set.score + align.len2) > cur_max_score:  # otherwise this set can't be the best with current align
                cur_set_aligns = [sorted_aligns[i].clone() for i in scored_set.indexes] + [align.clone()]
                score, uncovered = get_score(scored_set.score, cur_set_aligns, cyclic_ref_lens, scored_set.uncovered, seq,
                                             region_struct_variations, penalties)
                if score is None:  # incorrect set, e.g. after excluding internal overlap one alignment is 0 or too small
                    continue
                if score + uncovered < max_score:
                    sets_to_remove.append(scored_set)
                elif score > cur_max_score:
                    cur_max_score = score
                    new_scored_set = ScoredAlignSet(score, scored_set.indexes + [idx], uncovered)
        for bad_set in sets_to_remove:
            all_scored_sets.remove(bad_set)
        if new_scored_set:
            all_scored_sets.append(new_scored_set)
            if cur_max_score > max_score:
                max_score = cur_max_score
                best_set = new_scored_set.indexes

    # save best selection to real aligns and skip others (as redundant)
    real_aligns = list([sorted_aligns[i] for i in best_set])
    return real_aligns


def get_added_len(set_aligns, cur_align):
    last_align_idx = -2
    last_align = set_aligns[last_align_idx]
    added_right = cur_align.end() - max(cur_align.start() - 1, last_align.end())
    added_left = 0
    while cur_align.start() < last_align.start():
        added_left += last_align.start() - cur_align.start()
        last_align_idx -= 1
        if -last_align_idx <= len(set_aligns):
            prev_start = last_align.start()  # in case of overlapping of old and new last_align
            last_align = set_aligns[last_align_idx]
            added_left -= max(0, min(prev_start, last_align.end()) - cur_align.start() + 1)
        else:
            break
    return added_right + added_left


def get_score(score, aligns, cyclic_ref_lens, uncovered_len, seq, region_struct_variations, penalties):
    if len(aligns) > 1:
        align1, align2 = aligns[-2], aligns[-1]
        reduced_len = exclude_internal_overlaps(align1, align2)  # reduced_len is for align1 only
        # check whether the set is still correct, i.e both alignments are rather large
        if min(align1.len2, align2.len2) < max(qconfig.min_cluster, qconfig.min_alignment):
            return None, None

        added_len = get_added_len(aligns, aligns[-1])
        uncovered_len -= added_len - reduced_len
        score += added_len - reduced_len
        is_extensive_misassembly, aux_data = is_misassembly(align1, align2, seq, cyclic_ref_lens, region_struct_variations)
        if is_extensive_misassembly:
            score -= penalties['extensive']
        elif abs(aux_data['inconsistency']) > qconfig.MAX_INDEL_LENGTH and not aux_data['is_scaffold_gap']:
            score -= penalties['local']
        elif aux_data['is_scaffold_gap']:
            score -= penalties['scaffold']
    else:
        score += aligns[-1].len2
        uncovered_len -= aligns[-1].len2
    return score, uncovered_len