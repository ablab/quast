############################################################################
# Copyright (c) 2011-2012 Saint-Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################

import os
import shutil
import sys
import fastaparser
import genes_parser
import subprocess
import collections
import itertools
from libs import json_saver
from libs.html_saver import html_saver
from qutils import id_to_str

def do(reference, filenames, output_dir, nucmer_dir, genes_filename, operons_filename, all_pdf, draw_plots, json_output_dir, results_dir):

    # some important constants
    nucmer_prefix = os.path.join(os.path.abspath(sys.path[0]), nucmer_dir + '/nucmer_')
    #threshold = 10.0 # in %
    min_gap_size = 50 # for calculating number or gaps in genome coverage
    min_overlap = 100 # for genes and operons finding

    print 'Running Genome analyzer...'

    if not os.path.isdir(output_dir):
        os.mkdir(output_dir)

    ########################################################################
    report_dict = {'header' : []}
    for filename in filenames:
        report_dict[os.path.basename(filename)] = []

    # reading genome size
    genome_size = fastaparser.get_lengths_from_fastafile(reference)[0]
    # reading reference name
    # >gi|48994873|gb|U00096.2| Escherichia coli str. K-12 substr. MG1655, complete genome
    ref_file = open(reference, 'r')
    reference_name = ref_file.readline().split()[0][1:]
    ref_file.close()

    # RESULTS file
    result_filename = output_dir + '/genome_info.txt'
    res_file = open(result_filename, 'w')
    res_file.write('reference: ' + reference_name + '\n')
    res_file.write('genome size: ' + str(genome_size) + '\n\n')

    res_file.write('min gap size: ' + str(min_gap_size) + '\n')
    res_file.write('min gene/operon overlap: ' + str(min_overlap) + '\n\n')

    # reading genes
    genes = genes_parser.get_genes_from_file(genes_filename, 'gene')
    genes_found = []
    if len(genes) == 0:
        print '  Warning: no genes loaded.'
    else:
        print '  Loaded ' + str(len(genes)) + ' genes'
        res_file.write('genes: ' + str(len(genes)) + '\n')
        genes_found = [0 for gene in genes] # 0 - gene isn't found, 1 - gene is found, 2 - part of gene is found

    # reading operons
    operons = genes_parser.get_genes_from_file(operons_filename, 'operon')
    operons_found = []
    if len(operons) == 0:
        print '  Warning: no operons loaded.'
    else:
        print '  Loaded ' + str(len(operons)) + ' operons'
        res_file.write('operons: ' + str(len(operons)) + '\n')
        operons_found = [0 for operon in operons] # 0 - gene isn't found, 1 - gene is found, 2 - part of gene is found

    # header
    res_file.write('\n\n')
    res_file.write('  %-20s  | %-20s| %-12s| %-10s| %-10s| %-10s| %-10s\n' % ('contigs file', 'mapped genome (%)', 'gaps', 'genes', 'partial', 'operons', 'partial'))
    res_file.write('  %-20s  | %-20s| %-12s| %-10s| %-10s| %-10s| %-10s\n' % ('', '', 'number', '', 'genes', '', 'operons'))
    res_file.write('======================================================================================================\n')


    report_dict['header'].append('Mapped genome (%)')
    report_dict['header'].append('Genes')
    report_dict['header'].append('Operons')

    # for cumulative plots:
    files_contigs = {}   #  "filename" : [ [contig_blocks] ]   

    # for histograms
    full_genes   = []
    full_operons = []
    genome_mapped = []

    # process all contig files  
    for id, filename in enumerate(filenames):
        print ' ', id_to_str(id), os.path.basename(filename), '...'

        # for cumulative plots
        contig_blocks = {'':[]}
        files_contigs[filename] = []

        nucmer_filename = nucmer_prefix + os.path.basename(filename) + '.coords'
        if not os.path.isfile(nucmer_filename):
            print '  ERROR: nucmer coord file (' + nucmer_filename + ') not found, skipping...'
            report_dict[os.path.basename(filename)] += ['N/A'] * 3
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

        genome = [0 for i in range(genome_size + 1)]
        aligned_blocks = []

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
            contig_name = line.split('|')[-1].strip()
            if contig_name in contig_blocks:
                contig_blocks[contig_name].append((s1, e1))
            else:
                contig_blocks[contig_name] = [(s1, e1)]
            aligned_blocks.append([s1, e1])
            for i in range(s1, e1 + 1):
                genome[i] = 1
        coordfile.close()

        # for cumulative plots:
        contig_tuples = fastaparser.read_fasta(filename)  # list of FASTA entries (in tuples: name, seq)
        contig_tuples = sorted(contig_tuples, key=lambda contig: len(contig[1]), reverse = True)
        for contig in contig_tuples:
            contig_name = contig[0][1:]
            if contig_name not in contig_blocks:
                contig_blocks[contig_name] = []
            files_contigs[filename].append( contig_blocks[contig_name] )

        # counting genome coverage and gaps number
        covered_bp = 0
        gaps_count = 0
        cur_gap_size = 0
        for i in range(1, genome_size + 1):
            if genome[i] == 1:
                covered_bp += 1
                cur_gap_size = 0
            else:
                cur_gap_size += 1
                if cur_gap_size == min_gap_size:
                    gaps_count += 1

        genome_coverage = float(covered_bp) * 100 / float(genome_size)
        res_file.write('  %-20s  | %-20s| %-12s|' % (os.path.basename(filename), genome_coverage, str(gaps_count)))
        report_dict[os.path.basename(filename)].append('%.3f' % genome_coverage)
        genome_mapped.append(genome_coverage)

        # finding genes
        total_full = 0
        total_partial = 0
        found_genes_filename = os.path.join(output_dir, os.path.basename(filename) + '_genes.txt')
        found_genes_file = open(found_genes_filename, 'w')
        for i, gene in enumerate(genes):
            genes_found[i] = 0
            for block in aligned_blocks:
                if gene.end <= block[0] or block[1] <= gene.start:   # [0] - start, [1] - end
                    continue
                elif block[0] <= gene.start and gene.end <= block[1]:
                    if genes_found[i] == 2: # already found as partial gene
                        total_partial -= 1
                    genes_found[i] = 1
                    total_full += 1
                    found_genes_file.write(str(id + 1) + "\t" + str(gene.start) + "\t" + str(gene.end) + "\n")
                    break
                elif genes_found[i] == 0 and min(gene.end, block[1]) - max(gene.start, block[0]) >= min_overlap:
                    genes_found[i] = 2
                    total_partial += 1

        res_file.write(' %-10s| %-10s|' % (str(total_full), str(total_partial)))
        found_genes_file.close()
        if genes:
            report_dict[os.path.basename(filename)].append('%s + %s part' % (str(total_full), str(total_partial)))
            full_genes.append(total_full)
        else:
            report_dict[os.path.basename(filename)].append("N/A")

        # finding operons
        total_full = 0
        total_partial = 0
        found_operons_filename = os.path.join(output_dir, os.path.basename(filename) + '_operons.txt')
        found_operons_file = open(found_operons_filename, 'w')
        for i, operon in enumerate(operons):
            operons_found[i] = 0
            for block in aligned_blocks:
                if operon.end <= block[0] or block[1] <= operon.start:   # [0] - start, [1] - end
                    continue
                elif block[0] <= operon.start and operon.end <= block[1]:
                    if operons_found[i] == 2: # already found as partial gene
                        total_partial -= 1
                    operons_found[i] = 1
                    total_full += 1
                    found_operons_file.write(str(id + 1) + "\t" + str(operon.start) + "\t" + str(operon.end) + "\n")
                    break
                elif operons_found[i] == 0 and min(operon.end, block[1]) - max(operon.start, block[0]) >= min_overlap:
                    operons_found[i] = 2
                    total_partial += 1

        res_file.write(' %-10s| %-10s|' % (str(total_full), str(total_partial)))
        found_operons_file.close()
        if operons:
            report_dict[os.path.basename(filename)].append('%s + %s part' % (str(total_full), str(total_partial)))
            full_operons.append(total_full)
        else:
            report_dict[os.path.basename(filename)].append("N/A")

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

    # saving html
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
            plotter.histogram(filenames, full_genes, output_dir + '/complete_genes_histogram', 'Number of complete genes', all_pdf, 'Number of complete genes')
        if operons:
            plotter.genes_operons_plot(filenames, files_contigs, operons, operons_found, output_dir + '/operons_cumulative_plot', 'operons', all_pdf)
            plotter.histogram(filenames, full_operons, output_dir + '/complete_operons_histogram', 'Number of complete operons', all_pdf, 'Number of complete operons')

        plotter.histogram(filenames, genome_mapped, output_dir + '/genome_mapped_histogram', 'Genome mapped, %', all_pdf, top_value=100)

    print '  Done'

    return report_dict
