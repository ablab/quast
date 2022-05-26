############################################################################
# Copyright (c) 2015-2022 Saint Petersburg State University
# Copyright (c) 2011-2015 Saint Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################

import os
import itertools
from quast_libs import fastaparser, N50, plotter, reporting, qconfig, qutils

from quast_libs.log import get_logger
logger = get_logger(qconfig.LOGGER_DEFAULT_NAME)


######## MAIN ############
def do(ref_fpath, aligned_contigs_fpaths, output_dirpath,
       aligned_lengths_lists, aligned_stats_dirpath):

    if not os.path.isdir(aligned_stats_dirpath):
        os.mkdir(aligned_stats_dirpath)

    ########################################################################
    report_dict = {'header': []}
    for contigs_fpath in aligned_contigs_fpaths:
        report_dict[qutils.name_from_fpath(contigs_fpath)] = []

    ########################################################################
    logger.print_timestamp()
    logger.main_info('Running NA-NGA calculation...')

    ref_chr_lengths = fastaparser.get_chr_lengths_from_fastafile(ref_fpath)
    reference_length = sum(ref_chr_lengths.values())
    assembly_lengths = []
    for contigs_fpath in aligned_contigs_fpaths:
        assembly_lengths.append(sum(fastaparser.get_chr_lengths_from_fastafile(contigs_fpath).values()))

    for i, (contigs_fpath, lens, assembly_len) in enumerate(
            zip(aligned_contigs_fpaths, aligned_lengths_lists, assembly_lengths)):
        sorted_lengths = sorted(lens, reverse=True)
        na50, la50 = N50.NG50_and_LG50(sorted_lengths, assembly_len)
        auNA = N50.au_metric(sorted_lengths, assembly_len)
        nax, lax = N50.NG50_and_LG50(sorted_lengths, assembly_len, qconfig.x_for_additional_Nx)
        if not qconfig.is_combined_ref:
            nga50, lga50 = N50.NG50_and_LG50(sorted_lengths, reference_length)
            ngax, lgax = N50.NG50_and_LG50(sorted_lengths, reference_length, qconfig.x_for_additional_Nx)
            auNGA = N50.au_metric(sorted_lengths, reference_length)

        logger.info('  ' +
                    qutils.index_to_str(i) +
                    qutils.label_from_fpath(contigs_fpath) +
                 ', Largest alignment = ' + str(max(lens)) +
                 ', NA50 = ' + str(na50) +
                 (', NGA50 = ' + str(nga50) if not qconfig.is_combined_ref and nga50 else '') +
                 ', LA50 = ' + str(la50) +
                 (', LGA50 = ' + str(lga50) if not qconfig.is_combined_ref and lga50 else ''))
        report = reporting.get(contigs_fpath)
        report.add_field(reporting.Fields.LARGALIGN, max(lens))
        report.add_field(reporting.Fields.TOTAL_ALIGNED_LEN, sum(lens))
        report.add_field(reporting.Fields.NA50, na50)
        report.add_field(reporting.Fields.NAx, nax)
        report.add_field(reporting.Fields.auNA, ('%.1f' % auNA if auNA is not None else None))
        report.add_field(reporting.Fields.LA50, la50)
        report.add_field(reporting.Fields.LAx, lax)
        if not qconfig.is_combined_ref:
            report.add_field(reporting.Fields.NGA50, nga50)
            report.add_field(reporting.Fields.NGAx, ngax)
            report.add_field(reporting.Fields.auNGA, ('%.1f' % auNGA if auNGA is not None else None))
            report.add_field(reporting.Fields.LGA50, lga50)
            report.add_field(reporting.Fields.LGAx, lgax)

    ########################################################################
    num_contigs = max([len(aligned_lengths_lists[i]) for i in range(len(aligned_lengths_lists))])

    # saving to html
    if qconfig.html_report:
        from quast_libs.html_saver import html_saver
        html_saver.save_assembly_lengths(output_dirpath, aligned_contigs_fpaths, assembly_lengths)

    if qconfig.draw_plots:
        # Drawing cumulative plot (aligned contigs)...
        plotter.cumulative_plot(ref_fpath, aligned_contigs_fpaths, aligned_lengths_lists,
                                os.path.join(aligned_stats_dirpath, 'cumulative_plot'),
                                'Cumulative length (aligned contigs)')

        # Drawing NAx and NGAx plots...
    plotter.Nx_plot(output_dirpath, num_contigs > qconfig.max_points, aligned_contigs_fpaths, aligned_lengths_lists, aligned_stats_dirpath + '/NAx_plot', 'NAx',
                    assembly_lengths)
    if not qconfig.is_combined_ref:
        plotter.Nx_plot(output_dirpath, num_contigs > qconfig.max_points, aligned_contigs_fpaths, aligned_lengths_lists,
                        aligned_stats_dirpath + '/NGAx_plot', 'NGAx', [reference_length for i in range(len(aligned_contigs_fpaths))])

    logger.main_info('Done.')
    return report_dict
