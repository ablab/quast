#!/usr/bin/python

import os
from common import *

name = os.path.basename(__file__)[5:-3]
incorrect_chars_contigs = 'incorrect_chars_in_sequence.fasta'
only_ns_in_conitgs = 'only_Ns_in_sequence.fasta'


run_quast(name, contigs=[contigs_1k_1, incorrect_chars_contigs], expected_exit_code=4)
