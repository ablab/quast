#!/usr/bin/python

import os
from common import *

name = os.path.basename(__file__)[5:-3]
contigs = ['saureus/ABySS.fasta.gz', 'saureus/E+V-SC.fasta.gz', 'saureus/EULER-SR.fasta.gz',
           'saureus/IDBA.fasta.gz', 'saureus/SOAPdenovo2.fasta.gz', 'saureus/SPAdes.fasta.gz']

run_quast(name, contigs=contigs, params='-R saureus/reference.fa.gz --fast -G saureus/genes.txt -t 4')
assert_report_header(name, contigs=contigs)
assert_metric_comparison(name, '# contigs (>= 1000 bp)', '<=', '# contigs')
assert_metric_comparison(name, '# contigs', '<=', '# contigs (>= 0 bp)')

assert_metric_comparison(name, 'NA50', '<=', 'N50')
assert_metric_comparison(name, 'NGA50', '<=', 'NG50')
assert_metric_comparison(name, 'Largest alignment', '<=', 'Largest contig')
assert_metric_comparison(name, 'Total aligned length', '<=', 'Total length')

assert_metric_comparison(name, '# misassembled contigs', '<=', '# misassemblies')

assert_metric_comparison(name, 'Genome fraction (%)', '<=', value='100.0')
assert_metric_comparison(name, '# genomic features', '<=', value='2622')