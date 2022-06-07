############################################################################
# Copyright (c) 2015-2022 Saint Petersburg State University
# Copyright (c) 2011-2015 Saint Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################

import datetime
import os
import platform
import sys
from distutils.version import LooseVersion
from os.path import basename

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

# default values for options
contig_thresholds = "0,1000,5000,10000,25000,50000"
x_for_additional_Nx = 90
min_contig = None
DEFAULT_MIN_CONTIG = 500
genes_lengths = "0,300,1500,3000"
ref_fpath = ''
prokaryote = True  # former cyclic
is_fungus = False
gene_finding = False
rna_gene_finding = False
ambiguity_usage = 'one'
ambiguity_score = 0.99
meta_ambiguity_score = 0.9
reuse_combined_alignments = False
alignments_for_reuse_dirpath = None
use_all_alignments = False
max_threads = None
min_alignment = None
DEFAULT_MIN_ALIGNMENT = 65
min_IDY = None
DEFAULT_MIN_IDY = 95.0
META_MIN_IDY = 90.0
estimated_reference_size = None
strict_NA = False
split_scaffolds = False
draw_plots = True
draw_circos = False
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
no_read_stats = False
show_snps = True
glimmer = False
is_combined_ref = False
check_for_fragmented_ref = False
MAX_REFERENCE_FRAGMENTS = 100  # for metaQUAST, do not download too fragmented references
unaligned_part_size = 500
unaligned_mis_threshold = 0.5  # former 'umt' in analyze_contigs.py
all_labels_from_dirs = False
run_busco = False
large_genome = False
use_kmc = False
report_all_metrics = False

# ideal assembly section
optimal_assembly = False
optimal_assembly_insert_size = 'auto'
optimal_assembly_default_IS = 255
optimal_assembly_min_IS = 63
optimal_assembly_max_IS = 1023

# print in stdout only main information
silent = False

# QUAST is run by AGB
is_agb_mode = False

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
html_aux_dir = "report_html_aux"
contig_report_fname_pattern = 'contigs_report_%s'
icarus_report_fname_pattern = 'all_alignments_%s.tsv'
detailed_contigs_reports_dirname = 'contigs_reports'
aligner_output_dirname = 'minimap_output'
optimal_assembly_basename = 'upper_bound_assembly'

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
reads_stats_dirname = 'reads_stats'
coverage_thresholds = [1, 5, 10]
trivial_deletions_fname = 'trivial_deletions.bed'
sv_bed_fname = 'structural_variations.bed'
used_colors = None
used_ls = None
MAX_PE_IS = 1000  # for separating mate-pairs from paired-ends; the former should be excluded from physical coverage calculation
cov_fpath = None
phys_cov_fpath = None

# dirnames
busco_dirname = 'busco_stats'

# for Icarus
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
ICARUS_MAX_CHROMOSOMES = 50
max_contigs_num_for_size_viewer = 1000
min_contig_for_size_viewer = 10000
contig_len_delta = 0.05
min_similar_contig_size = 10000

# other settings (mostly constants). Can't be changed by command-line options

# indels and misassemblies
SHORT_INDEL_THRESHOLD = 5 # for separating short and long indels
SPLIT_ALIGN_THRESHOLD = 20 # for splitting low-identity alignments by the indels/mismatches
local_misassembly_min_length = 200  # for separating indels and local misassemblies (former MAX_INDEL_LENGTH)
DEFAULT_EXT_MIS_SIZE = 1000
extensive_misassembly_threshold = None  # for separating local and extensive misassemblies (relocation)
fragmented_max_indent = local_misassembly_min_length # for fake translocation in fragmented reference

# large genome params
LARGE_EXTENSIVE_MIS_THRESHOLD = 7000
LARGE_MIN_CONTIG = 3000
LARGE_MIN_ALIGNMENT = 500

# Upperbound
upperbound_min_connections = None
MIN_CONNECT_MP = 2  # minimal reads for scaffolding
MIN_CONNECT_LR = 1

# k-mer stats
unique_kmer_len = 101

# BSS fine-tuning params
BSS_MAX_SETS_NUMBER = 10  # for ambiguous contigs
BSS_critical_number_of_aligns = 100
BSS_EXTENSIVE_PENALTY = 250
BSS_LOCAL_PENALTY = 25

# for parallelization
DEFAULT_MAX_THREADS = 4  # this value is used if QUAST fails to determine number of CPUs
assemblies_num = 1
memory_efficient = False
space_efficient = False

# genome analyzer
analyze_gaps = True
min_gap_size = 50  # for calculating number or gaps in genome coverage
min_gene_overlap = 100  # to partial genes/operons finding
ALL_FEATURES_TYPE = 'ANY'

# basic_stats
GC_bin_size = 1.0
GC_contig_bin_size = 5
GC_window_size = 100
GC_window_size_large = 600

# plotter and reporting and maybe other modules in the future
assembly_labels_by_fpath = {}
assemblies_fpaths = []
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
features = dict()
labels = None
sam_fpaths = None
bam_fpaths = None
reference_sam = None
reference_bam = None
bed = None
reads_fpaths = []
forward_reads = []
reverse_reads = []
mp_forward_reads = []
mp_reverse_reads = []
interlaced_reads = []
mp_interlaced_reads = []
paired_reads = []
mate_pairs = []
unpaired_reads = []
pacbio_reads = []
nanopore_reads = []
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


def get_mode(binary_path=None):
    if binary_path is None:
        binary_path = sys.argv[0]
    mode = 'default'
    if basename(binary_path).startswith("metaquast"):
        mode = 'meta'
    elif basename(binary_path).startswith("quast-lg") or large_genome:
        mode = 'large'
    return mode


def print_version(mode=None):
    if mode is None:
        mode = get_mode()
    full_version = 'QUAST v' + quast_version()
    if mode == 'meta':
        full_version += ' (MetaQUAST mode)'
    elif mode == 'large':
        full_version += ' (QUAST-LG mode)'
    sys.stdout.write(full_version + '\n')
    sys.stdout.flush()


def usage(show_hidden=False, mode=None, short=True, stream=sys.stdout):
    if mode is None:
        mode = get_mode()
    stream.write("")
    meta = True if mode == 'meta' else False
    if mode == 'meta':
        stream.write('MetaQUAST: Quality Assessment Tool for Metagenome Assemblies\n')
    elif mode == 'large':
        stream.write('QUAST-LG: Quality Assessment Tool for Large Genome Assemblies\n')
    else:
        stream.write('QUAST: Quality Assessment Tool for Genome Assemblies\n')
    stream.write("Version: " + quast_version() + '\n')

    # defaults which depend on mode (large or not, meta or not)
    if mode == 'large':
        m_default, i_default, x_default = LARGE_MIN_CONTIG, LARGE_MIN_ALIGNMENT, LARGE_EXTENSIVE_MIS_THRESHOLD
    else:
        m_default, i_default, x_default = DEFAULT_MIN_CONTIG, DEFAULT_MIN_ALIGNMENT, DEFAULT_EXT_MIS_SIZE
    if mode == 'meta':
        min_idy_default = META_MIN_IDY
    else:
        min_idy_default = DEFAULT_MIN_IDY

    stream.write("\n")
    stream.write('Usage: python ' + sys.argv[0] + ' [options] <files_with_contigs>\n')
    stream.write("\n")

    stream.write("Options:\n")
    stream.write("-o  --output-dir  <dirname>       Directory to store all result files [default: quast_results/results_<datetime>]\n")
    if meta:
        stream.write("-r   <filename,filename,...>      Comma-separated list of reference genomes or directory with reference genomes\n")
        stream.write("--references-list <filename>      Text file with list of reference genome names for downloading from NCBI\n")
        stream.write("-g  --features [type:]<filename>  File with genomic feature coordinates in the references (GFF, BED, NCBI or TXT)\n")
        stream.write("                                  Optional 'type' can be specified for extracting only a specific feature type from GFF\n")
    else:
        stream.write("-r                <filename>      Reference genome file\n")
        stream.write("-g  --features [type:]<filename>  File with genomic feature coordinates in the reference (GFF, BED, NCBI or TXT)\n")
        stream.write("                                  Optional 'type' can be specified for extracting only a specific feature type from GFF\n")
    stream.write("-m  --min-contig  <int>           Lower threshold for contig length [default: %d]\n" % m_default)
    stream.write("-t  --threads     <int>           Maximum number of threads [default: 25% of CPUs]\n")

    stream.write("\n")
    if short:
        stream.write("These are basic options. To see the full list, use --help\n")
    else:
        stream.write("Advanced options:\n")
        stream.write("-s  --split-scaffolds                 Split assemblies by continuous fragments of N's and add such \"contigs\" to the comparison\n")
        stream.write("-l  --labels \"label, label, ...\"      Names of assemblies to use in reports, comma-separated. If contain spaces, use quotes\n")
        stream.write("-L                                    Take assembly names from their parent directory names\n")
        if mode != 'large':
            stream.write("-e  --eukaryote                       Genome is eukaryotic (primarily affects gene prediction)\n")
            stream.write("    --fungus                          Genome is fungal (primarily affects gene prediction)\n")
            stream.write("    --large                           Use optimal parameters for evaluation of large genomes\n")
            stream.write("                                      In particular, imposes '-e -m %d -i %d -x %d' (can be overridden manually)\n" %
                         (LARGE_MIN_CONTIG, LARGE_MIN_ALIGNMENT, LARGE_EXTENSIVE_MIS_THRESHOLD))
        stream.write("-k  --k-mer-stats                     Compute k-mer-based quality metrics (recommended for large genomes)\n"
                     "                                      This may significantly increase memory and time consumption on large genomes\n")
        stream.write("    --k-mer-size                      Size of k used in --k-mer-stats [default: %d]\n" % unique_kmer_len)
        stream.write("    --circos                          Draw Circos plot\n")
        if mode == 'meta':
            stream.write("-f  --gene-finding                    Predict genes using MetaGeneMark\n")
        elif mode == 'large':
            stream.write("-f  --gene-finding                    Predict genes using GeneMark-ES\n")
        else:
            stream.write("-f  --gene-finding                    Predict genes using GeneMarkS (prokaryotes, default) or GeneMark-ES (eukaryotes, use --eukaryote)\n")
        if not meta:
            stream.write("    --mgm                             Use MetaGeneMark for gene prediction (instead of the default finder, see above)\n")
        stream.write("    --glimmer                         Use GlimmerHMM for gene prediction (instead of the default finder, see above)\n")
        stream.write("    --gene-thresholds <int,int,...>   Comma-separated list of threshold lengths of genes to search with Gene Finding module\n")
        stream.write("                                      [default: %s]\n" % genes_lengths)
        stream.write("    --rna-finding                     Predict ribosomal RNA genes using Barrnap\n")
        stream.write("-b  --conserved-genes-finding         Count conserved orthologs using BUSCO (only on Linux)\n")
        stream.write("    --operons  <filename>             File with operon coordinates in the reference (GFF, BED, NCBI or TXT)\n")
        if not meta:
            stream.write("    --est-ref-size <int>              Estimated reference size (for computing NGx metrics without a reference)\n")
        else:
            stream.write("    --max-ref-number <int>            Maximum number of references (per each assembly) to download after looking in SILVA database.\n")
            stream.write("                                      Set 0 for not looking in SILVA at all [default: %s]\n" % max_references)
            stream.write("    --blast-db <filename>             Custom BLAST database (.nsq file). By default, MetaQUAST searches references in SILVA database\n")
            stream.write("    --use-input-ref-order             Use provided order of references in MetaQUAST summary plots (default order: by the best average value)\n")
        stream.write("    --contig-thresholds <int,int,...> Comma-separated list of contig length thresholds [default: %s]\n" % contig_thresholds)
        stream.write("    --x-for-Nx <int>                  Value of 'x' for Nx, Lx, etc metrics reported in addition to N50, L50, etc (0, 100) [default: %s]\n" % x_for_additional_Nx)
        if meta:
            stream.write("    --reuse-combined-alignments       Reuse the alignments from the combined_reference stage on runs_per_reference stages.\n")
        stream.write("-u  --use-all-alignments              Compute genome fraction, # genes, # operons in QUAST v1.* style.\n")
        stream.write("                                      By default, QUAST filters Minimap\'s alignments to keep only best ones\n")
        stream.write("-i  --min-alignment <int>             The minimum alignment length [default: %s]\n" % i_default)
        stream.write("    --min-identity <float>            The minimum alignment identity (80.0, 100.0) [default: %.1f]\n" % min_idy_default)
        stream.write("-a  --ambiguity-usage <none|one|all>  Use none, one, or all alignments of a contig when all of them\n")
        stream.write("                                      are almost equally good (see --ambiguity-score) [default: %s]\n" % ambiguity_usage)
        stream.write("    --ambiguity-score <float>         Score S for defining equally good alignments of a single contig. All alignments are sorted \n")
        stream.write("                                      by decreasing LEN * IDY% value. All alignments with LEN * IDY% < S * best(LEN * IDY%) are \n")
        stream.write("                                      discarded. S should be between 0.8 and 1.0 [default: %.2f]\n" % ambiguity_score)
        if meta:
            stream.write("    --unique-mapping                  Disable --ambiguity-usage=all for the combined reference run,\n")
            stream.write("                                      i.e. use user-specified or default ('%s') value of --ambiguity-usage\n" % ambiguity_usage)
        stream.write("    --strict-NA                       Break contigs in any misassembly event when compute NAx and NGAx.\n")
        stream.write("                                      By default, QUAST breaks contigs only by extensive misassemblies (not local ones)\n")
        stream.write("-x  --extensive-mis-size  <int>       Lower threshold for extensive misassembly size. All relocations with inconsistency\n")
        stream.write("                                      less than extensive-mis-size are counted as local misassemblies [default: %s]\n" % x_default)
        stream.write("    --local-mis-size  <int>           Lower threshold on local misassembly size. Local misassemblies with inconsistency\n")
        stream.write("                                      less than local-mis-size are counted as (long) indels [default: %s]\n" % local_misassembly_min_length)
        stream.write("    --scaffold-gap-max-size  <int>    Max allowed scaffold gap length difference. All relocations with inconsistency\n")
        stream.write("                                      less than scaffold-gap-size are counted as scaffold gap misassemblies [default: %s]\n" % scaffolds_gap_threshold)
        stream.write("    --unaligned-part-size  <int>      Lower threshold for detecting partially unaligned contigs. Such contig should have\n")
        stream.write("                                      at least one unaligned fragment >= the threshold [default: %s]\n" % unaligned_part_size)
        stream.write("    --skip-unaligned-mis-contigs      Do not distinguish contigs with >= 50% unaligned bases as a separate group\n")
        stream.write("                                      By default, QUAST does not count misassemblies in them\n")
        stream.write("    --fragmented                      Reference genome may be fragmented into small pieces (e.g. scaffolded reference) \n")
        stream.write("    --fragmented-max-indent  <int>    Mark translocation as fake if both alignments are located no further than N bases \n")
        stream.write("                                      from the ends of the reference fragments [default: %s]\n" % local_misassembly_min_length)
        stream.write("                                      Requires --fragmented option\n")
        stream.write("    --upper-bound-assembly            Simulate upper bound assembly based on the reference genome and reads\n")
        stream.write("    --upper-bound-min-con  <int>      Minimal number of 'connecting reads' needed for joining upper bound contigs into a scaffold\n")
        stream.write("                                      [default: %d for mate-pairs and %d for long reads]\n" % (MIN_CONNECT_MP, MIN_CONNECT_LR))
        stream.write("    --est-insert-size  <int>          Use provided insert size in upper bound assembly simulation [default: auto detect from reads or %d]\n" % optimal_assembly_default_IS)
        stream.write("    --report-all-metrics              Keep all quality metrics in the main report even if their values are '-' for all assemblies or \n"
                     "                                      if they are not applicable (e.g., reference-based metrics in the no-reference mode)\n")
        stream.write("    --plots-format  <str>             Save plots in specified format [default: %s].\n" % plot_extension)
        stream.write("                                      Supported formats: %s\n" % ', '.join(supported_plot_extensions))
        stream.write("    --memory-efficient                Run everything using one thread, separately per each assembly.\n")
        stream.write("                                      This may significantly reduce memory consumption on large genomes\n")
        stream.write("    --space-efficient                 Create only reports and plots files. Aux files including .stdout, .stderr, .coords will not be created.\n")
        stream.write("                                      This may significantly reduce space consumption on large genomes. Icarus viewers also will not be built\n")
        stream.write("-1  --pe1     <filename>              File with forward paired-end reads (in FASTQ format, may be gzipped)\n")
        stream.write("-2  --pe2     <filename>              File with reverse paired-end reads (in FASTQ format, may be gzipped)\n")
        stream.write("    --pe12    <filename>              File with interlaced forward and reverse paired-end reads. (in FASTQ format, may be gzipped)\n")
        stream.write("    --mp1     <filename>              File with forward mate-pair reads (in FASTQ format, may be gzipped)\n")
        stream.write("    --mp2     <filename>              File with reverse mate-pair reads (in FASTQ format, may be gzipped)\n")
        stream.write("    --mp12    <filename>              File with interlaced forward and reverse mate-pair reads (in FASTQ format, may be gzipped)\n")
        stream.write("    --single  <filename>              File with unpaired short reads (in FASTQ format, may be gzipped)\n")
        stream.write("    --pacbio     <filename>           File with PacBio reads (in FASTQ format, may be gzipped)\n")
        stream.write("    --nanopore   <filename>           File with Oxford Nanopore reads (in FASTQ format, may be gzipped)\n")
        stream.write("    --ref-sam <filename>              SAM alignment file obtained by aligning reads to reference genome file\n")
        stream.write("    --ref-bam <filename>              BAM alignment file obtained by aligning reads to reference genome file\n")
        stream.write("    --sam     <filename,filename,...> Comma-separated list of SAM alignment files obtained by aligning reads to assemblies\n"
                         "                                      (use the same order as for files with contigs)\n")
        stream.write("    --bam     <filename,filename,...> Comma-separated list of BAM alignment files obtained by aligning reads to assemblies\n"
                     "                                      (use the same order as for files with contigs)\n")
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
        # stream.write("    --no-gzip                         Do not compress large output files\n")m
        stream.write("    --no-read-stats                   Do not align reads to assemblies\n"
                     "                                      Reads will be aligned to reference and used for coverage analysis,\n"
                     "                                      upper bound assembly simulation, and structural variation detection.\n"
                     "                                      Use this option if you do not need read statistics for assemblies.\n")
        stream.write("    --fast                            A combination of all speedup options except --no-check\n")
        if show_hidden:
            stream.write("\n")
            stream.write("Hidden options:\n")
            stream.write("-d  --debug                 Run in a debug mode\n")
            stream.write("--no-portable-html          Do not embed CSS and JS files in HTML report\n")
            stream.write("-j  --save-json             Save the output also in the JSON format\n")
            stream.write("-J  --save-json-to <path>   Save the JSON output to a particular path\n")
            if meta:
                stream.write("--read-support              Use read coverage specified in contig names (SPAdes/Velvet style) for calculating Avg contig read support\n")
            stream.write("--cov  <filename>           File with read coverage (for Icarus alignment viewer)\n")
            stream.write("--phys-cov  <filename>      File with physical coverage (for Icarus alignment viewer)\n")

        stream.write("\n")
        stream.write("Other:\n")
        stream.write("    --silent                          Do not print detailed information about each step to stdout (log file is not affected)\n")
        if meta:
            stream.write("    --test                            Run MetaQUAST on the data from the test_data folder, output to quast_test_output\n")
            stream.write("    --test-no-ref                     Run MetaQUAST without references on the data from the test_data folder, output to quast_test_output.\n")
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
