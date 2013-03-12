#!/usr/bin/python

import os
from common import *

name = os.path.basename(__file__)[5:-3]

run_quast(name, contigs=[contigs_1_10k], params='--meta --gene-finding -R ' + reference_10k)
