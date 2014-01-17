############################################################################
# Copyright (c) 2011-2014 Saint-Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################

from __future__ import with_statement
import logging
import os
import platform
import shutil
import tempfile

from libs import reporting, qconfig, qutils
from libs.fastaparser import read_fasta, write_fasta

from libs.log import get_logger
logger = get_logger(qconfig.LOGGER_DEFAULT_NAME)

LICENSE_LIMITATIONS_MODE = False
OUTPUT_FASTA = False # whether output only .gff or with corresponding .fasta files

def gc_content(sequence):
    GC_count = sequence.count('G') + sequence.count('C')
    ACGT_length = len(sequence) - sequence.count('N')
    if not ACGT_length:
        return 0
    return 100 * GC_count / ACGT_length


def gmhmm_p(tool_exec, fasta_fpath, heu_fpath, out_fpath, err_file, index):
    """ Run GeneMark.hmm with this heuristic model (heu_dirpath)
        prompt> gmhmmp -m heu_11_45.mod sequence
        prompt> gm -m heu_11_45.mat sequence"""
    return_code = qutils.call_subprocess(
        [tool_exec, '-d', '-p', '0', '-m', heu_fpath, '-o', out_fpath, fasta_fpath],
        stdout=err_file,
        stderr=err_file,
        indent='    ' + qutils.index_to_str(index))

    return return_code == 0 and os.path.isfile(out_fpath)


def install_genemark(tool_dirpath):
    """Installation instructions for GeneMark Suite.

    Please, copy key "gm_key" into users home directory as:
    cp gm_key ~/.gm_key
    (genemark_suite_linux_XX/gmsuite/INSTALL)
    """
    import subprocess
    import filecmp
    gm_key_fpath = os.path.join(tool_dirpath, 'gm_key')
    gm_key_dst = os.path.expanduser('~/.gm_key')
    if not os.path.isfile(gm_key_dst) or \
        (not filecmp.cmp(gm_key_dst, gm_key_fpath) and os.path.getmtime(gm_key_dst) < os.path.getmtime(gm_key_fpath)):
        shutil.copyfile(gm_key_fpath, gm_key_dst)
    # checking the installation
    tool_exec_fpath = os.path.join(tool_dirpath, 'gmhmmp')
    proc = subprocess.Popen([tool_exec_fpath], stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    while not proc.poll():
        line = proc.stdout.readline()
        if line.find('license period has ended') != -1:
            logger.warning('License period for GeneMark has ended! \n' \
                           'To update license, please visit http://topaz.gatech.edu/license_download.cgi page and fill in the form.\n' \
                           'You should choose GeneMarkS tool and your operating system (note that GeneMark is free for non-commercial use).\n' \
                           'Download the license key and replace your ~/.gm_key with the updated version. After that you can restart QUAST.\n')
            return False
    return True


# Gene = namedtuple('Gene', ['contig_id', 'strand', 'left_index', 'right_index', 'seq'])
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


def add_genes_to_gff(genes, gff_fpath):
    gff = open(gff_fpath, 'w')
    gff.write('##gff out for GeneMark.hmm PROKARYOTIC\n')
    gff.write('##gff-version 3\n')

    for id, gene in enumerate(genes):
        contig_id, strand, left_index, right_index, str_seq = gene
        gff.write('%s\tGeneMark\tgene\t%d\t%d\t.\t%s\t.\tID=%d\n' %
            (contig_id, left_index, right_index, strand, id + 1))
    gff.close()


def add_genes_to_fasta(genes, fasta_fpath):
    def inner():
        for i, gene in enumerate(genes):
            contig_id, strand, left_index, right_index, gene_fasta = gene
            length = right_index - left_index
            gene_id = '>gene_%d|GeneMark.hmm|%d_nt|%s|%d|%d|%s' % (
                i + 1, length, strand, left_index, right_index, contig_id
            )
            yield gene_id, gene_fasta

    write_fasta(fasta_fpath, inner())


def gmhmm_p_everyGC(tool_dirpath, fasta_fpath, err_fpath, index, tmp_dirpath):
    tool_exec_fpath = os.path.join(tool_dirpath, 'gmhmmp')
    heu_dirpath = os.path.join(tool_dirpath, 'heuristic_mod')

    tmp_dirpath = tempfile.mkdtemp(dir=tmp_dirpath)
    for ind, seq in read_fasta(fasta_fpath):
        gc = min(70, max(30, gc_content(seq)))
        gc = gc - gc % 5  # rounds to a divisible by 5
        current_fname = str(gc) + '.fasta'
        current_fpath = os.path.join(tmp_dirpath, current_fname)
        with open(current_fpath, 'a') as current_file:
            current_file.write('>' + ind + '\n' + seq + '\n')

    genes = []
    _, _, fnames = os.walk(tmp_dirpath).next()
    for fname in fnames:
        sub_fasta_fpath = os.path.join(tmp_dirpath, fname)
        out_fpath = sub_fasta_fpath + '.gmhmm'

        gc_str, ext = os.path.splitext(fname)
        heu_fpath = os.path.join(heu_dirpath, 'heu_11_' + gc_str + '.mod')
        with open(err_fpath, 'a') as err_file:
            ok = gmhmm_p(tool_exec_fpath, sub_fasta_fpath, heu_fpath,
                       out_fpath, err_file, index)
            if ok:
                genes.extend(parse_gmhmm_out(out_fpath))

    if not qconfig.debug:
        shutil.rmtree(tmp_dirpath)

    return genes


def gmhmm_p_metagenomic(tool_dirpath, fasta_fpath, err_fpath, index, tmp_dirpath=None):
    tool_exec_fpath = os.path.join(tool_dirpath, 'gmhmmp')
    heu_fpath = os.path.join(tool_dirpath, '../MetaGeneMark_v1.mod')
    gmhmm_fpath = fasta_fpath + '.gmhmm'

    with open(err_fpath, 'a') as err_file:
        if gmhmm_p(tool_exec_fpath, fasta_fpath, heu_fpath, gmhmm_fpath, err_file, index):
            return list(parse_gmhmm_out(gmhmm_fpath))
        else:
            return None


def predict_genes(index, contigs_fpath, gene_lengths, out_dirpath, tool_dirpath, tmp_dirpath, gmhmm_p_function):
    assembly_name = qutils.name_from_fpath(contigs_fpath)
    assembly_label = qutils.label_from_fpath(contigs_fpath)

    logger.info('  ' + qutils.index_to_str(index) + assembly_label)

    err_fpath = os.path.join(out_dirpath, assembly_name + '_genemark.stderr')

    genes = gmhmm_p_function(tool_dirpath, contigs_fpath, err_fpath, index, tmp_dirpath)

    if not genes:
        unique_count = None
        count = None  # [None] * len(gene_lengths)
    else:
        tool_name = "genemark"
        out_gff_fpath = os.path.join(out_dirpath, assembly_name + '_' + tool_name + '_genes.gff')
        add_genes_to_gff(genes, out_gff_fpath)
        if OUTPUT_FASTA:
            out_fasta_fpath = os.path.join(out_dirpath, assembly_name + '_' + tool_name + '_genes.fasta')
            add_genes_to_fasta(genes, out_fasta_fpath)

        count = [sum([gene[3] - gene[2] > x for gene in genes]) for x in gene_lengths]
        unique_count = len(set([gene[4] for gene in genes]))
        total_count = len(genes)

        logger.info('  ' + qutils.index_to_str(index) + '  Genes = ' + str(unique_count) + ' unique, ' + str(total_count) + ' total')
        logger.info('  ' + qutils.index_to_str(index) + '  Predicted genes (GFF): ' + out_gff_fpath)

    return unique_count, count


def do(fasta_fpaths, gene_lengths, out_dirpath, meta):
    logger.print_timestamp()
    if LICENSE_LIMITATIONS_MODE:
        logger.warning("GeneMark tool can't be started because of license limitations!")
        return

    if meta:
        tool_name = 'MetaGeneMark'
        tool_dirname = 'metagenemark'
        gmhmm_p_function = gmhmm_p_metagenomic
    else:
        tool_name = 'GeneMark'
        tool_dirname = 'genemark'
        gmhmm_p_function = gmhmm_p_everyGC

    logger.info('Running %s...' % tool_name)

    tool_dirpath = os.path.join(qconfig.LIBS_LOCATION, tool_dirname, qconfig.platform_name)
    if not os.path.exists(tool_dirpath):
        logger.warning('  Sorry, can\'t use %s on this platform, skipping gene prediction.' % tool_name)
    else:
        successful = install_genemark(tool_dirpath)
        if not successful:
            return

        if not os.path.isdir(out_dirpath):
            os.mkdir(out_dirpath)
        tmp_dirpath = os.path.join(out_dirpath, 'tmp')
        if not os.path.isdir(tmp_dirpath):
            os.mkdir(tmp_dirpath)

        n_jobs = min(len(fasta_fpaths), qconfig.max_threads)
        from joblib import Parallel, delayed
        results = Parallel(n_jobs=n_jobs)(delayed(predict_genes)(
            index, fasta_fpath, gene_lengths, out_dirpath, tool_dirpath, tmp_dirpath, gmhmm_p_function)
            for index, fasta_fpath in enumerate(fasta_fpaths))

        # saving results
        for i, fasta_path in enumerate(fasta_fpaths):
            report = reporting.get(fasta_path)
            unique_count, count = results[i]
            if unique_count is not None:
                report.add_field(reporting.Fields.PREDICTED_GENES_UNIQUE, unique_count)
            if count is not None:
                report.add_field(reporting.Fields.PREDICTED_GENES, count)

        if not qconfig.debug:
            shutil.rmtree(tmp_dirpath)

        logger.info('Done.')