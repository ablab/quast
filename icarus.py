#!/usr/bin/env python

############################################################################
# Copyright (c) 2015-2016 Saint Petersburg State University
# Copyright (c) 2011-2015 Saint Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################

import sys

sys.stderr.write("\n")
sys.stderr.write("Icarus: visualizer for de novo assembly evaluation\n")
sys.stderr.write("\n")
sys.stderr.write("Icarus is embedded into QUAST and MetaQUAST pipelines,\n")
sys.stderr.write("please run quast.py (for single-genome evaluation) or \n")
sys.stderr.write("metaquast.py (for metagenomic datasets)\n")
sys.stderr.write("\n")
sys.stderr.write("Icarus main menu will be saved to <output_dir>/icarus.html\n")
sys.stderr.write("Icarus viewers will be saved to <output_dir>/icarus_viewers/\n")
sys.stderr.write("\n")
sys.stderr.write("Note: if you use --fast or --no-html options, Icarus will not run\n")
