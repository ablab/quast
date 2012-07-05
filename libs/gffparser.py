############################################################################
# Copyright (c) 2011-2012 Saint-Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################

import os
import re

def get_genes_from_file(filename, keyword):
    if not filename or not os.path.exists(filename):
        print '  Warning! ' + keyword + '\'s file not specified or doesn\'t exist!'
        return []

    ext = os.path.splitext(filename)[1]
    genes = []
    genes_file = open(filename, 'r')

    if ext == '.txt':

        line = genes_file.readline().rstrip()
        while not line:
            line = genes_file.readline().rstrip()

        if line.startswith("gi|"):
            # __deprecated__ parsing TXT

            # EXAMPLE:
            # gi|48994873|gb|U00096.2|	1	4263805	4264884
            # gi|48994873|gb|U00096.2|	2	795085	795774

            genes_file.seek(0)

            for line in genes_file:
                sections = line.split()
                if len(sections) != 4:
                    continue
                    #if sections[0] != reference_name:
                #    continue
                s = int(sections[2])
                e = int(sections[3])
                genes.append([min(s,e), max(s,e)])

        else:
            # NCBI format.

            # EXAMLE:
            # 1. Phep_1459
            # heparinase II/III family protein[Pedobacter heparinus DSM 2366]
            # Other Aliases: Phep_1459
            # Genomic context: Chromosome
            # Annotation: NC_013061.1 (1733715..1735595, complement)
            # ID: 8252560

            genes_file.seek(0)

            for line in genes_file:
                line = line.rstrip()
                if line.startswith("Annotation"):
                    m = re.match(r'Annotation: (?P<id>\S+) \((?P<start>\d+)\.\.(?P<end>\d+)\)', line)
                    if not m:
                        m = re.match(r'Annotation: (?P<id>\S+) \((?P<end>\d+)\.\.(?P<start>\d+), complement\)', line)
                    if m:
                        genes.append([m.group('start'), m.group('end')])

    elif ext.startswith('.gff'):
        # parsing GFF

        # EXAMPLE:
        #    ##gff-version   3
        #    ##sequence-region   ctg123 1 1497228       
        #    ctg123 . gene            1000  9000  .  +  .  ID=gene00001;Name=EDEN
        #    ctg123 . TF_binding_site 1000  1012  .  +  .  ID=tfbs00001;Parent=gene00001

        for line in genes_file:
            splitted = line.split('\t')
            if line.startswith('##') or len(splitted) < 8:  # it is comment line or incorrect line
                continue
            if splitted[2] == keyword:
                genes.append([int(splitted[3]), int(splitted[4])])

    else:
        print '  Warning! Incorrect format of ' + format + '\'s file! Specify file in GFF or TXT format!'
        print '    ' + filename + ' skipped'

    genes_file.close()
    return genes
