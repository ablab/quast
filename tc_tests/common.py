#!/usr/bin/python

from __future__ import with_statement
import os
import shutil
import sys


contigs_1k_1 = 'contigs_1k_1.fasta'
contigs_1k_2 = 'contigs_1k_2.fasta'
reference_1k = 'reference_1k.fa'
genes_1k = 'genes_1k.txt'
operons_1k = 'operons_1k.txt'

contigs_10k_1 = 'contigs_10k_1.fasta'
contigs_10k_2 = 'contigs_10k_2.fasta'
contigs_10k_1_scaffolds = 'contigs_10k_1_scaffolds.fasta'
contigs_10k_1_broken = 'contigs_10k_1_scaffolds_broken.fasta'
reference_10k = 'reference_10k.fa.gz'
genes_10k = 'genes_10k.txt'
operons_10k = 'operons_10k.txt'

contigs_1 = 'contigs_1.fasta'
contigs_2 = 'contigs_2.fasta'
reference = 'reference.fa.gz'
genes = 'genes.txt'
operons = 'operons.txt'

meta_contigs_1 = 'meta_contigs_1.fasta'
meta_contigs_2 = 'meta_contigs_2.fasta'
meta_references = ['meta_ref_1.fasta', 'meta_ref_2.fasta', 'meta_ref_3.fasta']

per_ref_dirname = "runs_per_reference"
combined_output_name = "combined_reference"


# Options:
# -o            <dirname>      Directory to store all result file. Default: quast_results/results_<datetime>
# -R            <filename>     Reference genome file
# -G  --genes   <filename>     Annotated genes file
# -O  --operons <filename>     Annotated operons file
# --min-contig  <int>          Lower threshold for contig length [default: 500]
#
#
# Advanced options:
# --threads <int>                   Maximum number of threads [default: number of CPUs]
#
# --gage                            Starts GAGE inside QUAST ("GAGE mode")
#
# --contig-thresholds <int,int,..>  Comma-separated list of contig length thresholds [default: 0,1000]
#
# --gene-finding                    Uses Gene Finding module
#
# --gene-thresholds <int,int,..>    Comma-separated list of threshold lengths of genes to search with Gene Finding module
#                                   [default is 0,300,1500,3000]
#
# --eukaryote                       Genome is an eukaryote
#
# --est-ref-size <int>              Estimated reference size (for computing NGx metrics without a reference)
#
# --scaffolds                       Provided assemblies are scaffolds
#
# --use-all-alignments              Computes Genome fraction, # genes, # operons metrics in compatible with QUAST v.1.* mode.
#                                   By default, QUAST filters Nucmer's alignments to keep only best ones
#
# --ambiguity-usage <none|one|all>  Uses none, one, or all alignments of a contig with multiple equally good alignments (probably a repeat).
#                                   [default is none]
#
# --strict-NA                       Breaks contigs by any misassembly event to compute NAx and NGAx.
#                                   By default, QUAST breaks contigs only by extensive misassemblies (not local ones)
#
# -m  --meta                        Metagenomic assembly. Uses MetaGeneMark for gene prediction.
#                                   Accepts multiple reference files (comma-separated list of filenames after -R option)
#
# -h  --help                        Prints this message

common_results_dirpath = 'results'
data_dirpath = 'data'

os.system('chmod -R 777 ' + data_dirpath)

if not os.path.exists(common_results_dirpath):
    os.makedirs(common_results_dirpath)
    os.system("chmod -R 777 " + common_results_dirpath)


def get_results_dirpath(dirname):
    dirpath = os.path.join(common_results_dirpath, dirname)
    return dirpath


def check_report_files(name, report_fnames=None):
    results_dirpath = get_results_dirpath(name)

    report_fnames = report_fnames or [
        'report.tex',
        'report.html',
        'report.tsv',
        'report.txt',
        'transposed_report.tex',
        'transposed_report.tsv',
        'transposed_report.txt']

    files_not_exist = []
    for fname in report_fnames:
        if not os.path.exists(os.path.join(results_dirpath, fname)):
            files_not_exist.append(fname)

    if files_not_exist:
        print >> sys.stderr, 'Test failed:',
        if len(files_not_exist) == 1:
            print >> sys.stderr, 'file', files_not_exist[0], 'does not exist in', results_dirpath
        else:
            print >> sys.stderr, 'the followning files does not exist in', results_dirpath + ':'
        for fname in files_not_exist:
            print >> sys.stderr, '  ' + fname
        exit(5)
    else:
        print 'All nesessary files exist'


def assert_report_header(name, contigs, fname='report.tsv'):
    results_dirpath = get_results_dirpath(name)

    with open(os.path.join(results_dirpath, fname)) as report_tsv_f:
        header = report_tsv_f.readline()
        if not header:
            print >> sys.stderr, 'Empty %s' % fname
            exit(6)

        if len(header.split('\t')) != len(contigs) + 1:
            print >> sys.stderr, 'Incorrect %s header: %s' % (fname, header)
            exit(6)

    print 'Report header in %s is OK' % fname


def assert_metric(name, metric, values=None, fname='report.tsv'):
    results_dirpath = get_results_dirpath(name)

    fpath = os.path.join(results_dirpath, fname)
    if not os.path.isfile(fpath):
        print >> sys.stderr, 'File %s does not exist' % fpath
        exit(5)

    with open(fpath) as report_tsv_f:
        for line in report_tsv_f:
            tokens = line[:-1].split('\t')
            if len(tokens) > 1 and tokens[0] == metric:
                if values is None or tokens[1:] == values:
                    print 'Metric %s is OK' % metric
                    return True
                else:
                    print >> sys.stderr, 'Assertion of "%s" in %s failed: "%s" expected, got "%s" instead' \
                                         % (metric, fname, ' '.join(values), ' '.join(tokens[1:]))
                    exit(7)

    print >> sys.stderr, 'Assertion of "%s" in %s failed: no such metric in the file' % (metric, fname)
    exit(7)


def get_metric_values(name, metric, fname='report.tsv'):
    results_dirpath = get_results_dirpath(name)

    fpath = os.path.join(results_dirpath, fname)
    if not os.path.isfile(fpath):
        print >> sys.stderr, 'File %s does not exist' % fpath
        exit(5)

    with open(fpath) as report_tsv_f:
        for line in report_tsv_f:
            tokens = line[:-1].split('\t')
            if len(tokens) > 1 and tokens[0] == metric:
                return tokens[1:]


def assert_values_equal(name, metric=None, fname='report.tsv'):
    results_dirpath = get_results_dirpath(name)

    fpath = os.path.join(results_dirpath, fname)
    if not os.path.isfile(fpath):
        print >> sys.stderr, 'File %s does not exist' % fpath
        exit(5)

    def check_values_equal(tokens):
        values = tokens[1:]

        # assert all equal
        variants = set(values)
        if len(variants) == 1 or len(variants) == 0:
            print 'OK:', '\t'.join(tokens)
            return True
        else:
            print 'FAIL: Values differ:', '\t'.join(tokens)
            return False

    with open(fpath) as report_tsv_f:
        fail = False

        for line in report_tsv_f:
            tokens = line[:-1].split('\t')

            if tokens[0] == 'Assembly':
                continue

            if metric and tokens[0] == metric:
                if not check_values_equal(tokens):
                    exit(8)
                else:
                    return True

            if not check_values_equal(tokens):
                fail = True

        if fail:
            exit(8)
        else:
            return True


def run_quast(name, contigs=None, params='', expected_exit_code=0, meta=False):
    results_dirpath = get_results_dirpath(name)

    if os.path.exists(results_dirpath):
        shutil.rmtree(results_dirpath)
    os.makedirs(results_dirpath)
    os.system("chmod -R 777 " + results_dirpath)

    if not contigs:
        contigs = [contigs_10k_1, contigs_10k_2]

    os.chdir('data')
    quast_fpath = '../../quast.py'
    if meta:
        quast_fpath = '../../metaquast.py'
    cmd = quast_fpath + ' -o ../' + results_dirpath + ' ' + ' '.join(contigs) + ' ' + params
    print cmd
    print
    exit_code = os.system(cmd) >> 8
    print
    os.chdir('..')

    if expected_exit_code is None:
        return exit_code

    if exit_code != expected_exit_code:
        if expected_exit_code == 0:
            print >> sys.stderr, 'Quast finished abnormally with exit code %d' % exit_code
            exit(1)
        else:
            print >> sys.stderr, 'Expected exit code %d, got %d instead' % (expected_exit_code, exit_code)
            exit(4)

    print 'QUAST worked as expected with exit code %s' % exit_code
