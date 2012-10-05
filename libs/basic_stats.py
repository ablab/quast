############################################################################
# Copyright (c) 2011-2012 Saint-Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################

import os
import itertools
import fastaparser
from libs.html_saver import json_saver
from libs import qconfig
from qutils import id_to_str
import reporting

def GC_content(filename):  
    """
       Returns percent of GC for assembly and list of tuples (contig_length, GC_percent)
    """
    total_GC_amount = 0
    total_contig_length = 0
    GC_info = []
    for name, seq_full in fastaparser.read_fasta(filename): # in tuples: (name, seq)
        seq_full = seq_full.upper()
        total_GC_amount += seq_full.count("G") + seq_full.count("C")
        total_contig_length += len(seq_full)
        n = 100 # blocks of length 100
        # non-overlapping windows
        for seq in [seq_full[i:i+n] for i in range(0, (len(seq_full) / n) * n, n)]:
            # contig_length = len(seq)
            seq = seq.upper()
            GC_amount = seq.count("G") + seq.count("C")
            #GC_info.append((contig_length, GC_amount * 100.0 / contig_length))
            GC_info.append((1, GC_amount * 100.0 / n))

#        # sliding windows
#        seq = seq_full[0:n]
#        GC_amount = seq.count("G") + seq.count("C")
#        GC_info.append((1, GC_amount * 100.0 / n))
#        for i in range(len(seq_full) - n):
#            GC_amount = GC_amount - seq_full[i].count("G") - seq_full[i].count("C")
#            GC_amount = GC_amount + seq_full[i + n].count("G") + seq_full[i + n].count("C")
#            if GC_amount == 100:
#                print "YOU!", seq_full[i+1:i+1+n]
#            GC_info.append((1, GC_amount * 100.0 / n))

    return total_GC_amount * 100.0 / total_contig_length, GC_info


def do(reference, filenames, output_dir, all_pdf, draw_plots, json_output_dir, results_dir):
    
    if not os.path.isdir(output_dir):
        os.mkdir(output_dir)

    reference_length = None
    if reference:
        reference_length = sum(fastaparser.get_lengths_from_fastafile(reference))
        reference_GC, reference_GC_info = GC_content(reference)

        # Saving the reference in JSON
        if json_output_dir:
            json_saver.save_reference_length(json_output_dir, reference_length)

        # Saving for an HTML report
        if qconfig.html_report:
            from libs.html_saver import html_saver
            html_saver.save_reference_length(results_dir, reference_length)

        print 'Reference genome:'
        print ' ', reference, ', Reference length =', int(reference_length), ', Reference GC % =', '%.2f' % reference_GC

    print 'Contigs files: '
    lists_of_lengths = []
    numbers_of_Ns = []
    for id, filename in enumerate(filenames):
        print ' ', id_to_str(id), os.path.basename(filename)
        #lists_of_lengths.append(fastaparser.get_lengths_from_fastafile(filename))
        list_of_length = []
        number_of_Ns = 0
        for (name, seq) in fastaparser.read_fasta(filename):
            list_of_length.append(len(seq))
            number_of_Ns += seq.count('N')
        lists_of_lengths.append(list_of_length)
        numbers_of_Ns.append(number_of_Ns)

    # saving lengths to JSON
    if json_output_dir:
        json_saver.save_contigs_lengths(json_output_dir, filenames, lists_of_lengths)

    if qconfig.html_report:
        from libs.html_saver import html_saver
        html_saver.save_contigs_lengths(results_dir, filenames, lists_of_lengths)

    ########################################################################

    print 'Calculating N50 and L50...'

    lists_of_GC_info = []
    import N50
    for id, (filename, lengths_list, number_of_Ns) in enumerate(itertools.izip(filenames, lists_of_lengths, numbers_of_Ns)):
        report = reporting.get(filename)
        n50, l50 = N50.N50_and_L50(lengths_list)
        ng50, lg50 = None, None
        if reference:
            ng50, lg50 = N50.NG50_and_LG50(lengths_list, reference_length)
        n75, l75 = N50.N50_and_L50(lengths_list, 75)
        ng75, lg75 = None, None
        if reference:
            ng75, lg75 = N50.NG50_and_LG50(lengths_list, reference_length, 75)
        total_length = sum(lengths_list)
        total_GC, GC_info = GC_content(filename)
        lists_of_GC_info.append(GC_info)
        print ' ', id_to_str(id), os.path.basename(filename), \
            ', N50 =', n50,\
            ', L50 =', l50,\
            ', Total length =', total_length, \
            ', GC % = ', '%.2f' % total_GC,\
            ', N\'s % = ', '%.5f' % (float(100 * number_of_Ns) / float(total_length))\

        report.add_field(reporting.Fields.N50, n50)
        report.add_field(reporting.Fields.L50, l50)
        if reference:
            report.add_field(reporting.Fields.NG50, ng50)
            report.add_field(reporting.Fields.LG50, lg50)
        report.add_field(reporting.Fields.N75, n75)
        report.add_field(reporting.Fields.L75, l75)
        if reference:
            report.add_field(reporting.Fields.NG75, ng75)
            report.add_field(reporting.Fields.LG75, lg75)
        report.add_field(reporting.Fields.NUMCONTIGS, len(lengths_list))
        report.add_field(reporting.Fields.LARGCONTIG, max(lengths_list))
        report.add_field(reporting.Fields.TOTALLEN, total_length)
        report.add_field(reporting.Fields.GC, ('%.2f' % total_GC))
        report.add_field(reporting.Fields.UNCALLED, number_of_Ns)
        report.add_field(reporting.Fields.UNCALLED_PERCENT, ('%.5f' % (float(100 * number_of_Ns) / float(total_length))))
        if reference:
            report.add_field(reporting.Fields.REFLEN, int(reference_length))
            report.add_field(reporting.Fields.REFGC, '%.2f' %  reference_GC)

    if json_output_dir:
        json_saver.save_GC_info(json_output_dir, filenames, lists_of_GC_info)


    if qconfig.html_report:
        from libs.html_saver import html_saver
        html_saver.save_GC_info(results_dir, filenames, lists_of_GC_info)

    if draw_plots:
        import plotter
        ########################################################################import plotter
        plotter.cumulative_plot(reference, filenames, lists_of_lengths, output_dir + '/cumulative_plot', 'Cumulative length', all_pdf)
    
        ########################################################################
        # Drawing GC content plot...
        lists_of_GC_info_with_ref = lists_of_GC_info
        if reference:
            total_GC, GC_info = GC_content(reference)
            lists_of_GC_info_with_ref.append(GC_info)
        # Drawing cumulative plot...
        plotter.GC_content_plot(reference, filenames, lists_of_GC_info_with_ref, output_dir + '/GC_content_plot', all_pdf)

        ########################################################################
        # Drawing Nx and NGx plots...
        plotter.Nx_plot(filenames, lists_of_lengths, output_dir + '/Nx_plot', 'Nx', [], all_pdf)
        if reference:
            plotter.Nx_plot(filenames, lists_of_lengths, output_dir + '/NGx_plot', 'NGx', [reference_length for i in range(len(filenames))], all_pdf)
