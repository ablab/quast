############################################################################
# Copyright (c) 2011-2013 Saint-Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################

import logging
import os
import itertools
import fastaparser
from libs import reporting, qconfig, qutils

from libs.log import get_logger
logger = get_logger(qconfig.LOGGER_DEFAULT_NAME)


######## MAIN ############
def do(ref_fpath, contigs_fpaths, aligned_lengths_lists,
       nucmer_dirpath, output_dirpath, all_pdf, draw_plots,
       json_output_dirpath, results_dirpath):

    if not os.path.isdir(output_dirpath):
        os.mkdir(output_dirpath)

    ########################################################################

    nucmer_prefix = os.path.join(nucmer_dirpath, 'nucmer_output')

    ########################################################################
    report_dict = {'header': []}
    for contigs_fpath in contigs_fpaths:
        report_dict[qutils.name_from_fpath(contigs_fpath)] = []

    ########################################################################
    logger.print_timestamp()
    logger.info('Running NA-NGA tool...')

    reference_length = sum(fastaparser.get_lengths_from_fastafile(ref_fpath))
    assembly_lengths = []
    for contigs_fpath in contigs_fpaths:
        assembly_lengths.append(sum(fastaparser.get_lengths_from_fastafile(contigs_fpath)))

    ########################################################################

    logger.info('  Calculating NA50 and NGA50...')

    import N50
    for i, (contigs_fpath, lens, assembly_len) in enumerate(
            itertools.izip(contigs_fpaths, aligned_lengths_lists, assembly_lengths)):
        na50 = N50.NG50(lens, assembly_len)
        nga50 = N50.NG50(lens, reference_length)
        na75 = N50.NG50(lens, assembly_len, 75)
        nga75 = N50.NG50(lens, reference_length, 75)
        la50 = N50.LG50(lens, assembly_len)
        lga50 = N50.LG50(lens, reference_length)
        la75 = N50.LG50(lens, assembly_len, 75)
        lga75 = N50.LG50(lens, reference_length, 75)
        logger.info('    ' +
                    qutils.index_to_str(i) +
                    qutils.label_from_fpath(contigs_fpath) +
                 ', Largest alignment = ' + str(max(lens)) +
                 ', NA50 = ' + str(na50) +
                 ', NGA50 = ' + str(nga50) +
                 ', LA50 = ' + str(la50) +
                 ', LGA50 = ' + str(lga50))
        report = reporting.get(contigs_fpath)
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
    if json_output_dirpath:
        from libs.html_saver import json_saver
        json_saver.save_aligned_contigs_lengths(json_output_dirpath, contigs_fpaths, aligned_lengths_lists)
        json_saver.save_assembly_lengths(json_output_dirpath, contigs_fpaths, assembly_lengths)

    # saving to html
    if qconfig.html_report:
        from libs.html_saver import html_saver
        html_saver.save_aligned_contigs_lengths(results_dirpath, contigs_fpaths, aligned_lengths_lists)
        html_saver.save_assembly_lengths(results_dirpath, contigs_fpaths, assembly_lengths)

    if draw_plots:
        # Drawing cumulative plot (aligned contigs)...
        import plotter
        plotter.cumulative_plot(ref_fpath, contigs_fpaths, aligned_lengths_lists, output_dirpath + '/cumulative_plot', 'Cumulative length (aligned contigs)', all_pdf)

        # Drawing NAx and NGAx plots...
        plotter.Nx_plot(contigs_fpaths, aligned_lengths_lists, output_dirpath + '/NAx_plot', 'NAx', assembly_lengths, all_pdf)
        plotter.Nx_plot(contigs_fpaths, aligned_lengths_lists, output_dirpath + '/NGAx_plot', 'NGAx', [reference_length for i in range(len(contigs_fpaths))], all_pdf)

    logger.info('  Done.')
    return report_dict
