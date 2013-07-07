#!/usr/bin/python

import os
from common import *

name = os.path.basename(__file__)[5:-3]

gz_contigs = [contigs_1, contigs_1 + '.gz']
run_quast(name, contigs=gz_contigs)
check_report_files(name)
assert_report_header(name, gz_contigs)
assert_values_equal(name)
print ''
print ''

zip_contigs = [contigs_1, contigs_1 + '.zip']
run_quast(name, contigs=zip_contigs)
check_report_files(name)
assert_report_header(name, zip_contigs)
assert_values_equal(name)
print ''
print ''

# values = get_metric_values(name, '# contigs (>= 0 bp)')
# if values[0] != values[1]:
#     print 'compressed and uncompressed values differ'