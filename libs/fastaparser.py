############################################################################
# Copyright (c) 2011-2015 Saint-Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################

import os
import gzip
import zipfile
import bz2
from libs import qconfig
import itertools
# There is a pyfasta package -- http://pypi.python.org/pypi/pyfasta/
# Use it!

from libs.log import get_logger
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
        except Exception, e:
            logger.error('Can\'t open zip file: ' + str(e.message))
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
        except IOError, e:
            logger.exception(e)

    return fasta_file


def get_lengths_from_fastafile(fpath):
    """
        Takes filename of FASTA-file
        Returns list of lengths of sequences in FASTA-file
    """
    lengths = []
    l = 0
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
                    lengths.append(l)
                    l = 0
            else:
                l += len(line.strip())

    lengths.append(l)
    fasta_file.close()
    return lengths


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
        print '>%s' % name
        for i in xrange(0, len(seq), 60):
            print seq[i:i + 60]


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
