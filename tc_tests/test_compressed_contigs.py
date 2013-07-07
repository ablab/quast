#!/usr/bin/python

import os
from common import *

name = os.path.basename(__file__)[5:-3]

gz_contigs = [contigs_1, contigs_1 + '.gz']
run_quast(name + '_gz', contigs=gz_contigs)
check_report_files(name + '_gz')
assert_report_header(name + '_gz', gz_contigs)
assert_values_equal(name + '_gz')
print ''
print ''

zip_contigs = [contigs_1, contigs_1 + '.zip']
run_quast(name + '_zip', contigs=zip_contigs, expected_exit_code=20)
print ''
print ''

# values = get_metric_values(name, '# contigs (>= 0 bp)')
# if values[0] != values[1]:
#     print 'compressed and uncompressed values differ'