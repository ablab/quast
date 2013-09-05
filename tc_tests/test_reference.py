#!/usr/bin/python

import os
from common import run_quast, contigs_1k_2, assert_metric

name = os.path.basename(__file__)[5:-3]

run_quast(name, contigs=[contigs_1k_2], params='-R reference_1k.fa')
assert_metric(name, 'NGA50', ['760'])

run_quast(name, contigs=[contigs_1k_2], params='-R reference_1k.fa.gz')
assert_metric(name, 'NGA50', ['760'])
