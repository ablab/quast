############################################################################
# Copyright (c) 2011-2012 Saint-Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################

import os
import itertools
import fastaparser
import json_saver
import qconfig
from qutils import id_to_str

def do(reference, filenames, output_dir, all_pdf):
    
    if not os.path.isdir(output_dir):
        os.mkdir(output_dir)

    ########################################################################
    report_dict = {'header' : []}
    for filename in filenames:
        report_dict[os.path.basename(filename)] = []
    
    ########################################################################

    if reference:
        reference_length = fastaparser.get_lengths_from_fastafile(reference)[0]
        print 'Reference genome:'
        print ' ', reference, ', reference length =', int(reference_length)
    print 'Contigs files: '
    lists_of_lengths = []
    for id, filename in enumerate(filenames):
        print ' ', id_to_str(id), os.path.basename(filename)
        lists_of_lengths.append(fastaparser.get_lengths_from_fastafile(filename))

    # saving to JSON
    if qconfig.to_archive:
        json_saver.save_lengths(filenames, lists_of_lengths)

    ########################################################################

    print 'Calculating N50...'
    report_dict['header'].append('N50')
    if reference:
        report_dict['header'].append('NG50')
    report_dict['header'].append('N75')
    if reference:
        report_dict['header'].append('NG75')
    report_dict['header'].append('Number of contigs')
    report_dict['header'].append('Largest contig')
    report_dict['header'].append('Total length')
    if reference:
        report_dict['header'].append('Reference length')
    
    import N50
    for id, (filename, lengths_list) in enumerate(itertools.izip(filenames, lists_of_lengths)):
        n50 = N50.N50(lengths_list)
        if reference:
            ng50 = N50.NG50(lengths_list, reference_length)
        n75 = N50.N50(lengths_list, 75)
        if reference:
            ng75 = N50.NG50(lengths_list, reference_length, 75)
        total_length = sum(lengths_list)
        print ' ', id_to_str(id), os.path.basename(filename), \
            ', N50 =', n50, \
            ', Total length =', total_length
        report_dict[os.path.basename(filename)].append(n50)
        if reference:
            report_dict[os.path.basename(filename)].append(ng50)
        report_dict[os.path.basename(filename)].append(n75)
        if reference:
            report_dict[os.path.basename(filename)].append(ng75)
        report_dict[os.path.basename(filename)].append(len(lengths_list))
        report_dict[os.path.basename(filename)].append(max(lengths_list))
        report_dict[os.path.basename(filename)].append(total_length)
        if reference:
            report_dict[os.path.basename(filename)].append(int(reference_length))
        
    ########################################################################    

    # Drawing cumulative plot...
    import plotter
    plotter.cumulative_plot(filenames, lists_of_lengths, output_dir + '/cumulative_plot', 'Cumulative length', all_pdf)
    
    ########################################################################

    # Drawing Nx and NGx plots...
    plotter.Nx_plot(filenames, lists_of_lengths, output_dir + '/Nx_plot', 'Nx', [], all_pdf)
    if reference:
        plotter.Nx_plot(filenames, lists_of_lengths, output_dir + '/NGx_plot', 'NGx', [reference_length for i in range(len(filenames))], all_pdf)

    return report_dict
