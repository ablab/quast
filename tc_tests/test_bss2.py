#!/usr/bin/python

from __future__ import with_statement
import os
from common import *

name = os.path.basename(__file__)[5:-3]

# bss for Best Set Selection
run_quast(name, contigs=['scaffolds_with_many_repeats.fa.gz'], params=' -R ref_with_many_repeats.fa.gz --fast')

assert_metric(name, '# misassemblies', ['18'], 'report.tsv')
assert_metric(name, '# misassembled contigs', ['10'], 'report.tsv')
assert_metric(name, '# local misassemblies', ['182'], 'report.tsv')
