############################################################################
# Copyright (c) 2015-2022 Saint Petersburg State University
# Copyright (c) 2011-2015 Saint Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################

from __future__ import division

####################################################################################
###########################  CONFIGURABLE PARAMETERS  ##############################
####################################################################################

# Font of plot captions, axes labels and ticks
font = {'family': 'sans-serif',
        'style': 'normal',
        'weight': 'medium',
        'size': 10}

# Line params
line_width = 2.0
primary_line_style = 'solid' # 'solid', 'dashed', 'dashdot', or 'dotted'
secondary_line_style = 'dashed' # used only if --scaffolds option is set

# Legend params
n_columns = 4  # number of columns
with_grid = True
with_title = True
axes_fontsize = 'large' # fontsize of axes labels and ticks

# Special case: reference line params
reference_color = '#000000'
reference_ls = 'dashed' # ls = line style

# axis params:
logarithmic_x_scale = False  # for cumulative plots only

####################################################################################
########################  END OF CONFIGURABLE PARAMETERS  ##########################
####################################################################################
import math
import sys

from quast_libs import fastaparser, qconfig, reporting
from quast_libs.log import get_logger, get_main_logger
from quast_libs.qutils import label_from_fpath, parse_str_to_num, run_parallel
from quast_libs.plotter_data import get_color_and_ls, colors

main_logger = get_main_logger()
logger = get_logger(qconfig.LOGGER_DEFAULT_NAME)
meta_logger = get_logger(qconfig.LOGGER_META_NAME)

# checking if matplotlib is installed
can_draw_plots = False
if qconfig.draw_plots:
    try:
        import matplotlib
        matplotlib.use('Agg')  # non-GUI backend
        if matplotlib.__version__.startswith('0') or matplotlib.__version__.startswith('1.0'):
            main_logger.info('')
            main_logger.warning('Can\'t draw plots: matplotlib version is old! Please use matplotlib version 1.1 or higher.')
        else:
            # additionally check other imports
            stderr = sys.stderr
            sys.stderr = open('/dev/null', 'w')  # do not print matplotlib bad key warnings
            import matplotlib.pyplot as plt
            import matplotlib.ticker
            sys.stderr = stderr
            can_draw_plots = True
    except Exception:
        main_logger.info('')
        main_logger.warning('Can\'t draw plots: python-matplotlib is missing or corrupted.')

# for creating PDF file with all plots and tables
pdf_plots_figures = []
pdf_tables_figures = []
####################################################################################


class Plot(object):
    def __init__(self, x_vals, y_vals, color, ls, marker=None, markersize=1):
        self.x_vals, self.y_vals, self.color, self.ls, self.marker, self.markersize = x_vals, y_vals, color, ls, marker, markersize

    def plot(self):
        plt.plot(self.x_vals, self.y_vals, color=self.color, ls=self.ls, lw=line_width,
                 marker=self.marker, markersize=self.markersize)

    def get_max_y(self):
        return max(self.y_vals)


class Bar(object):
    def __init__(self, x_val, y_val, color, width=0.8, bottom=None, hatch='', edgecolor=None, align='edge'):
        self.x_val, self.y_val, self.color, self.width, self.bottom, self.hatch, self.edgecolor, self.align = \
            x_val, y_val, color, width, bottom, hatch, edgecolor, align

    def plot(self):
        plt.bar(self.x_val, self.y_val, width=self.width, align=self.align, color=self.color, edgecolor=self.edgecolor,
                hatch=self.hatch, bottom=self.bottom)

    def get_max_y(self):
        if isinstance(self.y_val, list):
            return max(self.y_val)
        else:
            if self.bottom:
                return self.y_val + self.bottom
            else:
                return self.y_val


def get_locators(is_histogram):
    xLocator = matplotlib.ticker.MaxNLocator(nbins=6, integer=True)
    yLocator = matplotlib.ticker.MaxNLocator(nbins=6, integer=True, steps=[1, 5, 10] if is_histogram else None)
    return xLocator, yLocator


def y_formatter(ylabel, max_y):
    if max_y <= 5 * 1e+3:
        mkfunc = lambda x, pos: '%d' % (x * 1)
        ylabel += ' (bp)'
    elif max_y <= 5 * 1e+6:
        mkfunc = lambda x, pos: '%d' % (x * 1e-3)
        ylabel += ' (kbp)'
    else:
        mkfunc = lambda x, pos: '%d' % (x * 1e-6)
        ylabel += ' (Mbp)'

    return ylabel, mkfunc


def set_ax(vertical_legend=False):
    ax = plt.gca()
    ax.set_axisbelow(True)
    # Shink current axis's height by 20% on the bottom
    box = ax.get_position()
    if vertical_legend:
        ax.set_position([box.x0, box.y0, box.width * 0.8, box.height * 1.0])
    else:
        ax.set_position([box.x0, box.y0 + box.height * 0.2, box.width, box.height * 0.8])
    ax.yaxis.grid(with_grid)
    plt.grid(with_grid)
    return ax


def add_labels(xlabel, ylabel, max_y, ax, logarithmic_x_scale=False, is_histogram=False):
    if ylabel and 'length' in ylabel:
        ylabel, mkfunc = y_formatter(ylabel, max_y)
        mkformatter = matplotlib.ticker.FuncFormatter(mkfunc)
        ax.yaxis.set_major_formatter(mkformatter)

    if xlabel:
        plt.xlabel(xlabel, fontsize=axes_fontsize)
    if ylabel:
        plt.ylabel(ylabel, fontsize=axes_fontsize)

    xLocator, yLocator = get_locators(is_histogram)
    ax.yaxis.set_major_locator(yLocator)
    ax.xaxis.set_major_locator(xLocator)
    if is_histogram and not xlabel:
        ax.axes.get_xaxis().set_visible(False)
    if logarithmic_x_scale:
        ax.set_xscale('log')


def add_legend(ax, legend_list, n_columns=None, vertical_legend=False):
    try:
        if vertical_legend:
            legend = ax.legend(legend_list, loc='center left', bbox_to_anchor=(1.0, 0.5), fancybox=True, shadow=True, numpoints=1)
        else:
            legend = ax.legend(legend_list, loc='upper center', bbox_to_anchor=(0.49, -0.15), fancybox=True, shadow=True,
                               ncol=n_columns if n_columns<3 else 3)
        for handle in legend.legendHandles:
            handle.set_hatch('')
    except Exception:
        pass


def save_to_pdf(all_pdf_fpath):
    try:
        from matplotlib.backends.backend_pdf import PdfPages
        all_pdf_file = PdfPages(all_pdf_fpath)
    except:
        logger.warning('PDF with all tables and plots cannot be created')
        return
    for figure in pdf_tables_figures:
        all_pdf_file.savefig(figure, bbox_inches='tight')
    for figure in pdf_plots_figures:
        all_pdf_file.savefig(figure)
    try:  # for matplotlib < v.1.0
        d = all_pdf_file.infodict()
        d['Title'] = 'QUAST full report'
        d['Author'] = 'QUAST'
        import datetime
        d['CreationDate'] = datetime.datetime.now()
        d['ModDate'] = datetime.datetime.now()
    except AttributeError:
        pass
    all_pdf_file.close()
    plt.close('all')  # closing all open figures


def save_plot(plot_fpath):
    plt.savefig(plot_fpath, bbox_inches='tight')


def create_plot(plot_fpath, title, plots, legend_list=None, x_label=None, y_label=None, vertical_legend=False, is_histogram=False,
                x_limit=None, y_limit=None, x_ticks=None, vertical_ticks=False, add_to_report=True, logger=logger):
    figure = plt.gcf()
    plt.rc('font', **font)
    max_y = 0

    ax = set_ax(vertical_legend)
    for plot in plots:
        max_y = max(max_y, plot.get_max_y())
        plot.plot()
    if legend_list:
        add_legend(ax, legend_list, n_columns=n_columns, vertical_legend=vertical_legend)
    add_labels(x_label, y_label, max_y, ax, is_histogram=is_histogram)
    if x_limit:
        plt.xlim(x_limit)
    y_limit = y_limit or [0, max(5, int(math.ceil(max_y * 1.1)))]
    plt.ylim(y_limit)
    if x_ticks:
        plt.xticks(range(len(x_ticks)), x_ticks, size='small', rotation='vertical' if vertical_ticks else None)

    if not can_draw_plots:
        plt.close()
        return
    if with_title:
        plt.title(title)
    plot_fpath += '.' + qconfig.plot_extension
    if qconfig.is_combined_ref:  # matplotlib needs to be run in parallel for combined reference to prevent fail in parallel runs per reference
        run_parallel(save_plot, [(plot_fpath,)], 2)
    else:
        save_plot(plot_fpath)

    logger.info('    saved to ' + plot_fpath)
    if add_to_report:
        pdf_plots_figures.append(figure)
    plt.close('all')


def cumulative_plot(reference, contigs_fpaths, lists_of_lengths, plot_fpath, title):
    if not can_draw_plots:
        return

    logger.info('  Drawing cumulative plot...')

    plots = []
    max_x = 0

    for (contigs_fpath, lengths) in zip(contigs_fpaths, lists_of_lengths):
        y_vals = [0]
        for l in sorted(lengths, reverse=True):
            y_vals.append(y_vals[-1] + l)
        x_vals = list(range(0, len(y_vals)))
        if x_vals:
            max_x = max(x_vals[-1], max_x)
        color, ls = get_color_and_ls(contigs_fpath)
        plots.append(Plot(x_vals, y_vals, color, ls))

    if reference:
        y_vals = [0]
        for l in sorted(fastaparser.get_chr_lengths_from_fastafile(reference).values(), reverse=True):
            y_vals.append(y_vals[-1] + l)
        x_vals = list(range(0, len(y_vals)))
        # extend reference curve to the max X-axis point
        reference_length = y_vals[-1]
        max_x = max(max_x, x_vals[-1])
        y_vals.append(reference_length)
        x_vals.append(max_x)
        plots.append(Plot(x_vals, y_vals, reference_color, reference_ls))

    legend_list = [label_from_fpath(fpath) for fpath in contigs_fpaths]
    if reference:
        legend_list += ['Reference']

    create_plot(plot_fpath, title, plots, legend_list, x_label='Contig index', y_label='Cumulative length',
                     x_limit=[0, max_x])


def frc_plot(results_dir, ref_fpath, contigs_fpaths, contigs_aligned_lengths, features_in_contigs_by_file, plot_fpath, title):
    if can_draw_plots:
        logger.info('  Drawing ' + title + ' FRCurve plot...')

    plots = []
    max_y = 0
    ref_length = sum(fastaparser.get_chr_lengths_from_fastafile(ref_fpath).values())
    json_vals_x = []  # coordinates for Nx-like plots in HTML-report
    json_vals_y = []
    max_features = max(sum(feature_in_contigs) for feature_in_contigs in features_in_contigs_by_file.values()) + 1

    aligned_contigs_fpaths = []
    for contigs_fpath in contigs_fpaths:
        aligned_lengths = contigs_aligned_lengths[contigs_fpath]
        feature_in_contigs = features_in_contigs_by_file[contigs_fpath]
        if not aligned_lengths or not feature_in_contigs:
            continue

        aligned_contigs_fpaths.append(contigs_fpath)
        len_with_zero_features = 0
        lengths = []
        non_zero_feature_in_contigs = []
        for l, feature in zip(aligned_lengths, feature_in_contigs):
            if feature == 0:
                len_with_zero_features += l
            else:
                lengths.append(l)
                non_zero_feature_in_contigs.append(feature)
        optimal_sorted_tuples = sorted(zip(lengths, non_zero_feature_in_contigs),
                                       key=lambda tuple: tuple[0] * 1.0 / tuple[1], reverse=True)  # sort by len/features ratio
        sorted_lengths = [tuple[0] for tuple in optimal_sorted_tuples]
        sorted_features = [tuple[1] for tuple in optimal_sorted_tuples]
        x_vals = []
        y_vals = []
        for features_n in range(max_features):
            features_cnt = 0
            cumulative_len = len_with_zero_features
            for l, feature in zip(sorted_lengths, sorted_features):
                if features_cnt + feature <= features_n:
                    features_cnt += feature
                    cumulative_len += l
                    if features_cnt == features_n:
                        break

            x_vals.append(features_n)
            y_vals.append(cumulative_len * 100.0 / ref_length)
            x_vals.append(features_n + 1)
            y_vals.append(cumulative_len * 100.0 / ref_length)

        json_vals_x.append(x_vals)
        json_vals_y.append(y_vals)
        max_y = max(max_y, max(y_vals))

        color, ls = get_color_and_ls(contigs_fpath)
        plots.append(Plot(x_vals, y_vals, color, ls))

    if qconfig.html_report:
        from quast_libs.html_saver import html_saver
        html_saver.save_coord(results_dir, json_vals_x, json_vals_y, 'coord' + title, aligned_contigs_fpaths)

    if can_draw_plots:
        title = 'FRCurve (' + title + ')'
        legend_list = [label_from_fpath(fpath) for fpath in aligned_contigs_fpaths]
        create_plot(plot_fpath, title, plots, legend_list, x_label='Feature space', y_label='Genome coverage (%)',
                    x_limit=[0, max_features], y_limit=[0, max(100, max_y)])


# common routine for Nx-plot and NGx-plot (and probably for others Nyx-plots in the future)
def Nx_plot(results_dir, reduce_points, contigs_fpaths, lists_of_lengths, plot_fpath, title='Nx', reference_lengths=None):
    if can_draw_plots:
        logger.info('  Drawing ' + title + ' plot...')

    plots = []
    json_vals_x = []  # coordinates for Nx-like plots in HTML-report
    json_vals_y = []

    for id, (contigs_fpath, lengths) in enumerate(zip(contigs_fpaths, lists_of_lengths)):
        if not lengths:
            json_vals_x.append([])
            json_vals_y.append([])
            continue
        lengths.sort(reverse=True)
        vals_x = [0.0]
        vals_y = [lengths[0]]
        # calculate values for the plot
        vals_Nx = [0.0]
        vals_l = [lengths[0]]
        lcur = 0
        # if Nx-plot then we just use sum of contigs lengths, else use reference_length
        lsum = sum(lengths)
        if reference_lengths:
            lsum = reference_lengths[id]
        min_difference = 0
        if reduce_points:
            min_difference = qconfig.min_difference
        for l in lengths:
            lcur += l
            x = lcur * 100.0 / lsum
            if can_draw_plots:
                vals_Nx.append(vals_Nx[-1] + 1e-10) # eps
                vals_l.append(l)
                vals_Nx.append(x)
                vals_l.append(l)
            if vals_y[-1] - l > min_difference or len(vals_x) == 1:
                vals_x.append(vals_x[-1] + 1e-10) # eps
                vals_y.append(l)
                vals_x.append(x)
                vals_y.append(l)
            # add to plot
        json_vals_x.append(vals_x)
        json_vals_y.append(vals_y)
        if can_draw_plots:
            vals_Nx.append(vals_Nx[-1] + 1e-10) # eps
            vals_l.append(0.0)
            vals_x.append(vals_x[-1] + 1e-10) # eps
            vals_y.append(0.0)
            color, ls = get_color_and_ls(contigs_fpath)
            plots.append(Plot(vals_Nx, vals_l, color, ls))

    if qconfig.html_report:
        from quast_libs.html_saver import html_saver
        html_saver.save_coord(results_dir, json_vals_x, json_vals_y, 'coord' + title, contigs_fpaths)

    if not can_draw_plots:
        return

    legend_list = [label_from_fpath(fpath) for fpath in contigs_fpaths]
    create_plot(plot_fpath, title, plots, legend_list, x_label='x', y_label='Contig length', x_limit=[0, 100])


# routine for GC-plot
def GC_content_plot(ref_fpath, contigs_fpaths, list_of_GC_distributions, plot_fpath):
    if not can_draw_plots or qconfig.no_gc:
        return
    title = 'GC content'
    logger.info('  Drawing ' + title + ' plot...')

    plots = []

    all_fpaths = contigs_fpaths
    if ref_fpath:
        all_fpaths = contigs_fpaths + [ref_fpath]

    for i, (GC_distribution_x, GC_distribution_y) in enumerate(list_of_GC_distributions):
        # for log scale
        for id2, v in enumerate(GC_distribution_y):
            if v == 0:
                GC_distribution_y[id2] = 0.1

        # add to plot
        if ref_fpath and (i == len(all_fpaths) - 1):
            color = reference_color
            ls = reference_ls
        else:
            color, ls = get_color_and_ls(all_fpaths[i])

        plots.append(Plot(GC_distribution_x, GC_distribution_y, color, ls))

    legend_list = [label_from_fpath(fpath) for fpath in contigs_fpaths]
    if ref_fpath:
        legend_list += ['Reference']
    create_plot(plot_fpath, title, plots, legend_list, x_label='GC (%)', y_label='# windows', x_limit=[0, 100])


def contigs_GC_content_plot(contigs_fpath, GC_distributions, plot_fpath):
    if not can_draw_plots or qconfig.no_gc:
        return
    title = label_from_fpath(contigs_fpath) + ' GC content'
    logger.info('  Drawing ' + title + ' plot...')

    plots = []
    color, ls = get_color_and_ls(contigs_fpath)
    x_vals, y_vals = GC_distributions

    for GC_x, GC_y in zip(x_vals, y_vals):
        plots.append(Bar(GC_x, GC_y, color, width=5))

    legend_list = [label_from_fpath(contigs_fpath)]
    create_plot(plot_fpath, title, plots, legend_list, x_label='GC (%)', y_label='# contigs', x_limit=[0, 100])


# common routine for genes and operons cumulative plots
def genes_operons_plot(reference_value, contigs_fpaths, files_feature_in_contigs, plot_fpath, title):
    if not can_draw_plots:
        return

    logger.info('  Drawing ' + title + ' cumulative plot...')

    plots = []
    max_x = 0

    for contigs_fpath in contigs_fpaths:
        # calculate values for the plot
        feature_in_contigs = files_feature_in_contigs[contigs_fpath]

        x_vals = list(range(len(feature_in_contigs) + 1))
        y_vals = [0]
        total_full = 0
        for feature_amount in feature_in_contigs:
            total_full += feature_amount
            y_vals.append(total_full)

        if len(x_vals) > 0:
            max_x = max(x_vals[-1], max_x)

        color, ls = get_color_and_ls(contigs_fpath)
        plots.append(Plot(x_vals, y_vals, color, ls))

    if reference_value:
        plots.append(Plot([0, max_x], [reference_value, reference_value], reference_color, reference_ls))

    title = 'Cumulative # complete ' + title
    legend_list = [label_from_fpath(fpath) for fpath in contigs_fpaths]
    if reference_value:
        legend_list += ['Reference']
    create_plot(plot_fpath, title, plots, legend_list, x_label='Contig index', y_label=title)


# common routine for Histograms
def histogram(contigs_fpaths, values, plot_fpath, title='', yaxis_title='', bottom_value=None,
              top_value=None):
    if not can_draw_plots:
        return
    if len(contigs_fpaths) < 2:  #
        logger.info('  Skipping drawing ' + title + ' histogram... (less than 2 columns histogram makes no sense)')
        return

    logger.info('  Drawing ' + title + ' histogram...')

    plots = []
    min_value = sorted(values)[0]
    max_value = sorted(values, reverse=True)[0]
    exponent = None
    if max_value == min_value:
        if max_value > 0:
            exponent = math.pow(10, math.floor(math.log(max_value, 10)))
        else:
            exponent = 1
    else:
        exponent = math.pow(10, math.floor(math.log(max_value - min_value, 10)))

    if not bottom_value:
        bottom_value = (math.floor(min_value / exponent) - 5) * exponent
    if not top_value:
        top_value = (math.ceil(max_value / exponent) + 1) * exponent

    #bars' params
    width = 0.3
    interval = width // 3
    start_pos = interval // 2

    for i, (contigs_fpath, val) in enumerate(zip(contigs_fpaths, values)):
        color, ls = get_color_and_ls(contigs_fpath)
        if ls == primary_line_style:
            hatch = ''
        else:
            hatch = 'x'
        plots.append(Bar(start_pos + (width + interval) * i, val, color, width=width, hatch=hatch))

    legend_list = [label_from_fpath(fpath) for fpath in contigs_fpaths]
    create_plot(plot_fpath, title, plots, legend_list, x_label='', y_label=yaxis_title, is_histogram=True,
                x_limit=[0, start_pos + width * len(contigs_fpaths) + interval * (len(contigs_fpaths) - 1)],
                y_limit=[max(bottom_value, 0), top_value])


def coverage_histogram(contigs_fpaths, values, plot_fpath, title='', bin_size=None, draw_bars=None, max_cov=None,
                       low_threshold=None, high_threshold=None):
    if not can_draw_plots:
        return

    logger.info('  Drawing ' + title + '...')

    plots = []
    max_y = 0
    max_x = max(len(v) for v in values)
    x_vals = list(range(0, max_x))
    bar_width = 1.0
    bar_widths = [bar_width] * max_x
    if high_threshold and draw_bars:
        x_vals.append(max_x + 1)
        bar_widths[-1] = 2.0
    x_ticks_labels = [str(x_val * bin_size + low_threshold) for x_val in x_vals]
    if low_threshold:
        x_vals = [x_val + 1 for x_val in x_vals]
        x_vals[0] = 0
        bar_widths[0] = 2.0

    for i, (contigs_fpath, y_vals) in enumerate(zip(contigs_fpaths, values)):
        max_y = max(max(y_vals), max_y)
        color, ls = get_color_and_ls(contigs_fpath)
        if draw_bars:
            for x_val, y_val, bar_width in zip(x_vals, y_vals, bar_widths):
                if bar_width == 2:
                    plots.append(Bar(x_val, y_val, color, width=bar_width, edgecolor='#595959', hatch='x'))
                else:
                    plots.append(Bar(x_val, y_val, color, width=bar_width))
            plots.append(Bar(0, 0, color=color))
        else:
            y_vals.append(y_vals[-1])
            plot_x_vals = [x_val + 0.5 for x_val in x_vals]
            plot_x_vals[-1] += 1
            plots.append(Plot(plot_x_vals, y_vals[:-1], marker='o', markersize=3, color=color, ls=ls))

    x_factor = max(1, len(x_vals) // 10)
    x_ticks = x_vals[::x_factor]
    x_ticks_labels = x_ticks_labels[::x_factor]

    if low_threshold:
        x_ticks_labels.insert(0, 0)
    if high_threshold:
        if low_threshold:
            last_tick = (high_threshold - low_threshold) // bin_size + 4  # first and last bars have width 2
        else:
            last_tick = high_threshold // bin_size + 2
        x_ticks = [x for x in x_ticks if x < last_tick]
        x_ticks_labels = x_ticks_labels[:len(x_ticks)]
        x_ticks.append(last_tick)
        x_ticks_labels.append(str(max_cov))

    for i in range(len(x_ticks) - 1, 0, -1):
        val, prev_val = x_ticks[i], x_ticks[i - 1]
        while val - 1 != prev_val:
            val -= 1
            x_ticks.insert(i, val)
            x_ticks_labels.insert(i, '')
    legend_list = [label_from_fpath(fpath) for fpath in contigs_fpaths]
    xlabel = 'Coverage depth (x)'
    ylabel = 'Total length'

    create_plot(plot_fpath, title, plots, legend_list, x_label=xlabel, y_label=ylabel, is_histogram=True,
                     x_limit=[0, max(x_ticks)], y_limit=[0, max_y * 1.1], x_ticks=x_ticks_labels)


# metaQuast summary plots (per each metric separately)
def draw_meta_summary_plot(html_fpath, output_dirpath, labels, ref_names, results, plot_fpath, title='', reverse=False,
                           yaxis_title='', print_all_refs=False, logger=logger):
    if can_draw_plots:
        logger.info('  Drawing ' + title + ' metaQUAST summary plot...')

    plots = []
    ref_num = len(ref_names)
    contigs_num = len(labels)
    max_y = 0

    arr_x = []
    arr_y = []
    mean_values = []
    arr_y_by_refs = []
    for j in range(contigs_num):
        to_plot_x = []
        to_plot_y = []
        arr = list(range(1, ref_num + 1))
        for i in range(ref_num):
            arr[i] += 0.07 * (j - (contigs_num - 1) * 0.5)
            to_plot_x.append(arr[i])
            if results[i][j] and results[i][j] != '-':
                to_plot_y.append(parse_str_to_num(results[i][j]))
            elif print_all_refs:
                to_plot_y.append(0)
            else:
                to_plot_y.append(None)
        arr_x.append(to_plot_x)
        arr_y.append(to_plot_y)

    selected_refs = []
    for i in range(ref_num):
        points_y = [arr_y[j][i] for j in range(contigs_num) if i < len(arr_y[j])]
        significant_points_y = [points_y[k] for k in range(len(points_y)) if points_y[k] is not None]
        if significant_points_y or print_all_refs:
            arr_y_by_refs.append(points_y)
            mean_values.append(sum(list(filter(None, points_y))) * 1.0 / len(points_y))
            selected_refs.append(ref_names[i])

    json_points_x = []
    json_points_y = []

    if not qconfig.use_input_ref_order:
        sorted_values = sorted(zip(mean_values, selected_refs, arr_y_by_refs), reverse=reverse, key=lambda x: x[0])
        mean_values, selected_refs, arr_y_by_refs = [[x[i] for x in sorted_values] for i in range(3)]

    for j in range(contigs_num):
        points_x = [arr_x[j][i] for i in range(len(arr_y_by_refs))]
        points_y = [arr_y_by_refs[i][j] for i in range(len(arr_y_by_refs))]
        max_y = max(max_y, max(points_y))
        color, ls = get_color_and_ls(None, labels[j])
        plots.append(Plot(points_x, points_y, color=color, ls='dotted', marker='o', markersize=7))
        if not qconfig.use_input_ref_order:
            json_points_x.append(points_x)
            json_points_y.append(points_y)

    refs_for_html = [r for r in selected_refs]  # for summary html, we need to sort values by average value anyway
    if qconfig.use_input_ref_order:
        sorted_values = sorted(zip(mean_values, selected_refs, arr_y_by_refs), reverse=reverse, key=lambda x: x[0])
        mean_values, refs_for_html, arr_y_by_refs = [[x[i] for x in sorted_values] for i in range(3)]
        for j in range(contigs_num):
            points_x = [arr_x[j][i] for i in range(len(arr_y_by_refs))]
            points_y = [arr_y_by_refs[i][j] for i in range(len(arr_y_by_refs))]
            json_points_x.append(points_x)
            json_points_y.append(points_y)

    if qconfig.html_report and html_fpath:
        from quast_libs.html_saver import html_saver
        html_saver.save_meta_summary(html_fpath, output_dirpath, json_points_x, json_points_y,
                                     title.replace(' ', '_'), labels, refs_for_html)
    if can_draw_plots:
        legend_list = labels
        create_plot(plot_fpath, title, plots, legend_list, y_label=yaxis_title, vertical_legend=True,
                    x_ticks=[''] + selected_refs, vertical_ticks=True,
                    x_limit=[0, len(selected_refs) + 1],
                    add_to_report=False, logger=logger)


# metaQuast misassemblies by types plots (all references for 1 assembly)
def draw_meta_summary_misassemblies_plot(results, ref_names, contig_num, plot_fpath, title=''):
    if can_draw_plots:
        meta_logger.info('  Drawing metaQUAST summary misassemblies plot for ' + title + '...')

    plots = []
    refs_num = len(ref_names)
    if len(title) > (120 + len('...')):
        title = title[:120] + '...'

    misassemblies = [reporting.Fields.MIS_RELOCATION, reporting.Fields.MIS_TRANSLOCATION, reporting.Fields.MIS_INVERTION]
    legend_n = []
    max_y = 0
    arr_x = list(range(1, refs_num + 1))
    bar_width = 0.3
    json_points_x = []
    json_points_y = []

    for j in range(refs_num):
        y = 0
        to_plot = []
        type_misassembly = 0
        while len(to_plot) == 0 and type_misassembly < len(misassemblies):
            result = results[type_misassembly][j][contig_num] if results[type_misassembly][j] else None
            if result and result != '-':
                to_plot.append(float(result))
                if can_draw_plots:
                    plots.append(Bar(arr_x[j], to_plot[0], colors[type_misassembly], width=bar_width, align='center'))
                    legend_n.append(type_misassembly)
                    y = float(to_plot[0])
                json_points_x.append(arr_x[j])
                json_points_y.append(to_plot[0])
            type_misassembly += 1
        for i in range(type_misassembly, len(misassemblies)):
            result = results[i][j][contig_num]
            if result and result != '-':
                to_plot.append(float(result))
                if can_draw_plots:
                    plots.append(Bar(arr_x[j], to_plot[-1], colors[i], width=bar_width, align='center', bottom=sum(to_plot[:-1])))
                    legend_n.append(i)
                    y += float(to_plot[-1])
                json_points_x.append(arr_x[j])
                json_points_y.append(to_plot[-1])
        if to_plot:
            max_y = max(max_y, y)
        else:
            for i in range(len(misassemblies)):
                json_points_x.append(arr_x[j])
                json_points_y.append(0)
    if can_draw_plots:
        legend_n = set(legend_n)
        legend_list = [misassemblies[i] for i in sorted(legend_n)]
        create_plot(plot_fpath, title, plots, legend_list, vertical_legend=True, x_ticks=[''] + ref_names, vertical_ticks=True,
                    x_limit=[0, refs_num + 1], add_to_report=False, logger=meta_logger)
    return json_points_x, json_points_y


# Quast misassemblies by types plot (for all assemblies)
def draw_misassemblies_plot(reports, plot_fpath, title='', yaxis_title=''):
    if not can_draw_plots:
        return

    logger.info('  Drawing misassemblies by types plot...')

    plots = []
    contigs_num = len(reports)
    labels = []
    for j in range(contigs_num):
        labels.append(reports[j].get_field(reporting.Fields.NAME))

    misassemblies = [reporting.Fields.MIS_RELOCATION, reporting.Fields.MIS_TRANSLOCATION, reporting.Fields.MIS_INVERTION,
                     reporting.Fields.MIS_ISTRANSLOCATIONS]
    legend_n = []
    main_arr_x = list(range(1, len(reports) + 1))
    arr_x = []
    arr_y = []
    for j in range(len(reports)):
        arr_x.append([0 for x in range(len(misassemblies))])
        arr_y.append([0 for x in range(len(misassemblies))])
        y = 0

        type_misassembly = 0
        while len(arr_x[j]) == 0 and type_misassembly < len(misassemblies):
            result = reports[j].get_field(misassemblies[type_misassembly])
            if result and result != '-':
                arr_y[j][type_misassembly] = float(result)
                arr_x[j][type_misassembly] = main_arr_x[j] + 0.07 * (type_misassembly - (len(misassemblies) * 0.5))
                legend_n.append(type_misassembly)
                y = float(result)
            type_misassembly += 1
        for i in range(type_misassembly, len(misassemblies)):
            result = reports[j].get_field(misassemblies[i])
            if result and result != '-':
                arr_y[j][i] = float(result)
                arr_x[j][i] = main_arr_x[j] + 0.07 * (i - (len(misassemblies) * 0.5))
                legend_n.append(i)
                y += float(result)

    for i in range(len(misassemblies)):
        points_x = [arr_x[j][i] for j in range(contigs_num) if arr_x[j][i] != 0]
        points_y = [arr_y[j][i] for j in range(contigs_num) if arr_y[j][i] != 0]
        if points_y and points_x:
            plots.append(Bar(points_x, points_y, color=colors[i], width=0.05, align='center'))
    for j in range(len(reports)):
        if arr_y[j]:
            points_y = [arr_y[j][i] for i in range(len(misassemblies))]
            significant_points_y = [arr_y[j][i] for i in range(len(misassemblies)) if arr_y[j][i] != 0]
            if len(significant_points_y) > 1:
                type_misassembly = 0
                while points_y[type_misassembly] == 0:
                    type_misassembly += 1
                point_x = main_arr_x[j] + 0.07 * (len(misassemblies) * 0.5)
                plots.append(Bar(point_x, points_y[type_misassembly], color=colors[0], width=0.05, align='center'))
                type_misassembly += 1
                for i in range(type_misassembly, len(arr_y[j])):
                    if points_y[i] > 0:
                        plots.append(Bar(point_x, points_y[i], color=colors[i], width=0.05, align='center', bottom=sum(points_y[:i])))

    legend_n = set(legend_n)
    legend_list = [misassemblies[i] for i in sorted(legend_n)]
    create_plot(plot_fpath, title, plots, legend_list, x_ticks=[''] + labels, x_limit=[0, contigs_num + 1])


def draw_report_table(report_name, extra_info, table_to_draw, column_widths):
    if not can_draw_plots or len(table_to_draw) <= 1:
        return

    # some magic constants ..
    font_size = 12.0
    font_scale = 2.0
    external_font_scale = 10.0
    letter_height_coeff = 0.10
    letter_width_coeff = 0.04

    # .. and their derivatives
    #font_scale = 2 * float(font["size"]) / font_size
    row_height = letter_height_coeff * font_scale
    nrows = len(table_to_draw)
    external_text_height = float(font["size"] * letter_height_coeff * external_font_scale) / font_size
    total_height = nrows * row_height + 2 * external_text_height
    total_width = letter_width_coeff * font_scale * sum(column_widths)

    figure = plt.figure(figsize=(total_width, total_height))
    plt.rc('font', **font)
    plt.axis('off')
    ### all cells are equal (no header and no row labels)
    #plt.text(0, 1. - float(2 * row_height) / total_height, report_name)
    #plt.text(0, 0, extra_info)
    #plt.table(cellText=table_to_draw,
    #    colWidths=[float(column_width) / sum(column_widths) for column_width in column_widths],
    #    rowLoc='right', loc='center')
    plt.text(0.5 - float(column_widths[0]) / (2 * sum(column_widths)),
                           1. - float(2 * row_height) / total_height, report_name.replace('_', ' ').capitalize())
    plt.text(0 - float(column_widths[0]) / (2 * sum(column_widths)), 0, extra_info)
    colLabels=table_to_draw[0][1:]
    rowLabels=[item[0] for item in table_to_draw[1:]]
    restValues=[item[1:] for item in table_to_draw[1:]]
    plt.table(cellText=restValues, rowLabels=rowLabels, colLabels=colLabels,
        colWidths=[float(column_width) / sum(column_widths) for column_width in column_widths[1:]],
        rowLoc='left', colLoc='center', cellLoc='right', loc='center')
    pdf_tables_figures.append(figure)
    plt.close()


def fill_all_pdf_file(all_pdf_fpath):
    if not can_draw_plots or not all_pdf_fpath:
        return

    # moving main report in the beginning
    global pdf_tables_figures
    global pdf_plots_figures
    if len(pdf_tables_figures):
        pdf_tables_figures = [pdf_tables_figures[-1]] + pdf_tables_figures[:-1]

    if qconfig.is_combined_ref:
        run_parallel(save_to_pdf, [(all_pdf_fpath,)], 2)
    else:
        save_to_pdf(all_pdf_fpath)
    pdf_tables_figures = []
    pdf_plots_figures = []

