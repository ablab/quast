#!/usr/bin/python

import os
from common import *

name = os.path.basename(__file__)[5:-3]
incorrect_chars_contigs = 'incorrect_chars_in_sequence.fasta'
only_ns_in_conitgs = 'only_Ns_in_sequence.fasta'


run_quast(name, contigs=[only_ns_in_conitgs], params='-R reference.fa')
check_report_files(name)
assert_report_header(name, [only_ns_in_conitgs])