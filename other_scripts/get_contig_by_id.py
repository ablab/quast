#!/usr/bin/python

# Get contigs with specified IDs from provided Fasta file and print them to stdout

import sys
import os

sys.path.append(os.path.join(os.path.abspath(sys.path[0]), '../libs'))

import fastaparser

# MAIN
if len(sys.argv) != 3:
	print("Usage: " + sys.argv[0] + " <input fasta> <contig id or file with list of contig ids>")	
	sys.exit()

if os.path.isfile(sys.argv[2]):
    list_of_ids = []
    for line in open(sys.argv[2]):
        list_of_ids.append(line.strip())
else:
    list_of_ids = [sys.argv[2]]

dict_of_all_contigs = dict(fastaparser.read_fasta(sys.argv[1]))
selected_contigs = []

for name in list_of_ids:
    if name in dict_of_all_contigs:
        selected_contigs.append((name, dict_of_all_contigs[name]))
    else:
        print >> sys.stderr, "Contig", name, "not found!"

for (name, seq) in selected_contigs:
    print '>' + name
    print seq    
