############################################################################
# Copyright (c) 2015-2016 Saint Petersburg State University
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

from quast_libs import reporting, qconfig, qutils
from quast_libs.ca_utils.misc import open_gzipsafe
from quast_libs.fastaparser import read_fasta, write_fasta, rev_comp
from quast_libs.genemark import add_genes_to_fasta
from quast_libs.genes_parser import Gene

from quast_libs.log import get_logger
from quast_libs.qutils import is_python2

logger = get_logger(qconfig.LOGGER_DEFAULT_NAME)

OUTPUT_FASTA = False # whether output only .gff or with corresponding .fasta files


def merge_gffs(gffs, out_path):
    '''Merges all GFF files into a single one, dropping GFF header.'''
    out_file = open_gzipsafe(out_path, 'w')
    out_file.write('##gff-version 3\n')
    for gff_path in gffs:
        with open(gff_path) as gff_file:
            out_file.writelines(itertools.islice(gff_file, 2, None))
    out_file.close()
    return out_path


def parse_gff(gff_path):
    gff_file = open_gzipsafe(gff_path)
    r = csv.reader(list(filter(lambda l: not l.startswith("#"), gff_file)),
        delimiter='\t')
    for index, _source, type, start, end, score, strand, phase, extra in r:
        if type != 'mRNA':
            continue  # We're only interested in genes here.

        attrs = dict(kv.split("=") for kv in extra.split(";"))
        yield index, attrs.get('Name'), int(start), int(end), strand
    gff_file.close()


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
    for seq_num, (ind, seq) in enumerate(read_fasta(fasta_fpath)):
        seq_num = str(seq_num)
        ind = ind[:qutils.MAX_CONTIG_NAME_GLIMMER]
        contig_path = os.path.join(base_dir, seq_num + '.fasta')
        gff_path = os.path.join(base_dir, seq_num + '.gff')

        write_fasta(contig_path, [(ind, seq)])
        if run(contig_path, gff_path) == 0:
            gffs.append(gff_path)
            contigs[ind] = seq

    if not gffs:
        return None, None, None, None

    out_gff_fpath = out_fpath + '_genes.gff' + ('.gz' if not qconfig.no_gzip else '')
    out_gff_path = merge_gffs(gffs, out_gff_fpath)
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
        genes.append(Gene(contig=contig, start=start, end=end, strand=strand, seq=gene_seq))
        for idx, gene_length in enumerate(gene_lengths):
            cnt[idx] += end - start > gene_length

    if OUTPUT_FASTA:
        out_fasta_fpath = out_fpath + '_genes.fasta'
        add_genes_to_fasta(genes, out_fasta_fpath)
    if not qconfig.debug:
        shutil.rmtree(base_dir)

    #return out_gff_path, out_fasta_path, len(unique), total, cnt
    return out_gff_path, genes, len(unique), total, cnt


def predict_genes(index, contigs_fpath, gene_lengths, out_dirpath, tool_dirpath, tmp_dirpath):
    assembly_label = qutils.label_from_fpath(contigs_fpath)
    corr_assembly_label = qutils.label_from_fpath_for_fname(contigs_fpath)

    logger.info('  ' + qutils.index_to_str(index) + assembly_label)

    out_fpath = os.path.join(out_dirpath, corr_assembly_label + '_glimmer')
    err_fpath = os.path.join(out_dirpath, corr_assembly_label + '_glimmer.stderr')

    #out_gff_path, out_fasta_path, unique, total, cnt = glimmerHMM(tool_dir,
    #    fasta_path, out_path, gene_lengths, err_path)

    out_gff_path, genes, unique, total, cnt = glimmerHMM(tool_dirpath,
        contigs_fpath, out_fpath, gene_lengths, err_fpath, tmp_dirpath, index)

    if out_gff_path:
        logger.info('  ' + qutils.index_to_str(index) + '  Genes = ' + str(unique) + ' unique, ' + str(total) + ' total')
        logger.info('  ' + qutils.index_to_str(index) + '  Predicted genes (GFF): ' + out_gff_path)

    return genes, unique, cnt


def compile_glimmer(logger, only_clean=False):
    tool_dirpath = os.path.join(qconfig.LIBS_LOCATION, 'glimmer')
    tool_src_dirpath = os.path.join(tool_dirpath, 'src')
    tool_exec_fpath = os.path.join(tool_dirpath, 'glimmerhmm')

    if only_clean:
        if os.path.isfile(tool_exec_fpath):
            os.remove(tool_exec_fpath)
        return True

    if not os.path.isfile(tool_exec_fpath):
        logger.main_info("Compiling GlimmerHMM...")
        return_code = qutils.call_subprocess(
            ['make', '-C', tool_src_dirpath],
            stdout=open(os.path.join(tool_src_dirpath, 'make.log'), 'w'),
            stderr=open(os.path.join(tool_src_dirpath, 'make.err'), 'w'),
            indent='    ')
        if return_code != 0 or not os.path.isfile(tool_exec_fpath):
            logger.error("Failed to compile GlimmerHMM (" + tool_src_dirpath +
                         ")!\nTry to compile it manually or do not use --gene-finding "
                         "option with --eukaryote.\nUse --debug option to see the command lines.")
            return None
    return tool_exec_fpath


def do(contigs_fpaths, gene_lengths, out_dirpath):
    logger.print_timestamp()
    logger.main_info('Running GlimmerHMM...')

    tool_dirpath = os.path.join(qconfig.LIBS_LOCATION, 'glimmer')
    tmp_dirpath = os.path.join(out_dirpath, 'tmp')
    tool_exec_fpath = compile_glimmer(logger)
    if not tool_exec_fpath:
        return

    if not os.path.isdir(out_dirpath):
        os.makedirs(out_dirpath)
    if not os.path.isdir(tmp_dirpath):
        os.makedirs(tmp_dirpath)

    n_jobs = min(len(contigs_fpaths), qconfig.max_threads)
    if is_python2():
        from joblib import Parallel, delayed
    else:
        from joblib3 import Parallel, delayed
    results = Parallel(n_jobs=n_jobs)(delayed(predict_genes)(
        index, contigs_fpath, gene_lengths, out_dirpath, tool_dirpath, tmp_dirpath)
        for index, contigs_fpath in enumerate(contigs_fpaths))

    genes_by_labels = dict()
    # saving results
    for i, contigs_fpath in enumerate(contigs_fpaths):
        report = reporting.get(contigs_fpath)
        label = qutils.label_from_fpath(contigs_fpath)
        genes_by_labels[label], unique, cnt = results[i]
        if unique is not None:
            report.add_field(reporting.Fields.PREDICTED_GENES_UNIQUE, unique)
        if cnt is not None:
            report.add_field(reporting.Fields.PREDICTED_GENES, cnt)
        if unique is None and cnt is None:
            logger.error(
                'Glimmer failed running Glimmer for %s. ' + ('Run with the --debug option'
                ' to see the command line.' if not qconfig.debug else '') % label)

    if not qconfig.debug:
        shutil.rmtree(tmp_dirpath)

    logger.main_info('Done.')
    return genes_by_labels