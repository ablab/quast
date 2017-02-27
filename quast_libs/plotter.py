############################################################################
# Copyright (c) 2015-2016 Saint Petersburg State University
# Copyright (c) 2011-2015 Saint Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################

####################################################################################
###########################  CONFIGURABLE PARAMETERS  ##############################
####################################################################################

# Feel free to add more colors
#colors = ['#E41A1C', '#377EB8', '#4DAF4A', '#984EA3', '#FF7F00', '#A65628', '#F781BF', '#FFFF33']  ## 8-color palette
#red, blue, green, magenta, orange, maroon, aqua, light green, light purple, olive, navy, team, lime
colors = ['#E31A1C', '#1F78B4', '#33A02C', '#6A3D9A', '#FF7F00', '#800000', '#A6CEE3', '#B2DF8A','#333300', '#CCCC00',
          '#000080', '#008080', '#00FF00'] # 14-color palette

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

from quast_libs import fastaparser, qconfig, reporting
from quast_libs.log import get_logger, get_main_logger
from quast_libs.qutils import label_from_fpath, parseStrToNum

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
            import matplotlib.pyplot as plt
            import matplotlib.ticker
            can_draw_plots = True
    except Exception:
        main_logger.info('')
        main_logger.warning('Can\'t draw plots: python-matplotlib is missing or corrupted.')

# for creating PDF file with all plots and tables
pdf_plots_figures = []
pdf_tables_figures = []

dict_color_and_ls = {}
####################################################################################


def save_colors_and_ls(fpaths, labels=None):
    if not labels:
        labels = [label_from_fpath(fpath) for fpath in fpaths]
    if not dict_color_and_ls:
        color_id = 0
        for i, fpath in enumerate(fpaths):
            ls = primary_line_style
            label = labels[i]
            # contigs and scaffolds should be equally colored but scaffolds should be dashed
            if fpath and fpath in qconfig.dict_of_broken_scaffolds:
                color = dict_color_and_ls[label_from_fpath(qconfig.dict_of_broken_scaffolds[fpath])][0]
                ls = secondary_line_style
            else:
                 color = colors[color_id % len(colors)]
                 color_id += 1
            dict_color_and_ls[label] = (color, ls)


def get_color_and_ls(fpath, label=None):
    if not label:
        label = label_from_fpath(fpath)
    """
    Returns tuple: color, line style
    """
    return dict_color_and_ls[label]


def get_locators():
    xLocator = matplotlib.ticker.MaxNLocator(nbins=6, integer=True)
    yLocator = matplotlib.ticker.MaxNLocator(nbins=6, integer=True)
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
    # Shink current axis's height by 20% on the bottom
    box = ax.get_position()
    if vertical_legend:
        ax.set_position([box.x0, box.y0, box.width * 0.8, box.height * 1.0])
    else:
        ax.set_position([box.x0, box.y0 + box.height * 0.2, box.width, box.height * 0.8])
    ax.yaxis.grid(with_grid)
    plt.grid(with_grid)
    return ax


def add_labels(xlabel, ylabel, max_y, ax, logarithmic_x_scale=False):
    if ylabel and 'length' in ylabel:
        ylabel, mkfunc = y_formatter(ylabel, max_y)
        mkformatter = matplotlib.ticker.FuncFormatter(mkfunc)
        ax.yaxis.set_major_formatter(mkformatter)

    plt.xlabel(xlabel, fontsize=axes_fontsize)
    if ylabel:
        plt.ylabel(ylabel, fontsize=axes_fontsize)

    xLocator, yLocator = get_locators()
    ax.yaxis.set_major_locator(yLocator)
    ax.xaxis.set_major_locator(xLocator)
    if logarithmic_x_scale:
        ax.set_xscale('log')


def save_plot(figure, plot_fpath, title, add_to_report=True):
    if not can_draw_plots:
        plt.close()
        return
    if with_title:
        plt.title(title)
    plot_fpath += '.' + qconfig.plot_extension
    plt.savefig(plot_fpath, bbox_inches='tight')
    logger.info('    saved to ' + plot_fpath)
    if add_to_report:
        pdf_plots_figures.append(figure)
    plt.close()


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


def cumulative_plot(reference, contigs_fpaths, lists_of_lengths, plot_fpath, title):
    if not can_draw_plots:
        return

    logger.info('  Drawing cumulative plot...')

    figure = plt.figure()
    plt.rc('font', **font)
    max_x = 0
    max_y = 0

    for (contigs_fpath, lengths) in zip(contigs_fpaths, lists_of_lengths):
        vals_length = [0]
        for l in sorted(lengths, reverse=True):
            vals_length.append(vals_length[-1] + l)
        vals_contig_index = list(range(0, len(vals_length)))
        if vals_contig_index:
            max_x = max(vals_contig_index[-1], max_x)
            max_y = max(max_y, vals_length[-1])
        color, ls = get_color_and_ls(contigs_fpath)
        plt.plot(vals_contig_index, vals_length, color=color, lw=line_width, ls=ls)

    if reference:
        y_vals = []
        for l in sorted(fastaparser.get_chr_lengths_from_fastafile(reference).values(), reverse=True):
            if y_vals:
                y_vals.append(y_vals[-1] + l)
            else:
                y_vals = [l]
        x_vals = list(range(0, len(y_vals))) # for reference only: starting from X=1)
        # extend reference curve to the max X-axis point
        reference_length = y_vals[-1]
        max_x = max(max_x, x_vals[-1])
        max_y = max(max_y, reference_length)
        y_vals.append(reference_length)
        x_vals.append(max_x)
        plt.plot(x_vals, y_vals,
                               color=reference_color, lw=line_width, ls=reference_ls)

    ax = set_ax()
    legend_list = [label_from_fpath(fpath) for fpath in contigs_fpaths]
    if reference:
        legend_list += ['Reference']

    add_legend(ax, legend_list, n_columns=n_columns)
    add_labels('Contig index', 'Cumulative length', max_y, ax, logarithmic_x_scale)

    save_plot(figure, plot_fpath, title)


def frc_plot(results_dir, ref_fpath, contigs_fpaths, contigs_aligned_lengths, features_in_contigs_by_file, plot_fpath, title):
    if can_draw_plots:
        logger.info('  Drawing ' + title + ' FRCurve plot...')

        figure = plt.figure()
        plt.rc('font', **font)

    max_y = 0
    ref_length = sum(fastaparser.get_chr_lengths_from_fastafile(ref_fpath).values())
    json_vals_x = []  # coordinates for Nx-like plots in HTML-report
    json_vals_y = []
    max_features = max(sum(feature_in_contigs) for feature_in_contigs in features_in_contigs_by_file.values()) + 1

    for contigs_fpath in contigs_fpaths:
        aligned_lengths = contigs_aligned_lengths[contigs_fpath]
        feature_in_contigs = features_in_contigs_by_file[contigs_fpath]
        if not aligned_lengths or not feature_in_contigs:
            json_vals_x.append([])
            json_vals_y.append([])
            continue

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
        x_vals = [0]
        y_vals = [0]
        for features_n in range(max_features):
            features_cnt = 0
            cumulative_len = len_with_zero_features
            for l, feature in zip(sorted_lengths, sorted_features):
                if features_cnt + feature <= features_n:
                    features_cnt += feature
                    cumulative_len += l

            x_vals.append(features_n)
            y_vals.append(cumulative_len * 100.0 / ref_length)
            x_vals.append(features_n + 1)
            y_vals.append(cumulative_len * 100.0 / ref_length)

        json_vals_x.append(x_vals)
        json_vals_y.append(y_vals)
        max_y = max(max_y, max(y_vals))

        if can_draw_plots:
            color, ls = get_color_and_ls(contigs_fpath)
            plt.plot(x_vals, y_vals, color=color, lw=line_width, ls=ls)

    if qconfig.html_report:
        from quast_libs.html_saver import html_saver
        html_saver.save_coord(results_dir, json_vals_x, json_vals_y, 'coord' + title, contigs_fpaths)

    if not can_draw_plots:
        return
    ax = set_ax()
    legend_list = [label_from_fpath(fpath) for fpath in contigs_fpaths]
    add_legend(ax, legend_list, n_columns=n_columns)
    add_labels('Feature space', 'Genome coverage (%)', max_y, ax, logarithmic_x_scale)

    plt.xlim([0, max_features])
    plt.ylim([0, max(100, max_y)])

    title = 'FRCurve (' + title + ')'
    save_plot(figure, plot_fpath, title)


# common routine for Nx-plot and NGx-plot (and probably for others Nyx-plots in the future)
def Nx_plot(results_dir, reduce_points, contigs_fpaths, lists_of_lengths, plot_fpath, title='Nx', reference_lengths=None):
    if can_draw_plots:
        logger.info('  Drawing ' + title + ' plot...')

        figure = plt.figure()
        plt.rc('font', **font)

    max_y = 0
    json_vals_x = []  # coordinates for Nx-like plots in HTML-report
    json_vals_y = []

    for id, (contigs_fpath, lengths) in enumerate(zip(contigs_fpaths, lists_of_lengths)):
        if not lengths:
            json_vals_x.append([])
            json_vals_y.append([])
            continue
        vals_x = [0.0]
        vals_y = [lengths[0]]
        lengths.sort(reverse=True)
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
            max_y = max(max_y, max(vals_l))
            color, ls = get_color_and_ls(contigs_fpath)
            plt.plot(vals_Nx, vals_l, color=color, lw=line_width, ls=ls)

    if qconfig.html_report:
        from quast_libs.html_saver import html_saver
        html_saver.save_coord(results_dir, json_vals_x, json_vals_y, 'coord' + title, contigs_fpaths)

    if not can_draw_plots:
        return
    ax = set_ax()
    legend_list = [label_from_fpath(fpath) for fpath in contigs_fpaths]
    add_legend(ax, legend_list, n_columns=n_columns)
    add_labels('x', 'Contig length', max_y, ax)

    plt.xlim([0, 100])

    save_plot(figure, plot_fpath, title)


# routine for GC-plot
def GC_content_plot(ref_fpath, contigs_fpaths, list_of_GC_distributions, plot_fpath):
    if not can_draw_plots or qconfig.no_gc:
        return
    title = 'GC content'

    logger.info('  Drawing ' + title + ' plot...')

    figure = plt.figure()
    plt.rc('font', **font)
    max_y = 0

    all_fpaths = contigs_fpaths
    if ref_fpath:
        all_fpaths = contigs_fpaths + [ref_fpath]

    for i, (GC_distribution_x, GC_distribution_y) in enumerate(list_of_GC_distributions):
        max_y = max(max_y, max(GC_distribution_y))

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

        plt.plot(GC_distribution_x, GC_distribution_y, color=color, lw=line_width, ls=ls)

    ax = set_ax()
    legend_list = [label_from_fpath(fpath) for fpath in contigs_fpaths]
    if ref_fpath:
        legend_list += ['Reference']
    add_legend(ax, legend_list, n_columns=n_columns)
    add_labels('GC (%)', '# windows', max_y, ax)

    plt.xlim([0, 100])

    save_plot(figure, plot_fpath, title)


# common routine for genes and operons cumulative plots
def genes_operons_plot(reference_value, contigs_fpaths, files_feature_in_contigs, plot_fpath, title):
    if not can_draw_plots:
        return

    logger.info('  Drawing ' + title + ' cumulative plot...')

    figure = plt.figure()
    plt.rc('font', **font)
    max_x = 0
    max_y = 0

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
            max_y = max(y_vals[-1], max_y)

        color, ls = get_color_and_ls(contigs_fpath)
        plt.plot(x_vals, y_vals, color=color, lw=line_width, ls=ls)

    if reference_value:
        plt.plot([1, max_x], [reference_value, reference_value],
            color=reference_color, lw=line_width, ls=reference_ls)
        max_y = max(reference_value, max_y)

    ax = set_ax()
    legend_list = [label_from_fpath(fpath) for fpath in contigs_fpaths]
    if reference_value:
        legend_list += ['Reference']
    add_legend(ax, legend_list, n_columns=n_columns)

    title = 'Cumulative # complete ' + title
    add_labels('Contig index', title, max_y, ax, logarithmic_x_scale)

    save_plot(figure, plot_fpath, title)


# common routine for Histograms
def histogram(contigs_fpaths, values, plot_fpath, title='', yaxis_title='', bottom_value=None,
              top_value=None):
    if not can_draw_plots:
        return
    if len(contigs_fpaths) < 2:  #
        logger.info('  Skipping drawing ' + title + ' histogram... (less than 2 columns histogram makes no sense)')
        return

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

    logger.info('  Drawing ' + title + ' histogram...')

    figure = plt.figure()
    plt.rc('font', **font)

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
        plt.bar(start_pos + (width + interval) * i, val, width, color=color, hatch=hatch)

    plt.ylabel(yaxis_title, fontsize=axes_fontsize)

    ax = set_ax()
    legend_list = [label_from_fpath(fpath) for fpath in contigs_fpaths]
    add_legend(ax, legend_list, n_columns=n_columns)

    ax.axes.get_xaxis().set_visible(False)
    plt.xlim([0, start_pos * 2 + width * len(contigs_fpaths) + interval * (len(contigs_fpaths) - 1)])
    plt.ylim([max(bottom_value, 0), top_value])
    yLocator = matplotlib.ticker.MaxNLocator(nbins=6, integer=True, steps=[1,5,10])
    ax.yaxis.set_major_locator(yLocator)

    save_plot(figure, plot_fpath, title)


def coverage_histogram(contigs_fpaths, values, plot_fpath, title='', bin_size=None, draw_bars=None, max_cov=None,
                       low_threshold=None, high_threshold=None):
    if not can_draw_plots:
        return

    logger.info('  Drawing ' + title + '...')

    figure = plt.figure()
    plt.rc('font', **font)

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
                    plt.bar(x_val, y_val, width=bar_width, color=color, edgecolor='black', hatch='x')
                else:
                    plt.bar(x_val, y_val, width=bar_width, color=color)
            plt.bar(0, 0, color=color)
        else:
            y_vals.append(y_vals[-1])
            plot_x_vals = [x_val + 0.5 for x_val in x_vals]
            plot_x_vals[-1] += 1
            plt.plot(plot_x_vals, y_vals[:-1], marker='o', markersize=3, color=color, ls=ls)

    ax = set_ax()
    xlabel = 'Coverage depth (x)'
    ylabel = 'Total length'
    add_labels(xlabel, ylabel, max_y, ax)

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

    plt.xticks(x_ticks, x_ticks_labels, size='small')
    legend_list = [label_from_fpath(fpath) for fpath in contigs_fpaths]
    add_legend(ax, legend_list, n_columns=n_columns)

    plt.xlim([0, max(x_ticks)])
    plt.ylim([0, max_y * 1.1])
    yLocator = matplotlib.ticker.MaxNLocator(nbins=6, integer=True, steps=[1,5,10])
    ax.yaxis.set_major_locator(yLocator)

    save_plot(figure, plot_fpath, title)


# metaQuast summary plots (per each metric separately)
def draw_meta_summary_plot(html_fpath, output_dirpath, labels, ref_names, results, plot_fpath, title='', reverse=False,
                           yaxis_title='', print_all_refs=False):
    if can_draw_plots:
        meta_logger.info('  Drawing ' + title + ' metaQUAST summary plot...')
        figure = plt.figure()
        plt.title(title)

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
                to_plot_y.append(parseStrToNum(results[i][j]))
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

    sorted_values = sorted(zip(mean_values, selected_refs, arr_y_by_refs), reverse=reverse, key=lambda x: x[0])
    mean_values, selected_refs, arr_y_by_refs = [[x[i] for x in sorted_values] for i in range(3)]

    json_points_x = []
    json_points_y = []
    for j in range(contigs_num):
        points_x = [arr_x[j][i] for i in range(len(arr_y_by_refs))]
        points_y = [arr_y_by_refs[i][j] for i in range(len(arr_y_by_refs))]
        max_y = max(max_y, max(points_y))
        if can_draw_plots:
            color, ls = get_color_and_ls(None, labels[j])
            plt.plot(points_x, points_y, 'o:', color=color, ls=ls)
        json_points_x.append(points_x)
        json_points_y.append(points_y)

    if qconfig.html_report and html_fpath:
        from quast_libs.html_saver import html_saver
        html_saver.save_meta_summary(html_fpath, output_dirpath, json_points_x, json_points_y,
                                     title.replace(' ', '_'), labels, selected_refs)
    if can_draw_plots:
        plt.xlim([0, len(selected_refs) + 1])
        if max_y < 5:
            plt.ylim([0, 5])
        else:
            plt.ylim([0, int(math.ceil(max_y * 1.05))])

        ax = set_ax(vertical_legend=True)
        legend_list = labels
        add_legend(ax, legend_list, vertical_legend=True)
        add_labels('', yaxis_title, max_y, ax)
        plt.xticks(range(len(selected_refs) + 1), [''] + selected_refs, size='small', rotation='vertical')

        save_plot(figure, plot_fpath, title, add_to_report=False)


# metaQuast misassemblies by types plots (all references for 1 assembly)
def draw_meta_summary_misassembl_plot(results, ref_names, contig_num, plot_fpath, title=''):
    if can_draw_plots:
        meta_logger.info('  Drawing metaQUAST summary misassemblies plot for ' + title + '...')

    refs_num = len(ref_names)
    if can_draw_plots:
        figure = plt.figure()
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
                    plt.bar(arr_x[j], to_plot[0], width=bar_width, color=colors[type_misassembly])
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
                    plt.bar(arr_x[j], to_plot[-1], width=bar_width, color=colors[i], bottom=sum(to_plot[:-1]))
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
        plt.xticks(range(1, refs_num + 1), ref_names, size='small', rotation='vertical')
        legend_n = set(legend_n)
        legend_list = [misassemblies[i] for i in sorted(legend_n)]

        plt.xlim([0, refs_num + 1])
        if max_y < 5:
            plt.ylim([0, 5])
        else:
            plt.ylim([0, int(math.ceil(max_y * 1.05))])
        ax = set_ax(vertical_legend=True)
        add_legend(ax, legend_list, vertical_legend=True)

        save_plot(figure, plot_fpath, title)
    return json_points_x, json_points_y


# Quast misassemblies by types plot (for all assemblies)
def draw_misassembl_plot(reports, plot_fpath, title='', yaxis_title=''):
    if not can_draw_plots:
        return

    logger.info('  Drawing misassemblies by types plot...')

    contigs_num = len(reports)
    labels = []
    figure = plt.figure()
    for j in range(contigs_num):
        labels.append(reports[j].get_field(reporting.Fields.NAME))

    plt.xticks(range(1, contigs_num + 1), labels, size='small')
    misassemblies = [reporting.Fields.MIS_RELOCATION, reporting.Fields.MIS_TRANSLOCATION, reporting.Fields.MIS_INVERTION,
                     reporting.Fields.MIS_ISTRANSLOCATIONS]
    legend_n = []
    max_y = 0
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
        max_y = max(max_y, y)
    for i in range(len(misassemblies)):
        points_x = [arr_x[j][i] for j in range(contigs_num) if arr_x[j][i] != 0]
        points_y = [arr_y[j][i] for j in range(contigs_num) if arr_y[j][i] != 0]
        if points_y and points_x:
            plt.bar(points_x, points_y, width=0.05, color=colors[i])
    for j in range(len(reports)):
        if (arr_y[j]):
            points_y = [arr_y[j][i] for i in range(len(misassemblies))]
            significant_points_y = [arr_y[j][i] for i in range(len(misassemblies)) if arr_y[j][i] != 0]
            if len(significant_points_y) > 1:
                type_misassembly = 0
                while points_y[type_misassembly] == 0:
                    type_misassembly += 1
                point_x = main_arr_x[j] + 0.07 * (len(misassemblies) * 0.5)
                plt.bar(point_x, points_y[type_misassembly], width=0.05, color=colors[0])
                type_misassembly += 1
                for i in range(type_misassembly, len(arr_y[j])):
                    if points_y[i] > 0:
                        plt.bar(point_x, points_y[i], width=0.05, color=colors[i], bottom=sum(points_y[:i]))

    plt.xlim([0, contigs_num + 1])
    if max_y < 5:
        plt.ylim([0, 5])
    else:
        plt.ylim([0, int(math.ceil(max_y * 1.1))])

    ax = set_ax()
    legend_n = set(legend_n)
    legend_list = [misassemblies[i] for i in sorted(legend_n)]
    add_legend(ax, legend_list, n_columns=n_columns)

    save_plot(figure, plot_fpath, title)


def draw_report_table(report_name, extra_info, table_to_draw, column_widths):
    if not can_draw_plots:
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


def fill_all_pdf_file(all_pdf):
    if not can_draw_plots or not all_pdf:
        return

    # moving main report in the beginning
    global pdf_tables_figures
    global pdf_plots_figures
    if len(pdf_tables_figures):
        pdf_tables_figures = [pdf_tables_figures[-1]] + pdf_tables_figures[:-1]

    for figure in pdf_tables_figures:
        all_pdf.savefig(figure, bbox_inches='tight')
    for figure in pdf_plots_figures:
        all_pdf.savefig(figure)

    try:  # for matplotlib < v.1.0
        d = all_pdf.infodict()
        d['Title'] = 'QUAST full report'
        d['Author'] = 'QUAST'
        import datetime
        d['CreationDate'] = datetime.datetime.now()
        d['ModDate'] = datetime.datetime.now()
    except AttributeError:
        pass
    all_pdf.close()
    pdf_tables_figures = []
    pdf_plots_figures = []
    plt.close('all')  # closing all open figures


