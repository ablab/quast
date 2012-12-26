############################################################################
# Copyright (c) 2011-2012 Saint-Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################

import datetime

contig_thresholds = "0,1000"
min_contig = 500
#genes_lengths = "0,300,600,900,1200,1500,1800,2100,2400,2700,3000"
genes_lengths = "0,300,1500,3000"

default_results_root_dirname = "quast_results"
output_dirname = "results_" + datetime.datetime.now().strftime('%Y_%m_%d_%H_%M_%S')
save_json = False
default_json_dirname = "json"

logfile = "quast.log"
corrected_dirname = "corrected_input"
plots_filename = "plots.pdf"

scaffolds = False
Ns_break_threshold = 10
debug = False
draw_plots = True
html_report = True
make_latest_symlink = True
reference = ''
genes = ''
operons = ''
with_gage = False
prokaryote = True # former cyclic
only_best_alignments = False
threads = None
mincluster = 65
estimated_reference_size = None

long_options = "output-dir= save-json-to= genes= operons= reference= contig-thresholds= min-contig= " \
               "genemark-thresholds= save-json gage eukaryote no-plots no-html help debug " \
               "only-best-alignments scaffolds threads= mincluster= est-ref-size=".split()
short_options = "o:G:O:R:t:M:k:J:jgehdsbT:c:r:"


# other constants. Can't be changed by command-line options

# genome analyzer
min_gap_size = 50 # for calculating number or gaps in genome coverage
min_gene_overlap = 100 # to partial genes/operons finding

# basic_stats
GC_bin_size = 1.0

# plotter and maybe other modules in the future
legend_names = None