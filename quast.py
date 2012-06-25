#!/usr/bin/env python

############################################################################
# Copyright (c) 2011-2012 Saint-Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################

import os
import sys

import quality
quality.main(sys.argv[1:], lib_dir=os.path.join(os.path.abspath(sys.path[0]), 'libs'), release_mode=True)
