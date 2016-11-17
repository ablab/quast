#!/usr/bin/python

from __future__ import with_statement
import os
from common import *

name = os.path.basename(__file__)[5:-3]


run_quast(name, contigs=[contigs_1k_1, contigs_1k_2], params=' -R reference_1k.fa.gz -L -s -m 100 '
                         '-G genes.ncbi -O operons.gff -t 3 -f --gene-thresholds 100,500 '
                         '--est-ref-size 100000 --gage --contig-thresholds 0,10,10000 '
                         '-u -i 10 -a all --strict-NA -x 200 --unaligned-part-size 100 '
                         '--fragmented --plots-format png --no-snps --no-gc --silent')

check_report_files(name, ['gage_report.txt',
                          'report.tsv',
                          'basic_stats/Nx_plot.png'])

assert_metric(name, 'Total length (>= 10 bp)', ['1000', '760'], 'report.tsv')
assert_metric(name, '# predicted genes (>= 100 bp)', ['1', '1'], 'report.tsv')
assert_metric(name, 'Genome fraction (%)', ['100.000', '76.000'], 'report.tsv')
