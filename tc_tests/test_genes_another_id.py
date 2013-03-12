#!/usr/bin/python

import os
from common import run_quast, contigs_2_1k, reference_1k, assert_metric

name = os.path.basename(__file__)[5:-3]


run_quast(name, contigs=[contigs_2_1k], params='-R ' + reference_1k + ' -G genes_1k_verbose__id_does_not_match.gff')
assert_metric(name, '# genes', ['1 + 1 part'])
