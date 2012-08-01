############################################################################
# Copyright (c) 2011-2012 Saint-Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################

import datetime

contig_thresholds = "201,1001"
min_contig = 0
genes_lengths = "0,300,600,900,1200,1500,1800,2100,2400,2700,3000"
orf_lengths = "200"

default_results_root_dirname = "quast_results"
output_dirname = "results_" + datetime.datetime.now().strftime('%Y_%m_%d_%H_%M_%S')
save_json = False
default_json_dirname = "json"

logfile = "quast.log"
corrected_dir = "corrected_input"
report_basename = "report"
gage_report_basename = "gage"
extra_report_filename = "extra_report.txt"
plots_filename = "plots.pdf"

draw_plots = True
make_latest_symlink = True
reference = ''
genes = ''
operons = ''
with_mauve = False
with_gage = False
with_genemark = False
cyclic = True
rc = True
extra_report = False

long_options = "output-dir= save-json-to= genes= operons= reference= contig-thresholds= min-contig= orf= genemark-thresholds= save-json mauve gage not-circular disable-rc genemark extra-report plain-report-no-plots help".split()
short_options = "o:G:O:R:t:M:f:e:J:jpmgndkxh"

