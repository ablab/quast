#!/usr/bin/python

import os
from common import *

name = os.path.basename(__file__)[5:-3]
contigs = ['contigs_10k_1.fasta']


run_quast(name, contigs=contigs, params='--gene-finding')
assert_metric(name, '# predicted genes (unique)', ['8'])

run_quast(name, contigs=contigs, params='--gene-finding --eukaryote --glimmer')
assert_metric(name, '# predicted genes (unique)', ['7'])