#!/usr/bin/python

import os
from common import *

name = os.path.basename(__file__)[5:-3]
contigs = [meta_contigs_1, meta_contigs_2]


run_quast(name, contigs=contigs, params='--fast --references-list ' + 'example_ref_list.txt', utility='metaquast')

meta_res_dirpath = os.path.join(name, 'combined_reference')
psittaci_dirpath = os.path.join(name, 'runs_per_reference', 'Lactobacillus_psittaci')
reuteri_dirpath = os.path.join(name, 'runs_per_reference', 'Lactobacillus__reuteri_DSM_20016')

check_report_files(meta_res_dirpath, fast=True)
check_report_files(psittaci_dirpath, fast=True)
check_report_files(reuteri_dirpath, fast=True)

assert_report_header(meta_res_dirpath, contigs=contigs)
assert_metric_comparison(name, 'Reference length', '>=', value='3000000', fname='combined_reference/report.tsv')
assert_metric_comparison(name, 'Reference length', '<=', value='9000000', fname='combined_reference/report.tsv')
