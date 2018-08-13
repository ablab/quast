#!/usr/bin/python

import os
from common import *

name = os.path.basename(__file__)[5:-3]
contigs = [contigs_10k_1, 'contigs_tiny.fasta']


# Glimmer
run_quast(name, contigs=contigs, params='--fast -m 0 --glimmer')
check_report_files(name, fast=True)
assert_report_header(name, contigs=contigs)
# should find nothing in contigs_tiny (but not crash)
assert_metric(name, '# predicted genes (unique)', ['7', '0'])
assert_metric(name, '# predicted genes (>= 0 bp)', ['6 + 1 part', '0 + 0 part'])

# GeneMarkS
run_quast(name, contigs=contigs, params='--fast -m 0 -f')
check_report_files(name, fast=True)
assert_report_header(name, contigs=contigs)
# should crash on contigs_tiny but this crash should be properly handled
assert_metric(name, '# predicted genes (unique)', ['8', '-'])
assert_metric(name, '# predicted genes (>= 0 bp)', ['7 + 1 part', '-'])

# GeneMark-ES
run_quast(name, contigs=contigs, params='--fast -m 0 -f --euk')
check_report_files(name, fast=True)
# should fail to find anything due to small (not eukaryotic) size of both
assert_report_header(name, contigs=contigs)
assert_metric(name, '# predicted genes (unique)', absent=True)
assert_metric(name, '# predicted genes (>= 0 bp)', absent=True)

# MetaGeneMark
run_quast(name, contigs=contigs, params='--fast -m 0 --mgm')
check_report_files(name, fast=True)
assert_report_header(name, contigs=contigs)
# should crash on contigs_tiny but this crash should be properly handled
assert_metric(name, '# predicted genes (unique)', ['9', '-'])
assert_metric(name, '# predicted genes (>= 0 bp)', ['8 + 1 part', '-'])