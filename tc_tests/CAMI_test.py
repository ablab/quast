#!/usr/bin/python

from __future__ import with_statement
import os
from common import *

name = os.path.basename(__file__)[:-3]

run_quast(name, contigs=['/Johnny/data/contigs/PAPERS/MetaQUAST/CAMI/CAMI/Gold_Assembly.fasta',
                         '/Johnny/data/contigs/PAPERS/MetaQUAST/CAMI/CAMI/SPAdes.fasta'],
                         params=' -t 4 --no-plots ', utility='metaquast')

check_report_files(name, ['icarus.html',
                          'icarus_viewers/contig_size_viewer.html',
                          'icarus_viewers/Thermus_sp._CCB_US3_UF1.html',
                          'krona_charts/summary_taxonomy_chart.html',
                          'report.html',
                          'runs_per_reference/Brevibacterium_casei/report.tsv'])

assert_metric(name, 'N50', ['24752', '16577'], 'combined_reference/report.tsv')
assert_metric_comparison(name, 'Reference length', '>=', value='70000000', fname='combined_reference/report.tsv')
assert_metric_comparison(name, 'Weissella_koreensis_KACC_15510', '>=', value='1.0', fname='summary/TSV/Duplication_ratio.tsv')
assert_metric_comparison(name, 'Weissella_koreensis_KACC_15510', '<=', value='1.1', fname='summary/TSV/Duplication_ratio.tsv')