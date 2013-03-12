from __future__ import with_statement
import os
import shutil


contigs_1_1k = 'contigs_1k_1.fasta'
contigs_2_1k = 'contigs_1k_2.fasta'
reference_1k = 'reference_1k.fa'
genes_1k = 'genes_1k.txt'
operons_1k = 'operons_1k.txt'

contigs_1_10k = 'contigs_10k_1.fasta'
contigs_2_10k = 'contigs_10k_2.fasta'
reference_10k = 'reference_10k.fa.gz'
genes_10k = 'genes_10k.txt'
operons_10k = 'operons_10k.txt'

contigs_1 = 'contigs.fasta'
contigs_2 = 'contigs.fasta'
reference = 'reference.fa'
genes = 'genes.txt'
operons = 'operons.txt'


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
# --allow-ambiguity                 Uses all alignments of a contig with multiple equally good alignments (probably a repeat).
#                                   By default, QUAST skips all alignments of such contigs
#
# --strict-NA                       Breaks contigs by any misassembly event to compute NAx and NGAx.
#                                   By default, QUAST breaks contigs only by extensive misassemblies (not local ones)
#
# -m  --meta                        Metagenomic assembly. Uses MetaGeneMark for gene prediction.
#                                   Accepts multiple reference files (comma-separated list of filenames after -R option)
#
# -h  --help                        Prints this message


common_results_dirpath = 'results'
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
        'transposed_report.txt',
        'report_html_aux']

    fail = False
    for fname in report_fnames:
        if not os.path.exists(os.path.join(results_dirpath, fname)):
            print 'File %s does not exists' % fname
            fail = True

    if fail:
        exit(5)


def assert_report_header(name, contigs, fname='report.tsv'):
    results_dirpath = get_results_dirpath(name)

    with open(os.path.join(results_dirpath, fname)) as report_tsv_f:
        header = report_tsv_f.readline()
        if len(header.split('\t')) != len(contigs) + 1:
            print 'Incorrect %s header: %s' % fname, header
            exit(6)


def assert_metric(name, metric, values, fname='report.tsv'):
    results_dirpath = get_results_dirpath(name)

    fpath = os.path.join(results_dirpath, fname)
    if not os.path.isfile(fpath):
        print 'File %s does not exist' % fpath
        exit(5)

    with open(fpath) as report_tsv_f:
        for line in report_tsv_f:
            tokens = line[:-1].split('\t')
            if len(tokens) > 1 and tokens[0] == metric:
                if values is None:
                    print 'Assertion of "%s" in %s failed: expected that there is no such metric, got values "%s" instead' \
                          % (metric, fname, ' '.join(tokens[1:]))
                    exit(7)

                if tokens[1:] != list(values):
                    print 'Assertion of "%s" in %s failed: "%s" expected, got "%s" instead' \
                          % (metric, fname, ' '.join(values), ' '.join(tokens[1:]))
                    exit(7)


def run_quast(name, contigs=None, params='', expected_exit_code=0):
    results_dirpath = get_results_dirpath(name)

    if os.path.isdir(results_dirpath):
        shutil.rmtree(results_dirpath)
        os.makedirs(results_dirpath)
        os.system("chmod -R 777 " + results_dirpath)

    if not contigs:
        contigs = [contigs_1_10k, contigs_2_10k]

    os.chdir('data')
    cmd = '../../quast.py -o ../' + results_dirpath + ' ' + ' '.join(contigs) + ' ' + params
    print cmd
    print
    exit_code = os.system(cmd) >> 8
    print
    os.chdir('..')

    if exit_code != 0:
        if exit_code != expected_exit_code:
            print 'Quast finished abnormally with exit code %s' % str(exit_code)
            exit(4)
        else:
            exit(0)