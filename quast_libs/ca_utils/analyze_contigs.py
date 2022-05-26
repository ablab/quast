############################################################################
# Copyright (c) 2015-2022 Saint Petersburg State University
# Copyright (c) 2011-2015 Saint Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################
from collections import defaultdict

from quast_libs import fastaparser, qconfig
from quast_libs.ca_utils.analyze_misassemblies import process_misassembled_contig, IndelsInfo, find_all_sv, Misassembly
from quast_libs.ca_utils.best_set_selection import get_best_aligns_sets, get_used_indexes, score_single_align
from quast_libs.ca_utils.misc import ref_labels_by_chromosomes


def add_potential_misassembly(ref, misassemblies_by_ref, refs_with_translocations):
    cur_ref = ref_labels_by_chromosomes[ref]
    misassemblies_by_ref[cur_ref].append(Misassembly.POSSIBLE_MISASSEMBLIES)
    refs_with_translocations.add(cur_ref)


def process_unaligned_part(seq, align, misassemblies_by_ref, refs_with_translocations, second_align=None):
    unaligned_part = seq
    unaligned_len = len(unaligned_part)
    count_ns = unaligned_part.count('N')
    possible_misassemblies = 0
    if unaligned_len - count_ns >= qconfig.unaligned_part_size:
        possible_misassemblies = 1
        add_potential_misassembly(align.ref, misassemblies_by_ref, refs_with_translocations)
        if second_align:
            possible_misassemblies += 1
            add_potential_misassembly(second_align.ref, misassemblies_by_ref, refs_with_translocations)
    return possible_misassemblies


def check_for_potential_translocation(seq, ctg_len, sorted_aligns, region_misassemblies, misassemblies_by_ref, log_out_f):
    prev_end = 0
    total_misassemblies_count = 0
    refs_with_translocations = set()
    for i, align in enumerate(sorted_aligns):
        if align.start() > prev_end + 1:
            prev_align = sorted_aligns[i - 1] if i > 0 else None
            possible_misassemblies = process_unaligned_part(seq[prev_end: align.start()], align, misassemblies_by_ref,
                                                            refs_with_translocations, second_align=prev_align)
            total_misassemblies_count += possible_misassemblies
        prev_end = align.end()
    if ctg_len > prev_end:
        possible_misassemblies = process_unaligned_part(seq[prev_end: ctg_len], sorted_aligns[-1],
                                                        misassemblies_by_ref, refs_with_translocations)
        total_misassemblies_count += possible_misassemblies
    if not total_misassemblies_count:
        return

    region_misassemblies.append(Misassembly.POTENTIALLY_MIS_CONTIGS)
    region_misassemblies.extend([Misassembly.POSSIBLE_MISASSEMBLIES] * total_misassemblies_count)
    for ref in refs_with_translocations:
        misassemblies_by_ref[ref].append(Misassembly.POTENTIALLY_MIS_CONTIGS)
    log_out_f.write('\t\tIt can contain up to %d interspecies translocations '
                    '(will be reported as Possible Misassemblies).\n' % total_misassemblies_count)


def check_partially_unaligned(seq, sorted_aligns, ctg_len):
    prev_end = 0
    for align in sorted_aligns:
        unaligned_len = align.start() - prev_end - 1 - seq[prev_end: align.start()].count('N')
        if unaligned_len >= qconfig.unaligned_part_size:
            return True
        prev_end = align.end()
    if ctg_len - prev_end - 1 - seq[prev_end:].count('N') >= qconfig.unaligned_part_size:
        return True
    return False


def save_unaligned_info(sorted_aligns, contig, ctg_len, unaligned_len, unaligned_info_file):
    is_fully_unaligned = unaligned_len == ctg_len
    unaligned_type = 'full' if is_fully_unaligned else 'partial'
    unaligned_parts = []
    if sorted_aligns:
        if sorted_aligns[0].start() - 1:
            unaligned_parts.append('%d-%d' % (1, sorted_aligns[0].start() - 1))
        prev_end = sorted_aligns[0].end()
        if len(sorted_aligns) > 1:
            for align in sorted_aligns[1:]:
                if align.start() - prev_end - 1 > 0:
                    unaligned_parts.append('%d-%d' % (prev_end + 1, align.start() - 1))
                prev_end = align.end()
        if ctg_len - sorted_aligns[-1].end():
            unaligned_parts.append('%d-%d' % (sorted_aligns[-1].end() + 1, ctg_len))
    else:
        unaligned_parts.append('%d-%d' % (1, ctg_len))
    unaligned_parts_str = ','.join((unaligned_parts))
    unaligned_info_file.write('\t'.join([contig, str(ctg_len), str(unaligned_len), unaligned_type, unaligned_parts_str]) + '\n')


def analyze_contigs(ca_output, contigs_fpath, unaligned_fpath, unaligned_info_fpath, aligns, ref_features, ref_lens,
                    is_cyclic=None):
    maxun = 10
    epsilon = 0.99

    unaligned = 0
    partially_unaligned = 0
    fully_unaligned_bases = 0
    partially_unaligned_bases = 0
    ambiguous_contigs = 0
    ambiguous_contigs_extra_bases = 0
    ambiguous_contigs_len = 0
    half_unaligned_with_misassembly = 0
    misassembly_internal_overlap = 0

    ref_aligns = dict()
    contigs_aligned_lengths = []
    aligned_lengths = []
    region_misassemblies = []
    misassembled_contigs = dict()
    misassemblies_in_contigs = []

    region_struct_variations = find_all_sv(qconfig.bed)

    istranslocations_by_ref = dict()
    misassemblies_by_ref = defaultdict(list)
    for ref in ref_labels_by_chromosomes.values():
        istranslocations_by_ref[ref] = dict((key, 0) for key in ref_labels_by_chromosomes.values())

    # for counting SNPs and indels (both original (.all_snps) and corrected from local misassemblies)
    total_indels_info = IndelsInfo()

    unaligned_file = open(unaligned_fpath, 'w')
    unaligned_info_file = open(unaligned_info_fpath, 'w')
    unaligned_info_file.write('\t'.join(['Contig', 'Total_length', 'Unaligned_length', 'Unaligned_type', 'Unaligned_parts']) + '\n')
    for contig, seq in fastaparser.read_fasta(contigs_fpath):
        #Recording contig stats
        ctg_len = len(seq)
        ca_output.stdout_f.write('CONTIG: %s (%dbp)\n' % (contig, ctg_len))
        contig_type = 'unaligned'
        misassemblies_in_contigs.append(0)
        contigs_aligned_lengths.append(0)
        filtered_aligns = []
        if contig in aligns:
            filtered_aligns = [align for align in aligns[contig] if align.len2 >= qconfig.min_alignment]

        #Check if this contig aligned to the reference
        if filtered_aligns:
            contig_type = 'correct'
            #Sort aligns by aligned_length * identity - unaligned_length (as we do in BSS)
            sorted_aligns = sorted(filtered_aligns, key=lambda x: (score_single_align(x), x.len2), reverse=True)
            top_len = sorted_aligns[0].len2
            top_id = sorted_aligns[0].idy
            top_score = score_single_align(sorted_aligns[0])
            top_aligns = []
            ca_output.stdout_f.write('Best alignment score: %.1f (LEN: %d, IDY: %.2f), Total number of alignments: %d\n'
                                     % (top_score, top_len, top_id, len(sorted_aligns)))

            #Check that top hit captures most of the contig
            if top_len > ctg_len * epsilon or ctg_len - top_len < maxun:
                #Reset top aligns: aligns that share the same value of longest and highest identity
                top_aligns.append(sorted_aligns[0])
                sorted_aligns = sorted_aligns[1:]

                #Continue grabbing alignments while length and identity are identical
                #while sorted_aligns and top_len == sorted_aligns[0].len2 and top_id == sorted_aligns[0].idy:
                while sorted_aligns and (score_single_align(sorted_aligns[0]) >= qconfig.ambiguity_score * top_score):
                    top_aligns.append(sorted_aligns[0])
                    sorted_aligns = sorted_aligns[1:]

                #Mark other alignments as insignificant (former ambiguous)
                if sorted_aligns:
                    ca_output.stdout_f.write('\t\tSkipping these alignments as insignificant (option --ambiguity-score is set to "%s"):\n' % str(qconfig.ambiguity_score))
                    for align in sorted_aligns:
                        ca_output.stdout_f.write('\t\t\tSkipping alignment ' + str(align) + '\n')

                if len(top_aligns) == 1:
                    #There is only one top align, life is good
                    ca_output.stdout_f.write('\t\tOne align captures most of this contig: %s\n' % str(top_aligns[0]))
                    ca_output.icarus_out_f.write(top_aligns[0].icarus_report_str() + '\n')
                    ref_aligns.setdefault(top_aligns[0].ref, []).append(top_aligns[0])
                    ca_output.coords_filtered_f.write(top_aligns[0].coords_str() + '\n')
                    aligned_lengths.append(top_aligns[0].len2)
                    contigs_aligned_lengths[-1] = top_aligns[0].len2
                else:
                    #There is more than one top align
                    ca_output.stdout_f.write('\t\tThis contig has %d significant alignments. [An ambiguously mapped contig]\n' %
                                             len(top_aligns))

                    #Increment count of ambiguously mapped contigs and bases in them
                    ambiguous_contigs += 1
                    # we count only extra bases, so we shouldn't include bases in the first alignment
                    # if --ambiguity-usage is 'none', the number of extra bases will be negative!
                    ambiguous_contigs_len += ctg_len

                    # Alex: skip all alignments or count them as normal (just different aligns of one repeat). Depend on --allow-ambiguity option
                    if qconfig.ambiguity_usage == "none":
                        ca_output.stdout_f.write('\t\tSkipping these alignments (option --ambiguity-usage is set to "none"):\n')
                        for align in top_aligns:
                            ca_output.stdout_f.write('\t\t\tSkipping alignment ' + str(align) + '\n')
                    elif qconfig.ambiguity_usage == "one":
                        ca_output.stdout_f.write('\t\tUsing only first of these alignment (option --ambiguity-usage is set to "one"):\n')
                        ca_output.stdout_f.write('\t\t\tAlignment: %s\n' % str(top_aligns[0]))
                        ca_output.icarus_out_f.write(top_aligns[0].icarus_report_str() + '\n')
                        ref_aligns.setdefault(top_aligns[0].ref, []).append(top_aligns[0])
                        aligned_lengths.append(top_aligns[0].len2)
                        contigs_aligned_lengths[-1] = top_aligns[0].len2
                        ca_output.coords_filtered_f.write(top_aligns[0].coords_str() + '\n')
                        top_aligns = top_aligns[1:]
                        for align in top_aligns:
                            ca_output.stdout_f.write('\t\t\tSkipping alignment ' + str(align) + '\n')
                    elif qconfig.ambiguity_usage == "all":
                        ca_output.stdout_f.write('\t\tUsing all these alignments (option --ambiguity-usage is set to "all"):\n')
                        # we count only extra bases, so we shouldn't include bases in the first alignment
                        ambiguous_contigs_extra_bases -= top_aligns[0].len2
                        first_alignment = True
                        contig_type = 'ambiguous'
                        while len(top_aligns):
                            ca_output.stdout_f.write('\t\t\tAlignment: %s\n' % str(top_aligns[0]))
                            ca_output.icarus_out_f.write(top_aligns[0].icarus_report_str(ambiguity=True) + '\n')
                            ca_output.coords_filtered_f.write(top_aligns[0].coords_str() + ('\n' if first_alignment else ' ambiguous\n'))
                            ref_aligns.setdefault(top_aligns[0].ref, []).append(top_aligns[0])
                            if first_alignment:
                                first_alignment = False
                                aligned_lengths.append(top_aligns[0].len2)
                                contigs_aligned_lengths[-1] = top_aligns[0].len2
                            ambiguous_contigs_extra_bases += top_aligns[0].len2
                            top_aligns = top_aligns[1:]
            else:
                # choose appropriate alignments (to maximize total size of contig alignment and reduce # misassemblies)
                is_ambiguous, too_much_best_sets, sorted_aligns, best_sets = get_best_aligns_sets(
                    sorted_aligns, ctg_len, ca_output.stdout_f, seq, ref_lens, is_cyclic, region_struct_variations)
                the_best_set = best_sets[0]
                used_indexes = list(range(len(sorted_aligns)) if too_much_best_sets else get_used_indexes(best_sets))
                if len(used_indexes) < len(sorted_aligns):
                    ca_output.stdout_f.write('\t\t\tSkipping redundant alignments after choosing the best set of alignments\n')
                    for idx in set([idx for idx in range(len(sorted_aligns)) if idx not in used_indexes]):
                        ca_output.stdout_f.write('\t\tSkipping redundant alignment ' + str(sorted_aligns[idx]) + '\n')

                if is_ambiguous:
                    ca_output.stdout_f.write('\t\tThis contig has several significant sets of alignments. [An ambiguously mapped contig]\n')
                    # similar to regular ambiguous contigs, see above
                    ambiguous_contigs += 1
                    ambiguous_contigs_len += ctg_len

                    if qconfig.ambiguity_usage == "none":
                        ambiguous_contigs_extra_bases -= (ctg_len - the_best_set.uncovered)
                        ca_output.stdout_f.write('\t\tSkipping all alignments in these sets (option --ambiguity-usage is set to "none"):\n')
                        for idx in used_indexes:
                            ca_output.stdout_f.write('\t\t\tSkipping alignment ' + str(sorted_aligns[idx]) + '\n')
                        continue
                    elif qconfig.ambiguity_usage == "one":
                        ambiguous_contigs_extra_bases += 0
                        ca_output.stdout_f.write('\t\tUsing only the very best set (option --ambiguity-usage is set to "one").\n')
                        if len(the_best_set.indexes) < len(used_indexes):
                            ca_output.stdout_f.write('\t\tSo, skipping alignments from other sets:\n')
                            for idx in used_indexes:
                                if idx not in the_best_set.indexes:
                                    ca_output.stdout_f.write('\t\t\tSkipping alignment ' + str(sorted_aligns[idx]) + '\n')
                    elif qconfig.ambiguity_usage == "all":
                        ca_output.stdout_f.write('\t\tUsing all alignments in these sets (option --ambiguity-usage is set to "all"):\n')
                        ca_output.stdout_f.write('\t\t\tThe very best set is shown in details below, the rest are:\n')
                        for idx, cur_set in enumerate(best_sets[1:]):
                            ca_output.stdout_f.write('\t\t\t\tGroup #%d. Score: %.1f, number of alignments: %d, unaligned bases: %d\n' % \
                                (idx + 2, cur_set.score, len(cur_set.indexes), cur_set.uncovered))
                        if too_much_best_sets:
                            ca_output.stdout_f.write('\t\t\t\tetc...\n')
                        ca_output.stdout_f.write('\t\t\tList of alignments used in the sets above but not in the best set (if any):\n')
                        for idx in (set(used_indexes) - set(the_best_set.indexes)):
                            align = sorted_aligns[idx]
                            ca_output.stdout_f.write('\t\tAlignment: %s\n' % str(align))
                            ref_aligns.setdefault(align.ref, []).append(align)
                            ambiguous_contigs_extra_bases += align.len2
                            ca_output.coords_filtered_f.write(align.coords_str() + " ambiguous\n")
                            ca_output.icarus_out_f.write(align.icarus_report_str(is_best=False) + '\n')

                ca_output.stdout_f.write('\t\t\tThe best set is below. Score: %.1f, number of alignments: %d, unaligned bases: %d\n' % \
                                             (the_best_set.score, len(the_best_set.indexes), the_best_set.uncovered))
                real_aligns = [sorted_aligns[i] for i in the_best_set.indexes]

                # main processing part
                if len(real_aligns) == 1:
                    the_only_align = real_aligns[0]

                    #There is only one alignment of this contig to the reference
                    ca_output.coords_filtered_f.write(the_only_align.coords_str() + '\n')
                    aligned_lengths.append(the_only_align.len2)
                    contigs_aligned_lengths[-1] = the_only_align.len2

                    begin, end = the_only_align.start(), the_only_align.end()
                    unaligned_bases = (begin - 1) + (ctg_len - end)
                    number_unaligned_ns = seq[:begin - 1].count('N') + seq[end:].count('N')
                    aligned_bases_in_contig = ctg_len - unaligned_bases
                    acgt_ctg_len = ctg_len - seq.count('N')
                    is_partially_unaligned = check_partially_unaligned(seq, real_aligns, ctg_len)
                    if is_partially_unaligned:
                        partially_unaligned += 1
                        partially_unaligned_bases += unaligned_bases - number_unaligned_ns
                        if aligned_bases_in_contig < qconfig.unaligned_mis_threshold * acgt_ctg_len:
                            contig_type = 'correct_unaligned'
                        ca_output.stdout_f.write('\t\tThis contig is partially unaligned. '
                                                 '(Aligned %d out of %d non-N bases (%.2f%%))\n'
                                                 % (aligned_bases_in_contig, acgt_ctg_len,
                                                    100.0 * aligned_bases_in_contig / acgt_ctg_len))
                        save_unaligned_info(real_aligns, contig, ctg_len, unaligned_bases, unaligned_info_file)
                    ca_output.stdout_f.write('\t\tAlignment: %s\n' % str(the_only_align))
                    ca_output.icarus_out_f.write(the_only_align.icarus_report_str() + '\n')
                    if is_partially_unaligned:
                        if begin - 1:
                            ca_output.stdout_f.write('\t\tUnaligned bases: 1 to %d (%d)\n' % (begin - 1, begin - 1))
                        if ctg_len - end:
                            ca_output.stdout_f.write('\t\tUnaligned bases: %d to %d (%d)\n' % (end + 1, ctg_len, ctg_len - end))
                        if qconfig.is_combined_ref:
                            check_for_potential_translocation(seq, ctg_len, real_aligns, region_misassemblies,
                                                              misassemblies_by_ref, ca_output.stdout_f)
                    ref_aligns.setdefault(the_only_align.ref, []).append(the_only_align)
                else:
                    #Sort real alignments by position on the contig
                    sorted_aligns = sorted(real_aligns, key=lambda x: (x.end(), x.start()))

                    #There is more than one alignment of this contig to the reference
                    ca_output.stdout_f.write('\t\tThis contig is misassembled.\n')
                    unaligned_bases = the_best_set.uncovered
                    number_unaligned_ns, prev_pos = 0, 0
                    for align in sorted_aligns:
                        number_unaligned_ns += seq[prev_pos: align.start() - 1].count('N')
                        prev_pos = align.end()
                    number_unaligned_ns += seq[prev_pos:].count('N')

                    aligned_bases_in_contig = ctg_len - unaligned_bases
                    number_ns = seq.count('N')
                    acgt_ctg_len = ctg_len - number_ns
                    is_partially_unaligned = check_partially_unaligned(seq, sorted_aligns, ctg_len)
                    if is_partially_unaligned:
                        partially_unaligned += 1
                        partially_unaligned_bases += unaligned_bases - number_unaligned_ns
                        ca_output.stdout_f.write('\t\tThis contig is partially unaligned. '
                                                 '(Aligned %d out of %d non-N bases (%.2f%%))\n'
                                                 % (aligned_bases_in_contig, acgt_ctg_len,
                                                 100.0 * aligned_bases_in_contig / acgt_ctg_len))
                        save_unaligned_info(sorted_aligns, contig, ctg_len, unaligned_bases, unaligned_info_file)

                    if aligned_bases_in_contig < qconfig.unaligned_mis_threshold * acgt_ctg_len:
                        ca_output.stdout_f.write('\t\t\tWarning! This contig is more unaligned than misassembled. ' + \
                                                 'Contig length is %d (number of Ns: %d) and total length of all aligns is %d\n' %
                                                 (ctg_len, number_ns, aligned_bases_in_contig))
                        contigs_aligned_lengths[-1] = sum(align.len2 for align in sorted_aligns)
                        for align in sorted_aligns:
                            ca_output.stdout_f.write('\t\tAlignment: %s\n' % str(align))
                            ca_output.icarus_out_f.write(align.icarus_report_str() + '\n')
                            ca_output.icarus_out_f.write('unknown\n')
                            ca_output.coords_filtered_f.write(align.coords_str() + '\n')
                            aligned_lengths.append(align.len2)
                            ref_aligns.setdefault(align.ref, []).append(align)

                        half_unaligned_with_misassembly += 1
                        ca_output.stdout_f.write('\t\tUnaligned bases: %d\n' % unaligned_bases)
                        contig_type = 'mis_unaligned'
                        ca_output.icarus_out_f.write('\t'.join(['CONTIG', contig, str(ctg_len), contig_type + '\n']))
                        ca_output.stdout_f.write('\n')
                        continue

                    ### processing misassemblies
                    is_misassembled, current_mio, indels_info, cnt_misassemblies, contig_aligned_length = \
                        process_misassembled_contig(sorted_aligns, is_cyclic, aligned_lengths, region_misassemblies,
                                                    ref_lens, ref_aligns, ref_features, seq, misassemblies_by_ref,
                                                    istranslocations_by_ref, region_struct_variations, ca_output)
                    contigs_aligned_lengths[-1] = contig_aligned_length
                    misassembly_internal_overlap += current_mio
                    total_indels_info += indels_info
                    if is_misassembled:
                        misassembled_contigs[contig] = ctg_len
                        contig_type = 'misassembled'
                        misassemblies_in_contigs[-1] = cnt_misassemblies
                    if is_partially_unaligned:
                        ca_output.stdout_f.write('\t\tUnaligned bases: %d\n' % unaligned_bases)
                        if qconfig.is_combined_ref:
                            check_for_potential_translocation(seq, ctg_len, sorted_aligns, region_misassemblies,
                                                              misassemblies_by_ref, ca_output.stdout_f)
        else:
            #No aligns to this contig
            ca_output.stdout_f.write('\t\tThis contig is unaligned. (%d bp)\n' % ctg_len)
            unaligned_file.write(contig + '\n')

            #Increment unaligned contig count and bases
            unaligned += 1
            number_ns = seq.count('N')
            fully_unaligned_bases += ctg_len - number_ns
            ca_output.stdout_f.write('\t\tUnaligned bases: %d (number of Ns: %d)\n' % (ctg_len, number_ns))
            save_unaligned_info([], contig, ctg_len, ctg_len, unaligned_info_file)

        ca_output.icarus_out_f.write('\t'.join(['CONTIG', contig, str(ctg_len), contig_type]) + '\n')
        ca_output.stdout_f.write('\n')

    unaligned_file.close()
    unaligned_info_file.close()
    misassembled_bases = sum(misassembled_contigs.values())

    # special case: --skip-unaligned-mis-contigs is specified
    if qconfig.unaligned_mis_threshold == 0.0:
        half_unaligned_with_misassembly = None

    result = {'region_misassemblies': region_misassemblies,
              'misassembled_contigs': misassembled_contigs, 'misassembled_bases': misassembled_bases,
              'misassembly_internal_overlap': misassembly_internal_overlap,
              'unaligned': unaligned, 'partially_unaligned': partially_unaligned,
              'partially_unaligned_bases': partially_unaligned_bases, 'fully_unaligned_bases': fully_unaligned_bases,
              'aligned_assembly_bases': sum(contigs_aligned_lengths),
              'ambiguous_contigs': ambiguous_contigs, 'ambiguous_contigs_extra_bases': ambiguous_contigs_extra_bases,
              'ambiguous_contigs_len': ambiguous_contigs_len,
              'half_unaligned_with_misassembly': half_unaligned_with_misassembly,
              'misassemblies_by_ref': misassemblies_by_ref,
              'istranslocations_by_refs': istranslocations_by_ref}

    return result, ref_aligns, total_indels_info, aligned_lengths, misassembled_contigs, misassemblies_in_contigs, contigs_aligned_lengths