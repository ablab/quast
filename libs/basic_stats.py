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
import reporting

def GC_content(filename):  
    """
       Returns percent of GC for assembly and list of tuples (contig_length, GC_percent)
    """
    total_GC_amount = 0
    total_contig_length = 0
    GC_info = []
    for name, seq in fastaparser.read_fasta(filename): # in tuples: (name, seq)
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

    reference_length = None
    if reference:
        reference_length = sum(fastaparser.get_lengths_from_fastafile(reference))
        reference_GC, reference_GC_info = GC_content(reference)

        # saving reference to JSON
        if json_output_dir:
            json_saver.save_reference_length(json_output_dir, reference_length)

        # saving to html
        html_saver.save_reference_length(results_dir, reference_length)

        print 'Reference genome:'
        print ' ', reference, ', Reference length =', int(reference_length), ', Reference GC % =', '%.2f' % reference_GC

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

    lists_of_GC_info = []
    import N50
    for id, (filename, lengths_list) in enumerate(itertools.izip(filenames, lists_of_lengths)):
        report = reporting.get(filename)
        n50 = N50.N50(lengths_list)
        ng50 = None
        if reference:
            ng50 = N50.NG50(lengths_list, reference_length)
        n75 = N50.N50(lengths_list, 75)
        ng75 = None
        if reference:
            ng75 = N50.NG50(lengths_list, reference_length, 75)
        total_length = sum(lengths_list)
        total_GC, GC_info = GC_content(filename)
        lists_of_GC_info.append(GC_info)
        print ' ', id_to_str(id), os.path.basename(filename), \
            ', N50 =', n50, \
            ', Total length =', total_length, \
            ', GC % = ', '%.2f' % total_GC

        report.add_field(reporting.Fields.N50, n50)
        if reference:
            report.add_field(reporting.Fields.NG50, ng50)
        report.add_field(reporting.Fields.N75, n75)
        if reference:
            report.add_field(reporting.Fields.NG75, ng75)
        report.add_field(reporting.Fields.NUMCONTIGS, len(lengths_list))
        report.add_field(reporting.Fields.LARGCONTIG, max(lengths_list))
        report.add_field(reporting.Fields.TOTALLEN, total_length)
        report.add_field(reporting.Fields.GC, ('%.2f' % total_GC))
        if reference:
            report.add_field(reporting.Fields.REFLEN, int(reference_length))
            report.add_field(reporting.Fields.REFGC, '%.2f' %  reference_GC)

    if json_output_dir:
        json_saver.save_GC_info(json_output_dir, filenames, lists_of_GC_info)

    html_saver.save_GC_info(results_dir, filenames, lists_of_GC_info)

    if draw_plots:
        ########################################################################

        # Drawing cumulative plot...
        import plotter
        plotter.cumulative_plot(reference, filenames, lists_of_lengths, output_dir + '/cumulative_plot', 'Cumulative length', all_pdf)
    
        ########################################################################

        # Drawing GC content plot...
        plotter.GC_content_plot(filenames, lists_of_GC_info, output_dir + '/GC_content_plot', all_pdf)
    
        ########################################################################

        # Drawing Nx and NGx plots...
        plotter.Nx_plot(filenames, lists_of_lengths, output_dir + '/Nx_plot', 'Nx', [], all_pdf)
        if reference:
            plotter.Nx_plot(filenames, lists_of_lengths, output_dir + '/NGx_plot', 'NGx', [reference_length for i in range(len(filenames))], all_pdf)
