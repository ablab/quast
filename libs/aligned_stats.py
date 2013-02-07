############################################################################
# Copyright (c) 2011-2013 Saint-Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################

import logging
import os
import itertools
import fastaparser
from libs import reporting, qconfig
from qutils import id_to_str, warning, print_timestamp


######## MAIN ############
def do(reference, filenames, aligned_lengths_lists, nucmer_dir, output_dir, all_pdf, draw_plots, json_output_dir, results_dir):

    if not os.path.isdir(output_dir):
        os.mkdir(output_dir)

    ########################################################################

    nucmer_prefix = os.path.join(nucmer_dir, 'nucmer_output')

    ########################################################################
    report_dict = {'header' : []}
    for filename in filenames:
        report_dict[os.path.basename(filename)] = []

    ########################################################################
    log = logging.getLogger('quast')

    print_timestamp()
    log.info('Running NA-NGA tool...')

    reference_length = sum(fastaparser.get_lengths_from_fastafile(reference))
    assembly_lengths = []
    for filename in filenames:
        assembly_lengths.append(sum(fastaparser.get_lengths_from_fastafile(filename)))

    ########################################################################

    log.info('  Calculating NA50 and NGA50...')

    import N50
    for id, (filename, lens, assembly_len) in enumerate(itertools.izip(filenames, aligned_lengths_lists, assembly_lengths)):
        na50 = N50.NG50(lens, assembly_len)
        nga50 = N50.NG50(lens, reference_length)
        na75 = N50.NG50(lens, assembly_len, 75)
        nga75 = N50.NG50(lens, reference_length, 75)
        la50 = N50.LG50(lens, assembly_len)
        lga50 = N50.LG50(lens, reference_length)
        la75 = N50.LG50(lens, assembly_len, 75)
        lga75 = N50.LG50(lens, reference_length, 75)
        log.info('    ' + id_to_str(id) + os.path.basename(filename) + \
            ', Largest alignment = ' + str(max(lens)) + \
            ', NA50 = ' + str(na50) + \
            ', NGA50 = ' + str(nga50) + \
            ', LA50 = ' + str(la50) +\
            ', LGA50 = ' + str(lga50) )
        report = reporting.get(filename)
        report.add_field(reporting.Fields.LARGALIGN, max(lens))
        report.add_field(reporting.Fields.NA50, na50)
        report.add_field(reporting.Fields.NGA50, nga50)
        report.add_field(reporting.Fields.NA75, na75)
        report.add_field(reporting.Fields.NGA75, nga75)
        report.add_field(reporting.Fields.LA50, la50)
        report.add_field(reporting.Fields.LGA50, lga50)
        report.add_field(reporting.Fields.LA75, la75)
        report.add_field(reporting.Fields.LGA75, lga75)

    ########################################################################

    # saving to JSON
    if json_output_dir:
        from libs.html_saver import json_saver
        json_saver.save_aligned_contigs_lengths(json_output_dir, filenames, aligned_lengths_lists)
        json_saver.save_assembly_lengths(json_output_dir, filenames, assembly_lengths)

    # saving to html
    if qconfig.html_report:
        from libs.html_saver import html_saver
        html_saver.save_aligned_contigs_lengths(results_dir, filenames, aligned_lengths_lists)
        html_saver.save_assembly_lengths(results_dir, filenames, assembly_lengths)

    if draw_plots:
        # Drawing cumulative plot (aligned contigs)...
        import plotter
        plotter.cumulative_plot(reference, filenames, aligned_lengths_lists, output_dir + '/cumulative_plot', 'Cumulative length (aligned contigs)', all_pdf)

        # Drawing NAx and NGAx plots...
        plotter.Nx_plot(filenames, aligned_lengths_lists, output_dir + '/NAx_plot', 'NAx', assembly_lengths, all_pdf)
        plotter.Nx_plot(filenames, aligned_lengths_lists, output_dir + '/NGAx_plot', 'NGAx', [reference_length for i in range(len(filenames))], all_pdf)

    log.info('  Done.')
    return report_dict
