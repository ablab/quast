############################################################################
# Copyright (c) 2011-2015 Saint-Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################

import matplotlib
matplotlib.use('Agg')
import os
import shutil
import qconfig
from libs.log import get_logger
import reporting
logger = get_logger(qconfig.LOGGER_META_NAME)

def get_results_for_metric(ref_names, metric, contigs_num, labels, output_dirpath, report_fname):

    all_rows = []
    cur_ref_names = []
    row = {'metricName': 'References', 'values': cur_ref_names}
    all_rows.append(row)
    results = []
    for i in range(contigs_num):
        row = {'metricName': labels[i], 'values': []}
        all_rows.append(row)
    for i, ref_name in enumerate(ref_names):
        results.append([])
        results_fpath = os.path.join(output_dirpath, ref_name + '_quast_output', report_fname)
        results_file = open(results_fpath, 'r')
        columns = map(lambda s: s.strip(), results_file.readline().split('\t'))
        if metric not in columns:
            all_rows[0]['values'] = cur_ref_names
            break
        cur_ref_names.append(ref_name)
        next_values = map(lambda s: s.strip(), results_file.readline().split('\t'))
        for j in range(contigs_num):
            values = next_values
            if not values[0]:
                metr_res = None
            else:
                metr_res = values[columns.index(metric)].split()[0]
                next_values = map(lambda s: s.strip(), results_file.readline().split('\t'))
            all_rows[j + 1]['values'].append(metr_res)
            results[i].append(metr_res)
    return results, all_rows, cur_ref_names


def do(summary_dirpath, labels, metrics, misassembl_metrics, ref_names):
    ref_names = sorted(ref_names)
    ref_names.append(qconfig.not_aligned_name) # extra case
    contigs_num = len(labels)

    for metric in metrics:
         if not isinstance(metric, tuple):
            summary_fpath_base = os.path.join(summary_dirpath, metric.replace(' ', '_'))
            results, all_rows, cur_ref_names = get_results_for_metric(ref_names, metric, contigs_num, labels, output_dirpath, qconfig.transposed_report_prefix + '.tsv')
            if not results or not results[0]:
                continue
            if cur_ref_names:
                print_file(all_rows, len(cur_ref_names), summary_fpath_base + '.txt')

                if qconfig.draw_plots:
                    import plotter
                    reverse = False
                    if reporting.get_quality(metric) == reporting.Fields.Quality.MORE_IS_BETTER:
                        reverse = True
                    y_label = None
                    if metric == reporting.Fields.TOTALLEN:
                        y_label = 'Total length '
                    elif metric in [reporting.Fields.LARGCONTIG, reporting.Fields.N50, reporting.Fields.NGA50, reporting.Fields.MIS_EXTENSIVE_BASES]:
                        y_label = 'Contig length '
                    plotter.draw_meta_summary_plot(labels, cur_ref_names, all_rows, results, summary_fpath_base, title=metric, reverse=reverse, yaxis_title=y_label)
                    if metric == reporting.Fields.MISASSEMBL:
                        mis_results = []
                        report_fname = os.path.join('contigs_reports', qconfig.transposed_report_prefix + '_misassemblies' + '.tsv')
                        for misassembl_metric in misassembl_metrics:
                            if ref_names[-1] == qconfig.not_aligned_name:
                                cur_ref_names = ref_names[:-1]
                            results, all_rows, cur_ref_names = get_results_for_metric(cur_ref_names, misassembl_metric[len(reporting.Fields.TAB):], contigs_num, labels, output_dirpath, report_fname)
                            if results:
                                mis_results.append(results)
                        if mis_results:
                            for contig_num in range(contigs_num):
                                summary_fpath_base = os.path.join(summary_dirpath, labels[contig_num] + '_misassemblies')
                                plotter.draw_meta_summary_misassembl_plot(mis_results, cur_ref_names, contig_num, summary_fpath_base, title=labels[contig_num])
    logger.info('')
    logger.info('  Text versions of reports and plots for each metric (for all references and assemblies) are saved to ' + summary_dirpath + '/')


def print_file(all_rows, ref_num, fpath):
    colwidths = [0] * (ref_num + 1)
    for row in all_rows:
        for i, cell in enumerate([row['metricName']] + map(val_to_str, row['values'])):
            colwidths[i] = max(colwidths[i], len(cell))
    txt_file = open(fpath, 'w')
    for row in all_rows:
        print >> txt_file, '  '.join('%-*s' % (colwidth, cell) for colwidth, cell
                                     in zip(colwidths, [row['metricName']] + map(val_to_str, row['values'])))


def val_to_str(val):
    if val is None:
        return '-'
    else:
        return str(val)