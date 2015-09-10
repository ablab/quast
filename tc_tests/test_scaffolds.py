#!/usr/bin/python

import os
from common import *

name = os.path.basename(__file__)[5:-3]
contigs = [contigs_10k_1, contigs_10k_1_scaffolds]


run_quast(name, contigs=contigs, params='-s')
check_report_files(name)
contigs += [contigs_10k_1_broken]
assert_report_header(name, contigs=contigs)
assert_metric(name, '# contigs', ['3', '1', '3'])