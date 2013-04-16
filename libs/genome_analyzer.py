############################################################################
# Copyright (c) 2011-2013 Saint-Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################

import logging
import os
import fastaparser
import genes_parser
from libs import reporting, qconfig
from libs.html_saver import json_saver
from qutils import id_to_str, notice, warning, error, print_timestamp


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
                notice('Reference name in %ss (%s) does not match the name of the reference (%s).'
                       'QUAST will ignore this ussue and count as if they matched.' % (feature, regions[0].seqname, chr_name),
                       indent='  ')
                region_2_chr_name[regions[0].seqname] = chr_name

        else:
            warning('Some of the reference names in %ss do not match the name of the reference (%s). '
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
            warning('Some of the reference names in %ss does not match any chromosome. '
                    'Check your %s file.' % (feature, feature), indent='  ')

        if all(chr_name is None for chr_name in region_2_chr_name.values()):
            warning('Reference names in %ss do not match any chromosome. Check your %s file.' % (feature, feature),
                    indent='  ')

    return region_2_chr_name


def do(reference, filenames, nucmer_dir, output_dir, genes_filename, operons_filename, all_pdf, draw_plots, json_output_dir, results_dir):
    # some important constants
    nucmer_prefix = os.path.join(nucmer_dir, 'nucmer_output')

    print_timestamp()
    log = logging.getLogger('quast')
    log.info('Running Genome analyzer...')

    if not os.path.isdir(output_dir):
        os.mkdir(output_dir)

    reference_chromosomes = {}
    genome_size = 0
    for name, seq in fastaparser.read_fasta(reference):
        chr_name = name.split()[0]
        chr_len = len(seq)
        genome_size += chr_len
        reference_chromosomes[chr_name] = chr_len

    # reading genome size
    #genome_size = fastaparser.get_lengths_from_fastafile(reference)[0]
    # reading reference name
    # >gi|48994873|gb|U00096.2| Escherichia coli str. K-12 substr. MG1655, complete genome
    #ref_file = open(reference, 'r')
    #reference_name = ref_file.readline().split()[0][1:]
    #ref_file.close()

    # RESULTS file
    result_filename = output_dir + '/genome_info.txt'
    res_file = open(result_filename, 'w')
    res_file.write('reference chromosomes:\n')
    for chr_name, chr_len in reference_chromosomes.iteritems():
        res_file.write('\t' + chr_name + ' (' + str(chr_len) + ' bp)\n')
    res_file.write('\n')
    res_file.write('total genome size: ' + str(genome_size) + '\n\n')
    res_file.write('gap min size: ' + str(qconfig.min_gap_size) + '\n')
    res_file.write('partial gene/operon min size: ' + str(qconfig.min_gene_overlap) + '\n\n')

    # reading genes and operons
    class FeatureContainer:
        def __init__(self, region_list=[], found_list=[], full_found=[], chr_names_dict={}):
            self.region_list = region_list
            self.found_list = found_list
            self.full_found = full_found
            self.chr_names_dict = chr_names_dict

    genes_container = FeatureContainer()
    operons_container = FeatureContainer()

    for feature_container, feature_filename, feature_name in [
            (genes_container, genes_filename, 'gene'),
            (operons_container, operons_filename, 'operon')]:

        feature_container.region_list = genes_parser.get_genes_from_file(feature_filename, feature_name)
        if len(feature_container.region_list) == 0:
            if feature_filename:
                warning('No ' + feature_name + 's were loaded.', indent='  ')
                res_file.write(feature_name + 's loaded: ' + 'None' + '\n')
            else:
                notice('Annotated ' + feature_name + 's file was not provided. Use -' + feature_name[0].capitalize() + ' option to specify it.',
                       indent='  ')
        else:
            log.info('  Loaded ' + str(len(feature_container.region_list)) + ' ' + feature_name + 's')
            res_file.write(feature_name + 's loaded: ' + str(len(feature_container.region_list)) + '\n')
            feature_container.found_list = [0] * len(feature_container.region_list)  # 0 - gene isn't found, 1 - gene is found, 2 - part of gene is found
            feature_container.chr_names_dict = chromosomes_names_dict(feature_name, feature_container.region_list, reference_chromosomes.keys())
            feature_container.full_found = []

    for filename in filenames:
        report = reporting.get(filename)
        report.add_field(reporting.Fields.REF_GENES, len(genes_container.region_list))
        report.add_field(reporting.Fields.REF_OPERONS, len(operons_container.region_list))

    # header
    res_file.write('\n\n')
    res_file.write('  %-20s  | %-20s| %-18s| %-12s| %-10s| %-10s| %-10s| %-10s|\n'
        % ('assembly', 'genome fraction (%)', 'duplication ratio', 'gaps', 'genes', 'partial', 'operons', 'partial'))
    res_file.write('  %-20s  | %-20s| %-18s| %-12s| %-10s| %-10s| %-10s| %-10s|\n'
        % ('', '', '', 'number', '', 'genes', '', 'operons'))
    res_file.write('================================================================================================================================\n')

    # for cumulative plots:
    files_genes_in_contigs = {}   #  "filename" : [ genes in sorted contigs (see below) ]
    files_operons_in_contigs = {}

    # for histograms
    genome_mapped = []

    # process all contig files  
    for id, filename in enumerate(filenames):
        log.info('  ' + id_to_str(id) + os.path.basename(filename))

        nucmer_base_filename = os.path.join(nucmer_prefix, os.path.basename(filename) + '.coords')
        if qconfig.use_all_alignments:
            nucmer_filename = nucmer_base_filename
        else:
            nucmer_filename = nucmer_base_filename + '.filtered'

        if not os.path.isfile(nucmer_filename):
            error('Nucmer\'s coords file (' + nucmer_filename + ') not found! Try to restart QUAST.',
                  indent='  ')
            #continue

        coordfile = open(nucmer_filename, 'r')
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
        aligned_blocks = {} # contig_name --> list of AlignedBlock
        # for cumulative plots
        files_genes_in_contigs[filename] = []  # i-th element is the number of genes in i-th contig (sorted by length)
        files_operons_in_contigs[filename] = []

        # for cumulative plots:
        contig_tuples = fastaparser.read_fasta(filename)  # list of FASTA entries (in tuples: name, seq)
        contig_tuples = sorted(contig_tuples, key=lambda contig: len(contig[1]), reverse=True)
        sorted_contigs_names = [name for (name, seq) in contig_tuples]
        for name in sorted_contigs_names:
            aligned_blocks[name] = []
            files_genes_in_contigs[filename].append(0)
            files_operons_in_contigs[filename].append(0)

        for line in coordfile:
            if line.strip() == '':
                break
            s1 = int(line.split('|')[0].split()[0])
            e1 = int(line.split('|')[0].split()[1])
            contig_name = line.split()[12].strip()
            chr_name = line.split()[11].strip()
            if chr_name not in genome_mapping:
                error("Something went wrong and chromosome names in your coords file (" + nucmer_base_filename + ") "
                      "differ from the names in the reference. Try to remove the file and restart QUAST.")
                continue
            aligned_blocks[contig_name].append(AlignedBlock(seqname=chr_name, start=s1, end=e1))
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
        gaps_filename = os.path.join(output_dir, os.path.basename(filename) + '_gaps.txt')
        gaps_file = open(gaps_filename, 'w')
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

        report = reporting.get(filename)

        genome_coverage = float(covered_bp) * 100 / float(genome_size)
        # calculating duplication ratio
        duplication_ratio = (report.get_field(reporting.Fields.TOTALLEN) + \
                             report.get_field(reporting.Fields.MISINTERNALOVERLAP) + \
                             report.get_field(reporting.Fields.AMBIGUOUSEXTRABASES) - \
                             report.get_field(reporting.Fields.UNALIGNEDBASES)) /\
                             ((genome_coverage / 100.0) * float(genome_size))

        res_file.write('  %-20s  | %-20s| %-18s| %-12s|'
            % (os.path.basename(filename), genome_coverage, duplication_ratio, str(gaps_count)))
        report.add_field(reporting.Fields.MAPPEDGENOME, '%.3f' % genome_coverage)
        report.add_field(reporting.Fields.DUPLICATION_RATIO, '%.3f' % duplication_ratio)
        genome_mapped.append(genome_coverage)

         # finding genes and operons
        for feature_container, feature_in_contigs, field, suffix in [
                (genes_container, files_genes_in_contigs[filename], reporting.Fields.GENES, '_genes.txt'),
                (operons_container, files_operons_in_contigs[filename], reporting.Fields.OPERONS, '_operons.txt')]:

            if not feature_container.region_list:
                res_file.write(' %-10s| %-10s|' % ('None', 'None'))
                continue    

            total_full = 0
            total_partial = 0
            found_filename = os.path.join(output_dir, os.path.basename(filename) + suffix)
            found_file = open(found_filename, 'w')
            print >>found_file, '%s\t\t%s\t%s' % ('ID or #', 'Start', 'End')
            print >>found_file, '============================'
            for i, region in enumerate(feature_container.region_list):
                feature_container.found_list[i] = 0
                for contig_id, name in enumerate(sorted_contigs_names):
                    cur_feature_is_found = False
                    for cur_block in aligned_blocks[name]:
                        if feature_container.chr_names_dict[region.seqname] != cur_block.seqname:
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
                                if feature_container.found_list[i] == 2: # already found as partial gene
                                    total_partial -= 1
                                feature_container.found_list[i] = 1
                                total_full += 1
                                id = str(region.id)
                                if id == 'None':
                                    id = '# ' + str(region.number + 1)
                                print >>found_file, '%s\t\t%d\t%d' % (id, region.start, region.end)
                                feature_in_contigs[contig_id] += 1  # inc number of found genes/operons in id-th contig

                                cur_feature_is_found = True
                                break
                            elif feature_container.found_list[i] == 0 and min(region.end, block.end) - max(region.start, block.start) >= qconfig.min_gene_overlap:
                                feature_container.found_list[i] = 2
                                total_partial += 1
                        if cur_feature_is_found:
                            break
                    if cur_feature_is_found:
                        break

            res_file.write(' %-10s| %-10s|' % (str(total_full), str(total_partial)))
            found_file.close()
            report.add_field(field, '%s + %s part' % (str(total_full), str(total_partial)))
            feature_container.full_found.append(total_full)

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
    if json_output_dir:
        if genes_container.region_list:
            json_saver.save_features_in_contigs(json_output_dir, filenames, 'genes', files_genes_in_contigs, ref_genes_num)
        if operons_container.region_list:
            json_saver.save_features_in_contigs(json_output_dir, filenames, 'operons', files_operons_in_contigs, ref_operons_num)

    if qconfig.html_report:
        from libs.html_saver import html_saver
        if genes_container.region_list:
            html_saver.save_features_in_contigs(results_dir, filenames, 'genes', files_genes_in_contigs, ref_genes_num)
        if operons_container.region_list:
            html_saver.save_features_in_contigs(results_dir, filenames, 'operons', files_operons_in_contigs, ref_operons_num)

    if draw_plots:
        # cumulative plots:
        import plotter
        if genes_container.region_list:
            plotter.genes_operons_plot(len(genes_container.region_list), filenames, files_genes_in_contigs,
                output_dir + '/genes_cumulative_plot', 'genes', all_pdf)
            plotter.histogram(filenames, genes_container.full_found, output_dir + '/complete_genes_histogram',
                '# complete genes', all_pdf)
        if operons_container.region_list:
            plotter.genes_operons_plot(len(operons_container.region_list), filenames, files_operons_in_contigs,
                output_dir + '/operons_cumulative_plot', 'operons', all_pdf)
            plotter.histogram(filenames, operons_container.full_found, output_dir + '/complete_operons_histogram',
                '# complete operons', all_pdf)
        plotter.histogram(filenames, genome_mapped, output_dir + '/genome_fraction_histogram', 'Genome fraction, %',
            all_pdf, top_value=100)

    log.info('Done.')

class AlignedBlock():
    def __init__(self, seqname=None, start=None, end=None):
        self.seqname = seqname
        self.start = start
        self.end = end
