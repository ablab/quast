import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import os
import shutil
import qconfig
import math
from libs.log import get_logger

logger = get_logger(qconfig.LOGGER_DEFAULT_NAME)


def do(output_dirpath, labels, metrics, ref_names):
    summary_dirpath = os.path.join(output_dirpath, 'summary')
    if os.path.exists(summary_dirpath):
        shutil.rmtree(summary_dirpath)
    os.mkdir(summary_dirpath)
    ref_num = len(ref_names)
    contigs_num = len(labels)
    colors = ['red', 'blue', 'green', 'yellow', 'black', 'grey', 'cyan', 'magenta']
    assert (contigs_num <= len(colors))
    labels = sorted(labels)
    ref_names = sorted(ref_names)
    for metric in metrics:
        all_rows = []
        row = {'metricName': 'References', 'values': ref_names}
        all_rows.append(row)
        if not isinstance(metric, tuple):
            summary_fpath = os.path.join(summary_dirpath, metric.replace(' ', '_') + '.txt')
            results = []
            ymax = 0
            metric_not_found = False
            for i in range(contigs_num):
                row = {'values': [], 'metricName': labels[i]}
                all_rows.append(row)
            for i, ref_name in enumerate(ref_names):
                results.append([])
                results_fpath = os.path.join(output_dirpath, ref_name + '_quast_output', 'transposed_report.tsv')
                results_file = open(results_fpath, 'r')
                columns = map(lambda s: s.strip(), results_file.readline().split('\t'))
                if metric not in columns:
                    metric_not_found = True
                    break
                for j in range(contigs_num):
                    values = map(lambda s: s.strip(), results_file.readline().split('\t'))
                    metr_res = values[columns.index(metric)].split()[0]
                    all_rows[j + 1]['values'].append(metr_res)
                    ymax = max(ymax, float(metr_res))
                    results[i].append(metr_res)
            if metric_not_found:
                continue
            fig = plt.figure()
            ax = fig.add_subplot(111)
            plt.xticks(range(1, ref_num + 1), ref_names, size='small')
            title = metric
            plt.title(title)
            box = ax.get_position()
            ax.set_position([box.x0, box.y0, box.width * 0.8, box.height * 1.0])

            for j in range(contigs_num):
                to_plot = []
                arr = range(1, ref_num + 1)
                for i in range(ref_num):
                    to_plot.append(results[i][j])
                    arr[i] += 0.07 * (j - (contigs_num - 1) * 0.5)
                ax.plot(arr, to_plot, 'ro', color=colors[j])
            plt.xlim([0, ref_num + 1])
            if ymax == 0:
                plt.ylim([-0.1, 0.1])
            else:
                plt.ylim([0, math.ceil(ymax * 1.05)])
            legend = []
            for j in range(contigs_num):
                legend.append(labels[j])

            ax.legend(legend, loc='center left', bbox_to_anchor=(1.0, 0.5), numpoints=1)
            current_figure = plt.gcf()
            default_size = current_figure.get_size_inches()
            current_figure.set_size_inches(2 * default_size)
            plt.savefig(os.path.join(summary_dirpath, metric + '.jpg'))
            print_file(all_rows, ref_num, summary_fpath)


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