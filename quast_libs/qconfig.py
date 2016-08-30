############################################################################
# Copyright (c) 2015-2016 Saint Petersburg State University
# Copyright (c) 2011-2015 Saint Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################

from __future__ import with_statement
import datetime
import os
import platform
import sys

QUAST_HOME = os.path.abspath(os.path.dirname(os.path.dirname(os.path.realpath(__file__))))
PACKAGE_NAME = 'quast_libs'
LIBS_LOCATION = os.path.join(QUAST_HOME, PACKAGE_NAME)
GIT_ROOT_URL = 'https://raw.githubusercontent.com/ablab/quast/master/'

SUPPORTED_PYTHON_VERSIONS = ['2.5', '2.6', '2.7']

LOGGER_DEFAULT_NAME = 'quast'
LOGGER_META_NAME = 'metaquast'
# error_log_fpath = os.path.join(LIBS_LOCATION, '..', 'error.log')

if platform.system() == 'Darwin':
    platform_name = 'macosx'
else:
    if platform.architecture()[0] == '64bit':
        platform_name = 'linux_64'
    else:
        platform_name = 'linux_32'

# support of large genomes
MAX_REFERENCE_LENGTH = 536870908  # Nucmer's max length of a reference file
splitted_ref = []
MAX_REFERENCE_FILE_LENGTH = 50000000  # Max length of one part of reference

# default values for options
contig_thresholds = "0,1000,5000,10000,25000,50000"
min_contig = 500
genes_lengths = "0,300,1500,3000"
ref_fpath = ''
with_gage = False
prokaryote = True  # former cyclic
gene_finding = False
ambiguity_usage = 'one'
ambiguity_score = 0.99
use_all_alignments = False
max_threads = None
min_cluster = 65
min_alignment = 0
min_IDY = 95.0
estimated_reference_size = None
strict_NA = False
scaffolds = False
draw_plots = True
html_report = True
save_json = False
meta = False
debug = False
test = False
test_sv = False
test_no_ref = False
no_check = False
no_check_meta = False  # for metaQUAST, without checking min-contig
unique_mapping = False  # for metaQUAST only
no_gc = False
no_sv = False
no_gzip = False
show_snps = True
glimmer = False
is_combined_ref = False
check_for_fragmented_ref = False
significant_part_size = 500
all_labels_from_dirs = False

# print in stdout only main information
silent = False

# the following 2 are for web-quast:
error_log_fpath = None
save_error = False

test_output_dirname = "quast_test_output"
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
contig_report_fname_pattern = 'contigs_report_%s'
icarus_report_fname_pattern = 'all_alignments_%s.tsv'
nucmer_output_dirname = 'nucmer_output'

# for MetaQUAST
downloaded_dirname = "quast_downloaded_references"
per_ref_dirname = "runs_per_reference"
meta_summary_dir = "summary"
not_aligned_name = "not_aligned"
combined_output_name = "combined_reference"
krona_dirname = "krona_charts"
combined_ref_name = 'combined_reference.fasta'
downloaded_ref_min_aligned_rate = 0.1
unique_contigs_fname_pattern = 'unique_contigs_%s.tsv'
calculate_read_support = False

# for reads analyzer
variation_dirname = 'structural_variations'
trivial_deletions_fname = 'trivial_deletions.bed'
manta_sv_fname = 'manta_sv.bed'
used_colors = None
used_ls = None

# for Icarus
draw_svg = False
create_icarus_html = True
icarus_css_name = 'icarus.css'
icarus_script_name = 'build_icarus.js'
icarus_dirname = 'icarus_viewers'
icarus_html_fname = 'icarus.html'
icarus_menu_template_fname = 'icarus_menu_templ.html'
icarus_viewers_template_fname = 'viewers_template.html'
icarus_link = 'View in Icarus contig browser'
contig_size_viewer_fname = 'contig_size_viewer.html'
contig_size_viewer_name = 'Contig size viewer'
contig_alignment_viewer_name = 'Contig alignment viewer'
one_alignment_viewer_name = 'alignment_viewer'
alignment_viewer_part_name = 'alignment_viewer_part_'
alignment_viewer_fpath = 'alignment_viewer.html'
MAX_SIZE_FOR_COMB_PLOT = 50000000
max_contigs_num_for_size_viewer = 1000
min_contig_for_size_viewer = 10000
contig_len_delta = 0.05
min_similar_contig_size = 10000

# other settings (mostly constants). Can't be changed by command-line options

# indels and misassemblies
SHORT_INDEL_THRESHOLD = 5 # for separating short and long indels
MAX_INDEL_LENGTH = 85  # for separating indels and local misassemblies (Nucmer default value)
extensive_misassembly_threshold = 1000  # for separating local and extensive misassemblies (relocation)
BSS_MAX_SETS_NUMBER = 10

# for parallelization
DEFAULT_MAX_THREADS = 4  # this value is used if QUAST fails to determine number of CPUs
assemblies_num = 1
memory_efficient = False

# genome analyzer
min_gap_size = 50  # for calculating number or gaps in genome coverage
min_gene_overlap = 100  # to partial genes/operons finding

# basic_stats
GC_bin_size = 1.0

# plotter and reporting and maybe other modules in the future
assembly_labels_by_fpath = {}
assemblies_fpaths = []
potential_scaffolds_assemblies = []
max_points = 1500 # max points on plots (== max number of contigs)
min_difference = 0
max_coverage_bins = 50
pcnt_covered_for_histogram = 0.9

# for scaffolds
dict_of_broken_scaffolds = {}
Ns_break_threshold = 10
scaffolds_gap_threshold = 10000

# for searching references in NCBI
downloaded_refs = False
identity_threshold = 80  # min % identity
min_length = 300
min_bitscore = 300
max_references = 50

# plot extension
plot_extension = "pdf"
supported_plot_extensions = ['emf', 'eps', 'pdf', 'png', 'ps', 'raw', 'rgba', 'svg', 'svgz']

###
output_dirpath = None
reference = None
genes = None
operons = None
labels = None
sam = None
bam = None
bed = None
forward_reads = None
reverse_reads = None
references_txt = None
json_output_dirpath = None


def check_python_version():
    if sys.version[0:3] not in SUPPORTED_PYTHON_VERSIONS:
        sys.stderr.write("ERROR! Python version " + sys.version[0:3] + " is not supported!\n" +\
                         "Supported versions are " + ", ".join(SUPPORTED_PYTHON_VERSIONS) + "\n")
        sys.exit(1)


def set_max_threads(logger):
    global max_threads
    if max_threads is None:
        try:
            import multiprocessing
            max_threads = max(1, multiprocessing.cpu_count() / 4)
        except (ImportError, NotImplementedError):
            logger.warning('Failed to determine the number of CPUs')
            max_threads = DEFAULT_MAX_THREADS
        logger.notice('Maximum number of threads is set to ' + str(max_threads) +
                      ' (use --threads option to set it manually)')


def quast_version():
    revision = None
    try:
        from quast_libs import version
        vers = version.__version__
        revision = version.__git_revision__
    except ImportError:
        with open(os.path.join(QUAST_HOME, 'VERSION.txt')) as f:
            lines = f.read().strip().split('\n')
        vers = lines[0]
        if len(lines) > 1:
            revision = lines[1]
    if revision:
        return vers + ', ' + revision
    else:
        return vers

    # version = None
    # build = None
    # if os.path.isfile(version_fpath):
    #     version_file = open(version_fpath)
    #     version = version_file.readline()
    #     if version:
    #         version = version.strip()
    #     else:
    #         version = "unknown"
    #     build = version_file.readline()
    #     if build:
    #         build = build.strip().lower()
    # if build:
    #     return version + ", " + build
    # else:
    #     return version


def print_version(meta=False):
    full_version = 'QUAST v' + quast_version()
    if meta:
        full_version += ' (MetaQUAST mode)'
    print >> sys.stderr, full_version


def usage(show_hidden=False, meta=False, short=True):
    print >> sys.stderr, ""
    if meta:
        print >> sys.stderr, 'MetaQUAST: QUality ASsessment Tool for Metagenome Assemblies'
    else:
        print >> sys.stderr, 'QUAST: QUality ASsessment Tool for Genome Assemblies'
    print >> sys.stderr, "Version:", quast_version(),

    print >> sys.stderr, "\n"
    print >> sys.stderr, 'Usage: python', sys.argv[0], '[options] <files_with_contigs>'
    print >> sys.stderr, ""

    print >> sys.stderr, "Options:"
    print >> sys.stderr, "-o  --output-dir  <dirname>   Directory to store all result files [default: quast_results/results_<datetime>]"
    if meta:
        print >> sys.stderr, "-R   <filename,filename,...>  Comma-separated list of reference genomes or directory with reference genomes"
        print >> sys.stderr, "--references-list <filename>  Text file with list of reference genomes for downloading from NCBI"
        print >> sys.stderr, "-G  --genes       <filename>  File with gene coordinates in the references"
    else:
        print >> sys.stderr, "-R                <filename>  Reference genome file"
        print >> sys.stderr, "-G  --genes       <filename>  File with gene coordinates in the reference"
    if not short:
        print >> sys.stderr, "-O  --operons     <filename>  File with operon coordinates in the reference"
    print >> sys.stderr, "-m  --min-contig  <int>       Lower threshold for contig length [default: %s]" % min_contig
    print >> sys.stderr, "-t  --threads     <int>       Maximum number of threads [default: 25% of CPUs]"

    print >> sys.stderr, ""
    if short:
        print >> sys.stderr, "These are basic options. To see the full list, use --help"
    else:
        print >> sys.stderr, "Advanced options:"
        print >> sys.stderr, "-s  --scaffolds                       Assemblies are scaffolds, split them and add contigs to the comparison"
        print >> sys.stderr, "-l  --labels \"label, label, ...\"      Names of assemblies to use in reports, comma-separated. If contain spaces, use quotes"
        print >> sys.stderr, "-L                                    Take assembly names from their parent directory names"
        if meta:
            print >> sys.stderr, "-f  --gene-finding                    Predict genes using MetaGeneMark"
        else:
            print >> sys.stderr, "-f  --gene-finding                    Predict genes (with GeneMark.hmm for prokaryotes (default), GeneMark-ES"
            print >> sys.stderr, "                                      for eukaryotes (--eukaryote), or MetaGeneMark for metagenomes (--meta)"
        print >> sys.stderr, "    --glimmer                         Predict genes with GlimmerHMM instead of GeneMark-ES"
        print >> sys.stderr, "    --gene-thresholds <int,int,...>   Comma-separated list of threshold lengths of genes to search with Gene Finding module"
        print >> sys.stderr, "                                      [default: %s]" % genes_lengths
        print >> sys.stderr, "-e  --eukaryote                       Genome is eukaryotic"
        if not meta:
            print >> sys.stderr, "    --meta                            Use MetaGeneMark for gene prediction"
            print >> sys.stderr, "    --est-ref-size <int>              Estimated reference size (for computing NGx metrics without a reference)"
        else:
            print >> sys.stderr, "    --max-ref-number <int>            Maximum number of references (per each assembly) to download after looking in SILVA database"
            print >> sys.stderr, "                                      Set 0 for not looking in SILVA at all [default: %s]" % max_references
        print >> sys.stderr, "    --gage                            Use GAGE (results are in gage_report.txt)"
        print >> sys.stderr, "    --contig-thresholds <int,int,...> Comma-separated list of contig length thresholds [default: %s]" % contig_thresholds
        print >> sys.stderr, "-u  --use-all-alignments              Compute genome fraction, # genes, # operons in QUAST v1.* style."
        print >> sys.stderr, "                                      By default, QUAST filters Nucmer\'s alignments to keep only best ones"
        print >> sys.stderr, "-i  --min-alignment <int>             Nucmer's parameter: the minimum alignment length [default: %s]" % min_alignment
        print >> sys.stderr, "    --min-identity <float>            Nucmer's parameter: the minimum alignment identity (80.0, 100.0) [default: %.1f]" % min_IDY
        print >> sys.stderr, "-a  --ambiguity-usage <none|one|all>  Use none, one, or all alignments (or aligned fragments internal overlaps) of a contig"
        print >> sys.stderr, "                                      when all of them are almost equally good (see --ambiguity-score) [default: %s]" % ambiguity_usage
        print >> sys.stderr, "    --ambiguity-score <float>         Score S for defining equally good alignments of a single contig. All alignments are sorted "
        print >> sys.stderr, "                                      by decreasing LEN * IDY% value. All alignments with LEN * IDY% < S * best(LEN * IDY%) are "
        print >> sys.stderr, "                                      discarded. S should be between 0.8 and 1.0 [default: %.2f]" % ambiguity_score
        if meta:
            print >> sys.stderr, "    --unique-mapping                  Disable --ambiguity-usage=all for the combined reference run,"
            print >> sys.stderr, "                                      i.e. use user-specified or default ('%s') value of --ambiguity-usage" % ambiguity_usage
        print >> sys.stderr, "    --strict-NA                       Break contigs in any misassembly event when compute NAx and NGAx"
        print >> sys.stderr, "                                      By default, QUAST breaks contigs only by extensive misassemblies (not local ones)"
        print >> sys.stderr, "-x  --extensive-mis-size  <int>       Lower threshold for extensive misassembly size. All relocations with inconsistency"
        print >> sys.stderr, "                                      less than extensive-mis-size are counted as local misassemblies [default: %s]" % extensive_misassembly_threshold
        print >> sys.stderr, "    --significant-part-size  <int>    Lower threshold for detecting partially unaligned contigs with both significant "
        print >> sys.stderr, "                                      aligned and unaligned parts [default: %s]" % significant_part_size
        print >> sys.stderr, "    --fragmented                      Reference genome may be fragmented into small pieces (e.g. scaffolded reference) "
        print >> sys.stderr, "    --plots-format  <str>             Save plots in specified format [default: %s]" % plot_extension
        print >> sys.stderr, "                                      Supported formats: %s" % ', '.join(supported_plot_extensions)
        print >> sys.stderr, "    --memory-efficient                Run Nucmer using one thread, separately per each assembly and each chromosome"
        print >> sys.stderr, "                                      This may significantly reduce memory consumption on large genomes"
        print >> sys.stderr, "-1  --reads1  <filename>              File with forward reads (in FASTQ format, may be gzipped)"
        print >> sys.stderr, "-2  --reads2  <filename>              File with reverse reads (in FASTQ format, may be gzipped)"
        print >> sys.stderr, "    --sam  <filename>                 SAM alignment file"
        print >> sys.stderr, "    --bam  <filename>                 BAM alignment file"
        print >> sys.stderr, "                                      Reads (or SAM/BAM file) are used for structural variation detection and coverage histogram building in Icarus"
        print >> sys.stderr, "    --sv-bedpe  <filename>            File with structural variations (in BEDPE format)"
        print >> sys.stderr, ""
        print >> sys.stderr, "Speedup options:"
        print >> sys.stderr, "    --no-check                        Do not check and correct input fasta files. Use at your own risk (see manual)"
        print >> sys.stderr, "    --no-plots                        Do not draw plots"
        print >> sys.stderr, "    --no-html                         Do not build html reports and Icarus viewers"
        print >> sys.stderr, "    --no-icarus                       Do not build Icarus viewers"
        print >> sys.stderr, "    --no-snps                         Do not report SNPs (may significantly reduce memory consumption on large genomes)"
        print >> sys.stderr, "    --no-gc                           Do not compute GC% and GC-distribution"
        print >> sys.stderr, "    --no-sv                           Do not run structural variation detection (make sense only if reads are specified)"
        print >> sys.stderr, "    --no-gzip                         Do not compress large output files"
        print >> sys.stderr, "    --fast                            A combination of all speedup options except --no-check"
        if show_hidden:
            print >> sys.stderr, ""
            print >> sys.stderr, "Hidden options:"
            print >> sys.stderr, "-d  --debug                 Run in a debug mode"
            print >> sys.stderr, "-c  --min-cluster   <int>   Nucmer's parameter: the minimum length of a cluster of matches [default: %s]" % min_cluster
            print >> sys.stderr, "-j  --save-json             Save the output also in the JSON format"
            print >> sys.stderr, "-J  --save-json-to <path>   Save the JSON output to a particular path"
            if meta:
                print >> sys.stderr, "--read-support              Use read coverage specified in contig names (SPAdes/Velvet style) for calculating Avg contig read support"
            print >> sys.stderr, "--no-icarus                 Do not create Icarus files"
            print >> sys.stderr, "--svg                       Draw contig alignment plot (in SVG format)"

        print >> sys.stderr, ""
        print >> sys.stderr, "Other:"
        print >> sys.stderr, "    --silent                          Do not print detailed information about each step to stdout (log file is not affected)"
        if meta:
            print >> sys.stderr, "    --test                            Run MetaQUAST on the data from the test_data folder, output to quast_test_output"
            print >> sys.stderr, "    --test-no-ref                     Run MetaQUAST without references on the data from the test_data folder, output to quast_test_output"
            print >> sys.stderr, "                                      MetaQUAST will download SILVA 16S rRNA database (~170 Mb) for searching reference genomes"
            print >> sys.stderr, "                                      Internet connection is required"
        else:
            print >> sys.stderr, "    --test                            Run QUAST on the data from the test_data folder, output to quast_test_output"
            print >> sys.stderr, "    --test-sv                         Run QUAST with structural variants detection on the data from the test_data folder, output to quast_test_output"
        print >> sys.stderr, "-h  --help                            Print full usage message"
        print >> sys.stderr, "-v  --version                         Print version"
        if show_hidden:
            print >> sys.stderr, "    --help-hidden                     Print this usage message with all hidden options"
    print >> sys.stderr, ""
