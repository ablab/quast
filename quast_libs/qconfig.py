############################################################################
# Copyright (c) 2015-2017 Saint Petersburg State University
# Copyright (c) 2011-2015 Saint Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################

import datetime
import os
import platform
import sys
from distutils.version import LooseVersion

QUAST_HOME = os.path.abspath(os.path.dirname(os.path.dirname(os.path.realpath(__file__))))
PACKAGE_NAME = 'quast_libs'
LIBS_LOCATION = os.path.join(QUAST_HOME, PACKAGE_NAME)
GIT_ROOT_URL = 'https://raw.githubusercontent.com/ablab/quast/master/'

SUPPORTED_PYTHON_VERSIONS = ['2.5+', '3.3+']  # major.minor format only, close ('-') and open ('+') ranges allowed

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
metagenemark = False
debug = False
portable_html = True
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
unaligned_part_size = 500
all_labels_from_dirs = False
force_nucmer = False

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
use_input_ref_order = False

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
cov_fpath = None
phys_cov_fpath = None

# other settings (mostly constants). Can't be changed by command-line options

# indels and misassemblies
SHORT_INDEL_THRESHOLD = 5 # for separating short and long indels
MAX_INDEL_LENGTH = 85  # for separating indels and local misassemblies (Nucmer default value)
extensive_misassembly_threshold = 1000  # for separating local and extensive misassemblies (relocation)
fragmented_max_indent = MAX_INDEL_LENGTH # for fake translocation in fragmented reference
BSS_MAX_SETS_NUMBER = 10

gap_filled_ns_threshold = 0.95

# for parallelization
DEFAULT_MAX_THREADS = 4  # this value is used if QUAST fails to determine number of CPUs
assemblies_num = 1
memory_efficient = False
space_efficient = False

# genome analyzer
min_gap_size = 50  # for calculating number or gaps in genome coverage
min_gene_overlap = 100  # to partial genes/operons finding

# basic_stats
GC_bin_size = 1.0
GC_contig_bin_size = 5

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
custom_blast_db_fpath = None
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
    def __next_version(version):
        components = version.split('.')
        for i in reversed(range(len(components))):
            if components[i].isdigit():
                components[i] = str(int(components[i]) + 1)
                break
        return '.'.join(components)

    current_version = sys.version.split()[0]
    supported_versions_msg = []
    for supported_versions in SUPPORTED_PYTHON_VERSIONS:
        major = supported_versions[0]
        if '-' in supported_versions:  # range
            min_inc, max_inc = supported_versions.split('-')
        elif supported_versions.endswith('+'):  # half open range
            min_inc, max_inc = supported_versions[:-1], major
        else:  # exact version
            min_inc = max_inc = supported_versions
        max_exc = __next_version(max_inc)
        supported_versions_msg.append("Python%s: %s" % (major, supported_versions.replace('+', " and higher")))
        if LooseVersion(min_inc) <= LooseVersion(current_version) < LooseVersion(max_exc):
            return True
    sys.stderr.write("ERROR! Python version " + current_version + " is not supported!\n" +
                     "Supported versions are " + ", ".join(supported_versions_msg) + "\n")
    sys.exit(1)


def set_max_threads(logger):
    global max_threads
    if max_threads is None:
        try:
            import multiprocessing
            max_threads = max(1, multiprocessing.cpu_count() // 4)
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
        version_file = open(os.path.join(QUAST_HOME, 'VERSION.txt'))
        lines = version_file.read().strip().split('\n')
        version_file.close()
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
    sys.stdout.write(full_version + '\n')
    sys.stdout.flush()


def usage(show_hidden=False, meta=False, short=True, stream=sys.stdout):
    stream.write("")
    if meta:
        stream.write('MetaQUAST: QUality ASsessment Tool for Metagenome Assemblies\n')
    else:
        stream.write('QUAST: QUality ASsessment Tool for Genome Assemblies\n')
    stream.write("Version: " + quast_version() + '\n')

    stream.write("\n")
    stream.write('Usage: python ' + sys.argv[0] + ' [options] <files_with_contigs>\n')
    stream.write("\n")

    stream.write("Options:\n")
    stream.write("-o  --output-dir  <dirname>   Directory to store all result files [default: quast_results/results_<datetime>]\n")
    if meta:
        stream.write("-R   <filename,filename,...>  Comma-separated list of reference genomes or directory with reference genomes\n")
        stream.write("--references-list <filename>  Text file with list of reference genomes for downloading from NCBI\n")
        stream.write("-G  --genes       <filename>  File with gene coordinates in the references (GFF, BED, NCBI or TXT)\n")
    else:
        stream.write("-R                <filename>  Reference genome file\n")
        stream.write("-G  --genes       <filename>  File with gene coordinates in the reference (GFF, BED, NCBI or TXT)\n")
    if not short:
        stream.write("-O  --operons     <filename>  File with operon coordinates in the reference (GFF, BED, NCBI or TXT)\n")
    stream.write("-m  --min-contig  <int>       Lower threshold for contig length [default: %s]\n" % min_contig)
    stream.write("-t  --threads     <int>       Maximum number of threads [default: 25% of CPUs]\n")

    stream.write("\n")
    if short:
        stream.write("These are basic options. To see the full list, use --help\n")
    else:
        stream.write("Advanced options:\n")
        stream.write("-s  --scaffolds                       Assemblies are scaffolds, split them and add contigs to the comparison\n")
        stream.write("-l  --labels \"label, label, ...\"      Names of assemblies to use in reports, comma-separated. If contain spaces, use quotes\n")
        stream.write("-L                                    Take assembly names from their parent directory names\n")
        stream.write("-e  --eukaryote                       Genome is eukaryotic\n")
        if meta:
            stream.write("-f  --gene-finding                    Predict genes using MetaGeneMark\n")
        else:
            stream.write("-f  --gene-finding                    Predict genes using GeneMarkS (prokaryotes, default) or GeneMark-ES (eukaryotes, use --eukaryote)\n")
        stream.write("    --glimmer                         Use GlimmerHMM for gene prediction (instead of the default finder, see above)\n")
        if not meta:
            stream.write("    --mgm                             Use MetaGeneMark for gene prediction (instead of the default finder, see above)\n")
        stream.write("    --gene-thresholds <int,int,...>   Comma-separated list of threshold lengths of genes to search with Gene Finding module\n")
        stream.write("                                      [default: %s]\n" % genes_lengths)
        if not meta:
            stream.write("    --est-ref-size <int>              Estimated reference size (for computing NGx metrics without a reference)\n")
        else:
            stream.write("    --max-ref-number <int>            Maximum number of references (per each assembly) to download after looking in SILVA database\n")
            stream.write("                                      Set 0 for not looking in SILVA at all [default: %s]\n" % max_references)
            stream.write("    --blast-db <filename>             Custom BLAST database (.nsq file). By default, MetaQUAST searches references in SILVA database\n")
            stream.write("    --use-input-ref-order             Use provided order of references in MetaQUAST summary plots (default order: by the best average value).\n")
        stream.write("    --gage                            Use GAGE (results are in gage_report.txt)\n")
        stream.write("    --contig-thresholds <int,int,...> Comma-separated list of contig length thresholds [default: %s]\n" % contig_thresholds)
        stream.write("-u  --use-all-alignments              Compute genome fraction, # genes, # operons in QUAST v1.* style.\n")
        stream.write("                                      By default, QUAST filters Nucmer\'s alignments to keep only best ones\n")
        stream.write("-i  --min-alignment <int>             Nucmer's parameter: the minimum alignment length [default: %s]\n" % min_alignment)
        stream.write("    --min-identity <float>            Nucmer's parameter: the minimum alignment identity (80.0, 100.0) [default: %.1f]\n" % min_IDY)
        stream.write("-a  --ambiguity-usage <none|one|all>  Use none, one, or all alignments of a contig when all of them\n")
        stream.write("                                      are almost equally good (see --ambiguity-score) [default: %s]\n" % ambiguity_usage)
        stream.write("    --ambiguity-score <float>         Score S for defining equally good alignments of a single contig. All alignments are sorted \n")
        stream.write("                                      by decreasing LEN * IDY% value. All alignments with LEN * IDY% < S * best(LEN * IDY%) are \n")
        stream.write("                                      discarded. S should be between 0.8 and 1.0 [default: %.2f]\n" % ambiguity_score)
        if meta:
            stream.write("    --unique-mapping                  Disable --ambiguity-usage=all for the combined reference run,\n")
            stream.write("                                      i.e. use user-specified or default ('%s') value of --ambiguity-usage\n" % ambiguity_usage)
        stream.write("    --strict-NA                       Break contigs in any misassembly event when compute NAx and NGAx\n")
        stream.write("                                      By default, QUAST breaks contigs only by extensive misassemblies (not local ones)\n")
        stream.write("-x  --extensive-mis-size  <int>       Lower threshold for extensive misassembly size. All relocations with inconsistency\n")
        stream.write("                                      less than extensive-mis-size are counted as local misassemblies [default: %s]\n" % extensive_misassembly_threshold)
        stream.write("    --scaffold-gap-max-size  <int>    Max allowed scaffold gap length difference. All relocations with inconsistency\n")
        stream.write("                                      less than scaffold-gap-size are counted as scaffold gap misassemblies [default: %s]\n" % scaffolds_gap_threshold)
        stream.write("    --unaligned-part-size  <int>      Lower threshold for detecting partially unaligned contigs. Such contig should have\n")
        stream.write("                                      at least one unaligned fragment >= the threshold [default: %s]\n" % unaligned_part_size)
        stream.write("    --fragmented                      Reference genome may be fragmented into small pieces (e.g. scaffolded reference) \n")
        stream.write("    --fragmented-max-indent  <int>    Mark translocation as fake if both alignments are located no further than N bases \n")
        stream.write("                                      from the ends of the reference fragments [default: %s]\n" % MAX_INDEL_LENGTH)
        stream.write("                                      Requires --fragmented option.\n")
        stream.write("    --plots-format  <str>             Save plots in specified format [default: %s]\n" % plot_extension)
        stream.write("                                      Supported formats: %s\n" % ', '.join(supported_plot_extensions))
        stream.write("    --memory-efficient                Run Nucmer using one thread, separately per each assembly and each chromosome\n")
        stream.write("                                      This may significantly reduce memory consumption on large genomes\n")
        stream.write("    --space-efficient                 Create only reports and plots files. .stdout, .stderr, .coords and other aux files will not be created\n")
        stream.write("                                      This may significantly reduce space consumption on large genomes. Icarus viewers also will not be built\n")
        stream.write("-1  --reads1  <filename>              File with forward reads (in FASTQ format, may be gzipped)\n")
        stream.write("-2  --reads2  <filename>              File with reverse reads (in FASTQ format, may be gzipped)\n")
        stream.write("    --sam  <filename>                 SAM alignment file\n")
        stream.write("    --bam  <filename>                 BAM alignment file\n")
        stream.write("                                      Reads (or SAM/BAM file) are used for structural variation detection and\n")
        stream.write("                                      coverage histogram building in Icarus\n")
        stream.write("    --sv-bedpe  <filename>            File with structural variations (in BEDPE format)\n")
        stream.write("\n")
        stream.write("Speedup options:\n")
        stream.write("    --no-check                        Do not check and correct input fasta files. Use at your own risk (see manual)\n")
        stream.write("    --no-plots                        Do not draw plots\n")
        stream.write("    --no-html                         Do not build html reports and Icarus viewers\n")
        stream.write("    --no-icarus                       Do not build Icarus viewers\n")
        stream.write("    --no-snps                         Do not report SNPs (may significantly reduce memory consumption on large genomes)\n")
        stream.write("    --no-gc                           Do not compute GC% and GC-distribution\n")
        stream.write("    --no-sv                           Do not run structural variation detection (make sense only if reads are specified)\n")
        stream.write("    --no-gzip                         Do not compress large output files\n")
        stream.write("    --fast                            A combination of all speedup options except --no-check\n")
        if show_hidden:
            stream.write("\n")
            stream.write("Hidden options:\n")
            stream.write("-d  --debug                 Run in a debug mode\n")
            stream.write("--no-portable-html          Do not embed CSS and JS files in HTML report\n")
            stream.write("-c  --min-cluster   <int>   Nucmer's parameter: the minimum length of a cluster of matches [default: %s]\n" % min_cluster)
            stream.write("-j  --save-json             Save the output also in the JSON format\n")
            stream.write("-J  --save-json-to <path>   Save the JSON output to a particular path\n")
            if meta:
                stream.write("--read-support                   Use read coverage specified in contig names (SPAdes/Velvet style) for calculating Avg contig read support\n")
            stream.write("--cov  <filename>                File with read coverage (for Icarus alignment viewer)\n")
            stream.write("--phys-cov  <filename>           File with physical coverage (for Icarus alignment viewer)\n")
            stream.write("--no-icarus                      Do not create Icarus files\n")
            stream.write("--svg                            Draw contig alignment plot (in SVG format)\n")
            stream.write("--force-nucmer                   Use nucmer instead of E-MEM for aligning contigs. \n")

        stream.write("\n")
        stream.write("Other:\n")
        stream.write("    --silent                          Do not print detailed information about each step to stdout (log file is not affected)\n")
        if meta:
            stream.write("    --test                            Run MetaQUAST on the data from the test_data folder, output to quast_test_output\n")
            stream.write("    --test-no-ref                     Run MetaQUAST without references on the data from the test_data folder, output to quast_test_output\n")
            stream.write("                                      MetaQUAST will download SILVA 16S rRNA database (~170 Mb) for searching reference genomes\n")
            stream.write("                                      Internet connection is required\n")
        else:
            stream.write("    --test                            Run QUAST on the data from the test_data folder, output to quast_test_output\n")
            stream.write("    --test-sv                         Run QUAST with structural variants detection on the data from the test_data folder,\n")
            stream.write("                                      output to quast_test_output\n")
        stream.write("-h  --help                            Print full usage message\n")
        stream.write("-v  --version                         Print version\n")
        if show_hidden:
            stream.write("    --help-hidden                     Print this usage message with all hidden options\n")
    stream.write("\n")
    stream.write("Online QUAST manual is available at http://quast.sf.net/manual\n")
    stream.flush()
