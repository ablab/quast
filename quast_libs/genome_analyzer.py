############################################################################
# Copyright (c) 2015-2016 Saint Petersburg State University
# Copyright (c) 2011-2015 Saint Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################

import logging
import os

from quast_libs import fastaparser, genes_parser, reporting, qconfig, qutils
from quast_libs.html_saver import json_saver

from quast_libs.log import get_logger
from quast_libs.qutils import is_python2

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
    They can differ, e.g. U22222 in the list and gi|48994873|gb|U22222| in the reference
    """
    region_2_chr_name = {}

    # single chromosome
    if len(chr_names) == 1:
        chr_name = chr_names[0]

        for region in regions:
            if region.seqname in chr_name or chr_name in region.seqname:
                region_2_chr_name[region.seqname] = chr_name
            else:
                region_2_chr_name[region.seqname] = None

        if len(region_2_chr_name) == 1:
            if region_2_chr_name[regions[0].seqname] is None:
                logger.notice('Reference name in %ss (%s) does not match the name of the reference (%s). '
                              'QUAST will ignore this ussue and count as if they matched.' %
                              (feature, regions[0].seqname, chr_name),
                       indent='  ')
                region_2_chr_name[regions[0].seqname] = chr_name

        else:
            logger.warning('Some of the reference names in %ss do not match the name of the reference (%s). '
                    'Check your %s file.' % (feature, chr_name, feature), indent='  ')

    # multiple chromosomes
    else:
        for region in regions:
            no_chr_name_for_the_region = True
            for chr_name in chr_names:
                if region.seqname in chr_name or chr_name in region.seqname:
                    region_2_chr_name[region.seqname] = chr_name
                    no_chr_name_for_the_region = False
                    break
            if no_chr_name_for_the_region:
                region_2_chr_name[region.seqname] = None

        if None in region_2_chr_name.values():
            logger.warning('Some of the reference names in %ss does not match any chromosome. '
                    'Check your %s file.' % (feature, feature), indent='  ')

        if all(chr_name is None for chr_name in region_2_chr_name.values()):
            logger.warning('Reference names in %ss do not match any chromosome. Check your %s file.' % (feature, feature),
                    indent='  ')

    return region_2_chr_name


def process_single_file(contigs_fpath, index, nucmer_path_dirpath, genome_stats_dirpath,
                        reference_chromosomes, genes_container, operons_container):
    assembly_label = qutils.label_from_fpath(contigs_fpath)
    corr_assembly_label = qutils.label_from_fpath_for_fname(contigs_fpath)
    results = dict()
    ref_lengths = {}
    logger.info('  ' + qutils.index_to_str(index) + assembly_label)

    nucmer_base_fpath = os.path.join(nucmer_path_dirpath, corr_assembly_label + '.coords')
    if qconfig.use_all_alignments:
        nucmer_fpath = nucmer_base_fpath
    else:
        nucmer_fpath = nucmer_base_fpath + '.filtered'

    if not os.path.isfile(nucmer_fpath):
        logger.error('Nucmer\'s coords file (' + nucmer_fpath + ') not found! Try to restart QUAST.',
            indent='  ')
        return None

    coordfile = open(nucmer_fpath, 'r')
    for line in coordfile:
        if line.startswith('='):
            break

    # EXAMPLE:
    #    [S1]     [E1]  |     [S2]     [E2]  |  [LEN 1]  [LEN 2]  |  [% IDY]  | [TAGS]
    #=====================================================================================
    #  338980   339138  |     2298     2134  |      159      165  |    79.76  | gi|48994873|gb|U00096.2|	NODE_0_length_6088
    #  374145   374355  |     2306     2097  |      211      210  |    85.45  | gi|48994873|gb|U00096.2|	NODE_0_length_6088

    genome_mapping = {}
    for chr_name, chr_len in reference_chromosomes.items():
        genome_mapping[chr_name] = [0] * (chr_len + 1)

    contig_tuples = fastaparser.read_fasta(contigs_fpath)  # list of FASTA entries (in tuples: name, seq)
    contig_tuples = sorted(contig_tuples, key=lambda contig: len(contig[1]), reverse=True)
    sorted_contigs_names = [name for (name, seq) in contig_tuples]

    genes_in_contigs = [0] * len(sorted_contigs_names) # for cumulative plots: i-th element is the number of genes in i-th contig
    operons_in_contigs = [0] * len(sorted_contigs_names)
    aligned_blocks_by_contig_name = {} # for gene finding: contig_name --> list of AlignedBlock

    gene_searching_enabled = len(genes_container.region_list) or len(operons_container.region_list)
    if qconfig.memory_efficient and gene_searching_enabled:
        logger.warning('Run QUAST without genes and operons files to reduce memory consumption.')
    if gene_searching_enabled:
        for name in sorted_contigs_names:
            aligned_blocks_by_contig_name[name] = []
    for line in coordfile:
        if line.strip() == '':
            break
        s1 = int(line.split('|')[0].split()[0])
        e1 = int(line.split('|')[0].split()[1])
        s2 = int(line.split('|')[1].split()[0])
        e2 = int(line.split('|')[1].split()[1])
        contig_name = line.split()[12].strip()
        chr_name = line.split()[11].strip()

        if chr_name not in genome_mapping:
            logger.error("Something went wrong and chromosome names in your coords file (" + nucmer_base_fpath + ") " \
                         "differ from the names in the reference. Try to remove the file and restart QUAST.")
            return None

        if gene_searching_enabled:
            aligned_blocks_by_contig_name[contig_name].append(AlignedBlock(seqname=chr_name, start=s1, end=e1))
        if s2 == 0 and e2 == 0:  # special case: circular genome, contig starts on the end of a chromosome and ends in the beginning
            for i in range(s1, len(genome_mapping[chr_name])):
                genome_mapping[chr_name][i] = 1
            for i in range(1, e1 + 1):
                genome_mapping[chr_name][i] = 1
        else: #if s1 <= e1:
            for i in range(s1, e1 + 1):
                genome_mapping[chr_name][i] = 1
    coordfile.close()
    if qconfig.space_efficient and nucmer_fpath.endswith('.filtered'):
        os.remove(nucmer_fpath)

    # counting genome coverage and gaps number
    covered_bp = 0
    gaps_count = 0
    gaps_fpath = os.path.join(genome_stats_dirpath, corr_assembly_label + '_gaps.txt') if not qconfig.space_efficient else '/dev/null'
    gaps_file = open(gaps_fpath, 'w')
    for chr_name, chr_len in reference_chromosomes.items():
        gaps_file.write(chr_name + '\n')
        cur_gap_size = 0
        aligned_len = 0
        for i in range(1, chr_len + 1):
            if genome_mapping[chr_name][i] == 1:
                if cur_gap_size >= qconfig.min_gap_size:
                    gaps_count += 1
                    gaps_file.write(str(i - cur_gap_size) + ' ' + str(i - 1) + '\n')
                aligned_len += 1
                covered_bp += 1
                cur_gap_size = 0
            else:
                cur_gap_size += 1
        ref_lengths[chr_name] = aligned_len
        if cur_gap_size >= qconfig.min_gap_size:
            gaps_count += 1
            gaps_file.write(str(chr_len - cur_gap_size + 1) + ' ' + str(chr_len) + '\n')
    gaps_file.close()

    results["covered_bp"] = covered_bp
    results["gaps_count"] = gaps_count

    # finding genes and operons
    for container, feature_in_contigs, field, suffix in [
        (genes_container,
         genes_in_contigs,
         reporting.Fields.GENES,
         '_genes.txt'),

        (operons_container,
         operons_in_contigs,
         reporting.Fields.OPERONS,
         '_operons.txt')]:

        if not container.region_list:
            results[field + "_full"] = None
            results[field + "_partial"] = None
            continue

        total_full = 0
        total_partial = 0
        found_fpath = os.path.join(genome_stats_dirpath, corr_assembly_label + suffix)
        found_file = open(found_fpath, 'w')
        found_file.write('%s\t\t%s\t%s\t%s\n' % ('ID or #', 'Start', 'End', 'Type'))
        found_file.write('=========================================\n')

        # 0 - gene is not found,
        # 1 - gene is found,
        # 2 - part of gene is found
        found_list = [0] * len(container.region_list)
        for i, region in enumerate(container.region_list):
            found_list[i] = 0
            for contig_id, name in enumerate(sorted_contigs_names):
                cur_feature_is_found = False
                for cur_block in aligned_blocks_by_contig_name[name]:
                    if container.chr_names_dict[region.seqname] != cur_block.seqname:
                        continue

                    # computing circular genomes
                    if cur_block.start > cur_block.end:
                        blocks = [AlignedBlock(seqname=cur_block.seqname, start=cur_block.start, end=region.end + 1),
                                  AlignedBlock(seqname=cur_block.seqname, start=1, end=cur_block.end)]
                    else:
                        blocks = [cur_block]

                    for block in blocks:
                        if region.end <= block.start or block.end <= region.start:
                            continue
                        elif block.start <= region.start and region.end <= block.end:
                            if found_list[i] == 2:  # already found as partial gene
                                total_partial -= 1
                            found_list[i] = 1
                            total_full += 1
                            region_id = str(region.id)
                            if region_id == 'None':
                                region_id = '# ' + str(region.number + 1)
                            found_file.write('%s\t\t%d\t%d\tcomplete\n' % (region_id, region.start, region.end))
                            feature_in_contigs[contig_id] += 1  # inc number of found genes/operons in id-th contig

                            cur_feature_is_found = True
                            break
                        elif found_list[i] == 0 and min(region.end, block.end) - max(region.start, block.start) >= qconfig.min_gene_overlap:
                            found_list[i] = 2
                            total_partial += 1
                    if cur_feature_is_found:
                        break
                if cur_feature_is_found:
                    break
            # adding info about partially found genes/operons
            if found_list[i] == 2:  # partial gene/operon
                region_id = str(region.id)
                if region_id == 'None':
                    region_id = '# ' + str(region.number + 1)
                found_file.write('%s\t\t%d\t%d\tpartial\n' % (region_id, region.start, region.end))

        results[field + "_full"] = total_full
        results[field + "_partial"] = total_partial
        found_file.close()

    logger.info('  ' + qutils.index_to_str(index) + 'Analysis is finished.')

    return ref_lengths, (results, genes_in_contigs, operons_in_contigs)


def do(ref_fpath, aligned_contigs_fpaths, output_dirpath, json_output_dirpath,
       genes_fpaths, operons_fpaths, detailed_contigs_reports_dirpath, genome_stats_dirpath):

    nucmer_path_dirpath = os.path.join(detailed_contigs_reports_dirpath, 'nucmer_output')
    from quast_libs import search_references_meta
    if search_references_meta.is_quast_first_run:
        nucmer_path_dirpath = os.path.join(nucmer_path_dirpath, 'raw')

    logger.print_timestamp()
    logger.main_info('Running Genome analyzer...')

    if not os.path.isdir(genome_stats_dirpath):
        os.mkdir(genome_stats_dirpath)

    reference_chromosomes = {}
    genome_size = 0
    for name, seq in fastaparser.read_fasta(ref_fpath):
        chr_name = name.split()[0]
        chr_len = len(seq)
        genome_size += chr_len
        reference_chromosomes[chr_name] = chr_len

    # reading genome size
    # genome_size = fastaparser.get_lengths_from_fastafile(reference)[0]
    # reading reference name
    # >gi|48994873|gb|U00096.2| Escherichia coli str. K-12 substr. MG1655, complete genome
    # ref_file = open(reference, 'r')
    # reference_name = ref_file.readline().split()[0][1:]
    # ref_file.close()

    # RESULTS file
    result_fpath = genome_stats_dirpath + '/genome_info.txt'
    res_file = open(result_fpath, 'w')

    genes_container = FeatureContainer(genes_fpaths, 'gene')
    operons_container = FeatureContainer(operons_fpaths, 'operon')
    for container in [genes_container, operons_container]:
        if not container.fpaths:
            logger.notice('No file with ' + container.kind + 's provided. '
                          'Use the -' + container.kind[0].capitalize() + ' option '
                          'if you want to specify it.', indent='  ')
            continue

        for fpath in container.fpaths:
            container.region_list += genes_parser.get_genes_from_file(fpath, container.kind)

        if len(container.region_list) == 0:
            logger.warning('No ' + container.kind + 's were loaded.', indent='  ')
            res_file.write(container.kind + 's loaded: ' + 'None' + '\n')
        else:
            logger.info('  Loaded ' + str(len(container.region_list)) + ' ' + container.kind + 's')
            res_file.write(container.kind + 's loaded: ' + str(len(container.region_list)) + '\n')
            container.chr_names_dict = chromosomes_names_dict(container.kind, container.region_list, list(reference_chromosomes.keys()))

    for contigs_fpath in aligned_contigs_fpaths:
        report = reporting.get(contigs_fpath)
        if genes_container.fpaths:
            report.add_field(reporting.Fields.REF_GENES, len(genes_container.region_list))
        if operons_container.fpaths:
            report.add_field(reporting.Fields.REF_OPERONS, len(operons_container.region_list))

    # for cumulative plots:
    files_genes_in_contigs = {}   #  "filename" : [ genes in sorted contigs (see below) ]
    files_operons_in_contigs = {}

    # for histograms
    genome_mapped = []
    full_found_genes = []
    full_found_operons = []

    # process all contig files
    num_nf_errors = logger._num_nf_errors
    n_jobs = min(len(aligned_contigs_fpaths), qconfig.max_threads)
    if is_python2():
        from joblib import Parallel, delayed
    else:
        from joblib3 import Parallel, delayed
    process_results = Parallel(n_jobs=n_jobs)(delayed(process_single_file)(
        contigs_fpath, index, nucmer_path_dirpath, genome_stats_dirpath,
        reference_chromosomes, genes_container, operons_container)
        for index, contigs_fpath in enumerate(aligned_contigs_fpaths))
    num_nf_errors += len([res for res in process_results if res is None])
    logger._num_nf_errors = num_nf_errors
    process_results = [res for res in process_results if res]
    if not process_results:
        logger.main_info('Genome analyzer failed for all the assemblies.')
        res_file.close()
        return

    ref_lengths = [process_results[i][0] for i in range(len(process_results))]
    results_genes_operons_tuples = [process_results[i][1] for i in range(len(process_results))]
    for ref in reference_chromosomes:
        ref_lengths_by_contigs[ref] = [ref_lengths[i][ref] for i in range(len(ref_lengths))]
    res_file.write('reference chromosomes:\n')
    for chr_name, chr_len in reference_chromosomes.items():
        aligned_len = max(ref_lengths_by_contigs[chr_name])
        res_file.write('\t' + chr_name + ' (total length: ' + str(chr_len) + ' bp, maximal covered length: ' + str(aligned_len) + ' bp)\n')
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
    res_file.write('================================================================================================================\n')

    for contigs_fpath, (results, genes_in_contigs, operons_in_contigs) in zip(aligned_contigs_fpaths, results_genes_operons_tuples):
        assembly_name = qutils.name_from_fpath(contigs_fpath)

        files_genes_in_contigs[contigs_fpath] = genes_in_contigs
        files_operons_in_contigs[contigs_fpath] = operons_in_contigs
        full_found_genes.append(sum(genes_in_contigs))
        full_found_operons.append(sum(operons_in_contigs))

        covered_bp = results["covered_bp"]
        gaps_count = results["gaps_count"]
        genes_full = results[reporting.Fields.GENES + "_full"]
        genes_part = results[reporting.Fields.GENES + "_partial"]
        operons_full = results[reporting.Fields.OPERONS + "_full"]
        operons_part = results[reporting.Fields.OPERONS + "_partial"]

        report = reporting.get(contigs_fpath)
        genome_fraction = float(covered_bp) * 100 / float(genome_size)
        duplication_ratio = (report.get_field(reporting.Fields.TOTALLEN) +
                             report.get_field(reporting.Fields.MISINTERNALOVERLAP) +
                             report.get_field(reporting.Fields.AMBIGUOUSEXTRABASES) -
                             report.get_field(reporting.Fields.UNALIGNEDBASES)) /\
                            ((genome_fraction / 100.0) * float(genome_size))

        res_file.write('%-25s| %-10s| %-12s| %-10s|'
        % (assembly_name[:24], '%3.5f%%' % genome_fraction, '%1.5f' % duplication_ratio, gaps_count))

        report.add_field(reporting.Fields.MAPPEDGENOME, '%.3f' % genome_fraction)
        report.add_field(reporting.Fields.DUPLICATION_RATIO, '%.3f' % duplication_ratio)
        genome_mapped.append(genome_fraction)

        for (field, full, part) in [(reporting.Fields.GENES, genes_full, genes_part),
            (reporting.Fields.OPERONS, operons_full, operons_part)]:
            if full is None and part is None:
                res_file.write(' %-10s| %-10s|' % ('-', '-'))
            else:
                res_file.write(' %-10s| %-10s|' % (full, part))
                report.add_field(field, '%s + %s part' % (full, part))
        res_file.write('\n')
    res_file.close()

    if genes_container.region_list:
        ref_genes_num = len(genes_container.region_list)
    else:
        ref_genes_num = None

    if operons_container.region_list:
        ref_operons_num = len(operons_container.region_list)
    else:
        ref_operons_num = None

    # saving json
    if json_output_dirpath:
        if genes_container.region_list:
            json_saver.save_features_in_contigs(json_output_dirpath, aligned_contigs_fpaths, 'genes', files_genes_in_contigs, ref_genes_num)
        if operons_container.region_list:
            json_saver.save_features_in_contigs(json_output_dirpath, aligned_contigs_fpaths, 'operons', files_operons_in_contigs, ref_operons_num)

    if qconfig.html_report:
        from quast_libs.html_saver import html_saver
        if genes_container.region_list:
            html_saver.save_features_in_contigs(output_dirpath, aligned_contigs_fpaths, 'genes', files_genes_in_contigs, ref_genes_num)
        if operons_container.region_list:
            html_saver.save_features_in_contigs(output_dirpath, aligned_contigs_fpaths, 'operons', files_operons_in_contigs, ref_operons_num)

    if qconfig.draw_plots:
        # cumulative plots:
        from . import plotter
        if genes_container.region_list:
            plotter.genes_operons_plot(len(genes_container.region_list), aligned_contigs_fpaths, files_genes_in_contigs,
                genome_stats_dirpath + '/genes_cumulative_plot', 'genes')
            plotter.histogram(aligned_contigs_fpaths, full_found_genes, genome_stats_dirpath + '/complete_genes_histogram',
                '# complete genes')
        if operons_container.region_list:
            plotter.genes_operons_plot(len(operons_container.region_list), aligned_contigs_fpaths, files_operons_in_contigs,
                genome_stats_dirpath + '/operons_cumulative_plot', 'operons')
            plotter.histogram(aligned_contigs_fpaths, full_found_operons, genome_stats_dirpath + '/complete_operons_histogram',
                '# complete operons')
        plotter.histogram(aligned_contigs_fpaths, genome_mapped, genome_stats_dirpath + '/genome_fraction_histogram',
            'Genome fraction, %', top_value=100)

    logger.main_info('Done.')
    return [genes_container, operons_container]


class AlignedBlock():
    def __init__(self, seqname=None, start=None, end=None):
        self.seqname = seqname
        self.start = start
        self.end = end
