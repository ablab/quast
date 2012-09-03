############################################################################
# Copyright (c) 2011-2012 Saint-Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################

import datetime

contig_thresholds = "500,1000"
min_contig = 200
#genes_lengths = "0,300,600,900,1200,1500,1800,2100,2400,2700,3000"
genes_lengths = "0,300,1500,3000"

default_results_root_dirname = "quast_results"
output_dirname = "results_" + datetime.datetime.now().strftime('%Y_%m_%d_%H_%M_%S')
save_json = False
default_json_dirname = "json"

logfile = "quast.log"
corrected_dir = "corrected_input"
plots_filename = "plots.pdf"

draw_plots = True
make_latest_symlink = True
reference = ''
genes = ''
operons = ''
with_gage = False
cyclic = True
rc = False

long_options = "output-dir= save-json-to= genes= operons= reference= contig-thresholds= min-contig= genemark-thresholds= save-json gage not-circular disable-rc plain-report-no-plots help".split()
short_options = "o:G:O:R:t:M:e:J:jpgndh"

