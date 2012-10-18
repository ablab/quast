############################################################################
# Copyright (c) 2011-2012 Saint-Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################
from __future__ import with_statement
from collections import namedtuple

import os
import re
import shutil
from string import maketrans
import subprocess
import tempfile
import operator

from libs import reporting
from libs.fasta_parser import reading
from libs.gc_content import gc_content
from qutils import id_to_str

gmhmmp_path = ''
genemark_dir = ''
heu_lib = ''

def genemarkhmm_p(in_file_path, heu_path, out_file_path, err_file_header):
    """ Run GeneMark.hmm with this heuristic model (heu_path)
        prompt> gmhmmp -m heu_11_45.mod sequence
        prompt> gm -m heu_11_45.mat sequence"""
    global gmhmmp_path
    param = [gmhmmp_path, '-d', '-p', '0', '-m', heu_path, '-o', out_file_path, in_file_path]
    subprocess.call(param, stdout=err_file_header, stderr=err_file_header)
    return False if not os.path.isfile(out_file_path) else True


def install_genemark():
    """ Installation instructions for GeneMark Suite
        Please, copy key "gm_key" into users home directory as:
        cp gm_key ~/.gm_key
        (genemark_suite_linux_XX/gmsuite/INSTALL)"""

    global genemark_dir
    gm_key_path = os.path.join(genemark_dir, 'gm_key')

    if not os.path.isfile(os.path.expanduser('~/.gm_key')):
        shutil.copyfile(gm_key_path, os.path.expanduser('~/.gm_key')) # GeneMark needs this key to work

    return

def reverse_complement(seq):
    return seq[::-1].translate(maketrans('ACGT', 'TGCA'))

Gene = namedtuple('Gene', ['contig_id', 'strand', 'left_index', 'right_index', 'seq'])
def parse_gmhmm_out(file):
    genes = []
    reading_gene = False
    with open(file) as fin:
        for line in fin:
            if line.startswith('FASTA'):
                contig_id = line.strip().replace('FASTA definition line: ', '')
            if line.startswith('>gene'):
                reading_gene = True
                seq = []
                seq_id = line.strip()
                _, _, _, strand, left_index, right_index, _ = re.split(r'[|  \t]+', seq_id)
            elif reading_gene:
                if line.isspace():
                    reading_gene = False
                    str_seq = ''.join(seq)
                    left_index = int(left_index)
                    right_index = int(right_index)
                    genes.append(Gene(contig_id, strand, left_index, right_index, str_seq))
                seq.append(line.strip())
    return genes

def add_genes_to_gff(genes, gff_header):
    ID = 1
    for gene in genes:
        length = gene.left_index - gene.right_index
        for_print = ID, length, gene.strand, gene.left_index, gene.right_index, gene.contig_id
        gene_id = '>gene_%d|GeneMark.hmm|%d_nt|%s|%d|%d|%s'%for_print
        gff = gene_id, gene.left_index, gene.right_index, gene.strand, ID
        gff_header.write('%s       .      gene    %d %d .     %s       .       ID=%d\n'%gff)
        ID += 1
    return

def add_genes_to_fasta(genes, fasta_header):
    ID = 1
    for gene in genes:
        length = gene.right_index - gene.left_index
        for_print = ID, length, gene.strand, gene.left_index, gene.right_index, gene.contig_id
        gene_id = '>gene_%d|GeneMark.hmm|%d_nt|%s|%d|%d|%s'%for_print
        ID += 1
        fasta_header.write(gene_id + '\n')
        indices = xrange(1, length, 60)
        if gene.strand == '-':
            seq = reverse_complement(gene.seq)
            fasta_seq = '\n'.join(seq[l:r] for l, r in zip(indices, indices))
        else:
            fasta_seq = '\n'.join(gene.seq[l:r] for l, r in zip(indices, indices))
        fasta_header.write(fasta_seq + '\n\n')
    return

def genemarkhmm_p_everyGC(in_file_path, out_file_name, gene_lengths):
    out_gff_path = out_file_name + '.gff'
    out_fasta_path = out_file_name + '_genes.fasta'
    global heu_lib
    work_dir = tempfile.mkdtemp()
    fasta_file = open(in_file_path)
    for seq_id, seq in reading(fasta_file):
        gc = gc_content(seq)
        gc = min(70, max(30, gc))
        curr_filename = str(int(gc / 5) * 5) + '.fasta'
        curr_path = os.path.join(work_dir, curr_filename)
        with open(curr_path, 'a') as curr_out:
            curr_out.write(seq_id + '\n' + seq)
    genes = []
    for _ , _, filenames in os.walk(work_dir):
        pass
    for file_name in filenames:
        file_path = os.path.join(work_dir, file_name)
        file_out = file_path + '.genemark'
        gc_str = file_name.replace('.fasta', '')
        heu_path = os.path.join(heu_dir, 'heu_11_' + gc_str + '.mod')
        with open('genemark.errors', 'a') as err_file:
            if genemarkhmm_p(file_path, heu_path, file_out, err_file):
                genes.extend(parse_gmhmm_out(file_out))
    shutil.rmtree(work_dir)
    with open(out_gff_path, 'w') as gff_out:
        gff_out.write('##gff out for GeneMark.hmm PROKARYOTIC\n'
                      '##Sequence file name: %s'%in_file_path)
        add_genes_to_gff(genes, gff_out)
    with open(out_fasta_path, 'w') as fasta_out:
        fasta_out.write('##fasta out for GeneMark.hmm PROKARYOTIC\n'
                        '##Sequence file name: %s'%in_file_path)
        add_genes_to_fasta(genes, fasta_out)

    cnt = [sum([gene.right_index - gene.left_index > x for gene in genes])for x in gene_lengths]
    unique_count = len(set(map(operator.attrgetter('seq'), genes)))
    total_count = len(genes)

    return out_gff_path, out_fasta_path, unique_count, total_count, cnt

def run(filenames, gene_lengths, output_dir, lib_dir): # what is genes_lengths?

    print 'Running GeneMark tool...'
    global genemark_dir, gmhmmp_path, heu_dir

    if not os.path.isdir(output_dir):
        os.mkdir(output_dir)

    import struct
    if struct.calcsize("P") * 8 == 64:
        genemark_dir  = os.path.join(lib_dir, 'genemark_suite_linux_64/gmsuite')
    else:
        genemark_dir  = os.path.join(lib_dir, 'genemark_suite_linux_32/gmsuite')

    install_genemark()

    gmhmmp_path = os.path.join(genemark_dir, 'gmhmmp')
    heu_dir = os.path.join(genemark_dir, 'heuristic_mod')

    for id, filename in enumerate(filenames):
        report = reporting.get(filename)

        print ' ', id_to_str(id), os.path.basename(filename),

        out_name = os.path.basename(filename)
        out_path_name = os.path.join(output_dir, out_name)
        out_gff_path, out_fasta_path, unique, total, cnt = genemarkhmm_p_everyGC(filename, out_path_name, gene_lengths)

        print ', Genes =', unique, 'unique,', total, 'total'
        print '    GeneMark output: %s and %s'%(out_gff_path, out_fasta_path)

        report.add_field(reporting.Fields.GENEMARKUNIQUE, unique)
        report.add_field(reporting.Fields.GENEMARK, cnt)
        print cnt

        print '  Done'
    return