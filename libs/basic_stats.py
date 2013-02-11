############################################################################
# Copyright (c) 2011-2013 Saint-Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################

import logging
import os
import itertools
import fastaparser
from libs.html_saver import json_saver
from libs import qconfig
from qutils import id_to_str, print_timestamp
import reporting

def GC_content(filename):  
    """
       Returns percent of GC for assembly and GC distribution: (list of GC%, list of # windows)
    """
    total_GC_amount = 0
    total_contig_length = 0
    GC_bin_num = int(100 / qconfig.GC_bin_size) + 1
    GC_distribution_x = [i * qconfig.GC_bin_size for i in range(0, GC_bin_num)] # list of X-coordinates, i.e. GC %
    GC_distribution_y = [0] * GC_bin_num # list of Y-coordinates, i.e. # windows with GC % = x
    for name, seq_full in fastaparser.read_fasta(filename): # in tuples: (name, seq)
        total_GC_amount += seq_full.count("G") + seq_full.count("C")
        total_contig_length += len(seq_full) - seq_full.count("N")
        n = 100 # blocks of length 100
        # non-overlapping windows
        for seq in [seq_full[i:i+n] for i in range(0, len(seq_full), n)]:
            # skip block if it has less than half of ACGT letters (it also helps with "ends of contigs")
            ACGT_len = len(seq) - seq.count("N")
            if ACGT_len < (n / 2):
                continue

            GC_len = seq.count("G") + seq.count("C")
            GC_percent = 100.0 * GC_len / ACGT_len
            GC_distribution_y[int(int(GC_percent / qconfig.GC_bin_size) * qconfig.GC_bin_size)] += 1

#    GC_info = []
#    for name, seq_full in fastaparser.read_fasta(filename): # in tuples: (name, seq)
#        total_GC_amount += seq_full.count("G") + seq_full.count("C")
#        total_contig_length += len(seq_full) - seq_full.count("N")
#        n = 100 # blocks of length 100
#        # non-overlapping windows
#        for seq in [seq_full[i:i+n] for i in range(0, len(seq_full), n)]:
#            # skip block if it has less than half of ACGT letters (it also helps with "ends of contigs")
#            ACGT_len = len(seq) - seq.count("N")
#            if ACGT_len < (n / 2):
#                continue
#            # contig_length = len(seq)
#            GC_amount = seq.count("G") + seq.count("C")
#            #GC_info.append((contig_length, GC_amount * 100.0 / contig_length))
#            GC_info.append((1, 100 * GC_amount / ACGT_len))

#        # sliding windows
#        seq = seq_full[0:n]
#        GC_amount = seq.count("G") + seq.count("C")
#        GC_info.append((1, GC_amount * 100.0 / n))
#        for i in range(len(seq_full) - n):
#            GC_amount = GC_amount - seq_full[i].count("G") - seq_full[i].count("C")
#            GC_amount = GC_amount + seq_full[i + n].count("G") + seq_full[i + n].count("C")
#            GC_info.append((1, GC_amount * 100.0 / n))

    if total_contig_length == 0:
        total_GC = None
    else:
        total_GC = total_GC_amount * 100.0 / total_contig_length

    return total_GC, (GC_distribution_x, GC_distribution_y)


def do(reference, filenames, output_dir, all_pdf, draw_plots, json_output_dir, results_dir):
    log = logging.getLogger('quast')

    print_timestamp()
    log.info("Running Basic statistics processor...")
    
    if not os.path.isdir(output_dir):
        os.mkdir(output_dir)

    reference_length = None
    if reference:
        reference_length = sum(fastaparser.get_lengths_from_fastafile(reference))
        reference_GC, reference_GC_distribution = GC_content(reference)

        log.info('  Reference genome:')
        log.info('    ' + os.path.basename(reference) + ', Reference length = ' + str(reference_length) + ', Reference GC % = ' + '%.2f' % reference_GC)
    elif qconfig.estimated_reference_size:
        reference_length = qconfig.estimated_reference_size
        log.info('  Estimated reference length = ' + str(reference_length))

    if reference_length:
        # Saving the reference in JSON
        if json_output_dir:
            json_saver.save_reference_length(json_output_dir, reference_length)

        # Saving for an HTML report
        if qconfig.html_report:
            from libs.html_saver import html_saver
            html_saver.save_reference_length(results_dir, reference_length)

    log.info('  Contigs files: ')
    lists_of_lengths = []
    numbers_of_Ns = []
    for id, filename in enumerate(filenames):
        log.info('    ' + id_to_str(id) + os.path.basename(filename))
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

    log.info('  Calculating N50 and L50...')

    list_of_GC_distributions = []
    import N50
    for id, (filename, lengths_list, number_of_Ns) in enumerate(itertools.izip(filenames, lists_of_lengths, numbers_of_Ns)):
        report = reporting.get(filename)
        n50, l50 = N50.N50_and_L50(lengths_list)
        ng50, lg50 = None, None
        if reference_length:
            ng50, lg50 = N50.NG50_and_LG50(lengths_list, reference_length)
        n75, l75 = N50.N50_and_L50(lengths_list, 75)
        ng75, lg75 = None, None
        if reference_length:
            ng75, lg75 = N50.NG50_and_LG50(lengths_list, reference_length, 75)
        total_length = sum(lengths_list)
        total_GC, GC_distribution = GC_content(filename)
        list_of_GC_distributions.append(GC_distribution)
        log.info('    ' + id_to_str(id) + os.path.basename(filename) + \
            ', N50 = ' + str(n50) + \
            ', L50 = ' + str(l50) + \
            ', Total length = ' + str(total_length) + \
            ', GC % = ' + ('%.2f' % total_GC if total_GC is not None else 'undefined') + \
            ', # N\'s per 100 kbp = ' + ' %.2f' % (float(number_of_Ns) * 100000.0 / float(total_length)) )

        report.add_field(reporting.Fields.N50, n50)
        report.add_field(reporting.Fields.L50, l50)
        if reference_length:
            report.add_field(reporting.Fields.NG50, ng50)
            report.add_field(reporting.Fields.LG50, lg50)
        report.add_field(reporting.Fields.N75, n75)
        report.add_field(reporting.Fields.L75, l75)
        if reference_length:
            report.add_field(reporting.Fields.NG75, ng75)
            report.add_field(reporting.Fields.LG75, lg75)
        report.add_field(reporting.Fields.NUMCONTIGS, len(lengths_list))
        report.add_field(reporting.Fields.LARGCONTIG, max(lengths_list))
        report.add_field(reporting.Fields.TOTALLEN, total_length)
        report.add_field(reporting.Fields.GC, ('%.2f' % total_GC if total_GC else None))
        report.add_field(reporting.Fields.UNCALLED, number_of_Ns)
        report.add_field(reporting.Fields.UNCALLED_PERCENT, ('%.2f' % (float(number_of_Ns) * 100000.0 / float(total_length))))
        if reference:
            report.add_field(reporting.Fields.REFLEN, int(reference_length))
            report.add_field(reporting.Fields.REFGC, '%.2f' %  reference_GC)
        elif reference_length:
            report.add_field(reporting.Fields.ESTREFLEN, int(reference_length))

    if json_output_dir:
        json_saver.save_GC_info(json_output_dir, filenames, list_of_GC_distributions)

    if qconfig.html_report:
        from libs.html_saver import html_saver
        html_saver.save_GC_info(results_dir, filenames, list_of_GC_distributions)

    if draw_plots:
        import plotter
        ########################################################################import plotter
        plotter.cumulative_plot(reference, filenames, lists_of_lengths, output_dir + '/cumulative_plot', 'Cumulative length', all_pdf)
    
        ########################################################################
        # Drawing GC content plot...
        list_of_GC_distributions_with_ref = list_of_GC_distributions
        if reference:
            list_of_GC_distributions_with_ref.append(reference_GC_distribution)
        # Drawing cumulative plot...
        plotter.GC_content_plot(reference, filenames, list_of_GC_distributions_with_ref, output_dir + '/GC_content_plot', all_pdf)

        ########################################################################
        # Drawing Nx and NGx plots...
        plotter.Nx_plot(filenames, lists_of_lengths, output_dir + '/Nx_plot', 'Nx', [], all_pdf)
        if reference_length:
            plotter.Nx_plot(filenames, lists_of_lengths, output_dir + '/NGx_plot', 'NGx', [reference_length for i in range(len(filenames))], all_pdf)

    log.info('  Done.')
