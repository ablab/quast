import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import os
import shutil
import qconfig
import math
from libs.log import get_logger

logger = get_logger(qconfig.LOGGER_META_NAME)


def do(output_dirpath, labels, metrics, ref_names):
    summary_dirpath = os.path.join(output_dirpath, qconfig.meta_summary_dir)
    if os.path.exists(summary_dirpath):
        shutil.rmtree(summary_dirpath)
    os.mkdir(summary_dirpath)
    ref_names.append(qconfig.not_aligned_name)  # extra case
    ref_num = len(ref_names)
    contigs_num = len(labels)
    #labels = sorted(labels)
    #ref_names = sorted(ref_names)
    for metric in metrics:
        all_rows = []
        cur_ref_names = ref_names
        row = {'metricName': 'References', 'values': cur_ref_names}
        all_rows.append(row)
        if not isinstance(metric, tuple):
            summary_fpath_base = os.path.join(summary_dirpath, metric.replace(' ', '_'))
            results = []
            metric_not_found = False
            for i in range(contigs_num):
                row = {'metricName': labels[i], 'values': []}
                all_rows.append(row)
            for i, ref_name in enumerate(ref_names):
                results.append([])
                results_fpath = os.path.join(output_dirpath, ref_name + '_quast_output', 'transposed_report.tsv')
                results_file = open(results_fpath, 'r')
                columns = map(lambda s: s.strip(), results_file.readline().split('\t'))
                if metric not in columns:
                    if ref_name == qconfig.not_aligned_name:
                        cur_ref_names = ref_names[:-1]
                        all_rows[0]['values'] = cur_ref_names
                    else:
                        metric_not_found = True
                    break
                next_values = map(lambda s: s.strip(), results_file.readline().split('\t'))
                for j in range(contigs_num):
                    values = next_values
                    if values[0] != labels[j]:
                        metr_res = None
                    else:
                        metr_res = values[columns.index(metric)].split()[0]
                        next_values = map(lambda s: s.strip(), results_file.readline().split('\t'))
                    all_rows[j + 1]['values'].append(metr_res)
                    results[i].append(metr_res)
            if metric_not_found:
                continue

            if qconfig.draw_plots:
                import plotter
                plotter.draw_meta_summary_plot(labels, cur_ref_names, all_rows, results, summary_fpath_base, title=metric)
            print_file(all_rows, len(cur_ref_names), summary_fpath_base + '.txt')


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