############################################################################
# Copyright (c) 2011-2012 Saint-Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################

import os
import itertools
import fastaparser
from qutils import id_to_str

# Supported plot formats: .emf, .eps, .pdf, .png, .ps, .raw, .rgba, .svg, .svgz
#plots_format = '.svg' 
plots_format = '.pdf' 

matplotlib_error = False
try:
    import matplotlib
    matplotlib.use('Agg') # non-GUI backend
except:
    print 'Warning! Can\'t draw plots - please install python-matplotlib' 
    matplotlib_error = True

colors = ['#E41A1C', '#377EB8', '#4DAF4A', '#984EA3', '#FF7F00', '#A65628', '#F781BF', '#FFFF33']    

font = {'family' : 'sans-serif',
    'style'  : 'normal',
    'weight' : 'medium',
    'size'   : 10}

# plots params
linewdith = 3.0

# legend params
n_columns = 4
with_grid = True
with_title = True
axes_fontsize = 'large' # axes labels and ticks values

def cumulative_plot(filenames, lists_of_lengths, plot_filename, title, all_pdf=None):
    if matplotlib_error:
        return

    print '  Drawing cumulative plot...',
    import matplotlib.pyplot
    import matplotlib.ticker
    matplotlib.pyplot.figure()
    matplotlib.pyplot.rc('font', **font)
    color_id = 0

    for filename, lenghts in itertools.izip(filenames, lists_of_lengths):
        lenghts.sort(reverse = True)
        # calculate values for the plot
        vals_percent = []
        vals_length = []
        lcur = 0
        lind = 0
        for l in lenghts:
            lcur += l
            lind += 1
            x = lind
            vals_percent.append(x)
            y = lcur
            vals_length.append(y)
        # add to plot
        if color_id < len(colors):
            matplotlib.pyplot.plot(vals_percent, vals_length, color=colors[color_id % len(colors)], lw=linewdith)
        else:
            matplotlib.pyplot.plot(vals_percent, vals_length, color=colors[color_id % len(colors)], lw=linewdith, ls='dashed')
        color_id += 1
            
    matplotlib.pyplot.xlabel('Contig index', fontsize=axes_fontsize)
    matplotlib.pyplot.ylabel('Cumulative length (Mbp)', fontsize=axes_fontsize)
    if with_title:
        matplotlib.pyplot.title(title)
    matplotlib.pyplot.grid(with_grid)
    ax = matplotlib.pyplot.gca()
    # Shink current axis's height by 20% on the bottom
    box = ax.get_position()
    ax.set_position([box.x0, box.y0 + box.height * 0.2, box.width, box.height * 0.8])
    # Put a legend below current axis
    try: # for matplotlib <= 2009-12-09
        ax.legend(map(os.path.basename, filenames), loc='upper center', bbox_to_anchor=(0.5, -0.1), fancybox=True, shadow=False, ncol=n_columns)
    except ZeroDivisionError:
        pass
    
    mkfunc = lambda x, pos: '%d' % (x * 1e-6)     
    mkformatter = matplotlib.ticker.FuncFormatter(mkfunc)
    myLocator = matplotlib.ticker.LinearLocator(6)
    mxLocator = matplotlib.ticker.LinearLocator(6)
    
    ax.yaxis.set_major_formatter(mkformatter)
    ax.yaxis.set_major_locator(myLocator)
    ax.xaxis.set_major_locator(mxLocator)
    #ax.set_yscale('log')

    plot_filename += plots_format
    matplotlib.pyplot.savefig(plot_filename)
    print 'saved to', plot_filename
    
    if plots_format == '.pdf' and all_pdf:
        matplotlib.pyplot.savefig(all_pdf, format='pdf') 
    

# common routine for Nx-plot and NGx-plot (and probably for others Nyx-plots in the future)    
def Nx_plot(filenames, lists_of_lengths, plot_filename, title='Nx', reference_lengths=[], all_pdf=None):
    if matplotlib_error:
        return

    print '  Drawing ' + title + ' plot...',
    import matplotlib.pyplot
    import matplotlib.ticker
    matplotlib.pyplot.figure() 
    matplotlib.pyplot.rc('font', **font)
    color_id = 0
    x = 0

    for id, (filename, lengths) in enumerate(itertools.izip(filenames, lists_of_lengths)):
        lengths.sort(reverse = True)
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
        if color_id < len(colors):
            matplotlib.pyplot.plot(vals_Nx, vals_l, color=colors[color_id % len(colors)], lw=linewdith)
        else:
            matplotlib.pyplot.plot(vals_Nx, vals_l, color=colors[color_id % len(colors)], lw=linewdith, ls='dashed')
        color_id += 1

    matplotlib.pyplot.xlabel('x', fontsize=axes_fontsize)
    matplotlib.pyplot.ylabel('Contig length (Kbp)', fontsize=axes_fontsize)
    if with_title:
        matplotlib.pyplot.title(title)
    matplotlib.pyplot.grid(with_grid)
    ax = matplotlib.pyplot.gca()
    # Shink current axis's height by 20% on the bottom
    box = ax.get_position()
    ax.set_position([box.x0, box.y0 + box.height * 0.2, box.width, box.height * 0.8])
    # Put a legend below current axis
    try: # for matplotlib <= 2009-12-09
        ax.legend(map(os.path.basename, filenames), loc='upper center', bbox_to_anchor=(0.5, -0.1), fancybox=True, shadow=True, ncol=n_columns)
    except ZeroDivisionError:
        pass    
    
    mkfunc = lambda x, pos: '%d' % (x * 1e-3) 
    mkformatter = matplotlib.ticker.FuncFormatter(mkfunc)
    ax.yaxis.set_major_formatter(mkformatter)
    matplotlib.pyplot.xlim([0,100])
    #ax.invert_xaxis() 
    #matplotlib.pyplot.ylim(matplotlib.pyplot.ylim()[::-1])

    plot_filename += plots_format
    matplotlib.pyplot.savefig(plot_filename)
    print 'saved to', plot_filename

    if plots_format == '.pdf' and all_pdf:
        matplotlib.pyplot.savefig(all_pdf, format='pdf')


# common routine for genes and operons cumulative plots
def genes_operons_plot(filenames, files_contigs, genes, found, plot_filename, title, all_pdf=None):
    if matplotlib_error:
        return

    print '  Drawing ' + title + ' cumulative plot...',    
    import matplotlib.pyplot
    import matplotlib.ticker    
    matplotlib.pyplot.figure() 
    matplotlib.pyplot.rc('font', **font)   
    color_id = 0
    
    for filename in filenames:    
        # calculate values for the plot
        contigs = files_contigs[filename]    # [ [contig_blocks] ] 
        for i, gene in enumerate(genes):
            found[i] = 0  # mark all genes as 'not found'

        x_vals = [0]
        y_vals = [0]
        contig_no = 0
        total_full = 0        
        for aligned_blocks in contigs:
            contig_no += 1    
            for i, gene in enumerate(genes):
                if found[i] == 0:
                    for block in aligned_blocks:
                        if gene.end <= block[0] or block[1] <= gene.start:   # [0] - start, [1] - end
                            continue
                        elif block[0] <= gene.start and gene.end <= block[1]:
                            found[i] = 1
                            total_full += 1
                            break
            x_vals.append(contig_no)
            y_vals.append(total_full)                                
        if color_id < len(colors):
            matplotlib.pyplot.plot(x_vals, y_vals, color=colors[color_id % len(colors)], lw=linewdith)
        else:
            matplotlib.pyplot.plot(x_vals, y_vals, color=colors[color_id % len(colors)], lw=linewdith, ls='dashed')
        color_id += 1
        
    matplotlib.pyplot.xlabel('Contig index', fontsize=axes_fontsize)
    matplotlib.pyplot.ylabel('Cumulative no. ' + title, fontsize=axes_fontsize)
    matplotlib.pyplot.title(title.capitalize())
    matplotlib.pyplot.grid(False)
    ax = matplotlib.pyplot.gca()
    # Shink current axis's height by 20% on the bottom
    box = ax.get_position()
    ax.set_position([box.x0 + box.width * 0.2, box.y0 + box.height * 0.1, box.width * 0.8, box.height * 0.9])
    # Put a legend below current axis
    ax.legend(map(os.path.basename, filenames), loc='upper center', bbox_to_anchor=(0.5, -0.1), fancybox=True, shadow=False, ncol=4)

    #myLocator = matplotlib.ticker.LinearLocator(4)
    #mxLocator = matplotlib.ticker.LinearLocator(4)   
    #ax.yaxis.set_major_locator(myLocator)    
    #ax.xaxis.set_major_locator(mxLocator)  

    plot_filename += plots_format
    matplotlib.pyplot.savefig(plot_filename)
    print 'saved to', plot_filename

    if plots_format == '.pdf' and all_pdf:
        matplotlib.pyplot.savefig(all_pdf, format='pdf')

# common routine for Histograms    
def histogram(filenames, values, plot_filename, title='', all_pdf=None, yaxis_title='', bottom_value=None, top_value=None):    
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
        exponent  = math.pow(10, math.floor(math.log(max_value - min_value, 10)))

    if not bottom_value:
        bottom_value = (math.floor(min_value / exponent) - 1) * exponent
    if not top_value:
        top_value    = (math.ceil(max_value / exponent) + 1) * exponent

    print '  Drawing ' + title + ' histogram...',
    import matplotlib.pyplot
    import matplotlib.ticker
    matplotlib.pyplot.figure() 
    matplotlib.pyplot.rc('font', **font)
    color_id = 0
    x = 0

    #bars' params
    width = 0.3
    interval = width / 3
    start_pos = interval / 2

    #import numpy
    #positions = numpy.arange(len(filenames))

    for id, (filename, val) in enumerate(itertools.izip(filenames, values)):    
        cur_ls = 'solid'
        if id >= len(colors):
            cur_ls = 'dashed'

        matplotlib.pyplot.bar(start_pos + (width + interval) * id, val, width, color=colors[id % len(colors)], ls=cur_ls)


    #matplotlib.pyplot.xticks(positions + width, map(os.path.basename, filenames))
    matplotlib.pyplot.ylabel(yaxis_title, fontsize=axes_fontsize)
    if with_title:
        matplotlib.pyplot.title(title)
    
    ax = matplotlib.pyplot.gca()
    # Shink current axis's height by 20% on the bottom
    box = ax.get_position()
    ax.set_position([box.x0, box.y0 + box.height * 0.2, box.width, box.height * 0.8])
    ax.yaxis.grid(with_grid)
    # Put a legend below current axis
    try: # for matplotlib <= 2009-12-09
        ax.legend(map(os.path.basename, filenames), loc='upper center', bbox_to_anchor=(0.5, -0.1), fancybox=True, shadow=True, ncol=n_columns)
    except ZeroDivisionError:
        pass    

    ax.axes.get_xaxis().set_visible(False)   
    matplotlib.pyplot.xlim([0, start_pos * 2 + width * len(filenames) + interval * (len(filenames) - 1)])
    matplotlib.pyplot.ylim([bottom_value, top_value])

    plot_filename += plots_format
    matplotlib.pyplot.savefig(plot_filename)
    print 'saved to', plot_filename

    if plots_format == '.pdf' and all_pdf:
        matplotlib.pyplot.savefig(all_pdf, format='pdf')
