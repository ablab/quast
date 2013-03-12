############################################################################
# Copyright (c) 2011-2013 Saint-Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################

####################################################################################
###########################  CONFIGURABLE PARAMETERS  ##############################
####################################################################################

# Supported plot formats: .emf, .eps, .pdf, .png, .ps, .raw, .rgba, .svg, .svgz
plots_format = '.pdf'

# Feel free to add more colors
colors = ['#E41A1C', '#377EB8', '#4DAF4A', '#984EA3', '#FF7F00', '#A65628', '#F781BF', '#FFFF33']

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

####################################################################################
########################  END OF CONFIGURABLE PARAMETERS  ##########################
####################################################################################

import os
import itertools
import logging
from libs import fastaparser
from libs import qconfig
from libs.qutils import warning


# checking if matplotlib is installed
matplotlib_error = False
try:
    import matplotlib
    matplotlib.use('Agg')  # non-GUI backend
except:
    print
    warning('Can\'t draw plots: please install python-matplotlib.')
    matplotlib_error = True


####################################################################################


def get_color_and_ls(color_id, filename):
    """
    Returns tuple: color, line style
    """
    ls = primary_line_style

    # special case: we have scaffolds and contigs
    if qconfig.scaffolds:
        # contigs and scaffolds should be equally colored but scaffolds should be dashed
        if os.path.basename(filename) in qconfig.list_of_broken_scaffolds:
            next_color_id = color_id
        else:
            ls = secondary_line_style
            next_color_id = color_id + 1
    else:
        next_color_id = color_id + 1

    color = colors[color_id % len(colors)]
    return color, ls, next_color_id


def get_locators():
    xLocator = matplotlib.ticker.MaxNLocator(nbins=6, integer=True)
    yLocator = matplotlib.ticker.MaxNLocator(nbins=6, integer=True)
    return xLocator, yLocator


def y_formatter(ylabel, max_y):
    if max_y <= 3 * 1e+3:
        mkfunc = lambda x, pos: '%d' % (x * 1)
        ylabel += '(bp)'
    elif max_y <= 3 * 1e+6:
        mkfunc = lambda x, pos: '%d' % (x * 1e-3)
        ylabel += '(kbp)'
    else:
        mkfunc = lambda x, pos: '%d' % (x * 1e-6)
        ylabel += '(Mbp)'

    return ylabel, mkfunc


def cumulative_plot(reference, filenames, lists_of_lengths, plot_filename, title, all_pdf=None):
    if matplotlib_error:
        return

    log = logging.getLogger('quast')
    log.info('  Drawing cumulative plot...')
    import matplotlib.pyplot
    import matplotlib.ticker

    matplotlib.pyplot.figure()
    matplotlib.pyplot.rc('font', **font)
    max_x = 0
    max_y = 0
    color_id = 0

    for (filename, lenghts) in itertools.izip(filenames, lists_of_lengths):
        lenghts.sort(reverse=True)
        # calculate values for the plot
        vals_contig_index = [0]
        vals_length = [0]
        lcur = 0
        lind = 0
        for l in lenghts:
            lcur += l
            lind += 1
            x = lind
            vals_contig_index.append(x)
            y = lcur
            vals_length.append(y)
            # add to plot

        if len(vals_contig_index) > 0:
            max_x = max(vals_contig_index[-1], max_x)
            max_y = max(max_y, vals_length[-1])

        color, ls, color_id = get_color_and_ls(color_id, filename)
        matplotlib.pyplot.plot(vals_contig_index, vals_length, color=color, lw=line_width, ls=ls)

    if reference:
        reference_length = sum(fastaparser.get_lengths_from_fastafile(reference))
        matplotlib.pyplot.plot([0, max_x], [reference_length, reference_length],
                               color=reference_color, lw=line_width, ls=reference_ls)
        max_y = max(max_y, reference_length)

    if with_title:
        matplotlib.pyplot.title(title)
    matplotlib.pyplot.grid(with_grid)
    ax = matplotlib.pyplot.gca()
    # Shink current axis's height by 20% on the bottom
    box = ax.get_position()
    ax.set_position([box.x0, box.y0 + box.height * 0.2, box.width, box.height * 0.8])

    legend_list = map(os.path.basename, filenames)
    if qconfig.legend_names and len(filenames) == len(qconfig.legend_names):
        legend_list = qconfig.legend_names[:]
    if reference:
        legend_list += ['Reference']
    # Put a legend below current axis
    try: # for matplotlib <= 2009-12-09
        ax.legend(legend_list, loc='upper center', bbox_to_anchor=(0.5, -0.1), fancybox=True,
            shadow=True, ncol=n_columns)
    except ZeroDivisionError:
        pass

    ylabel = 'Cumulative length '
    ylabel, mkfunc = y_formatter(ylabel, max_y)
    matplotlib.pyplot.xlabel('Contig index', fontsize=axes_fontsize)
    matplotlib.pyplot.ylabel(ylabel, fontsize=axes_fontsize)

    mkformatter = matplotlib.ticker.FuncFormatter(mkfunc)
    ax.yaxis.set_major_formatter(mkformatter)

    xLocator, yLocator = get_locators()
    ax.yaxis.set_major_locator(yLocator)
    ax.xaxis.set_major_locator(xLocator)
    #ax.set_yscale('log')

    #matplotlib.pyplot.ylim([0, int(float(max_y) * 1.1)])

    plot_filename += plots_format
    matplotlib.pyplot.savefig(plot_filename)
    log.info('    saved to ' + plot_filename)

    if plots_format == '.pdf' and all_pdf:
        matplotlib.pyplot.savefig(all_pdf, format='pdf')


# common routine for Nx-plot and NGx-plot (and probably for others Nyx-plots in the future)
def Nx_plot(filenames, lists_of_lengths, plot_filename, title='Nx', reference_lengths=[], all_pdf=None):
    if matplotlib_error:
        return

    log = logging.getLogger('quast')
    log.info('  Drawing ' + title + ' plot...')
    import matplotlib.pyplot
    import matplotlib.ticker

    matplotlib.pyplot.figure()
    matplotlib.pyplot.rc('font', **font)
    max_y = 0
    color_id = 0

    for id, (filename, lengths) in enumerate(itertools.izip(filenames, lists_of_lengths)):
        lengths.sort(reverse=True)
        # calculate values for the plot
        vals_Nx = [0.0]
        vals_l = [lengths[0]]
        lcur = 0
        # if Nx-plot then we just use sum of contigs lengths, else use reference_length
        lsum = sum(lengths)
        if reference_lengths:
            lsum = reference_lengths[id]
        for l in lengths:
            lcur += l
            x = lcur * 100.0 / lsum
            vals_Nx.append(vals_Nx[-1] + 1e-10) # eps
            vals_l.append(l)
            vals_Nx.append(x)
            vals_l.append(l)
            # add to plot

        vals_Nx.append(vals_Nx[-1] + 1e-10) # eps
        vals_l.append(0.0)
        max_y = max(max_y, max(vals_l))

        color, ls, color_id = get_color_and_ls(color_id, filename)
        matplotlib.pyplot.plot(vals_Nx, vals_l, color=color, lw=line_width, ls=ls)

    if with_title:
        matplotlib.pyplot.title(title)
    matplotlib.pyplot.grid(with_grid)
    ax = matplotlib.pyplot.gca()
    # Shink current axis's height by 20% on the bottom
    box = ax.get_position()
    ax.set_position([box.x0, box.y0 + box.height * 0.2, box.width, box.height * 0.8])

    legend_list = map(os.path.basename, filenames)
    if qconfig.legend_names and len(filenames) == len(qconfig.legend_names):
        legend_list = qconfig.legend_names[:]
    # Put a legend below current axis
    try: # for matplotlib <= 2009-12-09
        ax.legend(legend_list, loc='upper center', bbox_to_anchor=(0.5, -0.1), fancybox=True,
            shadow=True, ncol=n_columns)
    except ZeroDivisionError:
        pass

    ylabel = 'Contig length  '
    ylabel, mkfunc = y_formatter(ylabel, max_y)
    matplotlib.pyplot.xlabel('x', fontsize=axes_fontsize)
    matplotlib.pyplot.ylabel(ylabel, fontsize=axes_fontsize)

    mkformatter = matplotlib.ticker.FuncFormatter(mkfunc)
    ax.yaxis.set_major_formatter(mkformatter)
    matplotlib.pyplot.xlim([0, 100])
    #ax.invert_xaxis() 
    #matplotlib.pyplot.ylim(matplotlib.pyplot.ylim()[::-1])
    xLocator, yLocator = get_locators()
    ax.yaxis.set_major_locator(yLocator)
    ax.xaxis.set_major_locator(xLocator)

    plot_filename += plots_format
    matplotlib.pyplot.savefig(plot_filename)
    log.info('    saved to ' + plot_filename)

    if plots_format == '.pdf' and all_pdf:
        matplotlib.pyplot.savefig(all_pdf, format='pdf')


# routine for GC-plot    
def GC_content_plot(reference, filenames, list_of_GC_distributions, plot_filename, all_pdf=None):
    if matplotlib_error:
        return
    title = 'GC content'

    log = logging.getLogger('quast')
    log.info('  Drawing ' + title + ' plot...')
    import matplotlib.pyplot
    import matplotlib.ticker

    matplotlib.pyplot.figure()
    matplotlib.pyplot.rc('font', **font)
    max_y = 0
    color_id = 0

    allfilenames = filenames
    if reference:
        allfilenames = filenames + [reference]
    for id, (GC_distribution_x, GC_distribution_y) in enumerate(list_of_GC_distributions):
        max_y = max(max_y, max(GC_distribution_y))

        # for log scale
        for id2, v in enumerate(GC_distribution_y):
            if v == 0:
                GC_distribution_y[id2] = 0.1

        # add to plot
        if reference and (id == len(allfilenames) - 1):
            color = reference_color
            ls = reference_ls
        else:
            color, ls, color_id = get_color_and_ls(color_id, allfilenames[id])

        matplotlib.pyplot.plot(GC_distribution_x, GC_distribution_y, color=color, lw=line_width, ls=ls)

    if with_title:
        matplotlib.pyplot.title(title)
    matplotlib.pyplot.grid(with_grid)
    ax = matplotlib.pyplot.gca()
    # Shink current axis's height by 20% on the bottom
    box = ax.get_position()
    ax.set_position([box.x0, box.y0 + box.height * 0.2, box.width, box.height * 0.8])
    # Put a legend below current axis
    legend_list = map(os.path.basename, allfilenames)
    if qconfig.legend_names and len(filenames) == len(qconfig.legend_names):
        legend_list = qconfig.legend_names[:]
        if reference:
            legend_list += ['Reference']
    elif reference:
        legend_list[-1] = 'Reference'
    try: # for matplotlib <= 2009-12-09
        ax.legend(legend_list, loc='upper center', bbox_to_anchor=(0.5, -0.1), fancybox=True,
            shadow=True, ncol=n_columns)
    except ZeroDivisionError:
        pass

    ylabel = '# windows'
    #ylabel, mkfunc = y_formatter(ylabel, max_y)
    matplotlib.pyplot.xlabel('GC (%)', fontsize=axes_fontsize)
    matplotlib.pyplot.ylabel(ylabel, fontsize=axes_fontsize)

    #mkformatter = matplotlib.ticker.FuncFormatter(mkfunc)
    #ax.yaxis.set_major_formatter(mkformatter)
    matplotlib.pyplot.xlim([0, 100])

    xLocator, yLocator = get_locators()
    ax.yaxis.set_major_locator(yLocator)
    ax.xaxis.set_major_locator(xLocator)

    #ax.set_yscale('symlog', linthreshy=0.5)
    #ax.invert_xaxis()
    #matplotlib.pyplot.ylim(matplotlib.pyplot.ylim()[::-1])

    plot_filename += plots_format
    matplotlib.pyplot.savefig(plot_filename)
    log.info('    saved to ' + plot_filename)

    if plots_format == '.pdf' and all_pdf:
        matplotlib.pyplot.savefig(all_pdf, format='pdf')


# common routine for genes and operons cumulative plots
def genes_operons_plot(reference_value, filenames, files_feature_in_contigs, plot_filename, title, all_pdf=None):
    if matplotlib_error:
        return

    log = logging.getLogger('quast')
    log.info('  Drawing ' + title + ' cumulative plot...')
    import matplotlib.pyplot
    import matplotlib.ticker

    matplotlib.pyplot.figure()
    matplotlib.pyplot.rc('font', **font)
    max_x = 0
    max_y = 0
    color_id = 0

    for filename in filenames:
        # calculate values for the plot
        feature_in_contigs = files_feature_in_contigs[filename]

        x_vals = range(len(feature_in_contigs) + 1)
        y_vals = [0]
        total_full = 0
        for feature_amount in feature_in_contigs:
            total_full += feature_amount
            y_vals.append(total_full)

        if len(x_vals) > 0:
            max_x = max(x_vals[-1], max_x)
            max_y = max(y_vals[-1], max_y)

        color, ls, color_id = get_color_and_ls(color_id, filename)
        matplotlib.pyplot.plot(x_vals, y_vals, color=color, lw=line_width, ls=ls)

    if reference_value:
        matplotlib.pyplot.plot([0, max_x], [reference_value, reference_value],
            color=reference_color, lw=line_width, ls=reference_ls)
        max_y = max(reference_value, max_y)

    matplotlib.pyplot.xlabel('Contig index', fontsize=axes_fontsize)
    matplotlib.pyplot.ylabel('Cumulative # complete ' + title, fontsize=axes_fontsize)
    if with_title:
        matplotlib.pyplot.title('Cumulative # complete ' + title)
    matplotlib.pyplot.grid(with_grid)
    ax = matplotlib.pyplot.gca()
    # Shink current axis's height by 20% on the bottom
    box = ax.get_position()
    ax.set_position([box.x0, box.y0 + box.height * 0.2, box.width, box.height * 0.8])

    legend_list = map(os.path.basename, filenames)
    if qconfig.legend_names and len(filenames) == len(qconfig.legend_names):
        legend_list = qconfig.legend_names[:]
    if reference_value:
        legend_list += ['Reference']
    # Put a legend below current axis
    try: # for matplotlib <= 2009-12-09
        ax.legend(legend_list, loc='upper center', bbox_to_anchor=(0.5, -0.1), fancybox=True,
            shadow=True, ncol=n_columns)
    except ZeroDivisionError:
        pass

    xLocator, yLocator = get_locators()
    ax.yaxis.set_major_locator(yLocator)
    ax.xaxis.set_major_locator(xLocator)
    #matplotlib.pyplot.ylim([0, int(float(max_y) * 1.1)])

    plot_filename += plots_format
    matplotlib.pyplot.savefig(plot_filename)
    log.info('    saved to ' + plot_filename)

    if plots_format == '.pdf' and all_pdf:
        matplotlib.pyplot.savefig(all_pdf, format='pdf')


# common routine for Histograms    
def histogram(filenames, values, plot_filename, title='', all_pdf=None, yaxis_title='', bottom_value=None,
              top_value=None):
    if matplotlib_error:
        return

    import math

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
        bottom_value = (math.floor(min_value / exponent) - 1) * exponent
    if not top_value:
        top_value = (math.ceil(max_value / exponent) + 1) * exponent

    log = logging.getLogger('quast')
    log.info('  Drawing ' + title + ' histogram...')
    import matplotlib.pyplot
    import matplotlib.ticker

    matplotlib.pyplot.figure()
    matplotlib.pyplot.rc('font', **font)

    #bars' params
    width = 0.3
    interval = width / 3
    start_pos = interval / 2

    #import numpy
    #positions = numpy.arange(len(filenames))

    color_id = 0
    for id, (filename, val) in enumerate(itertools.izip(filenames, values)):
        color, ls, color_id = get_color_and_ls(color_id, filename)
        if ls == primary_line_style:
            hatch = ''
        else:
            hatch = 'x'
        matplotlib.pyplot.bar(start_pos + (width + interval) * id, val, width, color=color, hatch=hatch)

    #matplotlib.pyplot.xticks(positions + width, map(os.path.basename, filenames))
    matplotlib.pyplot.ylabel(yaxis_title, fontsize=axes_fontsize)
    if with_title:
        matplotlib.pyplot.title(title)

    ax = matplotlib.pyplot.gca()
    # Shink current axis's height by 20% on the bottom
    box = ax.get_position()
    ax.set_position([box.x0, box.y0 + box.height * 0.2, box.width, box.height * 0.8])
    ax.yaxis.grid(with_grid)

    legend_list = map(os.path.basename, filenames)
    if qconfig.legend_names and len(filenames) == len(qconfig.legend_names):
        legend_list = qconfig.legend_names[:]
    # Put a legend below current axis
    try: # for matplotlib <= 2009-12-09
        ax.legend(legend_list, loc='upper center', bbox_to_anchor=(0.5, -0.1), fancybox=True,
            shadow=True, ncol=n_columns)
    except ZeroDivisionError:
        pass

    ax.axes.get_xaxis().set_visible(False)
    matplotlib.pyplot.xlim([0, start_pos * 2 + width * len(filenames) + interval * (len(filenames) - 1)])
    matplotlib.pyplot.ylim([bottom_value, top_value])
    yLocator = matplotlib.ticker.MaxNLocator(nbins=6, integer=True, steps=[1,5,10])
    ax.yaxis.set_major_locator(yLocator)

    plot_filename += plots_format
    matplotlib.pyplot.savefig(plot_filename)
    log.info('    saved to ' + plot_filename)

    if plots_format == '.pdf' and all_pdf:
        matplotlib.pyplot.savefig(all_pdf, format='pdf')