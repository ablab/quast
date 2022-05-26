############################################################################
# Copyright (c) 2015-2022 Saint Petersburg State University
# Copyright (c) 2011-2015 Saint Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################

from __future__ import with_statement
import logging
import os
from collections import defaultdict

from quast_libs import fastaparser, genes_parser, reporting, qconfig, qutils
from quast_libs.log import get_logger
from quast_libs.qutils import run_parallel

logger = get_logger(qconfig.LOGGER_DEFAULT_NAME)
ref_lengths_by_contigs = {}


# reading genes and operons
class FeatureContainer:
    def __init__(self, fpaths, kind=''):
        self.kind = kind  # 'gene' or 'operon'
        self.fpaths = fpaths
        self.region_list = []
        self.chr_names_dict = {}


def get_ref_aligned_lengths():
    return ref_lengths_by_contigs


def chromosomes_names_dict(feature, regions, chr_names):
    """
    returns dictionary to translate chromosome name in list of features (genes or operons) to
    chromosome name in reference file.
    """
    region_2_chr_name = {}

    for region in regions:
        if region.seqname in chr_names:
            region_2_chr_name[region.seqname] = region.seqname
        else:
            region_2_chr_name[region.seqname] = None

    if len(chr_names) == 1 and len(region_2_chr_name) == 1 and region_2_chr_name[regions[0].seqname] is None:
        chr_name = chr_names.pop()
        logger.notice('Reference name in file with genomic features of type "%s" (%s) does not match the name in the reference file (%s). '
                      'QUAST will ignore this issue and count as if they match.' %
                      (feature, regions[0].seqname, chr_name),
               indent='  ')
        for region in regions:
            region.seqname = chr_name
            region_2_chr_name[region.seqname] = chr_name
    elif all(chr_name is None for chr_name in region_2_chr_name.values()):
        logger.warning('Reference names in file with genomic features of type "%s" do not match any chromosome. Check your genomic feature file(s).' % (feature),
                indent='  ')
    elif None in region_2_chr_name.values():
        logger.warning('Some of the reference names in file with genomic features of type "%s" does not match any chromosome. '
                       'Check your genomic feature file(s).' % (feature), indent='  ')

    return region_2_chr_name


def process_single_file(contigs_fpath, index, coords_dirpath, genome_stats_dirpath,
                        reference_chromosomes, ns_by_chromosomes, containers):
    assembly_label = qutils.label_from_fpath(contigs_fpath)
    corr_assembly_label = qutils.label_from_fpath_for_fname(contigs_fpath)
    results = dict()
    ref_lengths = defaultdict(int)
    logger.info('  ' + qutils.index_to_str(index) + assembly_label)

    coords_base_fpath = os.path.join(coords_dirpath, corr_assembly_label + '.coords')
    if qconfig.use_all_alignments:
        coords_fpath = coords_base_fpath
    else:
        coords_fpath = coords_base_fpath + '.filtered'

    if not os.path.isfile(coords_fpath):
        logger.error('File with alignment coords (' + coords_fpath + ') not found! Try to restart QUAST.',
            indent='  ')
        return None, None

    # EXAMPLE:
    #    [S1]     [E1]  |     [S2]     [E2]  |  [LEN 1]  [LEN 2]  |  [% IDY]  | [TAGS]
    #=====================================================================================
    #  338980   339138  |     2298     2134  |      159      165  |    79.76  | gi|48994873|gb|U00096.2|	NODE_0_length_6088
    #  374145   374355  |     2306     2097  |      211      210  |    85.45  | gi|48994873|gb|U00096.2|	NODE_0_length_6088

    genome_mapping = {}
    for chr_name, chr_len in reference_chromosomes.items():
        genome_mapping[chr_name] = [0] * (chr_len + 1)

    contig_tuples = fastaparser.read_fasta(contigs_fpath)  # list of FASTA entries (in tuples: name, seq)
    sorted_contig_tuples = sorted(enumerate(contig_tuples), key=lambda x: len(x[1][1]), reverse=True)
    sorted_contigs_names = []
    contigs_order = []
    for idx, (name, _) in sorted_contig_tuples:
        sorted_contigs_names.append(name)
        contigs_order.append(idx)

    features_in_contigs = [0] * len(sorted_contigs_names)  # for cumulative plots: i-th element is the number of genes in i-th contig
    operons_in_contigs = [0] * len(sorted_contigs_names)
    aligned_blocks_by_contig_name = {} # for gene finding: contig_name --> list of AlignedBlock

    gene_searching_enabled = len(containers)
    if qconfig.memory_efficient and gene_searching_enabled:
        logger.warning('Analysis of genes and/or operons files (provided with -g and -O) requires extensive RAM usage, consider running QUAST without them if memory consumption is critical.')
    if gene_searching_enabled:
        for name in sorted_contigs_names:
            aligned_blocks_by_contig_name[name] = []
    with open(coords_fpath) as coordfile:
        for line in coordfile:
            s1 = int(line.split('|')[0].split()[0])
            e1 = int(line.split('|')[0].split()[1])
            s2 = int(line.split('|')[1].split()[0])
            e2 = int(line.split('|')[1].split()[1])
            contig_name = line.split()[12].strip()
            chr_name = line.split()[11].strip()

            if chr_name not in genome_mapping:
                logger.error("Something went wrong and chromosome names in your coords file (" + coords_base_fpath + ") " \
                             "differ from the names in the reference. Try to remove the file and restart QUAST.")
                return None

            if gene_searching_enabled:
                aligned_blocks_by_contig_name[contig_name].append(AlignedBlock(seqname=chr_name, start=s1, end=e1,
                                                                               contig=contig_name, start_in_contig=s2, end_in_contig=e2))
            for i in range(s1, e1 + 1):
                genome_mapping[chr_name][i] = 1

    for chr_name in genome_mapping.keys():
        for i in ns_by_chromosomes[chr_name]:
            genome_mapping[chr_name][i] = 0
        ref_lengths[chr_name] = sum(genome_mapping[chr_name])

    if qconfig.space_efficient and coords_fpath.endswith('.filtered'):
        os.remove(coords_fpath)

    # counting genome coverage and gaps number
    gaps_count = 0
    if qconfig.analyze_gaps:
        gaps_fpath = os.path.join(genome_stats_dirpath, corr_assembly_label + '_gaps.txt') if not qconfig.space_efficient else '/dev/null'
        with open(gaps_fpath, 'w') as gaps_file:
            for chr_name, chr_len in reference_chromosomes.items():
                gaps_file.write(chr_name + '\n')
                cur_gap_size = 0
                for i in range(1, chr_len + 1):
                    if genome_mapping[chr_name][i] == 1 or i in ns_by_chromosomes[chr_name]:
                        if cur_gap_size >= qconfig.min_gap_size:
                            gaps_count += 1
                            gaps_file.write(str(i - cur_gap_size) + ' ' + str(i - 1) + '\n')
                        cur_gap_size = 0
                    else:
                        cur_gap_size += 1
                if cur_gap_size >= qconfig.min_gap_size:
                    gaps_count += 1
                    gaps_file.write(str(chr_len - cur_gap_size + 1) + ' ' + str(chr_len) + '\n')

    results["gaps_count"] = gaps_count
    results[reporting.Fields.GENES + "_full"] = None
    results[reporting.Fields.GENES + "_partial"] = None
    results[reporting.Fields.OPERONS + "_full"] = None
    results[reporting.Fields.OPERONS + "_partial"] = None

    # finding genes and operons
    for container in containers:
        if not container.region_list:
            continue

        total_full = 0
        total_partial = 0
        found_fpath = os.path.join(genome_stats_dirpath, corr_assembly_label + '_genomic_features_' + container.kind.lower() + '.txt')
        found_file = open(found_fpath, 'w')
        found_file.write('%s\t\t%s\t%s\t%s\t%s\n' % ('ID or #', 'Start', 'End', 'Type', 'Contig'))
        found_file.write('=' * 50 + '\n')

        # 0 - gene is not found,
        # 1 - gene is found,
        # 2 - part of gene is found
        found_list = [0] * len(container.region_list)
        for i, region in enumerate(container.region_list):
            found_list[i] = 0
            gene_blocks = []
            if region.id is None:
                region.id = '# ' + str(region.number + 1)
            for contig_id, name in enumerate(sorted_contigs_names):
                cur_feature_is_found = False
                for cur_block in aligned_blocks_by_contig_name[name]:
                    if cur_block.seqname != region.seqname:
                        continue
                    if region.end <= cur_block.start or cur_block.end <= region.start:
                        continue
                    elif cur_block.start <= region.start and region.end <= cur_block.end:
                        if found_list[i] == 2:  # already found as partial gene
                            total_partial -= 1
                        found_list[i] = 1
                        total_full += 1
                        contig_info = cur_block.format_gene_info(region)
                        found_file.write('%s\t\t%d\t%d\tcomplete\t%s\n' % (region.id, region.start, region.end, contig_info))
                        if container.kind == 'operon':
                            operons_in_contigs[contig_id] += 1  # inc number of found genes/operons in id-th contig
                        else:
                            features_in_contigs[contig_id] += 1

                        cur_feature_is_found = True
                        break
                    elif min(region.end, cur_block.end) - max(region.start, cur_block.start) >= qconfig.min_gene_overlap:
                        if found_list[i] == 0:
                            found_list[i] = 2
                            total_partial += 1
                        gene_blocks.append(cur_block)
                    if cur_feature_is_found:
                        break
                if cur_feature_is_found:
                    break
            # adding info about partially found genes/operons
            if found_list[i] == 2:  # partial gene/operon
                contig_info = ','.join([block.format_gene_info(region) for block in sorted(gene_blocks, key=lambda block: block.start)])
                found_file.write('%s\t\t%d\t%d\tpartial\t%s\n' % (region.id, region.start, region.end, contig_info))

        if container.kind == 'operon':
            results[reporting.Fields.OPERONS + "_full"] = total_full
            results[reporting.Fields.OPERONS + "_partial"] = total_partial
        else:
            if results[reporting.Fields.GENES + "_full"] is None:
                results[reporting.Fields.GENES + "_full"] = 0
                results[reporting.Fields.GENES + "_partial"] = 0
            results[reporting.Fields.GENES + "_full"] += total_full
            results[reporting.Fields.GENES + "_partial"] += total_partial
        found_file.close()

    logger.info('  ' + qutils.index_to_str(index) + 'Analysis is finished.')
    unsorted_features_in_contigs = [features_in_contigs[idx] for idx in contigs_order]
    unsorted_operons_in_contigs = [operons_in_contigs[idx] for idx in contigs_order]

    return ref_lengths, (results, unsorted_features_in_contigs, features_in_contigs, unsorted_operons_in_contigs, operons_in_contigs)


def do(ref_fpath, aligned_contigs_fpaths, output_dirpath, features_dict, operons_fpaths,
       detailed_contigs_reports_dirpath, genome_stats_dirpath):

    coords_dirpath = os.path.join(detailed_contigs_reports_dirpath, qconfig.aligner_output_dirname)
    from quast_libs import search_references_meta
    if search_references_meta.is_quast_first_run:
        coords_dirpath = os.path.join(coords_dirpath, 'raw')

    logger.print_timestamp()
    logger.main_info('Running Genome analyzer...')

    if not os.path.isdir(genome_stats_dirpath):
        os.mkdir(genome_stats_dirpath)

    genome_size, reference_chromosomes, ns_by_chromosomes = fastaparser.get_genome_stats(ref_fpath)

    # reading genome size
    # genome_size = fastaparser.get_lengths_from_fastafile(reference)[0]
    # reading reference name
    # >gi|48994873|gb|U00096.2| Escherichia coli str. K-12 substr. MG1655, complete genome
    # ref_file = open(reference, 'r')
    # reference_name = ref_file.readline().split()[0][1:]
    # ref_file.close()

    # RESULTS file
    result_fpath = os.path.join(genome_stats_dirpath, 'genome_info.txt')
    res_file = open(result_fpath, 'w')

    containers = []
    for feature, feature_fpath in features_dict.items():
        containers.append(FeatureContainer([feature_fpath], feature))
    if not features_dict:
        logger.notice('No file with genomic features were provided. '
                      'Use the --features option if you want to specify it.\n', indent='  ')
    if operons_fpaths:
        containers.append(FeatureContainer(operons_fpaths, 'operon'))
    else:
        logger.notice('No file with operons were provided. '
                      'Use the -O option if you want to specify it.', indent='  ')
    for container in containers:
        if not container.fpaths:
            continue

        for fpath in container.fpaths:
            container.region_list += genes_parser.get_genes_from_file(fpath, container.kind)

        if len(container.region_list) == 0:
            logger.warning('No genomic features of type "' + container.kind + '" were loaded.', indent='  ')
            res_file.write('Genomic features of type "' + container.kind + '" loaded: ' + 'None' + '\n')
        else:
            logger.info('  Loaded ' + str(len(container.region_list)) + ' genomic features of type "' + container.kind + '"')
            res_file.write('Genomic features of type "' + container.kind + '" loaded: ' + str(len(container.region_list)) + '\n')
            container.chr_names_dict = chromosomes_names_dict(container.kind, container.region_list, list(reference_chromosomes.keys()))

    ref_genes_num, ref_operons_num = None, None
    for contigs_fpath in aligned_contigs_fpaths:
        report = reporting.get(contigs_fpath)
        genomic_features = 0
        for container in containers:
            if container.kind == 'operon':
                ref_operons_num = len(container.region_list)
                report.add_field(reporting.Fields.REF_OPERONS, len(container.region_list))
            else:
                genomic_features += len(container.region_list)
        if genomic_features:
            ref_genes_num = genomic_features
            report.add_field(reporting.Fields.REF_GENES, genomic_features)

    # for cumulative plots:
    files_features_in_contigs = {}   #  "filename" : [ genes in sorted contigs (see below) ]
    files_unsorted_features_in_contigs = {}   #  "filename" : [ genes in sorted contigs (see below) ]
    files_operons_in_contigs = {}
    files_unsorted_operons_in_contigs = {}

    # for histograms
    genome_mapped = []
    full_found_genes = []
    full_found_operons = []

    # process all contig files
    num_nf_errors = logger._num_nf_errors
    n_jobs = min(len(aligned_contigs_fpaths), qconfig.max_threads)

    parallel_run_args = [(contigs_fpath, index, coords_dirpath, genome_stats_dirpath,
                          reference_chromosomes, ns_by_chromosomes, containers)
                        for index, contigs_fpath in enumerate(aligned_contigs_fpaths)]
    ref_lengths, results_genes_operons_tuples = run_parallel(process_single_file, parallel_run_args, n_jobs, filter_results=True)
    num_nf_errors += len(aligned_contigs_fpaths) - len(ref_lengths)
    logger._num_nf_errors = num_nf_errors
    if not ref_lengths:
        logger.main_info('Genome analyzer failed for all the assemblies.')
        res_file.close()
        return

    for ref in reference_chromosomes:
        ref_lengths_by_contigs[ref] = [ref_lengths[i][ref] for i in range(len(ref_lengths))]
    res_file.write('reference chromosomes:\n')
    for chr_name, chr_len in reference_chromosomes.items():
        aligned_len = max(ref_lengths_by_contigs[chr_name])
        res_file.write('\t' + chr_name + ' (total length: ' + str(chr_len) + ' bp, ' +
                       'total length without N\'s: ' + str(chr_len - len(ns_by_chromosomes[chr_name])) +
                       ' bp, maximal covered length: ' + str(aligned_len) + ' bp)\n')
    res_file.write('\n')
    res_file.write('total genome size: ' + str(genome_size) + '\n\n')
    res_file.write('gap min size: ' + str(qconfig.min_gap_size) + '\n')
    res_file.write('partial gene/operon min size: ' + str(qconfig.min_gene_overlap) + '\n\n')
    # header
    # header
    res_file.write('\n\n')
    res_file.write('%-25s| %-10s| %-12s| %-10s| %-10s| %-10s| %-10s| %-10s|\n'
        % ('assembly', 'genome', 'duplication', 'gaps', 'genes', 'partial', 'operons', 'partial'))
    res_file.write('%-25s| %-10s| %-12s| %-10s| %-10s| %-10s| %-10s| %-10s|\n'
        % ('', 'fraction', 'ratio', 'number', '', 'genes', '', 'operons'))
    res_file.write('=' * 120 + '\n')

    for contigs_fpath, (results, unsorted_features_in_contigs, features_in_contigs, unsorted_operons_in_contigs, operons_in_contigs)\
            in zip(aligned_contigs_fpaths, results_genes_operons_tuples):
        assembly_name = qutils.name_from_fpath(contigs_fpath)

        files_features_in_contigs[contigs_fpath] = features_in_contigs
        files_unsorted_features_in_contigs[contigs_fpath] = unsorted_features_in_contigs
        files_operons_in_contigs[contigs_fpath] = operons_in_contigs
        files_unsorted_operons_in_contigs[contigs_fpath] = unsorted_operons_in_contigs
        full_found_genes.append(sum(features_in_contigs))
        full_found_operons.append(sum(operons_in_contigs))

        gaps_count = results["gaps_count"]
        genes_full = results[reporting.Fields.GENES + "_full"]
        genes_part = results[reporting.Fields.GENES + "_partial"]
        operons_full = results[reporting.Fields.OPERONS + "_full"]
        operons_part = results[reporting.Fields.OPERONS + "_partial"]

        report = reporting.get(contigs_fpath)

        res_file.write('%-25s| %-10s| %-12s| %-10s|'
        % (assembly_name[:24], report.get_field(reporting.Fields.MAPPEDGENOME), report.get_field(reporting.Fields.DUPLICATION_RATIO), gaps_count))

        genome_mapped.append(float(report.get_field(reporting.Fields.MAPPEDGENOME)))

        for (field, full, part) in [(reporting.Fields.GENES, genes_full, genes_part),
            (reporting.Fields.OPERONS, operons_full, operons_part)]:
            if full is None and part is None:
                res_file.write(' %-10s| %-10s|' % ('-', '-'))
            else:
                res_file.write(' %-10s| %-10s|' % (full, part))
                report.add_field(field, '%s + %s part' % (full, part))
        res_file.write('\n')
    res_file.close()

    if qconfig.html_report:
        from quast_libs.html_saver import html_saver
        if ref_genes_num:
            html_saver.save_features_in_contigs(output_dirpath, aligned_contigs_fpaths, 'features', files_features_in_contigs, ref_genes_num)
        if ref_operons_num:
            html_saver.save_features_in_contigs(output_dirpath, aligned_contigs_fpaths, 'operons', files_operons_in_contigs, ref_operons_num)

    if qconfig.draw_plots:
        # cumulative plots:
        from . import plotter
        from quast_libs.ca_utils.misc import contigs_aligned_lengths
        if ref_genes_num:
            plotter.genes_operons_plot(ref_genes_num, aligned_contigs_fpaths, files_features_in_contigs,
                genome_stats_dirpath + '/features_cumulative_plot', 'genomic features')
            plotter.frc_plot(output_dirpath, ref_fpath, aligned_contigs_fpaths, contigs_aligned_lengths, files_unsorted_features_in_contigs,
                             genome_stats_dirpath + '/features_frcurve_plot', 'genomic features')
            plotter.histogram(aligned_contigs_fpaths, full_found_genes, genome_stats_dirpath + '/complete_features_histogram',
                '# complete genomic features')
        if ref_operons_num:
            plotter.genes_operons_plot(ref_operons_num, aligned_contigs_fpaths, files_operons_in_contigs,
                genome_stats_dirpath + '/operons_cumulative_plot', 'operons')
            plotter.frc_plot(output_dirpath, ref_fpath, aligned_contigs_fpaths, contigs_aligned_lengths, files_unsorted_operons_in_contigs,
                             genome_stats_dirpath + '/operons_frcurve_plot', 'operons')
            plotter.histogram(aligned_contigs_fpaths, full_found_operons, genome_stats_dirpath + '/complete_operons_histogram',
                '# complete operons')
        plotter.histogram(aligned_contigs_fpaths, genome_mapped, genome_stats_dirpath + '/genome_fraction_histogram',
            'Genome fraction, %', top_value=100)

    logger.main_info('Done.')
    return containers


class AlignedBlock():
    def __init__(self, seqname=None, start=None, end=None, contig=None, start_in_contig=None, end_in_contig=None):
        self.seqname = seqname
        self.start = start
        self.end = end
        self.contig = contig
        self.start_in_contig = start_in_contig
        self.end_in_contig = end_in_contig

    def format_gene_info(self, region):
        start, end = self.start_in_contig, self.end_in_contig
        if self.start < region.start:
            region_shift = region.start - self.start
            if start < end:
                start += region_shift
            else:
                start -= region_shift
        if region.end < self.end:
            region_size = region.end - max(region.start, self.start)
            if start < end:
                end = start + region_size
            else:
                end = start - region_size
        return self.contig + ':' + str(start) + '-' + str(end)