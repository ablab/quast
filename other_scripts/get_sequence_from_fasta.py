#!/usr/bin/python

import sys
import os
sys.path.append(os.path.join(os.path.abspath(sys.path[0]), '../libs'))
import fastaparser

# TODO: rewrite it nicer!
# TODO: reverse compliment!
inp=sys.argv[1]
#fasta=fastaparser.read_fasta(inp)
start=4277440
end=4277460
start=1
end=3610546
for tup in fastaparser.read_fasta(inp):
  #print "start=", start, " res:", tup[1][start-1:end], "end:", end
#print fasta[1]
    print ">scaff0"
    print fastaparser.rev_comp(tup[1][start-1:end])

