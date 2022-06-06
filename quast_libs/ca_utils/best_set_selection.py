############################################################################
# Copyright (c) 2015-2022 Saint Petersburg State University
# Copyright (c) 2011-2015 Saint Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################

from heapq import heappush, heappop
try:
   from collections import OrderedDict
except ImportError:
   from quast_libs.site_packages.ordered_dict import OrderedDict
from quast_libs import qconfig
from quast_libs.ca_utils.analyze_misassemblies import is_misassembly, exclude_internal_overlaps, Misassembly, \
    is_fragmented_ref_fake_translocation
from quast_libs.ca_utils.misc import is_same_reference


class ScoredSet(object):
    __slots__ = ("score", "indexes", "uncovered")

    def __init__(self, score, indexes, uncovered):
        self.score = score
        self.indexes = indexes
        self.uncovered = uncovered


class PutativeBestSet(object):
    __slots__ = ("indexes", "score_drop", "uncovered")

    def __init__(self, indexes, score_drop, uncovered):
        self.indexes = indexes
        self.score_drop = score_drop
        self.uncovered = uncovered

    def __eq__(self, other):
        return not (self < other) and not (self > other)

    def __ne__(self, other):
        return not self == other

    def __lt__(self, other):  # "less than" means "better than"
        return (self.score_drop, self.uncovered) < (other.score_drop, other.uncovered)

    def __gt__(self, other):  # "great than" means "worse than"
        return (self.score_drop, self.uncovered) > (other.score_drop, other.uncovered)

    def __ge__(self, other):
        return (self > other) or (self == other)

    def __le__(self, other):
        return (self < other) or (self == other)


class PSA(object):  # PSA stands for Possibly Solid Alignment (solid alignments are definitely present in the best set)
    __slots__ = ("align", "num_sides", "start", "unique_end", "total_unique_len", "end_overlap_penalty", "start_overlap_penalty")

    overlap_penalty_coeff = 0
    max_single_side_penalty = 0
    min_unique_len = 0

    def __init__(self, align, ctg_unique_end=None, num_sides=2):
        self.align = align
        self.num_sides = num_sides
        self.start = align.start()  # just syntax sugar
        self.unique_end = align.end() if ctg_unique_end is None else min(align.end(), ctg_unique_end)
        self.total_unique_len = 0
        end_overlap = 0 if ctg_unique_end is None else max(0, align.end() - ctg_unique_end)
        self.end_overlap_penalty = min(end_overlap * self.overlap_penalty_coeff, self.max_single_side_penalty)
        self.start_overlap_penalty = self.max_single_side_penalty

    def is_solid(self):
        return self.total_unique_len > max(self.min_unique_len - 1,
                                       self.num_sides * self.max_single_side_penalty +
                                       self.start_overlap_penalty + self.end_overlap_penalty)

    def could_be_solid(self):
        return (self.unique_end - self.start + 1) + self.total_unique_len \
               > self.num_sides * self.max_single_side_penalty + self.start_overlap_penalty + self.end_overlap_penalty

    def intersect(self, other):
        self.total_unique_len += max(0, self.unique_end - max(other.end(), self.start - 1))
        self.unique_end = max(self.start - 1, min(self.unique_end, other.start() - 1))
        if other.start() < self.start:
            start_overlap = max(0, other.end() - self.start + 1)
            self.start_overlap_penalty = min(start_overlap * self.overlap_penalty_coeff, self.max_single_side_penalty)


class SolidRegion(object):
    __slots__ = ("start", "end")

    def __init__(self, align):
        self.start = align.start()
        self.end = align.end()

    def include(self, align):
        return self.start <= align.start() and align.end() <= self.end


def get_best_aligns_sets(sorted_aligns, ctg_len, stdout_f, seq, ref_lens, is_cyclic=False, region_struct_variations=None):
    penalties = dict()
    penalties['extensive'] = max(50, min(qconfig.BSS_EXTENSIVE_PENALTY, int(round(ctg_len * 0.05)))) - 1
    penalties['local'] = max(2, min(qconfig.BSS_LOCAL_PENALTY, int(round(ctg_len * 0.01)))) - 1
    penalties['scaffold'] = 5
    # internal overlap penalty (in any case should be less or equal to corresponding misassembly penalty
    penalties['overlap_multiplier'] = 0.5  # score -= overlap_multiplier * overlap_length

    sorted_aligns = sorted(sorted_aligns, key=lambda x: (x.end(), x.len2))

    # trying to optimise the algorithm if the number of possible alignments is large
    solids = []
    if len(sorted_aligns) > qconfig.BSS_critical_number_of_aligns:
        stdout_f.write('\t\t\tToo much alignments (%d), will use speed up heuristics\n' % len(sorted_aligns))

        # FIRST STEP: find solid aligns (which are present in the best selection for sure)
        # they should have unique (not covered by other aligns) region of length > 2 * extensive_penalty
        PSA.max_single_side_penalty = penalties['extensive']
        PSA.overlap_penalty_coeff = penalties['overlap_multiplier']
        PSA.min_unique_len = qconfig.min_alignment

        cur_PSA = PSA(sorted_aligns[-1], num_sides=1)
        ctg_unique_end = cur_PSA.start
        for align in reversed(sorted_aligns[:-1]):  # Note: aligns are sorted by their ends!
            if not cur_PSA or not cur_PSA.could_be_solid():
                if align.start() < ctg_unique_end:
                    cur_PSA = PSA(align, ctg_unique_end)
                    ctg_unique_end = cur_PSA.start
                continue
            cur_PSA.intersect(align)
            if cur_PSA.is_solid():
                solids.append(cur_PSA.align)
                cur_PSA = None
            if align.start() < ctg_unique_end:  # current align is not active anymore, will switch to new one
                cur_PSA = PSA(align, ctg_unique_end)
                ctg_unique_end = cur_PSA.start
        if cur_PSA:  # process the last PSA
            cur_PSA.num_sides = 1
            cur_PSA.start_overlap_penalty = 0
            if cur_PSA.could_be_solid():  # if the last PSA could be solid it is definitely solid
                solids.append(cur_PSA.align)

        stdout_f.write('\t\t\tFound %d solid alignments:\n' % len(solids))
        for align in reversed(solids):
            stdout_f.write('\t\tSolid alignment %s\n' % (str(align)))
        stdout_f.write('\t\t\tSkipping alignments located inside solid regions since they are redundant:\n')

        # SECOND STEP: remove all aligns which are inside solid ones
        nothing_skipped = True
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

            filtered_aligns = list(solids)
            idx = 0
            try:
                cur_region = solid_regions.pop()
                for idx, align in enumerate(sorted_aligns):
                    if align in solids:
                        continue
                    while not cur_region.include(align):
                        if align.start() > cur_region.end:
                            cur_region = solid_regions.pop()
                            continue
                        filtered_aligns.append(align)
                        break
                    else:
                        stdout_f.write('\t\tSkipping redundant alignment %s\n' % (str(align)))
                        nothing_skipped = False
            except IndexError:  # solid_regions is empty
                filtered_aligns += sorted_aligns[idx:]

            sorted_aligns = sorted(filtered_aligns, key=lambda x: (x.end(), x.len2))
        if nothing_skipped:
            stdout_f.write('\t\tNothing was skipped\n')

    # Stage 1: Dynamic programming for finding the best score
    stdout_f.write('\t\t\tLooking for the best set of alignments (out of %d total alignments)\n' % len(sorted_aligns))
    all_scored_sets = [ScoredSet(0, [], ctg_len)]
    max_score = 0

    cur_solid_idx = -1
    next_solid_idx = -1
    solids = sorted(solids, key=lambda x: (x.end(), x.len2), reverse=True)
    for idx, align in enumerate(sorted_aligns):
        local_max_score = 0
        new_scored_set = None
        if solids and align == solids[-1]:
            next_solid_idx = idx
            del solids[-1]
        for scored_set in reversed(all_scored_sets):
            if scored_set.indexes and scored_set.indexes[-1] < cur_solid_idx:
                break
            cur_set_aligns = [sorted_aligns[i] for i in scored_set.indexes] + [align]
            score, uncovered = get_score(scored_set.score, cur_set_aligns, ref_lens, is_cyclic, scored_set.uncovered,
                                         seq, region_struct_variations, penalties)
            if score is None:  # incorrect set, i.e. internal overlap excluding resulted in incorrectly short alignment
                continue
            if score > local_max_score:
                local_max_score = score
                new_scored_set = ScoredSet(score, scored_set.indexes + [idx], uncovered)
        if new_scored_set:
            all_scored_sets.append(new_scored_set)
            if local_max_score > max_score:
                max_score = local_max_score
        if next_solid_idx != cur_solid_idx:
            cur_solid_idx = next_solid_idx

    # Stage 2: DFS for finding multiple best sets with almost equally good score

    ## special case for speed up (when we really not very interested in whether the contig is ambiguous or not)
    if len(sorted_aligns) > qconfig.BSS_critical_number_of_aligns and qconfig.ambiguity_usage != 'all':
        stdout_f.write('\t\tAmbiguity for this contig is not checked for speed up '
                       '(too much alignments and --ambiguity-usage is "%s")\n' % qconfig.ambiguity_usage)
        best_set = all_scored_sets.pop()
        while best_set.score != max_score:
            best_set = all_scored_sets.pop()
        return False, False, sorted_aligns, [best_set]

    max_allowed_score_drop = max_score - max_score * qconfig.ambiguity_score

    putative_sets = []
    best_sets = []
    for scored_set in all_scored_sets:
        score_drop = max_score - scored_set.score
        if score_drop <= max_allowed_score_drop:
            heappush(putative_sets, PutativeBestSet([scored_set.indexes[-1]], score_drop, scored_set.uncovered))

    ambiguity_check_is_needed = True
    too_much_best_sets = False
    while len(putative_sets):
        putative_set = heappop(putative_sets)
        # special case: no options to enlarge this set, already at the left most point
        if putative_set.indexes[0] == -1:
            best_sets.append(ScoredSet(max_score - putative_set.score_drop, putative_set.indexes[1:],
                                       putative_set.uncovered))
            # special case: we added the very best set and we need decide what to do next (based on ambiguity-usage)
            if ambiguity_check_is_needed and len(best_sets) == 1:
                if not putative_sets:  # no ambiguity at all, only one good set was there
                    return False, too_much_best_sets, sorted_aligns, best_sets
                elif not qconfig.ambiguity_usage == 'all':  # several good sets are present (the contig is ambiguous) but we need only the best one
                    return True, too_much_best_sets, sorted_aligns, best_sets
                ambiguity_check_is_needed = False
            if len(best_sets) >= qconfig.BSS_MAX_SETS_NUMBER:
                too_much_best_sets = (len(putative_sets) > 0)
                break
            continue
        # the main part: trying to enlarge the set to the left (use "earlier" alignments)
        align = sorted_aligns[putative_set.indexes[0]]
        local_max_score = 0
        local_uncovered = putative_set.uncovered
        putative_predecessors = OrderedDict()
        for scored_set in all_scored_sets:
            # we can enlarge the set with "earlier" alignments only
            if scored_set.indexes and scored_set.indexes[-1] >= putative_set.indexes[0]:
                break
            cur_set_aligns = [sorted_aligns[i] for i in scored_set.indexes] + [align]
            score, uncovered = get_score(scored_set.score, cur_set_aligns, ref_lens, is_cyclic, scored_set.uncovered,
                                         seq, region_struct_variations, penalties)
            if score is not None:
                putative_predecessors[scored_set] = (score, uncovered)
                if score > local_max_score:
                    local_max_score = score
                    local_uncovered = uncovered
                elif score == local_max_score and uncovered < local_uncovered:
                    local_uncovered = uncovered
        for preceding_set, (score, uncovered) in putative_predecessors.items():
            score_drop = local_max_score - score + putative_set.score_drop
            if score_drop > max_allowed_score_drop:
                continue
            new_index = preceding_set.indexes[-1] if preceding_set.indexes else -1
            new_uncovered = uncovered + (putative_set.uncovered - local_uncovered)
            heappush(putative_sets, PutativeBestSet([new_index] + putative_set.indexes,
                                                    score_drop, new_uncovered))

    return True, too_much_best_sets, sorted_aligns, best_sets


def get_used_indexes(best_sets):
    return set([index for best_set in best_sets for index in best_set.indexes])


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


def get_score(score, aligns, ref_lens, is_cyclic, uncovered_len, seq, region_struct_variations, penalties):
    if len(aligns) > 1:
        align1, align2 = aligns[-2], aligns[-1] = aligns[-2].clone(), aligns[-1].clone()
        is_fake_translocation = is_fragmented_ref_fake_translocation(align1, align2, ref_lens)
        overlaped_len = max(0, align1.end() - align2.start() + 1)
        if len(aligns) > 2:  # does not affect score and uncovered but it is important for further checking on set correctness
            aligns[-3] = aligns[-3].clone()
            exclude_internal_overlaps(aligns[-3], align1)
        reduced_len, _ = exclude_internal_overlaps(align1, align2)  # reduced_len is for align1 only
        # check whether the set is still correct, i.e both alignments are rather large
        if min(align1.len2, align2.len2) < qconfig.min_alignment:
            return None, None

        added_len = get_added_len(aligns, aligns[-1])
        uncovered_len -= (added_len - reduced_len)
        score += score_single_align(align2, ctg_len=added_len) - score_single_align(align1, ctg_len=reduced_len)
        is_extensive_misassembly, aux_data = is_misassembly(align1, align2, seq, ref_lens, is_cyclic, region_struct_variations,
                                                            is_fake_translocation)
        if is_extensive_misassembly:
            misassembly_penalty = penalties['extensive']
            if align1.ref != align2.ref:
                if qconfig.is_combined_ref and not is_same_reference(align1.ref, align2.ref):
                    misassembly = Misassembly.INTERSPECTRANSLOCATION
                else:
                    misassembly = Misassembly.TRANSLOCATION
            elif abs(aux_data["inconsistency"]) > qconfig.extensive_misassembly_threshold:
                    misassembly = Misassembly.RELOCATION
                    score -= float(abs(aux_data["inconsistency"])) / ref_lens[align1.ref]
            else:
                    misassembly = Misassembly.INVERSION
            score -= misassembly - Misassembly.INVERSION
        elif aux_data['is_sv']:
            misassembly_penalty = 0
        elif abs(aux_data['inconsistency']) >= qconfig.local_misassembly_min_length and not aux_data['is_scaffold_gap']:
            misassembly_penalty = penalties['local']
        elif aux_data['is_scaffold_gap']:
            misassembly_penalty = penalties['scaffold']
        else:
            misassembly_penalty = 0
        overlap_penalty = min(overlaped_len * penalties['overlap_multiplier'], misassembly_penalty)
        score -= (misassembly_penalty + overlap_penalty)
    else:
        score += score_single_align(aligns[-1])
        uncovered_len -= aligns[-1].len2
    return score, uncovered_len


def score_single_align(align, ctg_len=None):
    if ctg_len is None:
        ctg_len = align.len2
    return ctg_len * align.idy / 100.0