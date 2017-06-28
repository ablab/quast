#!/usr/bin/python

import os
from common import *

name = os.path.basename(__file__)[5:-3]
contigs = ['contigs_100k_1.fasta']


run_quast(name, contigs=contigs, params='--gene-finding --eukaryote')
assert_metric(name, '# predicted genes (unique)', ['776'])