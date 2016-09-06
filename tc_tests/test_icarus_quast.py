#!/usr/bin/python

import os
from common import *

name = os.path.basename(__file__)[5:-3]
contigs = [contigs_1k_1, contigs_1k_2]


run_quast(name, contigs=contigs, params='-R reference_1k.fa', utility='icarus')
check_report_files(name)
assert_report_header(name, contigs=contigs)
assert_metric(name, 'N50', ['1000', '760'])
