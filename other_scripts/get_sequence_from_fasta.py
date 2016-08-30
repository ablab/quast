#!/usr/bin/python

import sys
import os
sys.path.append(os.path.join(os.path.abspath(sys.path[0]), '../'))
import quast_libs
from quast_libs import fastaparser

if len(sys.argv) <= 3 or len(sys.argv) >= 6:
    print("Returns [reverse-complement] sequence from START to END position from each entry of input fasta")
    print("Usage: " + sys.argv[0] + " <input fasta> <START> <END, -1 for the end> [any string -- optional parameter for reverse-complement]")
    sys.exit()

inp=sys.argv[1]
start=int(sys.argv[2])
end=int(sys.argv[3])
reverse = False
if len(sys.argv) == 5:
  reverse = True

for tup in fastaparser.read_fasta(inp):
    cur_start = min(start, len(tup[1]))
    if end == -1:
        cur_end = len(tup[1])
    else:
        cur_end = min(end, len(tup[1]))    
    print (">" + tup[0] + "_cropped_" + str(cur_start) + "_" + str(cur_end))
    if reverse:
        print (fastaparser.rev_comp(tup[1][cur_start - 1 : cur_end]))
    else:
        print (tup[1][cur_start - 1 : cur_end])    

