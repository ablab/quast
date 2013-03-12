#!/usr/bin/python

from __future__ import with_statement
import os
from common import *

name = os.path.basename(__file__)[5:-3]


run_quast(name, contigs=[contigs_1_10k], params='--gage -R reference_10k.fa.gz')

check_report_files(name, ['gage_report.txt',
                          'gage_report.tsv',
                          'gage_report.tex',
                          'gage_transposed_report.tex',
                          'gage_transposed_report.txt',
                          'gage_transposed_report.tsv'])

assert_metric(name, 'Assembly size', ['6710'], 'gage_report.tsv')



# gage_dirpath = os.path.join(get_results_dirpath(name), 'gage')
# if not os.path.isdir(gage_dirpath):
#     print 'Gage directory is not found'
#     exit(7)
#
# gage_report_fpath = os.path.join(gage_dirpath, 'gage_' + os.path.splitext(contigs_2)[0] + '.stdout')
# if not os.path.isfile(gage_report_fpath):
#     print 'Gage stdout file is not found'
#     exit(7)

# with open(gage_report_fpath) as f:
#     f.readline()
#     f.readline()
#     line = f.readline()
#     if line != 'Assembly Size: 5460\n':
#         print 'Gage assembly size expected to be 5460, but found %s' % line
#         exit(7)
