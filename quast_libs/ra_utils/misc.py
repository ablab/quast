############################################################################
# Copyright (c) 2015-2017 Saint Petersburg State University
# Copyright (c) 2011-2015 Saint Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################

from __future__ import with_statement
import os
import gzip
try:
    from urllib2 import urlopen
except:
    from urllib.request import urlopen
from os.path import join, isfile, exists, basename

import shutil

from quast_libs import qconfig
from quast_libs.qutils import compile_tool, get_dir_for_download, relpath, get_path_to_program, download_file, \
    download_external_tool

bwa_dirpath = join(qconfig.LIBS_LOCATION, 'bwa')
bedtools_dirpath = join(qconfig.LIBS_LOCATION, 'bedtools')
bedtools_bin_dirpath = join(qconfig.LIBS_LOCATION, 'bedtools', 'bin')
lap_dirpath = join(qconfig.LIBS_LOCATION, 'LAP')
sambamba_dirpath = join(qconfig.LIBS_LOCATION, 'sambamba')

gridss_dirpath = None
gridss_version = '1.4.1'
gridss_fname = 'gridss-' + gridss_version + '.jar'

gridss_external_fpath = join(qconfig.QUAST_HOME, 'external_tools/gridss', gridss_fname)
gridss_url = qconfig.GIT_ROOT_URL + relpath(gridss_external_fpath, qconfig.QUAST_HOME)

def bwa_fpath(fname):
    return get_path_to_program(fname, bwa_dirpath)


def sambamba_fpath(fname):
    platform_suffix = '_osx' if qconfig.platform_name == 'macosx' else '_linux'
    return join(sambamba_dirpath, fname + platform_suffix)


def bedtools_fpath(fname):
    return get_path_to_program(fname, bedtools_bin_dirpath)


def lap_fpath(fname):
    return join(lap_dirpath, fname)


def get_gridss_fpath():
    if not gridss_dirpath:
        return None
    return join(gridss_dirpath, gridss_fname)


def download_unpack_compressed_tar(name, download_url, downloaded_fpath, final_dirpath, logger, ext='gz'):
    if download_file(download_url, downloaded_fpath, name, move_file=False):
        unpack_tar(downloaded_fpath + '.download', final_dirpath, ext=ext)
        logger.main_info('  Done')
    else:
        logger.main_info('  Failed downloading %s from %s!' % (name, download_url))
        return False


def unpack_tar(fpath, dst_dirpath, ext='bz2'):
    shutil.move(fpath, fpath)
    import tarfile
    tar = tarfile.open(fpath, "r:" + ext)
    tar.extractall(dst_dirpath)
    tar.close()
    temp_dirpath = join(dst_dirpath, tar.members[0].name)
    from distutils.dir_util import copy_tree
    copy_tree(temp_dirpath, dst_dirpath)
    shutil.rmtree(temp_dirpath)
    os.remove(fpath)
    return True


def compile_bwa(logger, only_clean=False):
    return bwa_fpath('bwa') or compile_tool('BWA', bwa_dirpath, ['bwa'], only_clean=only_clean, logger=logger)


def compile_bedtools(logger, only_clean=False):
    return bedtools_fpath('bedtools') or \
           compile_tool('BEDtools', bedtools_dirpath, [join('bin', 'bedtools')], only_clean=only_clean, logger=logger)


def download_gridss(logger, bed_fpath=None, only_clean=False):
    global gridss_dirpath
    gridss_dirpath = get_dir_for_download('gridss', 'GRIDSS', [gridss_fname], logger, only_clean=only_clean)
    if not gridss_dirpath:
        return False

    gridss_fpath = get_gridss_fpath()
    if not qconfig.no_sv and bed_fpath is None and not isfile(gridss_fpath):
        if not download_external_tool(gridss_fname, gridss_dirpath, 'gridss'):
            logger.warning('Failed to download binary distribution from https://github.com/ablab/quast/external_tools/gridss. '
                           'QUAST SV module will be able to search trivial deletions only.')
            return False
    return True


def compile_reads_analyzer_tools(logger, only_clean=False):
    if compile_bwa(logger, only_clean) and compile_bedtools(logger, only_clean):
        return True
    return False


def correct_paired_reads_names(fpath, name_ending, output_dir):
    name, ext = os.path.splitext(fpath)
    try:
        if ext in ['.gz', '.gzip']:
            handler = gzip.open(fpath, mode='rt')
            corrected_fpath = join(output_dir, basename(name))
        else:
            handler = open(fpath)
            corrected_fpath = join(output_dir, basename(fpath))
    except IOError:
        return False
    first_read_name = None
    with handler as f:
        with open(corrected_fpath, 'w') as out_f:
            for i, line in enumerate(f):
                if i % 4 == 0:
                    full_read_name = line.split()[0] + name_ending
                    if not first_read_name:
                        first_read_name = full_read_name
                    out_f.write(full_read_name + '\n')
                elif i % 2 == 0:
                    out_f.write('+\n')
                else:
                    out_f.write(line)
    return corrected_fpath, first_read_name


def paired_reads_names_are_equal(reads_fpaths, temp_output_dir, logger):
    first_read_names = []

    for idx, fpath in enumerate(reads_fpaths):  # Note: will work properly only with exactly two files
        name_ending = '/%d' % (idx + 1)
        reads_type = 'forward'
        if idx:
            reads_type = 'reverse'

        _, ext = os.path.splitext(fpath)
        try:
            if ext in ['.gz', '.gzip']:
                handler = gzip.open(fpath, mode='rt')
            else:
                handler = open(fpath)
        except IOError:
            logger.notice('Cannot check equivalence of paired reads names, BWA may fail if reads are discordant')
            return True
        first_line = handler.readline()
        handler.close()
        full_read_name = first_line.strip().split()[0]
        if len(full_read_name) < 3 or not full_read_name.endswith(name_ending):
            logger.notice('Improper read names in ' + fpath + ' (' + reads_type + ' reads)! '
                           'Names should end with /1 (for forward reads) or /2 (for reverse reads) '
                           'but ' + full_read_name + ' was found!\nQUAST will attempt to fix read names.')
            corrected_fpath, full_read_name = correct_paired_reads_names(fpath, name_ending, temp_output_dir)
            if not full_read_name:
                logger.warning('Failed correcting read names. ')
                return False
            if reads_type == 'forward':
                qconfig.forward_reads[qconfig.forward_reads.index(fpath)] = corrected_fpath
            elif reads_type == 'reverse':
                qconfig.reverse_reads[qconfig.reverse_reads.index(fpath)] = corrected_fpath
        first_read_names.append(full_read_name[1:-2])  # truncate trailing /1 or /2 and @/> prefix (Fastq/Fasta)
    if len(first_read_names) != 2:  # should not happen actually
        logger.warning('Something bad happened and we failed to check paired reads names!')
        return False
    if first_read_names[0] != first_read_names[1]:
        logger.warning('Paired read names do not match! Check %s and %s!' % (first_read_names[0], first_read_names[1]))
        return False
    return True
