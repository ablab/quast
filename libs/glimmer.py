############################################################################
# Copyright (c) 2015 Saint Petersburg State University
# Copyright (c) 2011-2015 Saint Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################

from __future__ import with_statement
import os
import tempfile
import itertools
import csv
import shutil
import re

from libs import reporting, qconfig, qutils
from libs.fastaparser import read_fasta, write_fasta, rev_comp

from libs.log import get_logger
logger = get_logger(qconfig.LOGGER_DEFAULT_NAME)

OUTPUT_FASTA = False # whether output only .gff or with corresponding .fasta files

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
        for index, _source, type, start, end, score, strand, phase, extra in r:
            if type != 'mRNA':
                continue  # We're only interested in genes here.

            attrs = dict(kv.split("=") for kv in extra.split(";"))
            yield index, attrs.get('Name'), int(start), int(end), strand


def glimmerHMM(tool_dir, fasta_fpath, out_fpath, gene_lengths, err_path, tmp_dir, index):
    def run(contig_path, tmp_path):
        with open(err_path, 'a') as err_file:
            return_code = qutils.call_subprocess(
                [tool_exec, contig_path, '-d', trained_dir, '-g', '-o', tmp_path],
                stdout=err_file,
                stderr=err_file,
                indent='  ' + qutils.index_to_str(index) + '  ')
            return return_code

    tool_exec = os.path.join(tool_dir, 'glimmerhmm')

    # Note: why arabidopsis? for no particular reason, really.
    trained_dir = os.path.join(tool_dir, 'trained', 'arabidopsis')

    contigs = {}
    gffs = []
    base_dir = tempfile.mkdtemp(dir=tmp_dir)
    for ind, seq in read_fasta(fasta_fpath):
        ind = re.sub('[/. ]', '_', ind)
        contig_path = os.path.join(base_dir, ind + '.fasta')
        gff_path = os.path.join(base_dir, ind + '.gff')

        write_fasta(contig_path, [(ind, seq)])
        if run(contig_path, gff_path) == 0:
            gffs.append(gff_path)
            contigs[ind] = seq

    if not gffs:
        logger.error(
            'Glimmer failed running Glimmer for %s. ' + ('Run with the --debug option'
            ' to see the command line.' if not qconfig.debug else '') % qutils.label_from_fpath(fasta_fpath))
        return None, None, None, None

    out_gff_path = merge_gffs(gffs, out_fpath + '_genes.gff')
    unique, total = set(), 0
    genes = []
    cnt = [0] * len(gene_lengths)
    for contig, gene_id, start, end, strand in parse_gff(out_gff_path):
        total += 1
        if strand == '+':
            gene_seq = contigs[contig][start:end + 1]
        else:
            gene_seq = rev_comp(contigs[contig][start:end + 1])
        if gene_seq not in unique:
            unique.add(gene_seq)
        genes.append((gene_id, gene_seq))
        for idx, gene_length in enumerate(gene_lengths):
            cnt[idx] += end - start > gene_length

    if OUTPUT_FASTA:
        out_fasta_path = out_fpath + '_genes.fasta'
        write_fasta(out_fasta_path, genes)
    if not qconfig.debug:
        shutil.rmtree(base_dir)

    #return out_gff_path, out_fasta_path, len(unique), total, cnt
    return out_gff_path, len(unique), total, cnt


def predict_genes(index, contigs_fpath, gene_lengths, out_dirpath, tool_dirpath, tmp_dirpath):
    assembly_label = qutils.label_from_fpath_for_fname(contigs_fpath)

    logger.info('  ' + qutils.index_to_str(index) + assembly_label)

    out_fpath = os.path.join(out_dirpath, assembly_label + '_glimmer')
    err_fpath = os.path.join(out_dirpath, assembly_label + '_glimmer.stderr')

    #out_gff_path, out_fasta_path, unique, total, cnt = glimmerHMM(tool_dir,
    #    fasta_path, out_path, gene_lengths, err_path)

    out_gff_path, unique, total, cnt = glimmerHMM(tool_dirpath,
        contigs_fpath, out_fpath, gene_lengths, err_fpath, tmp_dirpath, index)

    if out_gff_path:
        logger.info('  ' + qutils.index_to_str(index) + '  Genes = ' + str(unique) + ' unique, ' + str(total) + ' total')
        logger.info('  ' + qutils.index_to_str(index) + '  Predicted genes (GFF): ' + out_gff_path)

    return unique, cnt


def do(contigs_fpaths, gene_lengths, out_dirpath):
    logger.print_timestamp()
    logger.info('Running GlimmerHMM...')

    tool_dirpath = os.path.join(qconfig.LIBS_LOCATION, 'glimmer')
    tool_src_dirpath = os.path.join(tool_dirpath, 'src')
    tool_exec_fpath = os.path.join(tool_dirpath, 'glimmerhmm')
    tmp_dirpath = os.path.join(out_dirpath, 'tmp')

    if not os.path.isfile(tool_exec_fpath):
        # making
        logger.info("Compiling GlimmerHMM...")
        return_code = qutils.call_subprocess(
            ['make', '-C', tool_src_dirpath],
            stdout=open(os.path.join(tool_src_dirpath, 'make.log'), 'w'),
            stderr=open(os.path.join(tool_src_dirpath, 'make.err'), 'w'),
            indent='    ')
        if return_code != 0 or not os.path.isfile(tool_exec_fpath):
            logger.error("Failed to compile GlimmerHMM (" + tool_src_dirpath +
                         ")!\nTry to compile it manually or do not use --gene-finding "
                         "option with --eukaryote.\nUse --debug option to see the command lines.")
            return

    if not os.path.isdir(out_dirpath):
        os.makedirs(out_dirpath)
    if not os.path.isdir(tmp_dirpath):
        os.makedirs(tmp_dirpath)

    n_jobs = min(len(contigs_fpaths), qconfig.max_threads)
    from joblib import Parallel, delayed
    results = Parallel(n_jobs=n_jobs)(delayed(predict_genes)(
        index, contigs_fpath, gene_lengths, out_dirpath, tool_dirpath, tmp_dirpath)
        for index, contigs_fpath in enumerate(contigs_fpaths))

    # saving results
    for i, contigs_fpath in enumerate(contigs_fpaths):
        report = reporting.get(contigs_fpath)
        unique, cnt = results[i]
        if unique is not None:
            report.add_field(reporting.Fields.PREDICTED_GENES_UNIQUE, unique)
        if cnt is not None:
            report.add_field(reporting.Fields.PREDICTED_GENES, cnt)

    if not qconfig.debug:
        shutil.rmtree(tmp_dirpath)

    logger.info('Done.')