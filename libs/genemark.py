############################################################################
# Copyright (c) 2011-2012 Saint-Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################

import os
import sys
import shutil
import subprocess
from libs import reporting
from qutils import id_to_str
import platform

def do(filenames, genes_lengths, output_dir, lib_dir):
    print 'Running GeneMark tool...'

    if not os.path.isdir(output_dir):
        os.mkdir(output_dir)

    import struct
    if struct.calcsize("P") * 8 == 64:
        genemark_path  = os.path.join(lib_dir, 'genemark_suite_linux_64/gmsuite')
    else:
        genemark_path  = os.path.join(lib_dir, 'genemark_suite_linux_32/gmsuite')

    gm_key_path = os.path.join(genemark_path, 'gm_key')
    probuild_path = os.path.join(genemark_path, 'probuild')
    gmhmmp_path = os.path.join(genemark_path, 'gmhmmp')
    heuristic_mod_dir = os.path.join(genemark_path, 'heuristic_mod')

    if not os.path.isfile(os.path.expanduser('~/.gm_key')):
        shutil.copyfile(gm_key_path, os.path.expanduser('~/.gm_key')) # GeneMark needs this key to work

    for id, filename in enumerate(filenames):
        report = reporting.get(filename)
        cnt = [0] * len(genes_lengths)
        print ' ', id_to_str(id), os.path.basename(filename),

        #Step 1: Find G+C composition of sequence, for example:
        #prompt> probuild --gc --seq  sequence
        #prompt> GC% = 45.2

        gc_content_filename = output_dir + '/' + os.path.basename(filename) + '.gc'
        gc_content = open(gc_content_filename, 'w')
        try:
            subprocess.call([probuild_path, '--gc', '--seq', filename], stdout=gc_content, stderr=gc_content)
        except EnvironmentError, ee:
            print "\nEnvironment Error occurs: ", ee
            continue

        gc_content.close()

        gc_content = open(gc_content_filename)
        gc = int(round(float(gc_content.read().split()[2]))) # GC% = 45.2
        print 'GC =', gc,
        gc_content.close()

        #Step 2: Select a heuristic model generated for a sequence with given (or close) G+C content
        #heu_11_45.mod
        if gc < 30:
            gc = 30
        if gc > 70:
            gc = 70
        heu_filename = os.path.join(heuristic_mod_dir, 'heu_11_' + str(gc) + '.mod')

        #Step 3: Run GeneMark.hmm (or GeneMark) with this heuristic model
        #prompt> gmhmmp -m heu_11_45.mod sequence
        #prompt> gm -m heu_11_45.mat sequence

        genemark_out_filename = output_dir + '/' + os.path.basename(filename) + '.genemark'
        genemark_errors = open(output_dir + '/' + os.path.basename(filename) + '.errors', 'w')
        try:
            subprocess.call([gmhmmp_path, '-d', '-p', '0', '-m', heu_filename, '-o', genemark_out_filename, filename],
                            stdout=genemark_errors, stderr=genemark_errors)
        except EnvironmentError, ee:
            print "\nEnvironment Error occurs: ", ee
            continue
        genemark_errors.close()

        max_gene_id = 0
        genes = set()
        ingene = False
        geneseq = ''
        for line in open(genemark_out_filename):
            if line[0] == '>':
                ingene = True
                geneseq = ''
                max_gene_id += 1
                a = line.split('|')
                curlen = int(a[2][:-3]) # 933_nt
                for i in xrange(len(genes_lengths)):
                    if curlen >= genes_lengths[i]:
                        cnt[i] += 1
                continue
            if ingene:
                if not line.strip():
                    ingene = False
                    genes.add(geneseq)
                    continue
                geneseq += line.strip()

        print ', Genes =', len(genes), 'unique,', max_gene_id, 'total'
        print '    GeneMark output', genemark_out_filename
        report.add_field(reporting.Fields.GENEMARKUNIQUE, len(genes))
        report.add_field(reporting.Fields.GENEMARK, cnt)

        print '  Done'
