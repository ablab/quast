############################################################################
# Copyright (c) 2011-2012 Saint-Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################

import os
import subprocess
from libs import report_maker
from qutils import id_to_str

def do(reference, contigs, output_dirpath, total_report_basename, min_contig, lib_dir):
    gage_results_path = os.path.join(output_dirpath, 'gage')

    # suffixes for files with report tables in plain text and tab separated formats
    if not os.path.isdir(gage_results_path):
        os.mkdir(gage_results_path)

    ########################################################################
    gage_tool_path = os.path.join(lib_dir, 'gage/getCorrectnessStats.sh')

    ########################################################################

    # dict with the main metrics (for total report)
    report_dict = {'header': ['Assembly']}
    for contig in contigs:
        report_dict[os.path.basename(contig)] = [os.path.basename(contig)]

    print 'Running GAGE tool...'
    metrics = ['Total units', 'Min', 'Max', 'N50', 'Genome Size', 'Assembly Size', 'Chaff bases',
               'Missing Reference Bases', 'Missing Assembly Bases', 'Missing Assembly Contigs',
               'Duplicated Reference Bases', 'Compressed Reference Bases', 'Bad Trim', 'Avg Idy', 'SNPs', 'Indels < 5bp',
               'Indels >= 5', 'Inversions', 'Relocation', 'Translocation',
               'Total units', 'BasesInFasta', 'Min', 'Max', 'N50']
    metrics_headers = ['Contigs #', 'Min contig', 'Max contig', 'N50', 'Genome size', 'Assembly size', 'Chaff bases',
                       'Missing reference bases', 'Missing assembly bases',
                       'Missing assembly contigs', 'Duplicated reference bases', 'Compressed reference bases',
                       'Bad trim', 'Avg idy', 'SNPs', 'Indels < 5bp', 'Indels >= 5', 'Inversions', 'Relocation', 'Translocation',
                       'Corrected contig #', 'Corrected assembly size', 'Min correct contig', 'Max correct contig',
                       'Corrected N50']

    for metric in metrics_headers:
        report_dict['header'].append(metric)

    tmp_dir = gage_results_path + '/tmp/'
    for id, filename in enumerate(contigs):
        print ' ', id_to_str(id), os.path.basename(filename), '...'
        # run gage tool
        logfilename_out = gage_results_path + '/gage_' + os.path.basename(filename) + '.stdout'
        logfilename_err = gage_results_path + '/gage_' + os.path.basename(filename) + '.stderr'
        print '    Logging to files', logfilename_out, 'and', os.path.basename(logfilename_err), '...',
        logfile_out = open(logfilename_out, 'w')
        logfile_err = open(logfilename_err, 'w')

        subprocess.call(
            ['sh', gage_tool_path, reference, filename, tmp_dir, str(min_contig)],
            stdout=logfile_out, stderr=logfile_err)

        logfile_out.close()
        logfile_err.close()
        print '  Done.'

        ## find metrics for total report:
        logfile_out = open(logfilename_out, 'r')
        cur_metric_id = 0
        for line in logfile_out:
            if metrics[cur_metric_id] in line:
                if (metrics[cur_metric_id].startswith('N50')):
                    report_dict[os.path.basename(filename)].append(line.split(metrics[cur_metric_id] + ':')[1].strip())
                else:
                    report_dict[os.path.basename(filename)].append(line.split(':')[1].strip())
                cur_metric_id += 1
                if cur_metric_id == len(metrics):
                    break
        logfile_out.close()

    print '  Done'

    report_maker.do(report_dict, total_report_basename, output_dirpath, min_contig)
  ##########################################################################
  # print '  Creating total report...'
  # total_report = total_report_basename + txt_ext
  # total_report_tab = total_report_basename + tsv_ext
  # tr_file = open(total_report, 'w')
  # tab_file = open(total_report_tab, 'w')
  #
  # # calculate columns widthes
  # col_widthes = [0 for i in range(len(report_dict['header']))]
  # for row in report_dict.keys():
  #     for id, value in enumerate(report_dict[row]):
  #         if len(str(value)) > col_widthes[id]:
  #             col_widthes[id] = len(str(value))
  #
  #             # to avoid confusions:
  # tr_file.write('Only contigs of length >= ' + str(min_contig) + ' were taken into account\n\n')
  # # header
  # for id, value in enumerate(report_dict['header']):
  #     tr_file.write(' ' + str(value).center(col_widthes[id]) + ' |')
  #     if id:
  #         tab_file.write('\t')
  #     tab_file.write(value)
  # tr_file.write('\n')
  # tab_file.write('\n')
  #
  # # metrics values
  # for contig_name in sorted(report_dict.keys()):
  #     if contig_name == 'header':
  #         continue
  #     for id, value in enumerate(report_dict[contig_name]):
  #         if id:
  #             tr_file.write(' ' + str(value).rjust(col_widthes[id]) + ' |')
  #             tab_file.write('\t')
  #         else:
  #             tr_file.write(' ' + str(value).ljust(col_widthes[id]) + ' |')
  #         tab_file.write(str(value))
  #     tr_file.write('\n')
  #     tab_file.write('\n')
  #
  # tr_file.close()
  # tab_file.close()
  # print '    Saved to', total_report, 'and', total_report_tab
  ##########################################################################

    print '  Done.'
