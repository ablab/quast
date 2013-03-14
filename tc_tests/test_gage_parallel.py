#!/usr/bin/python

from __future__ import with_statement
import os
from common import *

name = os.path.basename(__file__)[5:-3]


run_quast(name, contigs=[contigs_1k_1, contigs_1k_2], params='--gage -R reference_1k.fa.gz --threads 2')

check_report_files(name, ['gage_report.txt',
                          'gage_report.tsv',
                          'gage_report.tex',
                          'gage_transposed_report.tex',
                          'gage_transposed_report.txt',
                          'gage_transposed_report.tsv'])

assert_metric(name, 'Assembly size', ['1000', '760'], 'gage_report.tsv')
