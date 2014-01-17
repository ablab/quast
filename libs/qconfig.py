############################################################################
# Copyright (c) 2011-2014 Saint-Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################

import datetime
import os
import platform
import sys

LIBS_LOCATION = os.path.abspath(os.path.dirname(os.path.realpath(__file__)))

SUPPORTED_PYTHON_VERSIONS = ['2.5', '2.6', '2.7']

LOGGER_DEFAULT_NAME = 'quast'
# error_log_fpath = os.path.join(LIBS_LOCATION, '..', 'error.log')

if platform.system() == 'Darwin':
    platform_name = 'macosx'
else:
    if platform.architecture()[0] == '64bit':
        platform_name = 'linux_64'
    else:
        platform_name = 'linux_32'
# platform_name = 'linux_64'

# support of large genomes
MAX_REFERENCE_LENGTH = 536870908  # Nucmer's max length of a reference file
splitted_ref = []

# available options
long_options = "output-dir= save-json-to= genes= operons= reference= contig-thresholds= min-contig= "\
               "gene-thresholds= save-json gage eukaryote no-plots no-html help debug "\
               "ambiguity-usage= scaffolds threads= mincluster= est-ref-size= use-all-alignments gene-finding "\
               "strict-NA meta labels= test help-hidden".split()
short_options = "o:G:O:R:t:M:S:J:jehdsa:T:c:ufnml:L"

# default values for options
contig_thresholds = "0,1000"
min_contig = 500
genes_lengths = "0,300,1500,3000"
ref_fpath = ''
with_gage = False
prokaryote = True  # former cyclic
gene_finding = False
ambiguity_usage = 'one'
use_all_alignments = False
max_threads = None
mincluster = 65
estimated_reference_size = None
strict_NA = False
scaffolds = False
draw_plots = True
html_report = True
save_json = False
meta = False
debug = False
test = False


default_results_root_dirname = "quast_results"
output_dirname = "results_" + datetime.datetime.now().strftime('%Y_%m_%d_%H_%M_%S')
make_latest_symlink = True

default_json_dirname = "json"

# names of reports, log, etc.
corrected_dirname = "quast_corrected_input"
plots_fname = "report.pdf"
report_prefix = "report"
transposed_report_prefix = "transposed_report"
gage_report_prefix = "gage_"
html_aux_dir = "report_html_aux"

# other settings (mostly constants). Can't be changed by command-line options

# for separating indels into short and long ones
SHORT_INDEL_THRESHOLD = 5

# for parallelization
DEFAULT_MAX_THREADS = 4  # this value is used if QUAST fails to determine number of CPUs
assemblies_num = 1

# genome analyzer
min_gap_size = 50  # for calculating number or gaps in genome coverage
min_gene_overlap = 100  # to partial genes/operons finding

# basic_stats
GC_bin_size = 1.0

# plotter and reporting and maybe other modules in the future
assembly_labels_by_fpath = {}

# for scaffolds
list_of_broken_scaffolds = []
Ns_break_threshold = 10


def check_python_version():
    if sys.version[0:3] not in SUPPORTED_PYTHON_VERSIONS:
        sys.stderr.write("ERROR! Python version " + sys.version[0:3] + " is not supported!\n" +\
                         "Supported versions are " + ", ".join(SUPPORTED_PYTHON_VERSIONS) + "\n")
        sys.exit(1)


def quast_version():
    version_fpath = os.path.join(LIBS_LOCATION, '..', 'VERSION')
    version = "unknown"
    build = "unknown"
    if os.path.isfile(version_fpath):
        version_file = open(version_fpath)
        version = version_file.readline()
        if version:
            version = version.strip()
        else:
            version = "unknown"
        build = version_file.readline()
        if build:
            build = build.strip().lower()
        else:
            build = "unknown"
    return version, build


def usage(show_hidden=False, meta=False):
    print >> sys.stderr, 'QUAST: QUality ASsessment Tool for Genome Assemblies'
    version, build = quast_version()
    print >> sys.stderr, "Version", str(version),
    if build != "unknown":
        print >> sys.stderr, ", " + str(build)

    print >> sys.stderr, ""
    print >> sys.stderr, 'Usage: python', sys.argv[0], '[options] <files_with_contigs>'
    print >> sys.stderr, ""

    print >> sys.stderr, "Options:"
    print >> sys.stderr, "-o  --output-dir  <dirname>   Directory to store all result files [default: quast_results/results_<datetime>]"
    if meta:
        print >> sys.stderr, "-R                <filename>  Reference genomes (accepts multiple fasta files with multiple sequences each)"
    else:
        print >> sys.stderr, "-R                <filename>  Reference genome file"
    print >> sys.stderr, "-G  --genes       <filename>  File with gene coordiantes in the reference"
    print >> sys.stderr, "-O  --operons     <filename>  File with operon coordiantes in the reference"
    print >> sys.stderr, "-M  --min-contig  <int>       Lower threshold for contig length [default: %s]" % min_contig
    print >> sys.stderr, ""
    print >> sys.stderr, "Advanced options:"
    print >> sys.stderr, "-T  --threads      <int>              Maximum number of threads [default: number of CPUs]"
    print >> sys.stderr, "-l  --labels \"label, label, ...\"      Names of assemblies to use in reports, comma-separated. If contain spaces, use quotes"
    print >> sys.stderr, "-L                                    Take assembly names from their parent directory names"
    if meta:
        print >> sys.stderr, "-f  --gene-finding                    Predict genes using MetaGeneMark"
    else:
        print >> sys.stderr, "-f  --gene-finding                    Predict genes (with GeneMark.hmm for prokaryotes (default), GlimmerHMM"
        print >> sys.stderr, "                                      for eukaryotes (--eukaryote), or MetaGeneMark for metagenomes (--meta)"
    print >> sys.stderr, "-S  --gene-thresholds                 Comma-separated list of threshold lengths of genes to search with Gene Finding module"
    print >> sys.stderr, "                                      [default is %s]" % genes_lengths
    print >> sys.stderr, "-e  --eukaryote                       Genome is eukaryotic"
    if not meta:
        print >> sys.stderr, "-m  --meta                            Use MetaGeneMark for gene prediction. "
    print >> sys.stderr, "    --est-ref-size <int>              Estimated reference size (for computing NGx metrics without a reference)"
    print >> sys.stderr, "    --gage                            Use GAGE (results are in gage_report.txt)"
    print >> sys.stderr, "-t  --contig-thresholds               Comma-separated list of contig length thresholds [default: %s]" % contig_thresholds
    print >> sys.stderr, "-s  --scaffolds                       Assemblies are scaffolds, split them and add contigs to the comparison"
    print >> sys.stderr, "-u  --use-all-alignments              Compute genome fraction, # genes, # operons in the v.1.0-1.3 style."
    print >> sys.stderr, "                                      By default, QUAST filters Nucmer\'s alignments to keep only best ones"
    print >> sys.stderr, "-a  --ambiguity-usage <none|one|all>  Use none, one, or all alignments of a contig with multiple equally "
    print >> sys.stderr, "                                      good alignments [default is %s]" % ambiguity_usage
    print >> sys.stderr, "-n  --strict-NA                       Break contigs in any misassembly event when compute NAx and NGAx"
    print >> sys.stderr, "                                      By default, QUAST breaks contigs only by extensive misassemblies (not local ones)"
    print >> sys.stderr, "    --no-plots                        Do not draw plots (to speed up computation)"
    if show_hidden:
        print >> sys.stderr, ""
        print >> sys.stderr, "Hidden options:"
        print >> sys.stderr, "-d  --debug                 Run in a debug mode"
        print >> sys.stderr, "-L                          Take assembly names from their parent directory names"
        print >> sys.stderr, "-c  --mincluster   <int>    Nucmer's parameter: the minimum length of a cluster of matches [default: %s]" % mincluster
        print >> sys.stderr, "-j  --save-json             Save the output also in the JSON format"
        print >> sys.stderr, "-J  --save-json-to <path>   Save the JSON output to a particular path"
        print >> sys.stderr, "    --no-html               Do not build html report"
        print >> sys.stderr, "    --no-plots              Do not draw plots (to make quast faster)"
    print >> sys.stderr, ""
    print >> sys.stderr, "    --test                            Run QUAST on the data from the test_data folder, output to quast_test_output"
    print >> sys.stderr, "-h  --help                            Print this usage message"
    if show_hidden:
        print >> sys.stderr, "    --help-hidden                     Print this usage message with all hidden options"
