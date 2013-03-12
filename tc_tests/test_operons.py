#!/usr/bin/python

import os
from common import *

name = os.path.basename(__file__)[5:-3]


run_quast(name, contigs=[contigs_10k_1], params='-R ' + reference_10k + ' -O ' + operons_10k)
assert_metric(name, '# operons', ['1 + 1 part'])