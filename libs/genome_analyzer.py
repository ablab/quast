############################################################################
# Copyright (c) 2011-2013 Saint-Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################

import logging
import os
import fastaparser
import genes_parser
from libs import reporting, qconfig, qutils
from libs.html_saver import json_saver
from qutils import index_to_str

from libs.log import get_logger
logger = get_logger(qconfig.LOGGER_DEFAULT_NAME)


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


def do(ref_fpath, aligned_contigs_fpaths, all_pdf, draw_plots, output_dirpath, json_output_dirpath,
       genes_fpath, operons_fpath, detailed_contigs_reports_dirpath, genome_stats_dirpath):

    nucmer_path_dirpath = os.path.join(detailed_contigs_reports_dirpath, 'nucmer_output')

    logger.print_timestamp()
    logger.info('Running Genome analyzer...')

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
    res_file.write('reference chromosomes:\n')
    for chr_name, chr_len in reference_chromosomes.iteritems():
        res_file.write('\t' + chr_name + ' (' + str(chr_len) + ' bp)\n')
    res_file.write('\n')
    res_file.write('total genome size: ' + str(genome_size) + '\n\n')
    res_file.write('gap min size: ' + str(qconfig.min_gap_size) + '\n')
    res_file.write('partial gene/operon min size: ' + str(qconfig.min_gene_overlap) + '\n\n')

    # reading genes and operons
    class FeatureContainer:
        def __init__(self, kind='', fpath=''):
            self.kind = kind  # 'gene' or 'operon'
            self.fpath = fpath
            self.region_list = []
            self.found_list = []
            self.full_found = []
            self.chr_names_dict = {}

    genes_container = FeatureContainer('gene', genes_fpath)
    operons_container = FeatureContainer('operon', operons_fpath)

    for container in [genes_container, operons_container]:
        container.region_list = genes_parser.get_genes_from_file(container.fpath, container.kind)

        if len(container.region_list) == 0:
            if container.fpath:
                logger.warning('No ' + container.kind + 's were loaded.', indent='  ')
                res_file.write(container.kind + 's loaded: ' + 'None' + '\n')
            else:
                logger.notice('Annotated ' + container.kind + 's file was not provided. Use -'
                              + container.kind[0].capitalize() + ' option to specify it.', indent='  ')
        else:
            logger.info('  Loaded ' + str(len(container.region_list)) + ' ' + container.kind + 's')
            res_file.write(container.kind + 's loaded: ' + str(len(container.region_list)) + '\n')

            # 0 - gene is not found,
            # 1 - gene is found,
            # 2 - part of gene is found
            container.found_list = [0] * len(container.region_list)
            container.chr_names_dict = chromosomes_names_dict(container.kind, container.region_list, reference_chromosomes.keys())
            container.full_found = []

    for contigs_fpath in aligned_contigs_fpaths:
        report = reporting.get(contigs_fpath)
        report.add_field(reporting.Fields.REF_GENES, len(genes_container.region_list))
        report.add_field(reporting.Fields.REF_OPERONS, len(operons_container.region_list))

    # header
    res_file.write('\n\n')
    res_file.write('%-25s| %-10s| %-12s| %-10s| %-10s| %-10s| %-10s| %-10s|\n'
        % ('assembly', 'genome', 'duplication', 'gaps', 'genes', 'partial', 'operons', 'partial'))
    res_file.write('%-25s| %-10s| %-12s| %-10s| %-10s| %-10s| %-10s| %-10s|\n'
        % ('', 'fraction', 'ratio', 'number', '', 'genes', '', 'operons'))
    res_file.write('================================================================================================================\n')

    # for cumulative plots:
    files_genes_in_contigs = {}   #  "filename" : [ genes in sorted contigs (see below) ]
    files_operons_in_contigs = {}

    # for histograms
    genome_mapped = []

    # process all contig files  
    for i, contigs_fpath in enumerate(aligned_contigs_fpaths):
        assembly_name = qutils.name_from_fpath(contigs_fpath)
        assembly_label = qutils.label_from_fpath(contigs_fpath)

        logger.info('  ' + index_to_str(i) + assembly_label)

        nucmer_base_fpath = os.path.join(nucmer_path_dirpath, assembly_name + '.coords')
        if qconfig.use_all_alignments:
            nucmer_fpath = nucmer_base_fpath
        else:
            nucmer_fpath = nucmer_base_fpath + '.filtered'

        if not os.path.isfile(nucmer_fpath):
            logger.error('Nucmer\'s coords file (' + nucmer_fpath + ') not found! Try to restart QUAST.',
                  indent='  ')
            #continue

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
        for chr_name, chr_len in reference_chromosomes.iteritems():
            genome_mapping[chr_name] = [0] * (chr_len + 1)

        # for gene finding
        aligned_blocks_by_contig_name = {}  # contig_name --> list of AlignedBlock
        # for cumulative plots
        files_genes_in_contigs[contigs_fpath] = []  # i-th element is the number of genes in i-th contig (sorted by length)
        files_operons_in_contigs[contigs_fpath] = []

        # for cumulative plots:
        contig_tuples = fastaparser.read_fasta(contigs_fpath)  # list of FASTA entries (in tuples: name, seq)
        contig_tuples = sorted(contig_tuples, key=lambda contig: len(contig[1]), reverse=True)
        sorted_contigs_names = [name for (name, seq) in contig_tuples]

        for name in sorted_contigs_names:
            aligned_blocks_by_contig_name[name] = []
            files_genes_in_contigs[contigs_fpath].append(0)
            files_operons_in_contigs[contigs_fpath].append(0)

        for line in coordfile:
            if line.strip() == '':
                break
            s1 = int(line.split('|')[0].split()[0])
            e1 = int(line.split('|')[0].split()[1])
            contig_name = line.split()[12].strip()
            chr_name = line.split()[11].strip()

            if chr_name not in genome_mapping:
                logger.error("Something went wrong and chromosome names in your coords file (" + nucmer_base_fpath + ") "
                      "differ from the names in the reference. Try to remove the file and restart QUAST.")
                continue

            aligned_blocks_by_contig_name[contig_name].append(AlignedBlock(seqname=chr_name, start=s1, end=e1))
            if s1 <= e1:
                for i in range(s1, e1 + 1):
                    genome_mapping[chr_name][i] = 1
            else:  # circular genome, contig starts on the end of a chromosome and ends in the beginning
                for i in range(s1, len(genome_mapping[chr_name])):
                    genome_mapping[chr_name][i] = 1
                for i in range(1, e1 + 1):
                    genome_mapping[chr_name][i] = 1
        coordfile.close()

        # counting genome coverage and gaps number
        covered_bp = 0
        gaps_count = 0
        gaps_fpath = os.path.join(genome_stats_dirpath, assembly_name + '_gaps.txt')
        gaps_file = open(gaps_fpath, 'w')
        for chr_name, chr_len in reference_chromosomes.iteritems():
            print >>gaps_file, chr_name
            cur_gap_size = 0
            for i in range(1, chr_len + 1):
                if genome_mapping[chr_name][i] == 1:
                    if cur_gap_size >= qconfig.min_gap_size:
                        gaps_count += 1
                        print >>gaps_file, i - cur_gap_size, i - 1

                    covered_bp += 1
                    cur_gap_size = 0
                else:
                    cur_gap_size += 1

            if cur_gap_size >= qconfig.min_gap_size:
                gaps_count += 1
                print >>gaps_file, chr_len - cur_gap_size + 1, chr_len

        gaps_file.close()

        report = reporting.get(contigs_fpath)

        genome_fraction = float(covered_bp) * 100 / float(genome_size)
        # calculating duplication ratio
        duplication_ratio = (report.get_field(reporting.Fields.TOTALLEN) +
                             report.get_field(reporting.Fields.MISINTERNALOVERLAP) +
                             report.get_field(reporting.Fields.AMBIGUOUSEXTRABASES) -
                             report.get_field(reporting.Fields.UNALIGNEDBASES)) / \
                            ((genome_fraction / 100.0) * float(genome_size))

        res_file.write('%-25s| %-10s| %-12s| %-10s|'
            % (assembly_name[:24], str(genome_fraction) + '%', duplication_ratio, str(gaps_count)))

        report.add_field(reporting.Fields.MAPPEDGENOME, '%.3f' % genome_fraction)
        report.add_field(reporting.Fields.DUPLICATION_RATIO, '%.3f' % duplication_ratio)
        genome_mapped.append(genome_fraction)

        # finding genes and operons
        for container, feature_in_contigs, field, suffix in [
                (genes_container,
                 files_genes_in_contigs[contigs_fpath],
                 reporting.Fields.GENES,
                 '_genes.txt'),

                (operons_container,
                 files_operons_in_contigs[contigs_fpath],
                 reporting.Fields.OPERONS,
                 '_operons.txt')]:

            if not container.region_list:
                res_file.write(' %-10s| %-10s|' % ('-', '-'))
                continue    

            total_full = 0
            total_partial = 0
            found_fpath = os.path.join(genome_stats_dirpath, assembly_name + suffix)
            found_file = open(found_fpath, 'w')
            print >>found_file, '%s\t\t%s\t%s' % ('ID or #', 'Start', 'End')
            print >>found_file, '============================'

            for i, region in enumerate(container.region_list):
                container.found_list[i] = 0
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
                                if container.found_list[i] == 2:  # already found as partial gene
                                    total_partial -= 1
                                container.found_list[i] = 1
                                total_full += 1
                                i = str(region.id)
                                if i == 'None':
                                    i = '# ' + str(region.number + 1)
                                print >>found_file, '%s\t\t%d\t%d' % (i, region.start, region.end)
                                feature_in_contigs[contig_id] += 1  # inc number of found genes/operons in id-th contig

                                cur_feature_is_found = True
                                break
                            elif container.found_list[i] == 0 and min(region.end, block.end) - max(region.start, block.start) >= qconfig.min_gene_overlap:
                                container.found_list[i] = 2
                                total_partial += 1
                        if cur_feature_is_found:
                            break
                    if cur_feature_is_found:
                        break

            res_file.write(' %-10s| %-10s|' % (str(total_full), str(total_partial)))
            found_file.close()
            report.add_field(field, '%s + %s part' % (str(total_full), str(total_partial)))
            container.full_found.append(total_full)

        # finishing output for current contigs file
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
        from libs.html_saver import html_saver
        if genes_container.region_list:
            html_saver.save_features_in_contigs(output_dirpath, aligned_contigs_fpaths, 'genes', files_genes_in_contigs, ref_genes_num)
        if operons_container.region_list:
            html_saver.save_features_in_contigs(output_dirpath, aligned_contigs_fpaths, 'operons', files_operons_in_contigs, ref_operons_num)

    if draw_plots:
        # cumulative plots:
        import plotter
        if genes_container.region_list:
            plotter.genes_operons_plot(len(genes_container.region_list), aligned_contigs_fpaths, files_genes_in_contigs,
                genome_stats_dirpath + '/genes_cumulative_plot', 'genes', all_pdf)
            plotter.histogram(aligned_contigs_fpaths, genes_container.full_found, genome_stats_dirpath + '/complete_genes_histogram',
                '# complete genes', all_pdf)
        if operons_container.region_list:
            plotter.genes_operons_plot(len(operons_container.region_list), aligned_contigs_fpaths, files_operons_in_contigs,
                genome_stats_dirpath + '/operons_cumulative_plot', 'operons', all_pdf)
            plotter.histogram(aligned_contigs_fpaths, operons_container.full_found, genome_stats_dirpath + '/complete_operons_histogram',
                '# complete operons', all_pdf)
        plotter.histogram(aligned_contigs_fpaths, genome_mapped, genome_stats_dirpath + '/genome_fraction_histogram', 'Genome fraction, %',
            all_pdf, top_value=100)

    logger.info('Done.')
    return genome_size


class AlignedBlock():
    def __init__(self, seqname=None, start=None, end=None):
        self.seqname = seqname
        self.start = start
        self.end = end
