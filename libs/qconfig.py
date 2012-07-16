############################################################################
# Copyright (c) 2011-2012 Saint-Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################

import datetime

contig_thresholds = "110,201,501,1001"
min_contig = 0
genes_lengths = "0,300,600,900,1200,1500,1800,2100,2400,2700,3000"
orf_lengths = "200"

output_dir = "results_" + datetime.datetime.now().strftime('%Y_%m_%d_%H_%M_%S')
archive_dir = 'quast_results_archive_json'
json_results_dir = 'results_json_' + datetime.datetime.now().strftime('%Y_%m_%d_%H_%M_%S')
save_archive = False
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

long_options = "output-dir= genes= operons= reference= contig-thresholds= min-contig= orf= genemark-thresholds= mauve gage not-circular disable-rc genemark extra-report save-archive plain-report-no-plots help".split()
short_options = "o:G:O:R:t:M:f:e:apmgndkxh"

