############################################################################
# Copyright (c) 2011-2012 Saint-Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################

import re
import sys

txt_pattern = re.compile(r'(?P<seqname>\S+)\s+(?P<number>\d+)\s+(?P<start>\d+)\s+(?P<end>\d+)', re.I)

def filter_genes_txt(source_fn, dest_fn, ref_start, ref_end):
    with open(source_fn, 'r') as s_file:
        with open(dest_fn, 'w') as d_file:
            for line in s_file:
                m = txt_pattern.match(line)
                if m:
                    s = int(m.group('start'))
                    e = int(m.group('end'))
                    start, end = min(s, e), max(s, e)
                    if ref_start <= start <= ref_end or ref_start <= end <= ref_end:
                        d_file.write(line)

if __name__ == '__main__':
    args = sys.argv[1:]
    filter_genes_txt(args[0], args[1], int(args[2]), int(args[3]))
