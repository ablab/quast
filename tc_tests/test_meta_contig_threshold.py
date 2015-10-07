#!/usr/bin/python

import os
from common import *

name = os.path.basename(__file__)[5:-3]
contigs = [meta_contigs_1, meta_contigs_2]


run_quast(name, contigs=contigs, params='-m 1000 -R ' + ','.join(meta_references), meta=True)
meta_combined_dirpath = os.path.join(name, combined_output_name)
check_report_files(meta_combined_dirpath)
assert_report_header(meta_combined_dirpath, contigs=contigs)
assert_metric(meta_combined_dirpath, '# contigs', ['9', '7'])
meta_ref1_dirpath = os.path.join(name, per_ref_dirname, 'meta_ref_1')
check_report_files(meta_ref1_dirpath)
assert_report_header(meta_ref1_dirpath, contigs=contigs)
assert_metric(meta_ref1_dirpath, '# contigs', ['1', '1'])
