#!/usr/bin/python

from __future__ import with_statement
import os
from common import *

name = os.path.basename(__file__)[5:-3]

# bss for Best Set Selection
run_quast(name, contigs=['ecoli_ctg_repeats.fasta.gz'], params=' -R ecoli_ref_repeats.fasta.gz --fast')

assert_metric(name, '# misassemblies', ['2'], 'report.tsv')
assert_metric(name, '# misassembled contigs', ['1'], 'report.tsv')
assert_metric(name, '# local misassemblies', ['14'], 'report.tsv')
