#!/usr/bin/python

import os
from common import *

name = os.path.basename(__file__)[5:-3]


run_quast(name, contigs=[contigs_10k_2], params='--min-contig 10')
assert_metric(name, '# contigs', ['4'])

run_quast(name, contigs=[contigs_10k_2], params='--min-contig 1000')
assert_metric(name, '# contigs', ['2'])

run_quast(name, contigs=[contigs_10k_2], params='--min-contig 10000', expected_exit_code=4)