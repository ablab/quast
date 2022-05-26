############################################################################
# Copyright (c) 2015-2022 Saint Petersburg State University
# Copyright (c) 2011-2015 Saint Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################

import os
import re
import sys

from quast_libs import qutils, qconfig
from quast_libs.ca_utils.misc import open_gzipsafe

from quast_libs.log import get_logger
logger = get_logger(qconfig.LOGGER_DEFAULT_NAME)

txt_pattern_gi = re.compile(r'(?P<seqname>\S+)\s+(?P<gene_id>.+)\s+(?P<start>\d+)\s+(?P<end>\d+)$', re.I)

txt_pattern = re.compile(r'(?P<seqname>\S+)\s+(?P<gene_id>.+)\s+(?P<start>\d+)\s+(?P<end>\d+)$', re.I)

gff_pattern = re.compile(r'(?P<seqname>\S+)\s+\S+\s+(?P<feature>\S+)\s+(?P<start>\d+)\s+(?P<end>\d+)\s+\S+\s+(?P<strand>[\+\-\.]?)\s+\S+\s+(?P<attributes>.+)$', re.I)

ncbi_start_pattern = re.compile(r'(?P<number>\d+)\.\s*(?P<name>\S+)\s*$', re.I)


def get_genes_from_file(fpath, feature):
    if not fpath or not os.path.exists(fpath):
        # it is already checked in quast,py, so we need no more notification
        #print '  Warning! ' + feature + '\'s file not specified or doesn\'t exist!'
        return []

    genes_file = open_gzipsafe(fpath, 'r')
    genes = []

    line = genes_file.readline().rstrip()
    while line == '' or line.startswith('#'):
        line = genes_file.readline().rstrip()

    genes_file.seek(0)

    if fpath.endswith('bed') or fpath.endswith('bed.gz'):
        genes = parse_bed(genes_file)

    elif txt_pattern_gi.match(line) or txt_pattern.match(line):
        genes = parse_txt(genes_file)

    elif gff_pattern.match(line):
        genes = parse_gff(genes_file, feature)

    elif ncbi_start_pattern.match(line):
        try:
            genes = parse_ncbi(genes_file)
        except ParseException:
            exc_type, exc_value, _ = sys.exc_info()
            logger.warning('Parsing exception ' + exc_value)
            logger.warning(fpath + ' was skipped')
            genes = []
    else:
        logger.warning('Incorrect format of ' + feature + '\'s file! GFF, NCBI and the plain TXT format accepted. See manual.')
        logger.warning(fpath + ' was skipped')

    genes_file.close()
    return genes


# Parsing NCBI format

# Example:
#   1. Phep_1459
#   heparinase II/III family protein[Pedobacter heparinus DSM 2366]
#   Other Aliases: Phep_1459
#   Genomic context: Chromosome
#   Annotation: NC_013061.1 (1733715..1735595, complement)
#   ID: 8252560
def parse_ncbi(ncbi_file):
    annotation_pattern = re.compile(r'Annotation: (?P<seqname>.+) \((?P<start>\d+)\.\.(?P<end>\d+)(, complement)?\)', re.I)
    chromosome_pattern = re.compile(r'Chromosome: (?P<chromosome>\S+);', re.I)
    id_pattern = re.compile(r'ID: (?P<id>\d+)', re.I)

    genes = []

    line = ncbi_file.readline()
    while line != '':
        while line.rstrip() == '' or line.startswith('##'):
            if line == '':
                break
            line = ncbi_file.readline()

        m = ncbi_start_pattern.match(line.rstrip())
        while not m:
            m = ncbi_start_pattern.match(line.rstrip())

        gene = Gene(number=int(m.group('number')),
                    name=qutils.correct_name(m.group('name')))

        the_rest_lines = []

        line = ncbi_file.readline()
        while line != '' and not ncbi_start_pattern.match(line.rstrip()):
            the_rest_lines.append(line.rstrip())
            line = ncbi_file.readline()

        for info_line in the_rest_lines:
            if info_line.startswith('Chromosome:'):
                m = re.match(chromosome_pattern, info_line)
                if m:
                    gene.chromosome = m.group('chromosome')

            if info_line.startswith('Annotation:'):
                m = re.match(annotation_pattern, info_line)
                if m:
                    gene.seqname = m.group('seqname')
                    gene.start = int(m.group('start'))
                    gene.end = int(m.group('end'))

                    to_trim = 'Chromosome' + ' ' + str(gene.chromosome)
                    if gene.chromosome and gene.seqname.startswith(to_trim):
                        gene.seqname = gene.seqname[len(to_trim):]
                        gene.seqname.lstrip(' ,')

                else:
                    logger.warning('Wrong NCBI annotation for gene ' + str(gene.number) + '. ' + gene.name + '. Skipping this gene.')

            if info_line.startswith('ID:'):
                m = re.match(id_pattern, info_line)
                if m:
                    gene.id = m.group('id')
                else:
                    logger.warning('Can\'t parse gene\'s ID in NCBI format. Gene is ' + str(gene.number) + '. ' + gene.name + '. Skipping it.')

        if gene.start is not None and gene.end is not None:
            genes.append(gene)
        # raise ParseException('NCBI format parsing error: provide start and end for gene ' + gene.number + '. ' + gene.name + '.')
    return genes


# Parsing txt format

# Example:
#   U00096.2    1	4263805	4264884
#   U00096.2	2	795085	795774
def parse_txt(file):
    genes = []

    number = 0

    for line in file:
        line = line.rstrip()
        m = txt_pattern_gi.match(line) or txt_pattern.match(line)
        if m:
            gene = Gene(number=number,
                        seqname=qutils.correct_name(m.group('seqname')))
            number += 1
            s = int(m.group('start'))
            e = int(m.group('end'))
            gene.start = min(s, e)
            gene.end = max(s, e)
            gene.id = m.group('gene_id')
            genes.append(gene)

    return genes


# Parsing GFF

# Example:
#   ##gff-version   3
#   ##seqname-region   ctg123 1 1497228
#   ctg123 . gene            1000  9000  .  +  .  ID=gene00001;Name=EDEN
#   ctg123 . TF_binding_site 1000  1012  .  +  .  ID=tfbs00001;Parent=gene00001
def parse_gff(file, feature):
    genes = []

    number = 0

    for line in file:
        m = gff_pattern.match(line.rstrip())
        if m and (feature == qconfig.ALL_FEATURES_TYPE or m.group('feature').lower() == feature.lower()):
            gene = Gene(seqname=qutils.correct_name(m.group('seqname')),
                        start=int(m.group('start')),
                        end=int(m.group('end')))

            attributes = m.group('attributes').split(';')
            for attr in attributes:
                if attr and attr != '' and '=' in attr:
                    key = attr.split('=')[0]
                    val = attr[len(key) + 1:]
                    if key.lower() == 'id':
                        gene.id = val
                    if key.lower() == 'name':
                        gene.name = val
                    gene.attributes[key.lower()] = val

            gene.number = number
            number += 1

            genes.append(gene)

    return genes


def parse_bed(file):
    genes = []

    number = 0

    for line in file:
        fs = line.rstrip().split()
        if fs:
            seqname = fs[0]
            s = int(fs[1])
            e = int(fs[2])
            gene = Gene(number=number, seqname=qutils.correct_name(seqname))
            gene.start = min(s, e)
            gene.end = max(s, e)
            gene.id = fs[3] if len(fs) > 3 else None
            if s < e:
                gene.strand = '+'
            else:
                gene.strand = '-'
            number += 1

            genes.append(gene)

    return genes


class ParseException(Exception):
    def __init__(self, value, *args, **kwargs):
        super(ParseException, self).__init__(*args, **kwargs)
        self.value = value
    def __str__(self):
        return repr(self.value)


class Gene():
    def __init__(self, id='', seqname='', start=None, end=None,
                 number=None, name='', chromosome=None, contig=None, strand=None, seq=None, protein=None, is_full=None):
        self.id = id
        self.seqname = seqname
        self.start = start
        self.end = end
        self.number = number
        self.name = name
        self.chromosome = chromosome
        self.contig = contig
        self.strand = strand
        self.seq = seq
        self.protein = protein
        self.attributes = dict()
        self.is_full = is_full

