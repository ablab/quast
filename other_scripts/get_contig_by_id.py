#!/usr/bin/python

# Convert contigs (i.e a reference) for experiment of running SPAdes on E. coli MC reads in "IonTorrent" mode
# (all series of repeated nucleotides are changed to single nucleotides).

import sys
import os
import fastaparser

# MAIN
if len(sys.argv) < 3:
	print("Usage: " + sys.argv[0] + " <input fasta> <contig id>")	
	sys.exit()

new_fasta = []
for name, seq in fastaparser.read_fasta(sys.argv[1]): 
    if name == sys.argv[2]:
        print '>' + name
        print seq
        break
