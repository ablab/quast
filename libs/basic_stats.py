############################################################################
# Copyright (c) 2011-2012 Saint-Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################

import os
import itertools
import fastaparser
import json_saver
from qutils import id_to_str
from html_saver import html_saver


def GC_content(filename):  
    """
       Returns percent of GC for assembly and list of tuples (contig_length, GC_percent)
    """
    fasta_entries = fastaparser.read_fasta(filename) # in tuples: (name, seq)    
    total_GC_amount = 0
    total_contig_length = 0
    GC_info = []
    for (name, seq) in fasta_entries:
        contig_length = len(seq)
        total_contig_length += contig_length
        seq = seq.upper()
        GC_amount = seq.count("G") + seq.count("C")
        total_GC_amount += GC_amount
        GC_info.append((contig_length, GC_amount * 100.0 / contig_length))
    return total_GC_amount * 100.0 / total_contig_length, GC_info


def do(reference, filenames, output_dir, all_pdf, draw_plots, json_output_dir, results_dir):
    
    if not os.path.isdir(output_dir):
        os.mkdir(output_dir)

    ########################################################################
    report_dict = {'header' : []}
    for filename in filenames:
        report_dict[os.path.basename(filename)] = []
    
    ########################################################################

    if reference:
        reference_length = fastaparser.get_lengths_from_fastafile(reference)[0]

        # saving reference to JSON
        if json_output_dir:
            json_saver.save_reference_length(json_output_dir, reference_length)

        # saving to html
        html_saver.save_reference_length(results_dir, reference_length)

        print 'Reference genome:'
        print ' ', reference, ', reference length =', int(reference_length)
    print 'Contigs files: '
    lists_of_lengths = []
    for id, filename in enumerate(filenames):
        print ' ', id_to_str(id), os.path.basename(filename)
        lists_of_lengths.append(fastaparser.get_lengths_from_fastafile(filename))

    # saving lengths to JSON
    if json_output_dir:
        json_saver.save_contigs_lengths(json_output_dir, filenames, lists_of_lengths)

    # saving to html
    html_saver.save_contigs_lengths(results_dir, filenames, lists_of_lengths)

    ########################################################################

    print 'Calculating N50...'
    report_dict['header'].append('N50')
    if reference:
        report_dict['header'].append('NG50')
    report_dict['header'].append('N75')
    if reference:
        report_dict['header'].append('NG75')
    report_dict['header'].append('Number of contigs')
    report_dict['header'].append('Largest contig')
    report_dict['header'].append('Total length')
    if reference:
        report_dict['header'].append('Reference length')
    
    lists_of_GC_info = []
    import N50
    for id, (filename, lengths_list) in enumerate(itertools.izip(filenames, lists_of_lengths)):
        n50 = N50.N50(lengths_list)
        if reference:
            ng50 = N50.NG50(lengths_list, reference_length)
        n75 = N50.N50(lengths_list, 75)
        if reference:
            ng75 = N50.NG50(lengths_list, reference_length, 75)
        total_length = sum(lengths_list)
        total_GC, GC_info = GC_content(filename)
        lists_of_GC_info.append(GC_info)
        print ' ', id_to_str(id), os.path.basename(filename), \
            ', N50 =', n50, \
            ', Total length =', total_length, \
            ', GC % = ', total_GC
        
        report_dict[os.path.basename(filename)].append(n50)
        if reference:
            report_dict[os.path.basename(filename)].append(ng50)
        report_dict[os.path.basename(filename)].append(n75)
        if reference:
            report_dict[os.path.basename(filename)].append(ng75)
        report_dict[os.path.basename(filename)].append(len(lengths_list))
        report_dict[os.path.basename(filename)].append(max(lengths_list))
        report_dict[os.path.basename(filename)].append(total_length)
        if reference:
            report_dict[os.path.basename(filename)].append(int(reference_length))

    if draw_plots:
        ########################################################################

        # Drawing cumulative plot...
        import plotter
        plotter.cumulative_plot(filenames, lists_of_lengths, output_dir + '/cumulative_plot', 'Cumulative length', all_pdf)
    
        ########################################################################

        # Drawing GC content plot...
        import plotter
        plotter.GC_content_plot(filenames, lists_of_GC_info, output_dir + '/GC_content_plot', all_pdf)
    
        ########################################################################

        # Drawing Nx and NGx plots...
        plotter.Nx_plot(filenames, lists_of_lengths, output_dir + '/Nx_plot', 'Nx', [], all_pdf)
        if reference:
            plotter.Nx_plot(filenames, lists_of_lengths, output_dir + '/NGx_plot', 'NGx', [reference_length for i in range(len(filenames))], all_pdf)

    return report_dict
