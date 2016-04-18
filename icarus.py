#!/usr/bin/env python

############################################################################
# Copyright (c) 2015-2016 Saint Petersburg State University
# Copyright (c) 2011-2015 Saint Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################

import sys

print >> sys.stderr, 'Icarus: visualizer for de novo assembly evaluation'
print >> sys.stderr, ""
print >> sys.stderr, 'Icarus is embedded into QUAST and MetaQUAST pipelines,'
print >> sys.stderr, "please run quast.py (for single-genome evaluation) or "
print >> sys.stderr, "metaquast.py (for metagenomic datasets)."
print >> sys.stderr, ""
print >> sys.stderr, "Icarus main menu is in <output_dir>/icarus.html"
print >> sys.stderr, "Icarus viewers are under <output_dir>/icarus_viewers/"
print >> sys.stderr, ""
print >> sys.stderr, "Note: if you use --fast or --no-html options, Icarus is not run."