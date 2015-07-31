#!/usr/bin/python

############################################################################
# Copyright (c) 2015 Saint Petersburg State University
# Copyright (c) 2011-2015 Saint Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################


import sys
sys.path.append(os.path.join(os.path.abspath(sys.path[0]), '../'))
import libs
from libs import fastaparser

KMER_SIZE   = 5
KMERS_FNAME = "kmers.fa"
REF_MARGINS = 300
REF_FNAME   = "ref.fa"

if len(sys.argv) != 4:
    print "Usage:", sys.argv[0], "reference pos1 pos2"
    sys.exit(0)

pos1 = int(sys.argv[2])
pos2 = int(sys.argv[3])

if pos1 > pos2:
    pos = pos1
    pos1 = pos2
    pos2 = pos

reference = fastaparser.read_fasta(sys.argv[1])[0][1]  # Returns list of FASTA entries (in tuples: name, seq)
if len(reference) < pos2:
    pos2 = len(reference)

ref_file = open(REF_FNAME, 'w')
ref_file.write(">reference\n")
ref_file.write(reference[max(0, pos1 - 1 - REF_MARGINS) : min(len(reference), pos2 + REF_MARGINS)] + "\n")
ref_file.close()

misassembled_site = reference[pos1 - 1 : pos2]
kmers = set()

i = pos1 - 1
while i + KMER_SIZE <= pos2:
    kmers.add(reference[i : i + KMER_SIZE])
    i += 1

kmers_file = open(KMERS_FNAME, 'w')
for id, kmer in enumerate(kmers):
    kmers_file.write(">kmer_" + str(id) + "\n")
    kmers_file.write(kmer + "\n")
kmers_file.close()

#print "from ", pos1, "to", pos2, reference[pos1 - 1 : pos2]
#print "kmers:", kmers
