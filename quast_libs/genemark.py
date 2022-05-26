############################################################################
# Copyright (c) 2015-2022 Saint Petersburg State University
# Copyright (c) 2011-2015 Saint Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################

from __future__ import with_statement
import os
import glob
import shutil
import tempfile

try:
   from collections import OrderedDict
except ImportError:
   from quast_libs.site_packages.ordered_dict import OrderedDict

from quast_libs import reporting, qconfig, qutils
from quast_libs.ca_utils.misc import open_gzipsafe
from quast_libs.fastaparser import write_fasta, get_chr_lengths_from_fastafile
from quast_libs.genes_parser import Gene

from quast_libs.log import get_logger
from quast_libs.qutils import run_parallel

logger = get_logger(qconfig.LOGGER_DEFAULT_NAME)

LICENSE_LIMITATIONS_MODE = False
OUTPUT_FASTA = False  # whether output only .gff or with corresponding .fasta files


def gmhmm_p(tool_exec, fasta_fpath, heu_fpath, out_fpath, err_file, index):
    """ Run GeneMark.hmm with this heuristic model (heu_dirpath)
        prompt> gmhmmp -m heu_11_45.mod sequence
        prompt> gm -m heu_11_45.mat sequence"""
    return_code = qutils.call_subprocess(
        [tool_exec, '-d', '-a', '-p', '0', '-m', heu_fpath, '-o', out_fpath, fasta_fpath],
        stdout=err_file,
        stderr=err_file,
        indent='    ' + qutils.index_to_str(index))

    return return_code == 0 and os.path.isfile(out_fpath)


def install_genemark():
    """Installation instructions for GeneMark.
    Please, copy key "gm_key" into users home directory as:
    cp gm_key ~/.gm_key
    """
    import filecmp
    base_genemark_dir = os.path.join(qconfig.LIBS_LOCATION, 'genemark')
    gm_key_fpath = os.path.join(base_genemark_dir, 'gm_keys',
                                'gm_key_' + ('32' if qconfig.platform_name == 'linux_32' else '64'))
    try:
        gm_key_dst = os.path.expanduser('~/.gm_key')
        if not os.path.isfile(gm_key_dst) or \
            (not filecmp.cmp(gm_key_dst, gm_key_fpath) and os.path.getmtime(gm_key_dst) < os.path.getmtime(gm_key_fpath)):
            shutil.copyfile(gm_key_fpath, gm_key_dst)
        return True
    except:
        return False


def is_license_valid(out_dirpath, fasta_fpaths):
    # checking the installation
    err_fpath = os.path.join(out_dirpath, qutils.label_from_fpath_for_fname(fasta_fpaths[0]) + '_genemark.stderr')
    if os.path.isfile(err_fpath):
        with open(err_fpath) as err_f:
            for line in err_f:
                if line.find('license period has ended') != -1:
                    logger.main_info()
                    logger.warning('License period for GeneMark has ended! \n'
                                   'To update license, please visit http://exon.gatech.edu/GeneMark/license_download.cgi page and fill in the form.\n'
                                   'You should choose GeneMarkS tool and your operating system (note that GeneMark is free for non-commercial use).\n'
                                   'Download the license key and replace your ~/.gm_key with the updated version. After that you can restart QUAST.\n')
                    return False
    return True


# Gene = namedtuple('Gene', ['contig_id', 'strand', 'left_index', 'right_index', 'seq'])
def parse_gmhmm_out(out_fpath):
    reading_gene = False
    reading_protein = False
    protein = ''
    genes_by_id = OrderedDict()
    gene_id = None
    with open(out_fpath) as f:
        for line in f:
            if line.startswith('>gene'):
                seq = []
                seq_id, contig_id = line.strip().split('\t')
                # >gene_2|GeneMark.hmm|57_nt|+|1|57	>NODE_3_length_713_cov_1.25228
                gene_id, _, seq_len, strand, left_index, right_index = seq_id.split('|')
                gene_id = gene_id[1:]
                contig_id = contig_id[1:]
                if 'nt' in seq_len:
                    reading_gene = True
                elif 'aa' in seq_len:
                    reading_protein = True
            elif reading_gene or reading_protein:
                if line.isspace():
                    left_index = int(left_index)
                    right_index = int(right_index)
                    if reading_gene:
                        seq = ''.join(seq)
                        reading_gene = False
                    elif reading_protein:
                        protein = ''.join(seq)
                        seq = []
                        reading_protein = False
                    #genes.append(Gene(contig_id, strand, left_index, right_index, str_seq))
                    gene = genes_by_id[gene_id] if gene_id in genes_by_id else \
                        Gene(contig=contig_id, start=left_index, end=right_index, strand=strand)
                    if seq:
                        gene.seq = seq
                        seq = []
                    if protein:
                        gene.protein = protein
                        protein = None
                    genes_by_id[gene_id] = gene
                else:
                    seq.append(line.strip())
    return list(genes_by_id.values())


def parse_gtf_out(out_fpath):
    with open(out_fpath) as f:
        for line in f:
            if 'CDS' in line:
                l = line.strip().split()
                gene = Gene(contig=l[0], strand=l[6], start=int(l[3]), end=int(l[4]), seq=l[9])
                yield gene


def add_genes_to_gff(genes, gff_fpath, prokaryote):
    gff = open_gzipsafe(gff_fpath, 'w')
    if prokaryote:
        if qconfig.metagenemark:
            gff.write('##gff out for MetaGeneMark\n')
        else:
            gff.write('##gff out for GeneMarkS PROKARYOTIC\n')
    else:
        gff.write('##gff out for GeneMark-ES EUKARYOTIC\n')
    gff.write('##gff-version 3\n')

    for id, gene in enumerate(genes):
        gff.write('%s\tGeneMark\tgene\t%d\t%d\t.\t%s\t.\tID=%d\n' %
            (gene.contig, gene.start, gene.end, gene.strand, id + 1))
        if gene.seq:
            gff.write('##Nucleotide sequence:\n')
            for i in range(0, len(gene.seq), 60):
                gff.write('##' + gene.seq[i:i + 60] + '\n')
        if gene.protein:
            gff.write('##Protein sequence:\n')
            for i in range(0, len(gene.protein), 60):
                gff.write('##' + gene.protein[i:i + 60] + '\n')
            gff.write('\n')
    gff.close()


def add_genes_to_fasta(genes, fasta_fpath):
    def inner():
        for i, gene in enumerate(genes):
            length = gene.end - gene.start
            gene_id = '>gene_%d|GeneMark.hmm|%d_nt|%s|%d|%d|%s' % (
                i + 1, length, gene.strand, gene.start, gene.end, gene.contig
            )
            yield gene_id, gene.seq

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
            genes = parse_gmhmm_out(out_fpath)

    if not qconfig.debug:
        shutil.rmtree(tmp_dirpath)

    return genes


def gmhmm_p_metagenomic(tool_dirpath, fasta_fpath, err_fpath, index, tmp_dirpath=None, num_threads=None):
    tool_exec_fpath = os.path.join(tool_dirpath, 'gmhmmp')
    heu_fpath = os.path.join(tool_dirpath, '../MetaGeneMark_v1.mod')
    gmhmm_fpath = fasta_fpath + '.gmhmm'

    with open(err_fpath, 'w') as err_file:
        if gmhmm_p(tool_exec_fpath, fasta_fpath, heu_fpath, gmhmm_fpath, err_file, index):
            return parse_gmhmm_out(gmhmm_fpath)
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
         '--out', tmp_dirpath] + (['--fungus'] if qconfig.is_fungus else []),
        stdout=err_file,
        stderr=err_file,
        indent='    ' + qutils.index_to_str(index))
    if return_code != 0:
        return
    genes = []
    fnames = [fname for (path, dirs, files) in os.walk(tmp_dirpath) for fname in files]
    for fname in fnames:
        if fname.endswith('gtf'):
            genes.extend(parse_gtf_out(os.path.join(tmp_dirpath, fname)))
    return genes


def predict_genes(index, contigs_fpath, gene_lengths, out_dirpath, tool_dirpath, tmp_dirpath, gmhmm_p_function,
                  prokaryote, num_threads):
    assembly_label = qutils.label_from_fpath(contigs_fpath)
    corr_assembly_label = qutils.label_from_fpath_for_fname(contigs_fpath)

    logger.info('  ' + qutils.index_to_str(index) + assembly_label)

    err_fpath = os.path.join(out_dirpath, corr_assembly_label + '_genemark.stderr')

    genes = gmhmm_p_function(tool_dirpath, contigs_fpath, err_fpath, index, tmp_dirpath, num_threads)
    contig_lengths = get_chr_lengths_from_fastafile(contigs_fpath)

    if not genes:
        unique_count = None
        full_cnt = None
        partial_cnt = None
    else:
        for gene in genes:
            gene.is_full = gene.start > 1 and gene.end < contig_lengths[gene.contig]
        tool_name = "genemark"
        out_gff_fpath = os.path.join(out_dirpath, corr_assembly_label + '_' + tool_name + '_genes.gff')
        add_genes_to_gff(genes, out_gff_fpath, prokaryote)
        if OUTPUT_FASTA:
            out_fasta_fpath = os.path.join(out_dirpath, corr_assembly_label + '_' + tool_name + '_genes.fasta')
            add_genes_to_fasta(genes, out_fasta_fpath)

        full_cnt = [sum([gene.end - gene.start >= threshold for gene in genes if gene.is_full])
                    for threshold in gene_lengths]
        partial_cnt = [sum([gene.end - gene.start >= threshold for gene in genes if not gene.is_full])
                       for threshold in gene_lengths]
        gene_ids = [gene.seq if gene.seq else gene.name for gene in genes]
        unique_count = len(set(gene_ids))
        total_count = len(genes)

        logger.info('  ' + qutils.index_to_str(index) + '  Genes = ' + str(unique_count) + ' unique, ' + str(total_count) + ' total')
        logger.info('  ' + qutils.index_to_str(index) + '  Predicted genes (GFF): ' + out_gff_fpath)

    return genes, unique_count, full_cnt, partial_cnt


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

    logger.main_info('Running %s...' % tool_name)

    tool_dirpath = os.path.join(qconfig.LIBS_LOCATION, tool_dirname, qconfig.platform_name)
    if not os.path.exists(tool_dirpath):
        logger.warning('  Sorry, can\'t use %s on this platform, skipping gene prediction.' % tool_name)
    elif not install_genemark():
        logger.warning('  Can\'t copy the license key to ~/.gm_key, skipping gene prediction.')
    else:
        if not os.path.isdir(out_dirpath):
            os.mkdir(out_dirpath)
        tmp_dirpath = os.path.join(out_dirpath, 'tmp')
        if not os.path.isdir(tmp_dirpath):
            os.mkdir(tmp_dirpath)

        n_jobs = min(len(fasta_fpaths), qconfig.max_threads)
        num_threads = max(1, qconfig.max_threads // n_jobs)
        parallel_run_args = [(index, fasta_fpath, gene_lengths, out_dirpath, tool_dirpath, tmp_dirpath,
                              gmhmm_p_function, prokaryote, num_threads)
                             for index, fasta_fpath in enumerate(fasta_fpaths)]
        genes_list, unique_count, full_genes, partial_genes = run_parallel(predict_genes, parallel_run_args, n_jobs)
        if not is_license_valid(out_dirpath, fasta_fpaths):
            return

        genes_by_labels = dict()
        # saving results
        for i, fasta_path in enumerate(fasta_fpaths):
            report = reporting.get(fasta_path)
            label = qutils.label_from_fpath(fasta_path)
            genes_by_labels[label] = genes_list[i]
            if unique_count[i] is not None:
                report.add_field(reporting.Fields.PREDICTED_GENES_UNIQUE, unique_count[i])
            if full_genes[i] is not None:
                genes = ['%s + %s part' % (full_cnt, partial_cnt) for full_cnt, partial_cnt in zip(full_genes[i], partial_genes[i])]
                report.add_field(reporting.Fields.PREDICTED_GENES, genes)
            if unique_count[i] is None and full_genes[i] is None:
                logger.error('  ' + qutils.index_to_str(i) +
                     'Failed predicting genes in ' + label + '. ' +
                     ('File may be too small for GeneMark-ES. Try to use GeneMarkS instead (remove --eukaryote option).'
                         if tool_name == 'GeneMark-ES' and os.path.getsize(fasta_path) < 2000000 else ''))

        if not qconfig.debug:
            for dirpath in glob.iglob(tmp_dirpath + '*'):
                if os.path.isdir(dirpath):
                    shutil.rmtree(dirpath)

        logger.main_info('Done.')
        return genes_by_labels
