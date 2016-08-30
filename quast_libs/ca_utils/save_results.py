from quast_libs import qconfig, reporting
from quast_libs.ca_utils.analyze_misassemblies import Misassembly


def print_results(contigs_fpath, log_out_f, used_snps_fpath, total_indels_info, result):
    gaps = result['gaps']
    neg_gaps = result['neg_gaps']
    misassembled_contigs = result['misassembled_contigs']
    region_misassemblies = result['region_misassemblies']
    print >> log_out_f, 'Analysis is finished!'
    if qconfig.show_snps:
        print >> log_out_f, 'Founded SNPs were written into', used_snps_fpath
    print >> log_out_f, '\nResults:'

    print >> log_out_f, '\tLocal Misassemblies: %d' % region_misassemblies.count(Misassembly.LOCAL)
    print >> log_out_f, '\tMisassemblies: %d' % (len(region_misassemblies) - region_misassemblies.count(Misassembly.LOCAL)
                                                    - region_misassemblies.count(Misassembly.SCAFFOLD_GAP) - region_misassemblies.count(Misassembly.FRAGMENTED))
    print >> log_out_f, '\t\tRelocations: %d' % region_misassemblies.count(Misassembly.RELOCATION)
    print >> log_out_f, '\t\tTranslocations: %d' % region_misassemblies.count(Misassembly.TRANSLOCATION)
    if qconfig.is_combined_ref:
        print >> log_out_f, '\t\tInterspecies translocations: %d' % region_misassemblies.count(Misassembly.INTERSPECTRANSLOCATION)
    print >> log_out_f, '\t\tInversions: %d' % region_misassemblies.count(Misassembly.INVERSION)
    if qconfig.is_combined_ref:
        print >> log_out_f, '\tPotentially Misassembled Contigs (i/s translocations): %d' % result['contigs_with_istranslocations']
    if qconfig.scaffolds and contigs_fpath not in qconfig.dict_of_broken_scaffolds:
        print >> log_out_f, '\tScaffold gap misassemblies: %d' % region_misassemblies.count(Misassembly.SCAFFOLD_GAP)
    if qconfig.bed:
        print >> log_out_f, '\tFake misassemblies matched with structural variations: %d' % result['misassemblies_matched_sv']

    if qconfig.check_for_fragmented_ref:
        print >> log_out_f, '\tMisassemblies caused by fragmented reference: %d' % region_misassemblies.count(Misassembly.FRAGMENTED)
    print >> log_out_f, '\tMisassembled Contigs: %d' % len(misassembled_contigs)
    print >> log_out_f, '\tMisassembled Contig Bases: %d' % result['misassembled_bases']
    print >> log_out_f, '\tMisassemblies Inter-Contig Overlap: %d' % result['misassembly_internal_overlap']
    print >> log_out_f, 'Uncovered Regions: %d (%d)' % (result['uncovered_regions'], result['uncovered_region_bases'])
    print >> log_out_f, 'Unaligned Contigs: %d + %d part' % (result['unaligned'], result['partially_unaligned'])
    print >> log_out_f, 'Partially Unaligned Contigs with Misassemblies: %d' % result['partially_unaligned_with_misassembly']
    print >> log_out_f, 'Unaligned Contig Bases: %d' % (result['fully_unaligned_bases'] + result['partially_unaligned_bases'])

    print >> log_out_f, ''
    print >> log_out_f, 'Ambiguously Mapped Contigs: %d' % result['ambiguous_contigs']
    print >> log_out_f, 'Total Bases in Ambiguously Mapped Contigs: %d' % (result['ambiguous_contigs_len'])
    print >> log_out_f, 'Extra Bases in Ambiguously Mapped Contigs: %d' % result['ambiguous_contigs_extra_bases']
    if qconfig.ambiguity_usage == "all":
        print >> log_out_f, 'Note that --allow-ambiguity option was set to "all" and each of these contigs was used several times.'
    elif qconfig.ambiguity_usage == "none":
        print >> log_out_f, 'Note that --allow-ambiguity option was set to "none" and these contigs were skipped.'
    elif qconfig.ambiguity_usage == "one":
        print >> log_out_f, 'Note that --allow-ambiguity option was set to "one" and only first alignment per each of these contigs was used.'

    if qconfig.show_snps:
        #print >> log_out_f, 'Mismatches: %d' % result['SNPs']
        #print >> log_out_f, 'Single Nucleotide Indels: %d' % result['indels']

        print >> log_out_f, ''
        print >> log_out_f, '\tCovered Bases: %d' % result['region_covered']
        #print >> log_out_f, '\tAmbiguous Bases (e.g. N\'s): %d' % result['region_ambig']
        print >> log_out_f, ''
        print >> log_out_f, '\tSNPs: %d' % total_indels_info.mismatches
        print >> log_out_f, '\tInsertions: %d' % total_indels_info.insertions
        print >> log_out_f, '\tDeletions: %d' % total_indels_info.deletions
        #print >> log_out_f, '\tList of indels lengths:', indels_list
        print >> log_out_f, ''
        print >> log_out_f, '\tPositive Gaps: %d' % len(gaps)
        internal = 0
        external = 0
        summ = 0
        for gap in gaps:
            if gap[1] == gap[2]:
                internal += 1
            else:
                external += 1
                summ += gap[0]
        print >> log_out_f, '\t\tInternal Gaps: % d' % internal
        print >> log_out_f, '\t\tExternal Gaps: % d' % external
        print >> log_out_f, '\t\tExternal Gap Total: % d' % summ
        if external:
            avg = summ * 1.0 / external
        else:
            avg = 0.0
        print >> log_out_f, '\t\tExternal Gap Average: %.0f' % avg

        print >> log_out_f, '\tNegative Gaps: %d' % len(neg_gaps)
        internal = 0
        external = 0
        summ = 0
        for gap in neg_gaps:
            if gap[1] == gap[2]:
                internal += 1
            else:
                external += 1
                summ += gap[0]
        print >> log_out_f, '\t\tInternal Overlaps: % d' % internal
        print >> log_out_f, '\t\tExternal Overlaps: % d' % external
        print >> log_out_f, '\t\tExternal Overlaps Total: % d' % summ
        if external:
            avg = summ * 1.0 / external
        else:
            avg = 0.0
        print >> log_out_f, '\t\tExternal Overlaps Average: %.0f' % avg

        redundant = list(set(result['redundant']))
        print >> log_out_f, '\tContigs with Redundant Alignments: %d (%d)' % (len(redundant), result['total_redundant'])
    return result


def save_result(result, report, fname):
    region_misassemblies = result['region_misassemblies']
    region_struct_variations = result['region_struct_variations']
    misassemblies_matched_sv = result['misassemblies_matched_sv']
    misassembled_contigs = result['misassembled_contigs']
    misassembled_bases = result['misassembled_bases']
    misassembly_internal_overlap = result['misassembly_internal_overlap']
    unaligned = result['unaligned']
    partially_unaligned = result['partially_unaligned']
    partially_unaligned_bases = result['partially_unaligned_bases']
    fully_unaligned_bases = result['fully_unaligned_bases']
    ambiguous_contigs = result['ambiguous_contigs']
    ambiguous_contigs_extra_bases = result['ambiguous_contigs_extra_bases']
    SNPs = result['SNPs']
    indels_list = result['indels_list']
    total_aligned_bases = result['total_aligned_bases']
    partially_unaligned_with_misassembly = result['partially_unaligned_with_misassembly']
    partially_unaligned_with_significant_parts = result['partially_unaligned_with_significant_parts']
    contigs_with_istranslocations = result['contigs_with_istranslocations']

    report.add_field(reporting.Fields.MISLOCAL, region_misassemblies.count(Misassembly.LOCAL))
    report.add_field(reporting.Fields.MISASSEMBL, len(region_misassemblies) - region_misassemblies.count(Misassembly.LOCAL)
                     - region_misassemblies.count(Misassembly.SCAFFOLD_GAP) - region_misassemblies.count(Misassembly.FRAGMENTED))
    report.add_field(reporting.Fields.MISCONTIGS, len(misassembled_contigs))
    report.add_field(reporting.Fields.MISCONTIGSBASES, misassembled_bases)
    report.add_field(reporting.Fields.MISINTERNALOVERLAP, misassembly_internal_overlap)
    if qconfig.bed:
        report.add_field(reporting.Fields.STRUCT_VARIATIONS, misassemblies_matched_sv)
    report.add_field(reporting.Fields.UNALIGNED, '%d + %d part' % (unaligned, partially_unaligned))
    report.add_field(reporting.Fields.UNALIGNEDBASES, (fully_unaligned_bases + partially_unaligned_bases))
    report.add_field(reporting.Fields.AMBIGUOUS, ambiguous_contigs)
    report.add_field(reporting.Fields.AMBIGUOUSEXTRABASES, ambiguous_contigs_extra_bases)
    report.add_field(reporting.Fields.MISMATCHES, SNPs)
    # different types of indels:
    if indels_list is not None:
        report.add_field(reporting.Fields.INDELS, len(indels_list))
        report.add_field(reporting.Fields.INDELSBASES, sum(indels_list))
        report.add_field(reporting.Fields.MIS_SHORT_INDELS, len([i for i in indels_list if i <= qconfig.SHORT_INDEL_THRESHOLD]))
        report.add_field(reporting.Fields.MIS_LONG_INDELS, len([i for i in indels_list if i > qconfig.SHORT_INDEL_THRESHOLD]))

    if total_aligned_bases:
        report.add_field(reporting.Fields.SUBSERROR, "%.2f" % (float(SNPs) * 100000.0 / float(total_aligned_bases)))
        report.add_field(reporting.Fields.INDELSERROR, "%.2f" % (float(report.get_field(reporting.Fields.INDELS))
                                                                 * 100000.0 / float(total_aligned_bases)))

    # for misassemblies report:
    report.add_field(reporting.Fields.MIS_ALL_EXTENSIVE, len(region_misassemblies) - region_misassemblies.count(Misassembly.LOCAL)
                     - region_misassemblies.count(Misassembly.SCAFFOLD_GAP) - region_misassemblies.count(Misassembly.FRAGMENTED))
    report.add_field(reporting.Fields.MIS_RELOCATION, region_misassemblies.count(Misassembly.RELOCATION))
    report.add_field(reporting.Fields.MIS_TRANSLOCATION, region_misassemblies.count(Misassembly.TRANSLOCATION))
    report.add_field(reporting.Fields.MIS_INVERTION, region_misassemblies.count(Misassembly.INVERSION))
    report.add_field(reporting.Fields.MIS_EXTENSIVE_CONTIGS, len(misassembled_contigs))
    report.add_field(reporting.Fields.MIS_EXTENSIVE_BASES, misassembled_bases)
    report.add_field(reporting.Fields.MIS_LOCAL, region_misassemblies.count(Misassembly.LOCAL))
    if qconfig.is_combined_ref:
        report.add_field(reporting.Fields.MIS_ISTRANSLOCATIONS, region_misassemblies.count(Misassembly.INTERSPECTRANSLOCATION))
    if qconfig.meta:
        report.add_field(reporting.Fields.CONTIGS_WITH_ISTRANSLOCATIONS, contigs_with_istranslocations)
    if qconfig.scaffolds and fname not in qconfig.dict_of_broken_scaffolds:
        report.add_field(reporting.Fields.MIS_SCAFFOLDS_GAP, region_misassemblies.count(Misassembly.SCAFFOLD_GAP))
    if qconfig.check_for_fragmented_ref:
        report.add_field(reporting.Fields.MIS_FRAGMENTED, region_misassemblies.count(Misassembly.FRAGMENTED))

    # for unaligned report:
    report.add_field(reporting.Fields.UNALIGNED_FULL_CNTGS, unaligned)
    report.add_field(reporting.Fields.UNALIGNED_FULL_LENGTH, fully_unaligned_bases)
    report.add_field(reporting.Fields.UNALIGNED_PART_CNTGS, partially_unaligned)
    report.add_field(reporting.Fields.UNALIGNED_PART_WITH_MISASSEMBLY, partially_unaligned_with_misassembly)
    report.add_field(reporting.Fields.UNALIGNED_PART_SIGNIFICANT_PARTS, partially_unaligned_with_significant_parts)
    report.add_field(reporting.Fields.UNALIGNED_PART_LENGTH, partially_unaligned_bases)
    return report


def save_result_for_unaligned(result, report):
    unaligned_ctgs = report.get_field(reporting.Fields.CONTIGS)
    unaligned_length = report.get_field(reporting.Fields.TOTALLEN)
    report.add_field(reporting.Fields.UNALIGNED, '%d + %d part' % (unaligned_ctgs, 0))
    report.add_field(reporting.Fields.UNALIGNEDBASES, unaligned_length)

    report.add_field(reporting.Fields.UNALIGNED_FULL_CNTGS, unaligned_ctgs)
    report.add_field(reporting.Fields.UNALIGNED_FULL_LENGTH, unaligned_length)

