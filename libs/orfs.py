############################################################################
# Copyright (c) 2011-2012 Saint-Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################

import os
import sys
import itertools
import fastaparser
from qutils import id_to_str

def do(filenames, orf_length):

    print 'Running ORF tool (for length %d)...' % (orf_length)

    ########################################################################
    report_dict = {'header' : []}
    for filename in filenames:
        report_dict[os.path.basename(filename)] = []

    # http://www.ncbi.nlm.nih.gov/Taxonomy/Utils/wprintgc.cgi#SG11
    # 11. The Bacterial, Archaeal and Plant Plastid Code (transl_table=11)
    AAs    = 'FFLLSSSSYY**CC*WLLLLPPPPHHQQRRRRIIIMTTTTNNKKSSRRVVVVAAAADDEEGGGG'
    Starts = '---M---------------X------------XXXM---------------M------------' # Ms are starts, Xs are deprecated (rare) starts
    Base1  = 'TTTTTTTTTTTTTTTTCCCCCCCCCCCCCCCCAAAAAAAAAAAAAAAAGGGGGGGGGGGGGGGG'
    Base2  = 'TTTTCCCCAAAAGGGGTTTTCCCCAAAAGGGGTTTTCCCCAAAAGGGGTTTTCCCCAAAAGGGG'
    Base3  = 'TCAGTCAGTCAGTCAGTCAGTCAGTCAGTCAGTCAGTCAGTCAGTCAGTCAGTCAGTCAGTCAG'

    # precalc of start/stop codons based on transl_table
    starts = set()
    stops = set()
    for i in xrange(64):
	    if AAs[i] == '*':
		    stops.add((Base1[i], Base2[i], Base3[i]))
	    if Starts[i] == 'M':
		    starts.add((Base1[i], Base2[i], Base3[i]))

    def find_ORFs(genome, start, min):
	    gene = False # no gene
	    l = 0 # length of ORF
	    orf_seq = '' # sequence of ORF
	    orfs = set() # all found ORFs
	    for a, b, c in itertools.izip(genome[start::3], genome[start+1::3], genome[start+2::3]):
		    l += 1
		    if not gene and (a, b, c) in starts:
			    gene = True # gene
			    orf_seq = ''
			    l = 1
		    elif gene and (a, b, c) in stops:
			    gene = False
			    if min <= l:
				    orfs.add(orf_seq)
		    if gene:
				orf_seq += a + b + c
	    return orfs

    def reverse_complement(s):
	    return s.upper().translate('*****************************************************************TVGHEFCDIJMLKNOPQYSAUBWXRZ[\]^_`tvghefcdijmlknopqysaubwxrz*************************************************************************************************************************************')[::-1]

    report_dict['header'].append('# ORFs >= %dbp' % (orf_length * 3))
    for id, fasta_filename in enumerate(filenames):
        print ' ', id_to_str(id), os.path.basename(fasta_filename), '...'

        fasta = fastaparser.read_fasta(fasta_filename)
        cnt = 0
        orfs = set()
        for name, seq in fasta:
	        seq = seq.upper()
	        orfs.update(find_ORFs(seq, 0, orf_length))
	        orfs.update(find_ORFs(seq, 1, orf_length))
	        orfs.update(find_ORFs(seq, 2, orf_length))
	        rc_seq = reverse_complement(seq)
	        orfs.update(find_ORFs(rc_seq, 0, orf_length))
	        orfs.update(find_ORFs(rc_seq, 1, orf_length))
	        orfs.update(find_ORFs(rc_seq, 2, orf_length))
        cnt = len(orfs)
        report_dict[os.path.basename(fasta_filename)].append(cnt)

    return report_dict

