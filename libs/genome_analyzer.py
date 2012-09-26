############################################################################
# Copyright (c) 2011-2012 Saint-Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################

import os
import fastaparser
import genes_parser
from libs import json_saver, reporting, qconfig
from qutils import id_to_str

__location__ = os.path.realpath(os.path.join(os.getcwd(), os.path.dirname(__file__)))


s_Mapped_genome = 'Mapped genome (%)'
s_Genes = 'Genes'
s_Operons = 'Operons'


def chromosomes_names_dict(features, chr_names):
    """
    returns dictionary to translate chromosome name in list of features (genes or operons) to
    chromosome name in reference file.
    They can differ between each other, e.g. U22222 in the list and gi|48994873|gb|U22222| in the reference
    """
    no_chr = False
    chr_name_dict = {}
    for feature in features:
        for chr_name in chr_names:
            if feature.seqname in chr_name:
                chr_name_dict[feature.seqname] = chr_name
                break
        if feature.seqname not in chr_name_dict:
            no_chr = True
            chr_name_dict[feature.seqname] = None

    if no_chr:
        print '  Warning: Some of chromosome names in genes or operons differ from the names in the reference.'
    return chr_name_dict


def do(reference, filenames, nucmer_dir, output_dir, genes_filename, operons_filename, all_pdf, draw_plots, json_output_dir, results_dir):

    # some important constants
    nucmer_prefix = os.path.join(os.path.join(__location__, ".."), nucmer_dir, 'nucmer_output')
    #threshold = 10.0 # in %
    min_gap_size = 50 # for calculating number or gaps in genome coverage
    min_overlap = 100 # for genes and operons finding

    print 'Running Genome analyzer...'

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
    res_file.write('gap min size: ' + str(min_gap_size) + '\n')
    res_file.write('partial gene/operon min size: ' + str(min_overlap) + '\n\n')

    # reading genes
    genes = genes_parser.get_genes_from_file(genes_filename, 'gene')
    genes_found = []
    genes_chr_names_dict = {}
    if len(genes) == 0:
        print '  Warning: no genes loaded.'
        res_file.write('genes loaded: ' + 'None' + '\n')
    else:
        print '  Loaded ' + str(len(genes)) + ' genes'
        res_file.write('genes loaded: ' + str(len(genes)) + '\n')
        genes_found = [0 for _ in genes] # 0 - gene isn't found, 1 - gene is found, 2 - part of gene is found
        genes_chr_names_dict = chromosomes_names_dict(genes, reference_chromosomes.keys())

    # reading operons
    # TODO: clear copy-pasting!
    operons = genes_parser.get_genes_from_file(operons_filename, 'operon')
    operons_found = []
    operons_chr_names_dict = {}
    if len(operons) == 0:
        print '  Warning: no operons loaded.'
        res_file.write('operons loaded: ' + 'None' + '\n')
    else:
        print '  Loaded ' + str(len(operons)) + ' operons'
        res_file.write('operons loaded: ' + str(len(operons)) + '\n')
        operons_found = [0 for _ in operons] # 0 - gene isn't found, 1 - gene is found, 2 - part of gene is found
        operons_chr_names_dict = chromosomes_names_dict(operons, reference_chromosomes.keys())

    # header
    res_file.write('\n\n')
    res_file.write('  %-20s  | %-20s| %-18s| %-12s| %-10s| %-10s| %-10s| %-10s|\n'
        % ('assembly', 'genome fraction (%)', 'duplication ratio', 'gaps', 'genes', 'partial', 'operons', 'partial'))
    res_file.write('  %-20s  | %-20s| %-18s| %-12s| %-10s| %-10s| %-10s| %-10s|\n'
        % ('', '', '', 'number', '', 'genes', '', 'operons'))
    res_file.write('================================================================================================================================\n')

    # for cumulative plots:
    files_contigs = {}   #  "filename" : [ [contig_blocks] ]   

    # for histograms
    full_genes = []
    full_operons = []
    genome_mapped = []

    # process all contig files  
    for id, filename in enumerate(filenames):
        print ' ', id_to_str(id), os.path.basename(filename), '...'

        # for cumulative plots
        contig_blocks = {'':[]}
        files_contigs[filename] = []

        nucmer_filename = os.path.join(nucmer_prefix, os.path.basename(filename) + '.coords')
        if not os.path.isfile(nucmer_filename):
            print '  ERROR: nucmer coord file (' + nucmer_filename + ') not found, skipping...'
            continue

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
        #genome = [0 for i in range(genome_size + 1)]
        aligned_blocks = []
        #print genome_mapping

        # '''
        # nodes_len_coeff = collections.defaultdict(lambda:0.0)
        # for line in coordfile:
        #     sections = line.split('|')
        #     node_id = sections[len(sections) - 1]    

        #     len1 = float(sections[2].split()[0])
        #     len2 = float(sections[2].split()[1])
        #     idy = float(sections[3].strip())
        #     len_coef = (len1 + len2) / 2.0 * idy / 100.0
        #     if len_coef > nodes_len_coeff[node_id]:
        #         nodes_len_coeff[node_id] = len_coef

        # coordfile.seek(0)        

        # for line in coordfile: 
        #     sections = line.split('|')          
        #     node_id = sections[len(sections) - 1]    

        #     len1 = float(sections[2].split()[0])
        #     len2 = float(sections[2].split()[1])
        #     idy = float(sections[3].strip())
        #     len_coef = (len1 + len2) / 2.0 * idy / 100.0
        #     if len_coef > nodes_len_coeff[node_id] * (100.0 - threshold) / 100.0:
        #         s1 = int(sections[0].split()[0])
        #         e1 = int(sections[0].split()[1])
        #         for i in range(s1, e1 + 1):
        #             genome[i] = 1
        # '''

        for line in coordfile:
            s1 = int(line.split('|')[0].split()[0])
            e1 = int(line.split('|')[0].split()[1])
            contig_name = line.split()[-1].strip()
            chr_name = line.split()[11].strip()
            if contig_name in contig_blocks:
                contig_blocks[contig_name].append((s1, e1))
            else:
                contig_blocks[contig_name] = [(s1, e1)]
            aligned_blocks.append(Aligned_block(seqname=chr_name, start=s1, end=e1))
            for i in range(s1, e1 + 1):
                genome_mapping[chr_name][i] = 1
                #genome[i] = 1
        coordfile.close()

        # for cumulative plots:
        contig_tuples = fastaparser.read_fasta(filename)  # list of FASTA entries (in tuples: name, seq)
        contig_tuples = sorted(contig_tuples, key=lambda contig: len(contig[1]), reverse = True)
        for contig_name, seq in contig_tuples:
            if contig_name not in contig_blocks:
                contig_blocks[contig_name] = []
            files_contigs[filename].append( contig_blocks[contig_name] )

        # counting genome coverage and gaps number
        covered_bp = 0
        gaps_count = 0
        for chr_name, chr_len in reference_chromosomes.iteritems():
            cur_gap_size = 0
            for i in range(1, chr_len + 1):
                if genome_mapping[chr_name][i] == 1:
                    covered_bp += 1
                    cur_gap_size = 0
                else:
                    cur_gap_size += 1
                    if cur_gap_size == min_gap_size:
                        gaps_count += 1

        report = reporting.get(filename)

        genome_coverage = float(covered_bp) * 100 / float(genome_size)
        # calculating duplication ratio
        duplication_ratio = (report.get_field(reporting.Fields.TOTALLEN) - report.get_field(reporting.Fields.UNALIGNEDBASES)) /\
            ((genome_coverage / 100.0) * float(genome_size))

        res_file.write('  %-20s  | %-20s| %-18s| %-12s|'
            % (os.path.basename(filename), genome_coverage, duplication_ratio, str(gaps_count)))
        report.add_field(reporting.Fields.MAPPEDGENOME, '%.3f' % genome_coverage)
        report.add_field(reporting.Fields.DUPLICATION_RATIO, '%.3f' % duplication_ratio)
        genome_mapped.append(genome_coverage)

        # finding genes and operons
        for regionlist, chr_names_dict, field, suffix, full_list, found_list in [
                (genes, genes_chr_names_dict, reporting.Fields.GENES, '_genes.txt', full_genes, genes_found), 
                (operons, operons_chr_names_dict, reporting.Fields.OPERONS, '_operons.txt', full_operons, operons_found)]:

            if not regionlist:
                res_file.write(' %-10s| %-10s|' % ('None', 'None'))
                continue    

            total_full = 0
            total_partial = 0
            found_filename = os.path.join(output_dir, os.path.basename(filename) + suffix)
            found_file = open(found_filename, 'w')
            for i, region in enumerate(regionlist):
                found_list[i] = 0
                for block in aligned_blocks:
                    if chr_names_dict[region.seqname] != block.seqname:
                        continue
                    if region.end <= block.start or block.end <= region.start:
                        continue
                    elif block.start <= region.start and region.end <= block.end:
                        if found_list[i] == 2: # already found as partial gene
                            total_partial -= 1
                        found_list[i] = 1
                        total_full += 1
                        print >>found_file, '%d\t%d\t%d' % (id + 1, region.start, region.end)
                        break
                    elif found_list[i] == 0 and min(region.end, block.end) - max(region.start, block.start) >= min_overlap:
                        found_list[i] = 2
                        total_partial += 1

            res_file.write(' %-10s| %-10s|' % (str(total_full), str(total_partial)))
            found_file.close()
            report.add_field(field, '%s + %s part' % (str(total_full), str(total_partial)))
            full_list.append(total_full)

        # finishing output for current contigs file
        res_file.write('\n')

    res_file.close()

    # saving json
    if json_output_dir:
        if genes or operons:
            json_saver.save_contigs(json_output_dir, filenames, files_contigs)
        if genes:
            json_saver.save_genes(json_output_dir, genes, genes_found)
        if operons:
            json_saver.save_operons(json_output_dir, operons, operons_found)

    if qconfig.html_report:
        from libs.html_saver import html_saver
        if genes or operons:
            html_saver.save_contigs(results_dir, filenames, files_contigs)
        if genes:
            html_saver.save_genes(results_dir, genes, genes_found)
        if operons:
            html_saver.save_operons(results_dir, operons, operons_found)

    if draw_plots:
        # cumulative plots:
        import plotter
        if genes:
            plotter.genes_operons_plot(filenames, files_contigs, genes, genes_found, output_dir + '/genes_cumulative_plot', 'genes', all_pdf)
            plotter.histogram(filenames, full_genes, output_dir + '/complete_genes_histogram', '# complete genes', all_pdf)
        if operons:
            plotter.genes_operons_plot(filenames, files_contigs, operons, operons_found, output_dir + '/operons_cumulative_plot', 'operons', all_pdf)
            plotter.histogram(filenames, full_operons, output_dir + '/complete_operons_histogram', '# complete operons', all_pdf)

        plotter.histogram(filenames, genome_mapped, output_dir + '/genome_fraction_histogram', 'Genome fraction, %', all_pdf, top_value=100)

    print '  Done'

class Aligned_block():
    def __init__(self, seqname=None, start=None, end=None):
        self.seqname = seqname
        self.start = start
        self.end = end
