#!/usr/bin/python

from __future__ import with_statement
import os
from common import *

name = os.path.basename(__file__)[5:-3]


run_quast(name, contigs=[meta_contigs_1, meta_contigs_2, 'meta_contigs_2_with_ns.fasta'], params=''
          '-R meta_ref_1.fasta,meta_ref_2.fasta --glimmer -l "lab1,lab2,lab3"  '
          '--split-scaffolds --unique-mapping --fragmented --memory-efficient --no-html --silent --max-ref-numb 3 --min-identity 95', utility='metaquast')

check_report_files(name, ['summary/TXT/NGA50.txt',
                          'combined_reference/report.pdf',
                          'runs_per_reference/meta_ref_2/predicted_genes/lab3_broken_glimmer_genes.gff'])

assert_metric(name, '# contigs (>= 1000 bp)', ['9', '7', '7', '10'], 'combined_reference/report.tsv')
assert_metric(name, '# unaligned contigs', ['10 + 0 part', '7 + 0 part', '7 + 0 part', '7 + 0 part'], 'combined_reference/report.tsv')
