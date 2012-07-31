############################################################################
# Copyright (c) 2011-2012 Saint-Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################

### main function ###
import os

def do(report_dict, report_basename, output_dirpath, min_contig=0):
    report_dict = dict([(k, v[1:]) for k, v in report_dict.iteritems()])

    # suffixes for files with transposed and normal report tables
    txt_ext = '.txt'
    tsv_ext = '.tsv'

    # Where total report will be saved
    report_horizontal_path = os.path.join(output_dirpath, 'transposed_' + report_basename)
    report_vertical_path = os.path.join(output_dirpath, report_basename)

    print 'Summarizing...'
    print '  Creating total report...'
    report_txt_filename = report_vertical_path + txt_ext
    report_tsv_filename = report_vertical_path + tsv_ext
    txt_file = open(report_txt_filename, 'w')
    tsv_file = open(report_tsv_filename, 'w')

    # calculate columns widthes
    col_widthes = [0 for i in range(len(report_dict.keys()))]
    header_id = 0
    for id, col in enumerate(sorted(report_dict.keys())):
        if col == 'header':
            header_id = id
        for value in report_dict[col]:
            if len(str(value)) > col_widthes[id]:
                col_widthes[id] = len(str(value))

    # to avoid confusions:
    if min_contig:
        txt_file.write('Only contigs of length >= ' + str(min_contig) + ' were taken into account\n\n')

    # filling
    for i in range(len(report_dict['header'])):
        value = report_dict['header'][i]
        txt_file.write(' ' + str(value).ljust(col_widthes[header_id]) + ' ')
        tsv_file.write(str(value) + '\t')
        for id, contig_name in enumerate(sorted(report_dict.iterkeys())):
            if contig_name == 'header':
                continue
            value = report_dict[contig_name][i]
            txt_file.write(' ' + str(value).ljust(col_widthes[id]) + ' ')
            tsv_file.write(str(value) + '\t')
        txt_file.write('\n')
        tsv_file.write('\n')

    txt_file.close()
    tsv_file.close()
    print '    Saved to', report_txt_filename, 'and', report_tsv_filename

    print '  Transposed version of total report...'
    report_txt_filename = report_horizontal_path + txt_ext
    report_tsv_filename = report_horizontal_path + tsv_ext
    txt_file = open(report_txt_filename, 'w')
    tsv_file = open(report_tsv_filename, 'w')

    # calculate columns widthes
    col_widthes = [0 for i in range(len(report_dict['header']))]
    for row in report_dict.keys():
        for id, value in enumerate(report_dict[row]):
            if len(str(value)) > col_widthes[id]:
                col_widthes[id] = len(str(value))

    # to avoid confusions:
    if min_contig:
        txt_file.write('Only contigs of length >= ' + str(min_contig) + ' were taken into account\n\n')
    # header
    for id, value in enumerate(report_dict['header']):
        txt_file.write(' ' + str(value).center(col_widthes[id]) + ' |')
        if id:
            tsv_file.write('\t')
        tsv_file.write(value)
    txt_file.write('\n')
    tsv_file.write('\n')

    # metrics values
    for contig_name in sorted(report_dict.keys()):
        if contig_name == 'header':
            continue
        for id, value in enumerate(report_dict[contig_name]):
            if id:
                txt_file.write(' ' + str(value).rjust(col_widthes[id]) + ' |')
                tsv_file.write('\t')
            else:
                txt_file.write(' ' + str(value).ljust(col_widthes[id]) + ' |')
            tsv_file.write(str(value))
        txt_file.write('\n')
        tsv_file.write('\n')

    txt_file.close()
    tsv_file.close()
    print '    Saved to', report_txt_filename, 'and', report_tsv_filename

#    if all_pdf != None:
#        print '  Merging all pdfs...'
#        pdfs = ''
#        if glob.glob(output_dir + '/basic_stats/*.pdf'):
#            pdfs += output_dir + '/basic_stats/*.pdf '
#        if glob.glob(output_dir + '/aligned_stats/*.pdf'):
#            pdfs += output_dir + '/aligned_stats/*.pdf '
#        if glob.glob(output_dir + '/mauve/*.pdf'):
#            pdfs += output_dir + '/mauve/*.pdf '
#        if pdfs != '':
#            subprocess.call(['pdftk ' + pdfs  + ' cat output ' + all_pdf], shell=True)
#            print '    Saved to ' + all_pdf
#        else:
#            print '  There are no pdf files!'
#
#    print '  Done.'
