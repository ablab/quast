#!/usr/bin/python

import os
from common import *

name = os.path.basename(__file__)[5:-3]

run_quast(name, contigs=[contigs_10k_1], params='--mgm -R ' + reference_10k)
assert_metric(name, '# predicted genes (unique)')
