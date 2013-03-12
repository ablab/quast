#!/usr/bin/python

import os
from common import *

name = os.path.basename(__file__)[5:-3]

run_quast(name, contigs=[contigs_2_10k], params='-R ' + reference_10k)
