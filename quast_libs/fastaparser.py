############################################################################
# Copyright (c) 2015-2016 Saint Petersburg State University
# Copyright (c) 2011-2015 Saint Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################

from __future__ import with_statement
import os
import gzip
import zipfile
import bz2
from quast_libs import qconfig
import itertools
# There is a pyfasta package -- http://pypi.python.org/pypi/pyfasta/
# Use it!

from quast_libs.log import get_logger
logger = get_logger(qconfig.LOGGER_DEFAULT_NAME)


def _get_fasta_file_handler(fpath):
    fasta_file = None

    _, ext = os.path.splitext(fpath)

    if ext in ['.gz', '.gzip']:
        fasta_file = gzip.open(fpath)

    elif ext in ['.bz2', '.bzip2']:
        fasta_file = bz2.BZ2File(fpath)

    elif ext in ['.zip']:
        try:
            zfile = zipfile.ZipFile(fpath)
        except Exception as err:
            logger.error('Can\'t open zip file: ' + str(err))
        else:
            names = zfile.namelist()
            if len(names) == 0:
                logger.error('Reading %s: zip archive is empty' % fpath)

            if len(names) > 1:
                logger.warning('Zip archive must contain exactly one file. Using %s' % names[0])

            try:
                fasta_file = zfile.open(names[0])
            except AttributeError:
                logger.error('Use python 2.6 or newer to work with contigs directly in zip.', exit_with_code=20)
    else:
        try:
            fasta_file = open(fpath)
        except IOError as e:
            logger.exception(e)

    return fasta_file


def get_chr_lengths_from_fastafile(fpath):
    """
        Takes filename of FASTA-file
        Returns list of lengths of sequences in FASTA-file
    """
    chr_lengths = dict()
    l = 0
    chr_name = None
    fasta_file = _get_fasta_file_handler(fpath)
    for raw_line in fasta_file:
        if raw_line.find('\r') != -1:
            lines = raw_line.split('\r')
        else:
            lines = [raw_line]
        for line in lines:
            if not line:
                continue
            if line[0] == '>':
                if l:  # not the first sequence in FASTA
                    chr_lengths[chr_name] = l
                    l = 0
                chr_name = line[1:].strip()
            else:
                l += len(line.strip())

    chr_lengths[chr_name] = l
    fasta_file.close()
    return chr_lengths


def create_fai_file(fasta_fpath):
    l = 0
    total_offset = 0
    chr_offset = 0
    chr_name = None
    fai_fpath = fasta_fpath + '.fai'
    fai_fields = []
    with open(fasta_fpath) as in_f:
        for raw_line in in_f:
            if raw_line.find('\r') != -1:
                lines = raw_line.split('\r')
            else:
                lines = [raw_line]
            for line in lines:
                if not line:
                    continue
                if line[0] == '>':
                    if l:  # not the first sequence in FASTA
                        fai_fields.append([chr_name, l, total_offset, len(chr_line.strip()), len(chr_line)])
                        total_offset += chr_offset
                        l = 0
                        chr_offset = 0
                    chr_name = line[1:].strip()
                    total_offset += len(line)
                else:
                    if not l:
                        chr_line = line
                    l += len(line.strip())
                    chr_offset += len(line)
    fai_fields.append([chr_name, l, total_offset, len(chr_line.strip()), len(chr_line)])
    with open(fai_fpath, 'w') as out_f:
        for fields in fai_fields:
            out_f.write('\t'.join([str(fs) for fs in fields]) + '\n')


def split_fasta(fpath, output_dirpath):
    """
        Takes filename of FASTA-file and directory to output
        Creates separate FASTA-files for each sequence in FASTA-file
        Returns nothing
        Oops, similar to: pyfasta split --header "%(seqid).fasta" original.fasta
    """
    if not os.path.isdir(output_dirpath):
        os.mkdir(output_dirpath)
    outFile = None
    for line in open(fpath):
        if line[0] == '>':
            if outFile:
                outFile.close()
            outFile = open(os.path.join(output_dirpath, line[1:].strip() + '.fa'), 'w')
        if outFile:
            outFile.write(line)
    if outFile: # if filename is empty
        outFile.close()


def read_fasta(fpath):
    """
        Returns list of FASTA entries (in tuples: name, seq)
    """
    first = True
    seq = []
    name = ''

    fasta_file = _get_fasta_file_handler(fpath)

    for raw_line in fasta_file:
        if raw_line.find('\r') != -1:
            lines = raw_line.split('\r')
        else:
            lines = [raw_line]
        for line in lines:
            if not line:
                continue
            if line[0] == '>':
                if not first:
                    yield name, "".join(seq)

                first = False
                name = line.strip()[1:]
                seq = []
            else:
                seq.append(line.strip())

    if name or seq:
        yield name, "".join(seq)

    fasta_file.close()


def read_fasta_one_time(fpath):
    """
        Returns list of FASTA entries (in tuples: name, seq)
    """
    first = True
    seq = []
    name = ''

    fasta_file = _get_fasta_file_handler(fpath)
    list_seq = []

    for raw_line in fasta_file:
        if raw_line.find('\r') != -1:
            lines = raw_line.split('\r')
        else:
            lines = [raw_line]
        for line in lines:
            if not line:
                continue
            if line[0] == '>':
                if not first:
                    list_seq.append((name, "".join(seq)))

                first = False
                name = line.strip()[1:]
                seq = []
            else:
                seq.append(line.strip())

    if name or seq:
        list_seq.append((name, "".join(seq)))

    fasta_file.close()
    return list_seq


def print_fasta(fasta):
    for name, seq in fasta:
        print('>%s' % name)
        for i in xrange(0, len(seq), 60):
            print(seq[i:i + 60])


def write_fasta(fpath, fasta, mode='w'):
    outfile = open(fpath, mode)

    for name, seq in fasta:
        outfile.write('>%s\n' % name)
        for i in xrange(0, len(seq), 60):
            outfile.write(seq[i:i + 60] + '\n')
    outfile.close()


def comp(letter):
    return {'A': 'T', 'T': 'A', 'C': 'G', 'G': 'C', 'N': 'N'}[letter.upper()]


def rev_comp(seq):
    c = dict(zip('ATCGNatcgn', 'TAGCNtagcn'))
    return ''.join(c.get(nucleotide, '') for nucleotide in reversed(seq))
