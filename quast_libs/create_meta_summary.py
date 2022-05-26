############################################################################
# Copyright (c) 2015-2022 Saint Petersburg State University
# Copyright (c) 2011-2015 Saint Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################


import os
from quast_libs import plotter, reporting, qconfig, qutils
from quast_libs.ca_utils.misc import print_file
from quast_libs.log import get_logger
logger = get_logger(qconfig.LOGGER_META_NAME)


def get_results_for_metric(full_ref_names, metric, contigs_num, labels, output_dirpath, report_fname):
    all_rows = []
    ref_names = full_ref_names
    row = {'metricName': 'References', 'values': ref_names}
    all_rows.append(row)
    results = []
    for i in range(contigs_num):
        row = {'metricName': labels[i], 'values': []}
        all_rows.append(row)
    for i, ref_name in enumerate(full_ref_names):
        results.append([])
        cur_results = [None] * len(labels)
        results_fpath = os.path.join(output_dirpath, ref_name, report_fname)
        if ref_name == qconfig.not_aligned_name:
            results_fpath = os.path.join(os.path.dirname(output_dirpath), ref_name, report_fname)
        if os.path.exists(results_fpath):
            results_file = open(results_fpath, 'r')
            columns = [s.strip() for s in results_file.readline().split('\t')]
            if metric in columns:
                next_values = [s.strip() for s in results_file.readline().split('\t')]
                cur_results = [None] * len(labels)
                for j in range(contigs_num):
                    values = next_values
                    if values[0]:
                        metr_res = values[columns.index(metric)].split()[0]
                        next_values = [s.strip() for s in results_file.readline().split('\t')]
                        index_contig = labels.index(values[0])
                        cur_results[index_contig] = metr_res
            elif ref_name == qconfig.not_aligned_name:
                ref_names.pop(i)
                continue
        for j in range(contigs_num):
            all_rows[j + 1]['values'].append(cur_results[j])
            results[-1].append(cur_results[j])
    return results, all_rows, ref_names


def get_labels(combined_output_dirpath, report_fname):
    results_fpath = os.path.join(combined_output_dirpath, report_fname)
    results_file = open(results_fpath, 'r')
    values = [s.strip() for s in results_file.readline().split('\t')]
    return values[1:]


def do(html_fpath, output_dirpath, combined_output_dirpath, output_dirpath_per_ref, metrics, misassembly_metrics, ref_names):
    labels = get_labels(combined_output_dirpath, qconfig.report_prefix + '.tsv')
    contigs_num = len(labels)
    plots_dirname = qconfig.plot_extension.upper()
    for ext in ['TXT', plots_dirname, 'TEX', 'TSV']:
        if not os.path.isdir(os.path.join(output_dirpath, ext)):
            os.mkdir(os.path.join(output_dirpath, ext))
    for metric in metrics:
        if not isinstance(metric, tuple):
            metric_fname = metric.replace(' (%)', '').replace('#', 'num').replace('>=', 'ge').replace(' ', '_').replace("'", "")
            summary_txt_fpath = os.path.join(output_dirpath, 'TXT', metric_fname + '.txt')
            summary_tex_fpath = os.path.join(output_dirpath, 'TEX', metric_fname + '.tex')
            summary_tsv_fpath = os.path.join(output_dirpath, 'TSV', metric_fname + '.tsv')
            summary_plot_fname = os.path.join(output_dirpath, plots_dirname, metric_fname)
            results, all_rows, cur_ref_names = \
                get_results_for_metric(ref_names, metric, contigs_num, labels, output_dirpath_per_ref, qconfig.transposed_report_prefix + '.tsv')
            if not results or all(not value for result in results for value in result):
                continue
            if cur_ref_names:
                transposed_table = [{'metricName': 'Assemblies',
                                    'values': [all_rows[i]['metricName'] for i in range(1, len(all_rows))],}]
                for i in range(len(all_rows[0]['values'])):
                    values = []
                    for j in range(1, len(all_rows)):
                        values.append(all_rows[j]['values'][i])
                    transposed_table.append({'metricName': all_rows[0]['values'][i], # name of reference
                                             'values': values})

                print_file(transposed_table, summary_txt_fpath)

                reporting.save_tsv(summary_tsv_fpath, transposed_table)
                reporting.save_tex(summary_tex_fpath, transposed_table)
                reverse = False
                if reporting.get_quality(metric) == reporting.Fields.Quality.MORE_IS_BETTER:
                    reverse = True
                y_label = None
                if metric in [reporting.Fields.TOTALLEN, reporting.Fields.TOTALLENS__FOR_1000_THRESHOLD,
                              reporting.Fields.TOTALLENS__FOR_10000_THRESHOLD, reporting.Fields.TOTALLENS__FOR_50000_THRESHOLD]:
                    y_label = 'Total length'
                elif metric == reporting.Fields.TOTAL_ALIGNED_LEN:
                    y_label = 'Aligned length'
                elif metric in [reporting.Fields.LARGCONTIG, reporting.Fields.N50, reporting.Fields.NGA50,
                                reporting.Fields.MIS_EXTENSIVE_BASES]:
                    y_label = 'Contig length'
                elif metric == reporting.Fields.LARGALIGN:
                    y_label = 'Alignment length'
                plotter.draw_meta_summary_plot(html_fpath, output_dirpath, labels, cur_ref_names, results,
                                               summary_plot_fname, title=metric, reverse=reverse, yaxis_title=y_label,
                                               print_all_refs=True, logger=logger)
                if metric == reporting.Fields.MISASSEMBL:
                    mis_results = []
                    report_fname = os.path.join(qconfig.detailed_contigs_reports_dirname,
                                                qconfig.transposed_report_prefix + '_misassemblies' + '.tsv')
                    if ref_names[-1] == qconfig.not_aligned_name:
                        cur_ref_names = ref_names[:-1]
                    for misassembly_metric in misassembly_metrics:
                        results, all_rows, cur_ref_names = \
                            get_results_for_metric(cur_ref_names, misassembly_metric[len(reporting.Fields.TAB):],
                                                   contigs_num, labels, output_dirpath_per_ref, report_fname)
                        if results:
                            mis_results.append(results)
                    if mis_results:
                        json_points = []
                        for contig_num in range(contigs_num):
                            plot_fpath = os.path.join(output_dirpath, plots_dirname, qutils.slugify(labels[contig_num]) + '_misassemblies')
                            json_points.append(plotter.draw_meta_summary_misassemblies_plot(mis_results, cur_ref_names,
                                                                                            contig_num, plot_fpath,
                                                                                            title=labels[contig_num]))
                        if qconfig.html_report:
                            from quast_libs.html_saver import html_saver
                            if ref_names[-1] == qconfig.not_aligned_name:
                                cur_ref_names = ref_names[:-1]
                            if json_points:
                                html_saver.save_meta_misassemblies(html_fpath, output_dirpath, json_points, labels, cur_ref_names)
    logger.main_info('')
    logger.main_info('  Text versions of reports and plots for each metric (for all references and assemblies) are saved to ' + output_dirpath + '/')

