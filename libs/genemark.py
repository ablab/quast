############################################################################
# Copyright (c) 2011-2013 Saint-Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################

from __future__ import with_statement
import logging
import os
import platform
import shutil
import subprocess
import tempfile

from libs import reporting
from libs import qconfig
from libs.fastaparser import read_fasta, write_fasta
from qutils import id_to_str, print_timestamp

def gc_content(sequence):
    GC_count = sequence.count('G') + sequence.count('C')
    ACGT_length = len(sequence) - sequence.count('N')
    if not ACGT_length:
        return 0
    return 100 * GC_count / ACGT_length

def gmhmm_p(tool_exec, fasta_path, heu_path, out_path, err_file):
    """ Run GeneMark.hmm with this heuristic model (heu_path)
        prompt> gmhmmp -m heu_11_45.mod sequence
        prompt> gm -m heu_11_45.mat sequence"""
    param = [tool_exec, '-d', '-p', '0', '-m', heu_path, '-o', out_path, fasta_path]
    subprocess.call(param, stdout=err_file, stderr=err_file)
    return os.path.isfile(out_path)


def install_genemark(tool_dir):
    """Installation instructions for GeneMark Suite.

    Please, copy key "gm_key" into users home directory as:
    cp gm_key ~/.gm_key
    (genemark_suite_linux_XX/gmsuite/INSTALL)
    """
    gm_key_path = os.path.join(tool_dir, 'gm_key')
    if not os.path.isfile(os.path.expanduser('~/.gm_key')):
        # GeneMark needs this key to work.
        shutil.copyfile(gm_key_path, os.path.expanduser('~/.gm_key'))


#Gene = namedtuple('Gene', ['contig_id', 'strand', 'left_index', 'right_index', 'seq'])
def parse_gmhmm_out(out_path):
    reading_gene = False
    with open(out_path) as f:
        for line in f:
            if line.startswith('>gene'):
                reading_gene = True
                seq = []
                seq_id, contig_id = line.strip().split()
                # >gene_2|GeneMark.hmm|57_nt|+|1|57	>NODE_3_length_713_cov_1.25228
                _, _, _, strand, left_index, right_index = seq_id.split('|')
                contig_id = contig_id[1:]
            elif reading_gene:
                if line.isspace():
                    reading_gene = False
                    seq = ''.join(seq)
                    left_index = int(left_index)
                    right_index = int(right_index)
                    #genes.append(Gene(contig_id, strand, left_index, right_index, str_seq))
                    yield contig_id, strand, left_index, right_index, seq
                else:
                    seq.append(line.strip())


def add_genes_to_gff(genes, gff_path):
    gff = open(gff_path, 'w')
    gff.write('##gff out for GeneMark.hmm PROKARYOTIC\n')
    gff.write('##gff-version 3\n')

    for id, gene in enumerate(genes):
        contig_id, strand, left_index, right_index, str_seq = gene
        gff.write('%s\tGeneMark\tgene\t%d\t%d\t.\t%s\t.\tID=%d\n' %
            (contig_id, left_index, right_index, strand, id + 1))
    gff.close()


def add_genes_to_fasta(genes, fasta_path):
    def inner():
        for id, gene in enumerate(genes):
            contig_id, strand, left_index, right_index, gene_fasta = gene
            length = right_index - left_index
            gene_id = '>gene_%d|GeneMark.hmm|%d_nt|%s|%d|%d|%s' % (
                id + 1, length, strand, left_index, right_index, contig_id
            )
            yield gene_id, gene_fasta

    write_fasta(fasta_path, inner())


def gmhmm_p_everyGC(tool_dir, fasta_path, out_name, gene_lengths, err_path, tmp_dir):
    tool_exec = os.path.join(tool_dir, 'gmhmmp')
    heu_dir = os.path.join(tool_dir, 'heuristic_mod')

    out_gff_path = out_name + '_genes.gff'
    #out_fasta_path = out_name + '_genes.fasta'

    work_dir = tempfile.mkdtemp(dir=tmp_dir)
    for id, seq in read_fasta(fasta_path):
        gc = min(70, max(30, gc_content(seq)))
        curr_filename = str(gc - gc % 5) + '.fasta'
        current_path = os.path.join(work_dir, curr_filename)
        with open(current_path, 'a') as curr_out:
            curr_out.write('>' + id + '\n' + seq + '\n')

    genes = []
    _ , _, file_names = os.walk(work_dir).next()
    for file_name in file_names:
        file_path = os.path.join(work_dir, file_name)
        file_out = file_path + '.gmhmm'
        gc_str, ext = os.path.splitext(file_name)
        heu_path = os.path.join(heu_dir, 'heu_11_' + gc_str + '.mod')
        with open(err_path, 'a') as err_file:
            if gmhmm_p(tool_exec, file_path, heu_path, file_out, err_file):
                genes.extend(parse_gmhmm_out(file_out))

    if not qconfig.debug:
        shutil.rmtree(work_dir)

    add_genes_to_gff(genes, out_gff_path)
    #add_genes_to_fasta(genes, out_fasta_path)

    cnt = [sum([gene[3] - gene[2] > x for gene in genes]) for x in gene_lengths]
    unique_count = len(set([gene[4] for gene in genes]))
    total_count = len(genes)

    #return out_gff_path, out_fasta_path, unique_count, total_count, cnt
    return out_gff_path, unique_count, total_count, cnt


def predict_genes(id, fasta_path, gene_lengths, out_dir, tool_dir, tmp_dir):
    log = logging.getLogger('quast')
    log.info('  ' + id_to_str(id) + os.path.basename(fasta_path))

    out_name = os.path.basename(fasta_path)
    out_path = os.path.join(out_dir, out_name)
    err_path = os.path.join(out_dir, out_name + '_genemark.stderr')
    #out_gff_path, out_fasta_path, unique, total, cnt = gmhmm_p_everyGC(tool_dir,
    #    fasta_path, out_path, gene_lengths, err_path)
    out_gff_path, unique, total, cnt = gmhmm_p_everyGC(tool_dir,
        fasta_path, out_path, gene_lengths, err_path, tmp_dir)

    log.info('  ' + id_to_str(id) + '  Genes = ' + str(unique) + ' unique, ' + str(total) + ' total')
    log.info('  ' + id_to_str(id) + '  Predicted genes (GFF): ' + out_gff_path)

    log.info('  ' + id_to_str(id) + 'Gene prediction is finished.')
    return unique, cnt


def do(fasta_paths, gene_lengths, out_dir):
    print_timestamp()
    log = logging.getLogger('quast')
    log.info('Running GeneMark tool...')

    tmp_dir = os.path.join(out_dir, 'tmp')
    if not os.path.isdir(out_dir):
        os.mkdir(out_dir)
    if not os.path.isdir(tmp_dir):
        os.mkdir(tmp_dir)

    if platform.system() == 'Darwin':
        tool_dir = os.path.join(qconfig.LIBS_LOCATION, 'genemark', 'macosx')
    else:
        if platform.architecture()[0] == '64bit':
            tool_dir  = os.path.join(qconfig.LIBS_LOCATION, 'genemark', 'linux_64')
        else:
            tool_dir  = os.path.join(qconfig.LIBS_LOCATION, 'genemark', 'linux_32')

    install_genemark(tool_dir)

    n_jobs = min(len(fasta_paths), qconfig.max_threads)
    from joblib import Parallel, delayed
    results = Parallel(n_jobs=n_jobs)(delayed(predict_genes)(id, fasta_path, gene_lengths, out_dir, tool_dir, tmp_dir)
        for id, fasta_path in enumerate(fasta_paths))

    # saving results
    for id, fasta_path in enumerate(fasta_paths):
        report = reporting.get(fasta_path)
        unique, cnt = results[id]
        report.add_field(reporting.Fields.PREDICTED_GENES_UNIQUE, unique)
        report.add_field(reporting.Fields.PREDICTED_GENES, cnt)

    if not qconfig.debug:
        shutil.rmtree(tmp_dir)
    log.info('  Done.')