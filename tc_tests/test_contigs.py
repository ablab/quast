#!/usr/bin/python

import os
from common import *

name = os.path.basename(__file__)[5:-3]
incorrect_chars_contigs = 'incorrect_chars_in_sequence.fasta'
only_ns_in_conitgs = 'only_Ns_in_sequence.fasta'


run_quast(name, contigs=[incorrect_chars_contigs], expected_exit_code=4)

run_quast(name, contigs=[contigs_1_1k, incorrect_chars_contigs])
check_report_files(name)
assert_report_header(name, [contigs_1_1k])

run_quast(name, contigs=[only_ns_in_conitgs], expected_exit_code=4)

run_quast(name, contigs=[contigs_1_1k, only_ns_in_conitgs])
check_report_files(name)
assert_report_header(name, [contigs_1_1k])