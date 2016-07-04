############################################################################
# Copyright (c) 2015-2016 Saint Petersburg State University
# Copyright (c) 2011-2015 Saint Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################
from libs import fastaparser, qconfig
from libs.ca_utils.analyze_misassemblies import process_misassembled_contig, IndelsInfo, Misassembly, find_all_sv
from libs.ca_utils.best_set_selection import get_best_aligns_set
from libs.ca_utils.misc import ref_labels_by_chromosomes


def check_for_potential_translocation(seq, ctg_len, sorted_aligns, log_out_f):
    count_ns = 0
    unaligned_len = 0
    prev_start = 0
    for align in sorted_aligns:
        if align.start() > prev_start + 1:
            unaligned_part = seq[prev_start + 1: align.start()]
            unaligned_len += len(unaligned_part)
            count_ns += unaligned_part.count('N')
        prev_start = align.end()
    if ctg_len > sorted_aligns[-1].end() + 1:
        unaligned_part = seq[sorted_aligns[-1].end() + 1: ctg_len]
        unaligned_len += len(unaligned_part)
        count_ns += unaligned_part.count('N')
    # if contig consists mostly of Ns, it cannot contain interspecies translocations
    if count_ns / float(unaligned_len) >= 0.95 or unaligned_len - count_ns < qconfig.significant_part_size:
        return 0

    print >> log_out_f, '\t\tIt can contain interspecies translocations.'
    return 1


def analyze_contigs(ca_output, contigs_fpath, unaligned_fpath, aligns, ref_features, ref_lens, cyclic=None):
    maxun = 10
    epsilon = 0.99
    umt = 0.5  # threshold for misassembled contigs with aligned less than $umt * 100% (Unaligned Missassembled Threshold)
    ort = 0.9  # threshold for skipping aligns that significantly overlaps with adjacent aligns (Overlap Relative Threshold)
    oat = 25   # threshold for skipping aligns that significantly overlaps with adjacent aligns (Overlap Absolute Threshold)
    odgap = 1000 # threshold for detecting aligns that significantly overlaps with adjacent aligns (Overlap Detecting Gap)

    unaligned = 0
    partially_unaligned = 0
    fully_unaligned_bases = 0
    partially_unaligned_bases = 0
    ambiguous_contigs = 0
    ambiguous_contigs_extra_bases = 0
    partially_unaligned_with_misassembly = 0
    partially_unaligned_with_significant_parts = 0
    misassembly_internal_overlap = 0
    contigs_with_istranslocations = 0
    misassemblies_matched_sv = 0

    ref_aligns = dict()
    aligned_lengths = []
    region_misassemblies = []
    misassembled_contigs = dict()

    region_struct_variations = find_all_sv(qconfig.bed)

    references_misassemblies = {}
    for ref in ref_labels_by_chromosomes.values():
        references_misassemblies[ref] = dict((key, 0) for key in ref_labels_by_chromosomes.values())

    # for counting SNPs and indels (both original (.all_snps) and corrected from Nucmer's local misassemblies)
    total_indels_info = IndelsInfo()

    unaligned_file = open(unaligned_fpath, 'w')
    for contig, seq in fastaparser.read_fasta(contigs_fpath):
        #Recording contig stats
        ctg_len = len(seq)
        print >> ca_output.stdout_f, 'CONTIG: %s (%dbp)' % (contig, ctg_len)
        contig_type = 'unaligned'

        #Check if this contig aligned to the reference
        if contig in aligns:
            for align in aligns[contig]:
                sub_seq = seq[align.start(): align.end()]
                if 'N' in sub_seq:
                    ns_pos = [pos for pos in xrange(align.start(), align.end()) if seq[pos] == 'N']
            contig_type = 'correct'
            #Pull all aligns for this contig
            num_aligns = len(aligns[contig])

            #Sort aligns by length and identity
            sorted_aligns = sorted(aligns[contig], key=lambda x: (x.len2 * x.idy, x.len2), reverse=True)
            top_len = sorted_aligns[0].len2
            top_id = sorted_aligns[0].idy
            top_aligns = []
            print >> ca_output.stdout_f, 'Top Length: %s  Top ID: %s' % (top_len, top_id)

            #Check that top hit captures most of the contig
            if top_len > ctg_len * epsilon or ctg_len - top_len < maxun:
                #Reset top aligns: aligns that share the same value of longest and highest identity
                top_aligns.append(sorted_aligns[0])
                sorted_aligns = sorted_aligns[1:]

                #Continue grabbing alignments while length and identity are identical
                #while sorted_aligns and top_len == sorted_aligns[0].len2 and top_id == sorted_aligns[0].idy:
                while sorted_aligns and (sorted_aligns[0].len2 * sorted_aligns[0].idy >= qconfig.ambiguity_score * top_len * top_id):
                    top_aligns.append(sorted_aligns[0])
                    sorted_aligns = sorted_aligns[1:]

                #Mark other alignments as insignificant (former ambiguous)
                if sorted_aligns:
                    print >> ca_output.stdout_f, '\t\tSkipping these alignments as insignificant (option --ambiguity-score is set to "%s"):' % str(qconfig.ambiguity_score)
                    for align in sorted_aligns:
                        print >> ca_output.stdout_f, '\t\t\tSkipping alignment ', align

                if len(top_aligns) == 1:
                    #There is only one top align, life is good
                    print >> ca_output.stdout_f, '\t\tOne align captures most of this contig: %s' % str(top_aligns[0])
                    print >> ca_output.icarus_out_f, top_aligns[0].icarus_report_str()
                    ref_aligns.setdefault(top_aligns[0].ref, []).append(top_aligns[0])
                    print >> ca_output.coords_filtered_f, str(top_aligns[0])
                    aligned_lengths.append(top_aligns[0].len2)
                else:
                    #There is more than one top align
                    print >> ca_output.stdout_f, '\t\tThis contig has %d significant alignments. [An ambiguously mapped contig]' % len(
                        top_aligns)

                    #Increment count of ambiguously mapped contigs and bases in them
                    ambiguous_contigs += 1
                    # we count only extra bases, so we shouldn't include bases in the first alignment
                    # in case --allow-ambiguity is not set the number of extra bases will be negative!
                    ambiguous_contigs_extra_bases -= top_aligns[0].len2

                    # Alex: skip all alignments or count them as normal (just different aligns of one repeat). Depend on --allow-ambiguity option
                    if qconfig.ambiguity_usage == "none":
                        print >> ca_output.stdout_f, '\t\tSkipping these alignments (option --ambiguity-usage is set to "none"):'
                        for align in top_aligns:
                            print >> ca_output.stdout_f, '\t\t\tSkipping alignment ', align
                    elif qconfig.ambiguity_usage == "one":
                        print >> ca_output.stdout_f, '\t\tUsing only first of these alignment (option --ambiguity-usage is set to "one"):'
                        print >> ca_output.stdout_f, '\t\t\tAlignment: %s' % str(top_aligns[0])
                        print >> ca_output.icarus_out_f, top_aligns[0].icarus_report_str()
                        ref_aligns.setdefault(top_aligns[0].ref, []).append(top_aligns[0])
                        aligned_lengths.append(top_aligns[0].len2)
                        print >> ca_output.coords_filtered_f, str(top_aligns[0])
                        top_aligns = top_aligns[1:]
                        for align in top_aligns:
                            print >> ca_output.stdout_f, '\t\t\tSkipping alignment ', align
                    elif qconfig.ambiguity_usage == "all":
                        print >> ca_output.stdout_f, '\t\tUsing all these alignments (option --ambiguity-usage is set to "all"):'
                        # we count only extra bases, so we shouldn't include bases in the first alignment
                        first_alignment = True
                        while len(top_aligns):
                            print >> ca_output.stdout_f, '\t\t\tAlignment: %s' % str(top_aligns[0])
                            print >> ca_output.icarus_out_f, top_aligns[0].icarus_report_str(ambiguity=True)
                            ref_aligns.setdefault(top_aligns[0].ref, []).append(top_aligns[0])
                            if first_alignment:
                                first_alignment = False
                                aligned_lengths.append(top_aligns[0].len2)
                            ambiguous_contigs_extra_bases += top_aligns[0].len2
                            print >> ca_output.coords_filtered_f, str(top_aligns[0]), "ambiguous"
                            top_aligns = top_aligns[1:]
            else:
                # choose appropriate alignments (to maximize total size of contig alignment and reduce # misassemblies)
                if len(sorted_aligns) > 0:
                    sorted_aligns = sorted(sorted_aligns, key=lambda x: (x.end(), x.start()))
                    real_aligns = get_best_aligns_set(sorted_aligns, ctg_len, ca_output.stdout_f, seq,
                                                      cyclic_ref_lens=ref_lens if cyclic else None, region_struct_variations=region_struct_variations)
                    if len(sorted_aligns) > len(real_aligns):
                        print >> ca_output.stdout_f, '\t\t\tSkipping redundant alignments after choosing the best set of alignments'
                        for align in sorted_aligns:
                            if align not in real_aligns:
                                print >> ca_output.stdout_f, '\t\tSkipping redundant alignment %s' % (str(align))

                if len(real_aligns) == 1:
                    the_only_align = real_aligns[0]

                    #There is only one alignment of this contig to the reference
                    print >> ca_output.coords_filtered_f, str(the_only_align)
                    aligned_lengths.append(the_only_align.len2)

                    #Is the contig aligned in the reverse compliment?
                    #Record beginning and end of alignment in contig
                    if the_only_align.s2 > the_only_align.e2:
                        end, begin = the_only_align.s2, the_only_align.e2
                    else:
                        end, begin = the_only_align.e2, the_only_align.s2

                    if (begin - 1) or (ctg_len - end):
                        #Increment tally of partially unaligned contigs
                        partially_unaligned += 1

                        #Increment tally of partially unaligned bases
                        unaligned_bases = (begin - 1) + (ctg_len - end)
                        partially_unaligned_bases += unaligned_bases
                        print >> ca_output.stdout_f, '\t\tThis contig is partially unaligned. (Aligned %d out of %d bases)' % (top_len, ctg_len)
                        print >> ca_output.stdout_f, '\t\tAlignment: %s' % str(the_only_align)
                        print >> ca_output.icarus_out_f, the_only_align.icarus_report_str()
                        if begin - 1:
                            print >> ca_output.stdout_f, '\t\tUnaligned bases: 1 to %d (%d)' % (begin - 1, begin - 1)
                        if ctg_len - end:
                            print >> ca_output.stdout_f, '\t\tUnaligned bases: %d to %d (%d)' % (end + 1, ctg_len, ctg_len - end)
                        # check if both parts (aligned and unaligned) have significant length
                        if (unaligned_bases >= qconfig.significant_part_size) and (ctg_len - unaligned_bases >= qconfig.significant_part_size):
                            partially_unaligned_with_significant_parts += 1
                            print >> ca_output.stdout_f, '\t\tThis contig has both significant aligned and unaligned parts ' \
                                                         '(of length >= %d)!' % (qconfig.significant_part_size)
                            if qconfig.meta:
                                contigs_with_istranslocations += check_for_potential_translocation(seq, ctg_len, real_aligns,
                                                                                                   ca_output.stdout_f)
                    ref_aligns.setdefault(the_only_align.ref, []).append(the_only_align)
                else:
                    #Sort real alignments by position on the contig
                    sorted_aligns = sorted(real_aligns, key=lambda x: (x.end(), x.start()))

                    ## OBSOLETE CODE IS BELOW: now Best Set Selection should do this job more carefully
                    #Extra skipping of redundant alignments (fully or almost fully covered by adjacent alignments)
                    # if len(sorted_aligns) >= 3:
                    #     was_extra_skip = False
                    #     prev_end = sorted_aligns[0].end()
                    #     for i in range(1, len(sorted_aligns) - 1):
                    #         succ_start = sorted_aligns[i + 1].start()
                    #         gap = succ_start - prev_end - 1
                    #         if gap > odgap:
                    #             prev_end = sorted_aligns[i].end()
                    #             continue
                    #         overlap = 0
                    #         if prev_end - sorted_aligns[i].start() + 1 > 0:
                    #             overlap += prev_end - sorted_aligns[i].start() + 1
                    #         if sorted_aligns[i].end() - succ_start + 1 > 0:
                    #             overlap += sorted_aligns[i].end() - succ_start + 1
                    #         if gap < oat or (float(overlap) / sorted_aligns[i].len2) > ort:
                    #             if not was_extra_skip:
                    #                 was_extra_skip = True
                    #                 print >> ca_output.stdout_f, '\t\t\tSkipping redundant alignments which significantly overlap with adjacent alignments'
                    #             print >> ca_output.stdout_f, '\t\tSkipping redundant alignment %s' % (str(sorted_aligns[i]))
                    #             real_aligns.remove(sorted_aligns[i])
                    #         else:
                    #             prev_end = sorted_aligns[i].end()
                    #     if was_extra_skip:
                    #         sorted_aligns = sorted(real_aligns, key=lambda x: (x.start(), x.end()))

                    #There is more than one alignment of this contig to the reference
                    print >> ca_output.stdout_f, '\t\tThis contig is misassembled. %d total aligns.' % num_aligns

                    # Counting misassembled contigs which are mostly partially unaligned
                    # counting aligned and unaligned bases of a contig
                    aligned_bases_in_contig = 0
                    last_e2 = 0
                    for cur_align in sorted_aligns:
                        if cur_align.end() <= last_e2:
                            continue
                        elif cur_align.start() > last_e2:
                            aligned_bases_in_contig += (abs(cur_align.e2 - cur_align.s2) + 1)
                        else:
                            aligned_bases_in_contig += (cur_align.end() - last_e2)
                        last_e2 = cur_align.end()

                    #aligned_bases_in_contig = sum(x.len2 for x in sorted_aligns)
                    if aligned_bases_in_contig < umt * ctg_len:
                        print >> ca_output.stdout_f, '\t\t\tWarning! This contig is more unaligned than misassembled. ' + \
                            'Contig length is %d and total length of all aligns is %d' % (ctg_len, aligned_bases_in_contig)
                        partially_unaligned_with_misassembly += 1
                        for align in sorted_aligns:
                            print >> ca_output.stdout_f, '\t\tAlignment: %s' % str(align)
                            print >> ca_output.icarus_out_f, align.icarus_report_str()
                            print >> ca_output.coords_filtered_f, str(align)
                            aligned_lengths.append(align.len2)
                            ref_aligns.setdefault(align.ref, []).append(align)

                        #Increment tally of partially unaligned contigs
                        partially_unaligned += 1
                        #Increment tally of partially unaligned bases
                        partially_unaligned_bases += ctg_len - aligned_bases_in_contig
                        print >> ca_output.stdout_f, '\t\tUnaligned bases: %d' % (ctg_len - aligned_bases_in_contig)
                        # check if both parts (aligned and unaligned) have significant length
                        if (aligned_bases_in_contig >= qconfig.significant_part_size) and (ctg_len - aligned_bases_in_contig >= qconfig.significant_part_size):
                            partially_unaligned_with_significant_parts += 1
                            print >> ca_output.stdout_f, '\t\tThis contig has both significant aligned and unaligned parts ' \
                                                         '(of length >= %d)!' % (qconfig.significant_part_size)
                            if qconfig.meta:
                                contigs_with_istranslocations += check_for_potential_translocation(seq, ctg_len, sorted_aligns,
                                                                                                   ca_output.stdout_f)
                        continue

                    ### processing misassemblies
                    is_misassembled, current_mio, references_misassemblies, indels_info, misassemblies_matched_sv = \
                        process_misassembled_contig(sorted_aligns, cyclic, aligned_lengths, region_misassemblies, ref_lens,
                                                    ref_aligns, ref_features, seq, references_misassemblies, region_struct_variations, misassemblies_matched_sv, ca_output)
                    misassembly_internal_overlap += current_mio
                    total_indels_info += indels_info
                    if is_misassembled:
                        misassembled_contigs[contig] = len(seq)
                        contig_type = 'misassembled'
                    if ctg_len - aligned_bases_in_contig >= qconfig.significant_part_size:
                        print >> ca_output.stdout_f, '\t\tThis contig has significant unaligned parts ' \
                                                     '(of length >= %d)!' % (qconfig.significant_part_size)
                        if qconfig.meta:
                            contigs_with_istranslocations += check_for_potential_translocation(seq, ctg_len, sorted_aligns,
                                                                                               ca_output.stdout_f)
        else:
            #No aligns to this contig
            print >> ca_output.stdout_f, '\t\tThis contig is unaligned. (%d bp)' % ctg_len
            print >> unaligned_file, contig

            #Increment unaligned contig count and bases
            unaligned += 1
            fully_unaligned_bases += ctg_len
            print >> ca_output.stdout_f, '\t\tUnaligned bases: %d  total: %d' % (ctg_len, fully_unaligned_bases)

        print >> ca_output.icarus_out_f, '\t'.join(['CONTIG', contig, str(ctg_len), contig_type])
        print >> ca_output.stdout_f

    ca_output.coords_filtered_f.close()
    unaligned_file.close()
    misassembled_bases = sum(misassembled_contigs.itervalues())

    result = {'region_misassemblies': region_misassemblies,
              'region_struct_variations': region_struct_variations.get_count() if region_struct_variations else None,
              'misassemblies_matched_sv': misassemblies_matched_sv,
              'misassembled_contigs': misassembled_contigs, 'misassembled_bases': misassembled_bases,
              'misassembly_internal_overlap': misassembly_internal_overlap,
              'unaligned': unaligned, 'partially_unaligned': partially_unaligned,
              'partially_unaligned_bases': partially_unaligned_bases, 'fully_unaligned_bases': fully_unaligned_bases,
              'ambiguous_contigs': ambiguous_contigs, 'ambiguous_contigs_extra_bases': ambiguous_contigs_extra_bases,
              'partially_unaligned_with_misassembly': partially_unaligned_with_misassembly,
              'partially_unaligned_with_significant_parts': partially_unaligned_with_significant_parts,
              'contigs_with_istranslocations': contigs_with_istranslocations,
              'istranslocations_by_refs': references_misassemblies}

    return result, ref_aligns, total_indels_info, aligned_lengths, misassembled_contigs