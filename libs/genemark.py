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


def gmhmm_p(tool_exec, fasta_fpath, heu_fpath, out_fpath, err_file):
    """ Run GeneMark.hmm with this heuristic model (heu_dirpath)
        prompt> gmhmmp -m heu_11_45.mod sequence
        prompt> gm -m heu_11_45.mat sequence"""
    param = [tool_exec, '-d', '-p', '0', '-m', heu_fpath, '-o', out_fpath, fasta_fpath]
    subprocess.call(param, stdout=err_file, stderr=err_file)
    return os.path.isfile(out_fpath)


def install_genemark(tool_dirpath):
    """Installation instructions for GeneMark Suite.

    Please, copy key "gm_key" into users home directory as:
    cp gm_key ~/.gm_key
    (genemark_suite_linux_XX/gmsuite/INSTALL)
    """
    gm_key_path = os.path.join(tool_dirpath, 'gm_key')
    if not os.path.isfile(os.path.expanduser('~/.gm_key')):
        # GeneMark needs this key to work.
        shutil.copyfile(gm_key_path, os.path.expanduser('~/.gm_key'))


#Gene = namedtuple('Gene', ['contig_id', 'strand', 'left_index', 'right_index', 'seq'])
def parse_gmhmm_out(out_fpath):
    reading_gene = False
    with open(out_fpath) as f:
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


def gmhmm_p_everyGC(tool_dirpath, fasta_fpath, out_fname, gene_lengths, err_fpath, tmp_dirpath):
    tool_exec_fpath = os.path.join(tool_dirpath, 'gmhmmp')
    heu_dirpath = os.path.join(tool_dirpath, 'heuristic_mod')

    out_gff_fpath = out_fname + '_genes.gff'
    # out_fasta_path = out_name + '_genes.fasta'

    tmp_dirname = tempfile.mkdtemp(dir=tmp_dirpath)
    for id, seq in read_fasta(fasta_fpath):
        gc = min(70, max(30, gc_content(seq)))
        gc = gc - gc % 5  # rounds to a divisible by 5
        current_fname = str(gc) + '.fasta'
        current_fpath = os.path.join(tmp_dirname, current_fname)
        with open(current_fpath, 'a') as current_file:
            current_file.write('>' + id + '\n' + seq + '\n')

    genes = []
    _, _, fnames = os.walk(tmp_dirname).next()
    for fname in fnames:
        fpath = os.path.join(tmp_dirname, fname)
        gmhmm_fpath = fpath + '.gmhmm'
        gc_str, ext = os.path.splitext(fname)
        heu_fpath = os.path.join(heu_dirpath, 'heu_11_' + gc_str + '.mod')
        with open(err_fpath, 'a') as err_file:
            if gmhmm_p(tool_exec_fpath, fpath, heu_fpath, gmhmm_fpath, err_file):
                genes.extend(parse_gmhmm_out(gmhmm_fpath))

    if not qconfig.debug:
        shutil.rmtree(tmp_dirname)

    add_genes_to_gff(genes, out_gff_fpath)
    #add_genes_to_fasta(genes, out_fasta_path)

    cnt = [sum([gene[3] - gene[2] > x for gene in genes]) for x in gene_lengths]
    unique_count = len(set([gene[4] for gene in genes]))
    total_count = len(genes)

    #return out_gff_fpath, out_fasta_path, unique_count, total_count, cnt
    return out_gff_fpath, unique_count, total_count, cnt


def predict_genes(id, fasta_fpath, gene_lengths, out_dirpath, tool_dirpath, tmp_dirpath):
    log = logging.getLogger('quast')
    log.info('  ' + id_to_str(id) + os.path.basename(fasta_fpath))

    out_fname = os.path.basename(fasta_fpath)
    out_fpath = os.path.join(out_dirpath, out_fname)
    err_fpath = os.path.join(out_dirpath, out_fname + '_genemark.stderr')
    # out_gff_fpath, out_fasta_path, unique, total, cnt = gmhmm_p_everyGC(tool_dirpath,
    #     fasta_fpath, out_fpath, gene_lengths, err_fpath)

    if qconfig.meta:
        tool_exec_fpath = os.path.join(tool_dirpath, 'gmhmmp')
        heu_fpath = os.path.join(tool_dirpath, 'MetaGeneMark_v1.mod')
        gmhmm_fpath = fasta_fpath + '.gmhmm'
        out_gff_fpath = out_fname + '_genes.gff'

        with open(err_fpath, 'a') as err_file:
            ok = gmhmm_p(tool_exec_fpath, fasta_fpath, heu_fpath, gmhmm_fpath, err_file)
            if not ok:
                pass  # TODO
            else:
                genes = dict(parse_gmhmm_out(gmhmm_fpath))
                add_genes_to_gff(genes, out_gff_fpath)

                cnt = [sum([gene[3] - gene[2] > x for gene in genes]) for x in gene_lengths]
                unique_count = len(set([gene[4] for gene in genes]))
                total_count = len(genes)

                return out_gff_fpath, unique_count, total_count, cnt

    else:
        out_gff_fpath, unique, total, cnt = gmhmm_p_everyGC(
            tool_dirpath, fasta_fpath, out_fpath, gene_lengths, err_fpath, tmp_dirpath)

    log.info('  ' + id_to_str(id) + '  Genes = ' + str(unique) + ' unique, ' + str(total) + ' total')
    log.info('  ' + id_to_str(id) + '  Predicted genes (GFF): ' + out_gff_fpath)

    log.info('  ' + id_to_str(id) + 'Gene prediction is finished.')
    return unique, cnt


def do(fasta_fpaths, gene_lengths, out_dirpath):
    print_timestamp()
    log = logging.getLogger('quast')

    tool_name = 'MetaGeneMark' if qconfig.meta else 'GeneMark'
    tool_dirname = 'metagenemark' if qconfig.meta else 'genemark'

    log.info('Running %s tool...' % tool_name)

    tmp_dirpath = os.path.join(out_dirpath, 'tmp')
    if not os.path.isdir(out_dirpath):
        os.mkdir(out_dirpath)
    if not os.path.isdir(tmp_dirpath):
        os.mkdir(tmp_dirpath)

    tool_dirpath = os.path.join(qconfig.LIBS_LOCATION, tool_dirname, qconfig.platform_name)
    if not os.path.exists(tool_dirpath):
        log.warning('  Sorry, can\'t use %s on this platform, skipping gene prediction.' % tool_name)

    else:
        install_genemark(tool_dirpath)

        n_jobs = min(len(fasta_fpaths), qconfig.max_threads)
        from joblib import Parallel, delayed
        results = Parallel(n_jobs=n_jobs)(delayed(predict_genes)(id, fasta_fpath, gene_lengths, out_dirpath, tool_dirpath, tmp_dirpath)
            for id, fasta_fpath in enumerate(fasta_fpaths))

        # saving results
        for id, fasta_path in enumerate(fasta_fpaths):
            report = reporting.get(fasta_path)
            unique, cnt = results[id]
            report.add_field(reporting.Fields.PREDICTED_GENES_UNIQUE, unique)
            report.add_field(reporting.Fields.PREDICTED_GENES, cnt)

        if not qconfig.debug:
            shutil.rmtree(tmp_dirpath)
        log.info('  Done.')