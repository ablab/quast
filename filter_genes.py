############################################################################
# Copyright (c) 2011-2012 Saint-Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################

import re
import sys

txt_pattern = re.compile(r'(?P<seqname>\S+)\s+(?P<number>\d+)\s+(?P<start>\d+)\s+(?P<end>\d+)', re.I)
gff_pattern = re.compile(r'(?P<seqname>\S+)\s+\S+\s+(?P<feature>\S+)\s+(?P<start>\d+)\s+(?P<end>\d+)\s+\S+\s+(?P<strand>[\+\-]?)\s+\S+\s+(?P<attributes>\S+)', re.I)
ncbi_start_pattern = re.compile(r'(?P<number>\d+)\.\s*(?P<name>\S+)\s*$', re.I)


def filter(source_fn, dest_fn, ref_start, ref_end):
    with open(source_fn, 'r') as s_file:
        with open(dest_fn, 'w') as d_file:
            line = s_file.readline().rstrip()
            while line == '' or line.startswith('##'):
                line = s_file.readline().rstrip()

            s_file.seek(0)

            if txt_pattern.match(line):
                filter_genes(s_file, d_file, ref_start, ref_end, txt_pattern)

            elif gff_pattern.match(line):
                filter_genes(s_file, d_file, ref_start, ref_end, gff_pattern)

            elif ncbi_start_pattern.match(line):
                try:
                    filter_genes_ncbi(s_file, d_file, ref_start, ref_end)
                except ParseException as e:
                    print e
            else:
                print 'Incorrect format of file! Specify file in plaint TXT, GFF or NCBI format.'


def check_gene(start, end, ref_start, ref_end):
    return ref_start <= start <= ref_end or ref_start <= end <= ref_end


def filter_genes(s_file, d_file, ref_start, ref_end, pattern):
    for line in s_file:
        m = pattern.match(line)
        if m:
            s = int(m.group('start'))
            e = int(m.group('end'))
            start, end = min(s, e), max(s, e)
            if check_gene(start, end, ref_start, ref_end):
                d_file.write(line)


def filter_genes_ncbi(s_file, d_file, ref_start, ref_end):
    annotation_pattern = re.compile(r'Annotation: (?P<seqname>\S+) \((?P<start>\d+)\.\.(?P<end>\d+)(, complement)?\)', re.I)

    line = s_file.readline()
    while line != '':
        while line.rstrip() == '' or line.startswith('##'):
            if line == '':
                break
            line = s_file.readline()

        m = ncbi_start_pattern.match(line.rstrip())
        if m:
            gene_info_lines = [line.rstrip()]

            line = s_file.readline()
            while line != '' and not ncbi_start_pattern.match(line.rstrip()):
                gene_info_lines.append(line.rstrip())
                line = s_file.readline()

            ok = False
            for info_line in gene_info_lines:
                if info_line.startswith('Annotation:'):
                    m = re.match(annotation_pattern, info_line)
                    if m:
                        start = int(m.group('start'))
                        end = int(m.group('end'))
                        if check_gene(start, end, ref_start, ref_end):
                            ok = True

            if ok:
                for info_line in gene_info_lines:
                    d_file.write(info_line + '\n')
                d_file.write('\n')




class ParseException(Exception):
    def __init__(self, value, *args, **kwargs):
        super(ParseException, self).__init__(*args, **kwargs)
        self.value = value
    def __str__(self):
        return repr(self.value)




if __name__ == '__main__':
    args = sys.argv[1:]
    filter(args[0], args[1], int(args[2]), int(args[3]))

