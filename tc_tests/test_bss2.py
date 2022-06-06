#!/usr/bin/python

from __future__ import with_statement
import os
from common import *

name = os.path.basename(__file__)[5:-3]

# bss for Best Set Selection
run_quast(name, contigs=['scaffolds_with_many_repeats.fa.gz'], params=' -R ref_with_many_repeats.fa.gz --fast')

assert_metric(name, '# misassemblies', ['5'], 'report.tsv')
assert_metric(name, '# scaffold gap ext. mis.', ['12'], 'report.tsv')
assert_metric(name, '# scaffold gap loc. mis.', ['172'], 'report.tsv')
assert_metric(name, '# local misassemblies', ['4'], 'report.tsv')
assert_metric(name, '# misassembled contigs', ['4'], 'report.tsv')
