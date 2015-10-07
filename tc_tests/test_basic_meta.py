#!/usr/bin/python

import os
from common import *

name = os.path.basename(__file__)[5:-3]
contigs = [meta_contigs_1, meta_contigs_2]


run_quast(name, contigs=contigs, params='-R ' + ','.join(meta_references), meta=True)
meta_res_dirpath = os.path.join(name, 'combined_reference')
check_report_files(meta_res_dirpath)
assert_report_header(meta_res_dirpath, contigs=contigs)
assert_metric(meta_res_dirpath, 'N50', ['49658', '49658'])
