#!/usr/bin/python

############################################################################
# Copyright (c) 2011-2012 Saint-Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################


import sys
import os
import shutil
import re

def do(total_report, genome_info, output_filename, min_contig):

    total_report += '.txt'

    print 'Starting Extra report creation...'

    ## some checking
    if not os.path.isfile(genome_info):
        print 'Error: genome info file (' + genome_info + ') not found! Exiting...'
        sys.exit(0)
    if not os.path.isfile(total_report):
        print 'Error: total report file (' + total_report + ') not found! Exiting...'
        sys.exit(0)

    ## parsing genome_info header
    genome_size = ''
    genes = ''
    operons = ''
    genome_info_file = open(genome_info, 'r')
    for line in genome_info_file:
        if line.startswith('genome size'):
            genome_size = line.split(':')[1].strip()
        elif line.startswith('genes'):
            genes = line.split(':')[1].strip()
        elif line.startswith('operons'):
            operons = line.split(':')[1].strip()          
    genome_info_file.close()

    ##
    metrics_new_headers = ['Assembly', '# contigs', 'genome N50 (bp)', 'genome NA50 (bp)', 'Largest (bp)', \
        'Total (bp)', 'Mapped genome (%)', 'Misassemblies', 'Misassembled Contigs', 'Misassembled Contig Bases', '_aux Miscalled', '_aux Genes']
    metrics_old_headers = ['Assembly', 'Number of contigs', 'NG50', 'NGA50', 'Largest contig', \
        'Total length', 'Mapped genome (%)', 'Misassemblies', 'Misassembled Contigs', 'Misassembled Contig Bases', 'Number of MisCalled', 'Genes']

    # parsing header
    total_report_file = open(total_report, 'r')
    metrics_positions = [-1 for i in range(len(metrics_old_headers))]
    
    header = ""
    # skip lines with info about min contig
    while not header:
        header = total_report_file.readline()
        if header.startswith("Only contigs of length") or not header.strip():
            header = ""

    for id, title in enumerate(header.split('|')):
        for id2, title2 in enumerate(metrics_old_headers):
            if title2 in title:
                metrics_positions[id2] = id
                break

    # filling report_dict
    report_dict = {}
    report_dict['header'] = []

    for id, value in enumerate(metrics_new_headers):
        if not value.startswith('_aux'):
            report_dict['header'].append(value)        
        else:
            break

    report_dict['header'].append('Subs. Error (per 100 kbp)')
    report_dict['header'].append('Known genes')
    report_dict['header'].append('Complete genes')
    report_dict['header'].append('plus partial')    

    for line in total_report_file:       
        fields = line.split('|')
        cur_assembly = fields[0].strip()
        report_dict[cur_assembly] = []
        for id, metric in enumerate(metrics_new_headers):
            if not metric.startswith('_aux'):
                report_dict[cur_assembly].append(fields[metrics_positions[id]].strip())
            else:
                if metric == '_aux Miscalled':                    
                    if fields[metrics_positions[id]].strip() == '':
                        report_dict[cur_assembly].append('nd')
                    else:                        
                        report_dict[cur_assembly].append( "%.2f" % (float(fields[metrics_positions[id]]) * 100000.0 / float(genome_size)) )
                elif metric == '_aux Genes':
                    if genes == '':
                        report_dict[cur_assembly].append('-')    
                        report_dict[cur_assembly].append('nd')
                        report_dict[cur_assembly].append('nd')
                    else:                
                        report_dict[cur_assembly].append(genes)
                        full = fields[metrics_positions[id]].split()[0]
                        partial = fields[metrics_positions[id]].split()[2]
                        report_dict[cur_assembly].append(full.strip())
                        report_dict[cur_assembly].append(str(int(full.strip()) + int(partial.strip())))
                elif metric == '_aux Operons':
                    if operons == '':
                        report_dict[cur_assembly].append('-')    
                        report_dict[cur_assembly].append('nd')
                        report_dict[cur_assembly].append('nd')
                    else:
                        report_dict[cur_assembly].append(operons)
                        full = fields[metrics_positions[id]].split()[0]
                        partial = fields[metrics_positions[id]].split()[2]
                        report_dict[cur_assembly].append(full)
                        report_dict[cur_assembly].append(str(int(full.strip()) + int(partial.strip())))
                else:
                    break

    total_report_file.close()

    ### DEPRECATED
    ### to compare with other assemblers
    ### adding third-party assemblers' results from SC paper
#    paper_table = ''
#    folder_with_pt = os.path.join(os.path.abspath(sys.path[0]), 'libs/report')
#    if assembly_type == 'single cell':
#        paper_table = folder_with_pt + '/EColi_SC_lane1.txt'
#    elif assembly_type == 'normal':
#        paper_table = folder_with_pt + '/EColi_lane_normal.txt'
#        ###
#
#    if paper_table != '':
#        paper_table_file = open(paper_table, 'r')
#        for line in paper_table_file:
#            fields = line.split('|')
#            cur_assembler_name = fields[0].strip()
#            report_dict[cur_assembler_name] = []
#            for field in fields:
#                if field.strip() != '':
#                    report_dict[cur_assembler_name].append(field.strip())
#        paper_table_file.close()

    # calculate columns widthes
    col_widthes = [0 for i in range(len(report_dict['header']))]
    for row in report_dict.keys():            
        for id, value in enumerate(report_dict[row]):
            if len(str(value)) > col_widthes[id]:
                col_widthes[id] = len(str(value))        

    output_file = open(output_filename, 'w')

    # to avoid confusions:
    output_file.write('Only contigs of length >= ' + str(min_contig) + ' were taken into account\n\n');
    # header
    for id, value in enumerate(report_dict['header']):
        output_file.write( ' ' + str(value).center(col_widthes[id]) + ' |')
    output_file.write('\n')

    # metrics values
    for contig_name in sorted(report_dict.keys()):    
        if contig_name == 'header':
            continue
        for id, value in enumerate(report_dict[contig_name]):
            if id == 0:
                output_file.write( ' ' + str(value).ljust(col_widthes[id]) + ' |')
            else:
                output_file.write( ' ' + str(value).rjust(col_widthes[id]) + ' |')
        output_file.write('\n')

    output_file.close()

    print '  Saved to ' + output_filename
    print '  Done'
