#!/usr/bin/python

from __future__ import with_statement
import os
from common import *

name = os.path.basename(__file__)[:-3]

run_quast(name, contigs=['/Johnny/data/contigs/GAGE/Bumblebee/ABySS/genome.scf.fasta',
                         '/Johnny/data/contigs/GAGE/Bumblebee/CABOG/genome.scf.fasta',
                         '/Johnny/data/contigs/GAGE/Bumblebee/MSR-CA/genome.scf.fasta',
                         '/Johnny/data/contigs/GAGE/Bumblebee/SOAPdenovo/genome.scf.fasta'],
                         params='--est-ref-size 250000000 -t 4')

assert_metric(name, '# contigs (>= 50000 bp)', ['518', '509', '323', '354'], 'report.tsv')
assert_metric(name, 'N50', ['19117', '1017298', '1312286', '1399493'], 'report.tsv')
assert_metric(name, 'NG50', ['18581', '1124853', '1246384', '1374411'], 'report.tsv')