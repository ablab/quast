############################################################################
# Copyright (c) 2011-2012 Saint-Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################

import os
import itertools
import fastaparser
from libs import reporting, qconfig
from qutils import id_to_str

__location__ = os.path.realpath(os.path.join(os.getcwd(), os.path.dirname(__file__)))

def get_lengths_from_coordfile(nucmer_filename): # TODO: re-use Mappings from plantakolya
    """
    Returns list of aligned blocks' lengths
    """
    max_overlap = 0.1 # 10 %

    coordfile = open(nucmer_filename, 'r')
    for line in coordfile:
        if line.startswith('='):
            break

    # EXAMPLE:
    #    [S1]     [E1]  |     [S2]     [E2]  |  [LEN 1]  [LEN 2]  |  [% IDY]  | [TAGS]
    #=====================================================================================
    #  338980   339138  |     2298     2134  |      159      165  |    79.76  | gi|48994873|gb|U00096.2|	NODE_0_length_6088
    #  374145   374355  |     2306     2097  |      211      210  |    85.45  | gi|48994873|gb|U00096.2|	NODE_0_length_6088
    # 2302590  2302861  |        1      272  |      272      272  |  98.5294  | gi|48994873|gb|U00096.2|	NODE_681_length_272_
    # 2302816  2303087  |        1      272  |      272      272  |  98.5294  | gi|48994873|gb|U00096.2|	NODE_681_length_272_
    # 2302703  2302974  |        1      272  |      272      272  |  98.1618  | gi|48994873|gb|U00096.2|	NODE_681_length_272_
    # 2302477  2302748  |        1      272  |      272      272  |  96.6912  | gi|48994873|gb|U00096.2|	NODE_681_length_272_

    aligned_lengths = []

    for line in coordfile:
        splitted = line.split('|')
        len2 = int(splitted[2].split()[1])
        aligned_lengths.append(len2)
    coordfile.close()

    return aligned_lengths

######## MAIN ############
def do(reference, filenames, nucmer_dir, output_dir, all_pdf, draw_plots, json_output_dir, results_dir):

    if not os.path.isdir(output_dir):
        os.mkdir(output_dir)

    ########################################################################

    nucmer_prefix = os.path.join(os.path.join(__location__, ".."), nucmer_dir, 'nucmer_output')

    ########################################################################
    report_dict = {'header' : []}
    for filename in filenames:
        report_dict[os.path.basename(filename)] = []

    ########################################################################
    print 'Running NA-NGA tool...'

    reference_length = sum(fastaparser.get_lengths_from_fastafile(reference))
    lists_of_lengths = []
    assembly_lengths = []
    print 'Processing .coords files...'
    for id, filename in enumerate(filenames):
        print ' ', id_to_str(id) + os.path.basename(filename)
        nucmer_filename = os.path.join(nucmer_prefix, os.path.basename(filename) + '.coords.filtered')
        assembly_lengths.append(sum(fastaparser.get_lengths_from_fastafile(filename)))
        if not os.path.isfile(nucmer_filename):
            print '  Error: nucmer .coords.filtered file (' + nucmer_filename + ') not found, skipping...'
            lists_of_lengths.append([0])
        else:
            lists_of_lengths.append(get_lengths_from_coordfile(nucmer_filename))
    ########################################################################

    print 'Calculating NA50 and NGA50...'

    import N50
    for id, (filename, lens, assembly_len) in enumerate(itertools.izip(filenames, lists_of_lengths, assembly_lengths)):
        na50 = N50.NG50(lens, assembly_len)
        nga50 = N50.NG50(lens, reference_length)
        na75 = N50.NG50(lens, assembly_len, 75)
        nga75 = N50.NG50(lens, reference_length, 75)
        la50 = N50.LG50(lens, assembly_len)
        lga50 = N50.LG50(lens, reference_length)
        la75 = N50.LG50(lens, assembly_len, 75)
        lga75 = N50.LG50(lens, reference_length, 75)
        print ' ', id_to_str(id) + os.path.basename(filename) + \
            ', Largest alignment = ' + str(max(lens)) + \
            ', NA50 = ' + str(na50) + \
            ', NGA50 = ' + str(nga50) + \
            ', LA50 = ' + str(la50) +\
            ', LGA50 = ' + str(lga50)
        report = reporting.get(filename)
        report.add_field(reporting.Fields.LARGALIGN, max(lens))
        report.add_field(reporting.Fields.NA50, na50)
        report.add_field(reporting.Fields.NGA50, nga50)
        report.add_field(reporting.Fields.NA75, na75)
        report.add_field(reporting.Fields.NGA75, nga75)
        report.add_field(reporting.Fields.LA50, la50)
        report.add_field(reporting.Fields.LGA50, lga50)
        report.add_field(reporting.Fields.LA75, la75)
        report.add_field(reporting.Fields.LGA75, lga75)

    ########################################################################

    # saving to JSON
    if json_output_dir:
        from libs.html_saver import json_saver
        json_saver.save_aligned_contigs_lengths(json_output_dir, filenames, lists_of_lengths)
        json_saver.save_assembly_lengths(json_output_dir, filenames, assembly_lengths)

    # saving to html
    if qconfig.html_report:
        from libs.html_saver import html_saver
        html_saver.save_aligned_contigs_lengths(results_dir, filenames, lists_of_lengths)
        html_saver.save_assembly_lengths(results_dir, filenames, assembly_lengths)

    if draw_plots:
        # Drawing cumulative plot (aligned contigs)...
        import plotter
        plotter.cumulative_plot(reference, filenames, lists_of_lengths, output_dir + '/cumulative_plot', 'Cumulative length (aligned contigs)', all_pdf)

        # Drawing NAx and NGAx plots...
        plotter.Nx_plot(filenames, lists_of_lengths, output_dir + '/NAx_plot', 'NAx', assembly_lengths, all_pdf)
        plotter.Nx_plot(filenames, lists_of_lengths, output_dir + '/NGAx_plot', 'NGAx', [reference_length for i in range(len(filenames))], all_pdf)

    return report_dict
