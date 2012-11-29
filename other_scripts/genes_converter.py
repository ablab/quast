#!/usr/bin/python

# Converter from http://genome.ucsc.edu/cgi-bin/hgTables format to plain TXT format
#chrom	strand	txStart	txEnd

import sys
import os
import re

# MAIN
if len(sys.argv) != 3:
	print("Usage: " + sys.argv[0] + " <input genes> <output genes>")	
	sys.exit()

out = open(sys.argv[2], 'w')

i = 1
prev_pos1 = None
prev_pos2 = None
for line in open(sys.argv[1]):
    if line.startswith('#'):
        continue
        
    ref, strand, pos1, pos2 = line.split()
    if prev_pos1 == pos1 and prev_pos2 == pos2:
        continue

    if strand == '+':
        out.write(ref + '\t' + str(i) + '\t' + pos1 + '\t' + pos2 + '\n')
    else:
        out.write(ref + '\t' + str(i) + '\t' + pos2 + '\t' + pos1 + '\n')

    prev_pos1 = pos1
    prev_pos2 = pos2
    i += 1

out.close()
