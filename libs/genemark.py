############################################################################
# Copyright (c) 2011-2015 Saint-Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################

from __future__ import with_statement
import os
import shutil
import tempfile

from libs import reporting, qconfig, qutils
from libs.fastaparser import write_fasta

from libs.log import get_logger

logger = get_logger(qconfig.LOGGER_DEFAULT_NAME)

LICENSE_LIMITATIONS_MODE = False
OUTPUT_FASTA = False  # whether output only .gff or with corresponding .fasta files


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
    """Installation instructions for GeneMark.
    Please, copy key "gm_key" into users home directory as:
    cp gm_key ~/.gm_key
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
                           'To update license, please visit http://exon.gatech.edu/GeneMark/license_download.cgi page and fill in the form.\n' \
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
                seq_id, contig_id = line.strip().split('\t')
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


def parse_gtf_out(out_fpath):
    with open(out_fpath) as f:
        for line in f:
            if 'CDS' in line:
                l = line.strip().split()
                contig_id, strand, left_index, right_index, gene = l[0], l[6], l[3], l[4], l[9]
                left_index = int(left_index)
                right_index = int(right_index)
                yield contig_id, strand, left_index, right_index, gene


def add_genes_to_gff(genes, gff_fpath, prokaryote):
    gff = open(gff_fpath, 'w')
    if prokaryote:
        gff.write('##gff out for GeneMarkS PROKARYOTIC\n')
    else:
        gff.write('##gff out for GeneMark-ES EUKARYOTIC\n')
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


def gmhmm_p_everyGC(tool_dirpath, fasta_fpath, err_fpath, index, tmp_dirpath, num_threads):
    tmp_dirpath = tempfile.mkdtemp(dir=tmp_dirpath)

    tool_exec_fpath = os.path.join(tool_dirpath, 'gmsn.pl')
    err_file = open(err_fpath, 'w')
    fasta_name = qutils.name_from_fpath(fasta_fpath)
    return_code = qutils.call_subprocess(
        ['perl', tool_exec_fpath, '--name', fasta_name, '--clean', '--out', tmp_dirpath,
         fasta_fpath],
        stdout=err_file,
        stderr=err_file,
        indent='    ' + qutils.index_to_str(index))
    if return_code != 0:
        return

    genes = []
    tool_exec_fpath = os.path.join(tool_dirpath, 'gmhmmp')
    sub_fasta_fpath = os.path.join(tmp_dirpath, fasta_name)
    out_fpath = sub_fasta_fpath + '.gmhmm'
    heu_fpath = os.path.join(tmp_dirpath, fasta_name + '_hmm_heuristic.mod')
    with open(err_fpath, 'a') as err_file:
        ok = gmhmm_p(tool_exec_fpath, fasta_fpath, heu_fpath,
                   out_fpath, err_file, index)
        if ok:
            genes.extend(parse_gmhmm_out(out_fpath))

    if not qconfig.debug:
        shutil.rmtree(tmp_dirpath)

    return genes


def gmhmm_p_metagenomic(tool_dirpath, fasta_fpath, err_fpath, index, tmp_dirpath=None, num_threads=None):
    tool_exec_fpath = os.path.join(tool_dirpath, 'gmhmmp')
    heu_fpath = os.path.join(tool_dirpath, '../MetaGeneMark_v1.mod')
    gmhmm_fpath = fasta_fpath + '.gmhmm'

    with open(err_fpath, 'a') as err_file:
        if gmhmm_p(tool_exec_fpath, fasta_fpath, heu_fpath, gmhmm_fpath, err_file, index):
            return list(parse_gmhmm_out(gmhmm_fpath))
        else:
            return None


def gm_es(tool_dirpath, fasta_fpath, err_fpath, index, tmp_dirpath, num_threads):
    tool_exec_fpath = os.path.join(tool_dirpath, 'gmes_petap.pl')
    libs_dirpath = os.path.join(qconfig.LIBS_LOCATION, 'genemark-es', 'lib')
    err_file = open(err_fpath, 'w')
    tmp_dirpath += qutils.name_from_fpath(fasta_fpath)
    if not os.path.isdir(tmp_dirpath):
        os.mkdir(tmp_dirpath)
    return_code = qutils.call_subprocess(
        ['perl', '-I', libs_dirpath, tool_exec_fpath, '--ES', '--cores', str(num_threads), '--sequence', fasta_fpath,
         '--out', tmp_dirpath],
        stdout=err_file,
        stderr=err_file,
        indent='    ' + qutils.index_to_str(index))
    if return_code != 0:
        return
    genes = []
    _, _, fnames = os.walk(tmp_dirpath).next()
    for fname in fnames:
        if fname.endswith('gtf'):
            genes.extend(parse_gtf_out(os.path.join(tmp_dirpath, fname)))
    return genes


def predict_genes(index, contigs_fpath, gene_lengths, out_dirpath, tool_dirpath, tmp_dirpath, gmhmm_p_function,
                  prokaryote, num_threads):
    assembly_label = qutils.label_from_fpath_for_fname(contigs_fpath)

    logger.info('  ' + qutils.index_to_str(index) + assembly_label)

    err_fpath = os.path.join(out_dirpath, assembly_label + '_genemark.stderr')

    genes = gmhmm_p_function(tool_dirpath, contigs_fpath, err_fpath, index, tmp_dirpath, num_threads)

    if not genes:
        unique_count = None
        count = None  # [None] * len(gene_lengths)
    else:
        tool_name = "genemark"
        out_gff_fpath = os.path.join(out_dirpath, assembly_label + '_' + tool_name + '_genes.gff')
        add_genes_to_gff(genes, out_gff_fpath, prokaryote)
        if OUTPUT_FASTA:
            out_fasta_fpath = os.path.join(out_dirpath, assembly_label + '_' + tool_name + '_genes.fasta')
            add_genes_to_fasta(genes, out_fasta_fpath)

        count = [sum([gene[3] - gene[2] > x for gene in genes]) for x in gene_lengths]
        unique_count = len(set([gene[4] for gene in genes]))
        total_count = len(genes)

        logger.info('  ' + qutils.index_to_str(index) + '  Genes = ' + str(unique_count) + ' unique, ' + str(total_count) + ' total')
        logger.info('  ' + qutils.index_to_str(index) + '  Predicted genes (GFF): ' + out_gff_fpath)

    return unique_count, count


def do(fasta_fpaths, gene_lengths, out_dirpath, prokaryote, meta):
    logger.print_timestamp()
    if LICENSE_LIMITATIONS_MODE:
        logger.warning("GeneMark tool can't be started because of license limitations!")
        return

    if meta:
        tool_name = 'MetaGeneMark'
        tool_dirname = 'genemark'
        gmhmm_p_function = gmhmm_p_metagenomic
    elif prokaryote:
        tool_name = 'GeneMarkS'
        tool_dirname = 'genemark'
        gmhmm_p_function = gmhmm_p_everyGC
    else:
        tool_name = 'GeneMark-ES'
        tool_dirname = 'genemark-es'
        gmhmm_p_function = gm_es

    logger.info('Running %s...' % tool_name)

    tool_dirpath = os.path.join(qconfig.LIBS_LOCATION, tool_dirname, qconfig.platform_name)
    if not os.path.exists(tool_dirpath):
        logger.warning('  Sorry, can\'t use %s on this platform, skipping gene prediction.' % tool_name)
    else:
        successful = install_genemark(os.path.join(qconfig.LIBS_LOCATION, 'genemark', qconfig.platform_name))
        if not successful:
            return

        if not os.path.isdir(out_dirpath):
            os.mkdir(out_dirpath)
        tmp_dirpath = os.path.join(out_dirpath, 'tmp')
        if not os.path.isdir(tmp_dirpath):
            os.mkdir(tmp_dirpath)

        n_jobs = min(len(fasta_fpaths), qconfig.max_threads)
        num_threads = max(1, qconfig.max_threads // n_jobs)
        from joblib import Parallel, delayed
        results = Parallel(n_jobs=n_jobs)(delayed(predict_genes)(
            index, fasta_fpath, gene_lengths, out_dirpath, tool_dirpath, tmp_dirpath, gmhmm_p_function, prokaryote, num_threads)
            for index, fasta_fpath in enumerate(fasta_fpaths))

        # saving results
        for i, fasta_path in enumerate(fasta_fpaths):
            report = reporting.get(fasta_path)
            unique_count, count = results[i]
            if unique_count is not None:
                report.add_field(reporting.Fields.PREDICTED_GENES_UNIQUE, unique_count)
            if count is not None:
                report.add_field(reporting.Fields.PREDICTED_GENES, count)
            if unique_count is None and count is None:
                logger.error('  ' + qutils.index_to_str(i) +
                     'Failed predicting genes in ' + qutils.label_from_fpath(fasta_path) + '. ' +
                     ('File may be too small for GeneMark-ES. Try to use GeneMarkS instead (remove --eukaryote option).'
                         if tool_name == 'GeneMark-ES' and os.path.getsize(fasta_path) < 2000000 else ''))

        if not qconfig.debug:
            shutil.rmtree(tmp_dirpath)

        logger.info('Done.')
