############################################################################
# Copyright (c) 2011-2013 Saint-Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################

from __future__ import with_statement
import logging
import os
import tempfile
import subprocess
import itertools
import csv
import shutil

from libs import reporting
from libs import qconfig
from libs.fastaparser import read_fasta, write_fasta, rev_comp
from qutils import id_to_str, print_timestamp, error

def merge_gffs(gffs, out_path):
    '''Merges all GFF files into a single one, dropping GFF header.'''
    with open(out_path, 'w') as out_file:
        out_file.write('##gff-version 3\n')
        for gff_path in gffs:
            with open(gff_path, 'r') as gff_file:
                out_file.writelines(itertools.islice(gff_file, 2, None))

    return out_path


def parse_gff(gff_path):
    with open(gff_path) as gff_file:
        r = csv.reader(
            itertools.ifilter(lambda l: not l.startswith("#"), gff_file),
            delimiter='\t')
        for id, _source, type, start, end, score, strand, phase, extra in r:
            if type != 'mRNA':
                continue  # We're only interested in genes here.

            attrs = dict(kv.split("=") for kv in extra.split(";"))
            yield id, attrs.get('Name'), int(start), int(end), strand


def glimmerHMM(tool_dir, fasta_path, out_path, gene_lengths, err_path, tmp_dir):
    def run(contig_path, tmp_path):
        with open(err_path, 'a') as err_file:
            p = subprocess.call([tool_exec, contig_path,
                                 '-d', trained_dir,
                                 '-g', '-o', tmp_path],
                stdout=err_file, stderr=err_file)
            assert p is 0

    tool_exec = os.path.join(tool_dir, 'glimmerhmm')

    # Note: why arabidopsis? for no particular reason, really.
    trained_dir = os.path.join(tool_dir, 'trained', 'arabidopsis')

    contigs = {}
    gffs = []
    base_dir = tempfile.mkdtemp(dir=tmp_dir)
    for id, seq in read_fasta(fasta_path):
        contig_path = os.path.join(base_dir, id + '.fasta')
        gff_path = os.path.join(base_dir, id + '.gff')

        write_fasta(contig_path, [(id, seq)])
        run(contig_path, gff_path)
        gffs.append(gff_path)
        contigs[id] = seq

    out_gff_path = merge_gffs(gffs, out_path + '_genes.gff')
    #out_fasta_path = out_path + '_genes.fasta'
    unique, total = set(), 0
    #genes = []
    cnt = [0] * len(gene_lengths)
    for contig, gene_id, start, end, strand in parse_gff(out_gff_path):
        total += 1

        if strand == '+':
            gene_seq = contigs[contig][start:end + 1]
        else:
            gene_seq = rev_comp(contigs[contig][start:end + 1])

        if gene_seq not in unique:
            unique.add(gene_seq)

        #genes.append((gene_id, gene_seq))

        for idx, gene_length in enumerate(gene_lengths):
            cnt[idx] += end - start > gene_length

    #write_fasta(out_fasta_path, genes)
    if not qconfig.debug:
        shutil.rmtree(base_dir)

    #return out_gff_path, out_fasta_path, len(unique), total, cnt
    return out_gff_path, len(unique), total, cnt


def predict_genes(id, fasta_path, gene_lengths, out_dir, tool_dir, tmp_dir):
    log = logging.getLogger('quast')
    log.info('  ' + id_to_str(id) + os.path.basename(fasta_path))

    out_name = os.path.basename(fasta_path)
    out_path = os.path.join(out_dir, out_name)
    err_path = os.path.join(out_dir, out_name + '_glimmer.stderr')
    #out_gff_path, out_fasta_path, unique, total, cnt = glimmerHMM(tool_dir,
    #    fasta_path, out_path, gene_lengths, err_path)
    out_gff_path, unique, total, cnt = glimmerHMM(tool_dir,
        fasta_path, out_path, gene_lengths, err_path, tmp_dir)
    log.info('  ' + id_to_str(id) + '  Genes = ' + str(unique) + ' unique, ' + str(total) + ' total')
    log.info('  ' + id_to_str(id) + '  Predicted genes (GFF): ' + out_gff_path)

    log.info('  ' + id_to_str(id) + 'Gene prediction is finished.')
    return unique, cnt


def do(fasta_paths, gene_lengths, out_dir):
    print_timestamp()
    log = logging.getLogger('quast')
    log.info('Running GlimmerHMM...')

    tool_dir = os.path.join(qconfig.LIBS_LOCATION, 'glimmer')
    tool_src = os.path.join(tool_dir, 'src')
    tool_exec = os.path.join(tool_dir, 'glimmerhmm')
    tmp_dir = os.path.join(out_dir, 'tmp')

    if not os.path.isfile(tool_exec):
        # making
        log.info("Compiling GlimmerHMM...")
        try:
            subprocess.call(
                ['make', '-C', tool_src],
                stdout=open(os.path.join(tool_src, 'make.log'), 'w'), stderr=open(os.path.join(tool_src, 'make.err'), 'w'))
            if not os.path.isfile(tool_exec):
                raise
        except:
            error("Failed to compile GlimmerHMM (" + tool_src + ")! Try to compile it manually or set --disable-gene-finding option!")

    if not os.path.isdir(out_dir):
        os.makedirs(out_dir)
    if not os.path.isdir(tmp_dir):
        os.makedirs(tmp_dir)

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