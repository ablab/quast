#!/usr/bin/python

import os
from common import run_quast, contigs_2_1k, assert_metric

name = os.path.basename(__file__)[5:-3]

run_quast(name, contigs=[contigs_2_1k], params='-R reference_1k.fa')
assert_metric(name, 'NGA50', ['1360'])

run_quast(name, contigs=[contigs_2_1k], params='-R reference_1k.fa.gz')
assert_metric(name, 'NGA50', ['1360'])
