#!/usr/bin/python

import os
from common import *

name = os.path.basename(__file__)[5:-3]
contigs = [contigs_1k_1, contigs_1k_2]


run_quast(name, contigs=contigs, params='--threads 2')
check_report_files(name)
assert_report_header(name, contigs=contigs)
assert_metric(name, 'N50', ['1000', '760'])

run_quast(name, contigs=contigs, params='--threads 1')
check_report_files(name)
assert_report_header(name, contigs=contigs)
assert_metric(name, 'N50', ['1000', '760'])

run_quast(name, contigs=contigs, params='--threads 0')
check_report_files(name)
assert_report_header(name, contigs=contigs)
assert_metric(name, 'N50', ['1000', '760'])

run_quast(name, contigs=contigs, params='-G ' + genes_1k + ' -O ' + operons_1k + ' -R ' + reference_1k + ' --threads 2')
check_report_files(name)
assert_report_header(name, contigs=contigs)
assert_metric(name, 'N50', ['1000', '760'])
assert_metric(name, 'NGA50', ['1000', '760'])
assert_metric(name, '# genomic features', ['1 + 1 part', '1 + 1 part'])
assert_metric(name, '# operons', ['1 + 0 part', '1 + 0 part'])
