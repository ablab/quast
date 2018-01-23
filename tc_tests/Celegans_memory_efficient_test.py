#!/usr/bin/python

from __future__ import with_statement
import os
from common import *

name = os.path.basename(__file__)[:-3]

run_quast(name, contigs=['/Johnny/data/contigs/C.elegans/PROBABLE_CONTIGS/abyss.fa'],
                params=' -R /Johnny/data/contigs/C.elegans/REF_FILES/reference.fasta '
                       '-t 2 --memory-efficient --no-plots')

check_report_files(name, ['icarus.html',
                          'genome_stats/genome_info.txt'])

assert_metric(name, '# contigs (>= 50000 bp)', ['274'], 'report.tsv')
assert_metric(name, '# misassemblies', ['153'], 'report.tsv')
assert_metric(name, 'Largest alignment', ['202579'], 'report.tsv')
