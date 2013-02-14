#!/usr/bin/python

import sys
import fastaparser

# TODO: rewrite it nicer!
inp=sys.argv[1]
#fasta=fastaparser.read_fasta(inp)
start=4277440
end=4277460
start=31225
end=31252
for tup in fastaparser.read_fasta(inp):
  print "start=", start, " res:", tup[1][start-1:end], "end:", end
#print fasta[1]

