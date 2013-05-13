#!/usr/bin/python

# Get contigs with specified IDs from provided Fasta file and print them to stdout

import sys
import os

sys.path.append(os.path.join(os.path.abspath(sys.path[0]), '../libs'))

import qconfig, qutils
import fastaparser

def get_corr_name(name):
    return qutils.correct_name(name)
    # return re.sub(r'\W', '', re.sub(r'\s', '_', name))

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

origin_fasta = fastaparser.read_fasta(sys.argv[1])
dict_of_all_contigs = dict()
selected_contigs = []
for (name, seq) in origin_fasta:
    corr_name = get_corr_name(name)
    dict_of_all_contigs[corr_name] = seq

for name in list_of_ids:
    corr_name = get_corr_name(name)
    if corr_name in dict_of_all_contigs:
        selected_contigs.append((name, dict_of_all_contigs[corr_name]))
    else:
        print >> sys.stderr, "Contig", name, "(cor name:", corr_name, ") not found!"

for (name, seq) in selected_contigs:
    print '>' + name
    print seq    
