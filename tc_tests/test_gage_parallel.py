#!/usr/bin/python

from __future__ import with_statement
import os
from common import *

name = os.path.basename(__file__)[5:-3]


run_quast(name, contigs=[contigs_1_1k, contigs_2_1k], params='--gage -R ' + reference_1k + ' --threads 2')

check_report_files(name, ['gage_report.txt',
                          'gage_report.tsv',
                          'gage_report.tex',
                          'gage_transposed_report.tex',
                          'gage_transposed_report.txt',
                          'gage_transposed_report.tsv'])

assert_metric(name, 'Assembly size', ['760'], 'gage_report.tsv')
