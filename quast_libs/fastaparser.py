############################################################################
# Copyright (c) 2015-2022 Saint Petersburg State University
# Copyright (c) 2011-2015 Saint Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################

from __future__ import with_statement
import os
import sys
import gzip
import zipfile

try:
   from collections import OrderedDict
except ImportError:
   from quast_libs.site_packages.ordered_dict import OrderedDict

try:
    import bz2
except ImportError:
    from quast_libs.site_packages import bz2
if sys.version_info[0] == 3:
    import io
from quast_libs import qconfig
# There is a pyfasta package -- http://pypi.python.org/pypi/pyfasta/
# Use it!

from quast_libs.log import get_logger
logger = get_logger(qconfig.LOGGER_DEFAULT_NAME)


def _get_fasta_file_handler(fpath):
    fasta_file = None

    _, ext = os.path.splitext(fpath)

    if not os.access(fpath, os.R_OK):
        logger.error('Permission denied accessing ' + fpath, to_stderr=True, exit_with_code=1)

    if ext in ['.gz', '.gzip']:
        fasta_file = gzip.open(fpath, mode="rt")

    elif ext in ['.bz2', '.bzip2']:
        fasta_file = bz2.BZ2File(fpath, mode="r")
        fasta_file = _read_compressed_file(fasta_file)

    elif ext in ['.zip']:
        try:
            zfile = zipfile.ZipFile(fpath, mode="r")
        except Exception:
            exc_type, exc_value, _ = sys.exc_info()
            logger.error('Can\'t open zip file: ' + str(exc_value), exit_with_code=1)
        else:
            names = zfile.namelist()
            if len(names) == 0:
                logger.error('Reading %s: zip archive is empty' % fpath, exit_with_code=1)

            if len(names) > 1:
                logger.warning('Zip archive must contain exactly one file. Using %s' % names[0])

            try:
                fasta_file = zfile.open(names[0])
                fasta_file = _read_compressed_file(fasta_file)
            except AttributeError:
                logger.error('Use python 2.6 or newer to work with contigs directly in zip.', exit_with_code=20)
    else:
        try:
            fasta_file = open(fpath)
        except IOError:
            exc_type, exc_value, _ = sys.exc_info()
            logger.exception(exc_value, exit_code=1)

    return fasta_file


def _read_compressed_file(compressed_file):
    if sys.version_info[0] == 3:
        return io.TextIOWrapper(io.BytesIO(compressed_file.read()))  # return string instead of binary data
    return compressed_file


def __get_entry_name(line):
    """
        Extracts name from fasta entry line:
        ">chr1  length=100500; coverage=15;" ---> "chr1"
    """
    try:
        return line[1:].split()[0]
    except IndexError:
        return ''  # special case: line == ">"


def get_chr_lengths_from_fastafile(fpath):
    """
        Takes filename of FASTA-file
        Returns list of lengths of sequences in FASTA-file
    """
    chr_lengths = OrderedDict()
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
                chr_name = __get_entry_name(line)
            else:
                l += len(line.strip())

    chr_lengths[chr_name] = l
    fasta_file.close()
    return chr_lengths


def get_genome_stats(fasta_fpath, skip_ns=False):
    genome_size = 0
    reference_chromosomes = {}
    ns_by_chromosomes = {}
    for name, seq in read_fasta(fasta_fpath):
        chr_name = name.split()[0]
        chr_len = len(seq)
        genome_size += chr_len
        ns_by_chromosomes[chr_name] = set(x + 1 for x, s in enumerate(seq) if s == 'N')
        if skip_ns:
            genome_size -= len(ns_by_chromosomes[chr_name])
        reference_chromosomes[chr_name] = chr_len
    return genome_size, reference_chromosomes, ns_by_chromosomes


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
                    chr_name = __get_entry_name(line)
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
        Oops, similar to: pyfasta split --header "%(seqid)s.fasta" original.fasta
    """
    if not os.path.isdir(output_dirpath):
        os.mkdir(output_dirpath)
    outFile = None
    for line in open(fpath):
        if line[0] == '>':
            if outFile:
                outFile.close()
            outFile = open(os.path.join(output_dirpath, __get_entry_name(line) + '.fa'), 'w')
        if outFile:
            outFile.write(line)
    if outFile: # if filename is empty
        outFile.close()


def read_fasta(fpath):
    """
        Generator that returns FASTA entries in tuples (name, seq)
    """
    first = True
    seq = []
    name = ''

    fasta_file = _get_fasta_file_handler(fpath)

    for raw_line in fasta_file:
        lines = raw_line.split('\r')
        for line in lines:
            if not line:
                continue
            if line[0] == '>':
                if not first:
                    yield name, "".join(seq)

                first = False
                name = __get_entry_name(line)
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
    list_seq = []
    for (name, seq) in read_fasta(fpath):
        list_seq.append((name, seq))
    return list_seq


def read_fasta_str(fpath):
    """
        Returns string
    """
    fasta_file = _get_fasta_file_handler(fpath)
    list_seq = []

    for raw_line in fasta_file:
        lines = raw_line.split('\r')
        for line in lines:
            if not line:
                continue
            if line[0] != '>':
                list_seq.append(line.strip())

    fasta_file.close()
    fasta_str = ''.join(list_seq)
    return fasta_str


def print_fasta(fasta):
    for name, seq in fasta:
        print('>%s' % name)
        for i in range(0, len(seq), 60):
            print(seq[i:i + 60])


def write_fasta(fpath, fasta, mode='w'):
    outfile = open(fpath, mode)

    for name, seq in fasta:
        outfile.write('>%s\n' % name)
        for i in range(0, len(seq), 60):
            outfile.write(seq[i:i + 60] + '\n')
    outfile.close()


def comp(letter):
    return {'A': 'T', 'T': 'A', 'C': 'G', 'G': 'C', 'N': 'N'}[letter.upper()]


def rev_comp(seq):
    c = dict(zip('ATCGNatcgn', 'TAGCNtagcn'))
    return ''.join(c.get(nucleotide, '') for nucleotide in reversed(seq))
