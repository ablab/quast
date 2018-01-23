#!/usr/bin/python

from __future__ import with_statement
import os
from common import *

name = os.path.basename(__file__)[:-3]

run_quast(name, contigs=['/Johnny/data/contigs/C.elegans/PROBABLE_CONTIGS/abyss.fa',
                         '/Johnny/data/contigs/C.elegans/PROBABLE_CONTIGS/sga.fa',
                         '/Johnny/data/contigs/C.elegans/PROBABLE_CONTIGS/soapdenovo.fa',
                         '/Johnny/data/contigs/C.elegans/PROBABLE_CONTIGS/velvet.fa',
                         '/Johnny/data/contigs/C.elegans/SPAdes2.4.scaffolds.fasta'],
                 params=' -R /Johnny/data/contigs/C.elegans/REF_FILES/reference.fasta '
                        '-t 8 --no-plots --large')

check_report_files(name, ['icarus.html',
                          'genome_stats/genome_info.txt'])

assert_metric(name, '# contigs (>= 50000 bp)', ['274', '219', '263', '148', '243'], 'report.tsv')
assert_metric(name, '# misassemblies', ['95', '19', '22', '100', '183'], 'report.tsv')
assert_metric(name, 'Largest alignment', ['202579', '187587', '147763', '168091', '172159'], 'report.tsv')
