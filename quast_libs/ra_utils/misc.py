############################################################################
# Copyright (c) 2015-2017 Saint Petersburg State University
# Copyright (c) 2011-2015 Saint Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################

from __future__ import with_statement
import os
import gzip
import socket
try:
    from urllib2 import urlopen
except:
    from urllib.request import urlopen
from os.path import exists, join, isfile

import shutil

from quast_libs import qconfig
from quast_libs.qutils import compile_tool, check_prev_compilation_failed, get_dir_for_download, relpath

bwa_dirpath = join(qconfig.LIBS_LOCATION, 'bwa')
sambamba_dirpath = join(qconfig.LIBS_LOCATION, 'sambamba')
bedtools_dirpath = join(qconfig.LIBS_LOCATION, 'bedtools')
bedtools_bin_dirpath = join(qconfig.LIBS_LOCATION, 'bedtools', 'bin')

manta_version = '0.29.6'
manta_dirpath = None
config_manta_relpath = join('build', 'bin', 'configManta.py')

manta_external_dirpath = join(qconfig.QUAST_HOME, 'external_tools/manta')
manta_ext_linux_fpath = join(manta_external_dirpath, 'manta_linux.tar.bz2')
manta_ext_osx_fpath = join(manta_external_dirpath, 'manta_osx.tar.bz2')

manta_linux_url = qconfig.GIT_ROOT_URL + relpath(manta_ext_linux_fpath, qconfig.QUAST_HOME)
manta_osx_url = qconfig.GIT_ROOT_URL + relpath(manta_ext_osx_fpath, qconfig.QUAST_HOME)


def bwa_fpath(fname):
    return join(bwa_dirpath, fname)


def sambamba_fpath(fname):
    platform_suffix = '_osx' if qconfig.platform_name == 'macosx' else '_linux'
    return join(sambamba_dirpath, fname + platform_suffix)


def bedtools_fpath(fname):
    return join(bedtools_bin_dirpath, fname)


def get_manta_fpath():
    if not manta_dirpath:
        return None
    config_manta_fpath = join(manta_dirpath, config_manta_relpath)
    return config_manta_fpath


def print_manta_warning(logger):
    logger.main_info('Manta failed to compile, and QUAST SV module will be able to search trivial deletions only.')


def manta_compilation_failed():
    failed_compilation_flag = join(manta_dirpath, 'make.failed')
    if check_prev_compilation_failed('Manta', failed_compilation_flag):
        return True
    return False


def download_unpack_tar_bz(name, download_path, downloaded_fpath, final_dirpath, logger):
    content = None
    try:
        response = urlopen(download_path)
        content = response.read()
    except:
        logger.main_info('  Failed to establish connection!')
    if content:
        logger.main_info('  ' + name + ' successfully downloaded!')
        with open(downloaded_fpath + '.download', 'wb') as f:
            f.write(content)
        if exists(downloaded_fpath + '.download'):
            logger.info('  Unpacking ' + name + '...')
            unpack_tar(downloaded_fpath + '.download', final_dirpath)
            logger.main_info('  Done')
        else:
            logger.main_info('  Failed downloading %s from %s!' % (name, download_path))
            return False


def unpack_tar(fpath, dst_dirpath):
    shutil.move(fpath, fpath)
    import tarfile
    tar = tarfile.open(fpath, "r:bz2")
    tar.extractall(dst_dirpath)
    tar.close()
    temp_dirpath = join(dst_dirpath, tar.members[0].name)
    from distutils.dir_util import copy_tree
    copy_tree(temp_dirpath, dst_dirpath)
    shutil.rmtree(temp_dirpath)
    os.remove(fpath)
    return True


def compile_bwa(logger, only_clean=False):
    return compile_tool('BWA', bwa_dirpath, ['bwa'], only_clean=only_clean, logger=logger)


def compile_bedtools(logger, only_clean=False):
    return compile_tool('BEDtools', bedtools_dirpath, [join('bin', 'bedtools')], only_clean=only_clean, logger=logger)


def download_manta(logger, bed_fpath=None, only_clean=False):
    global manta_dirpath
    manta_dirpath = get_dir_for_download('manta' + manta_version, 'Manta', [config_manta_relpath], logger, only_clean=only_clean)
    if not manta_dirpath:
        return False

    manta_build_dirpath = join(manta_dirpath, 'build')
    config_manta_fpath = get_manta_fpath()
    if only_clean:
        if os.path.isdir(manta_build_dirpath):
            shutil.rmtree(manta_build_dirpath, ignore_errors=True)
        return True

    if not qconfig.no_sv and bed_fpath is None and not isfile(config_manta_fpath):
        if qconfig.platform_name == 'linux_64':
            url = manta_linux_url
            fpath = manta_ext_linux_fpath
        elif qconfig.platform_name == 'macosx':
            url = manta_osx_url
            fpath = manta_ext_osx_fpath
        else:
            logger.warning('Manta is not available for your platform.')
            return False

        if not exists(manta_build_dirpath):
            os.makedirs(manta_build_dirpath)
        manta_downloaded_fpath = join(manta_build_dirpath, 'manta.tar.bz2')

        if isfile(fpath):
            logger.info('Copying manta from ' + fpath)
            shutil.copy(fpath, manta_downloaded_fpath)
            logger.info('Unpacking ' + manta_downloaded_fpath + ' into ' + manta_build_dirpath)
            unpack_tar(manta_downloaded_fpath, manta_build_dirpath)
        else:
            failed_compilation_flag = join(manta_dirpath, 'make.failed')
            if check_prev_compilation_failed('Manta', failed_compilation_flag):
                print_manta_warning(logger)
                return False

            logger.main_info('  Downloading binary distribution of Manta...')
            download_unpack_tar_bz('Manta', url, manta_downloaded_fpath, manta_build_dirpath, logger)

        manta_demo_dirpath = join(manta_build_dirpath, 'share', 'demo')
        if os.path.isdir(manta_demo_dirpath):
            shutil.rmtree(manta_demo_dirpath, ignore_errors=True)
        if not isfile(config_manta_fpath):
            logger.warning('Failed to download binary distribution from https://github.com/ablab/quast/external_tools/manta '
                           'and unpack it into ' + join(manta_dirpath, 'build/'))
            print_manta_warning(logger)
            return False
    return True


def compile_reads_analyzer_tools(logger, only_clean=False):
    if compile_bwa(logger, only_clean) and compile_bedtools(logger, only_clean):
        return True
    return False


def paired_reads_names_are_equal(reads_fpaths, logger):
    first_read_names = []

    for idx, fpath in enumerate(reads_fpaths):  # Note: will work properly only with exactly two files
        name_ending = '/%d' % (idx + 1)
        reads_type = 'forward'
        if idx:
            reads_type = 'reverse'

        _, ext = os.path.splitext(fpath)
        handler = None
        try:
            if ext in ['.gz', '.gzip']:
                handler = gzip.open(fpath, mode='rt')
            else:
                handler = open(fpath)
        except IOError:
            logger.notice('Cannot check equivalence of paired reads names, BWA will fail if reads are discordant')
            return True
        first_line = handler.readline()
        full_read_name = first_line.strip().split()[0]
        if len(full_read_name) < 3 or not full_read_name.endswith(name_ending):
            logger.warning('Improper read names in ' + fpath + ' (' + reads_type + ' reads)! '
                           'Names should end with /1 (for forward reads) or /2 (for reverse reads) '
                           'but ' + full_read_name + ' was found!')
            return False
        first_read_names.append(full_read_name[1:-2])  # truncate trailing /1 or /2 and @/> prefix (Fastq/Fasta)
        handler.close()
    if len(first_read_names) != 2:  # should not happen actually
        logger.warning('Something bad happened and we failed to check paired reads names!')
        return False
    if first_read_names[0] != first_read_names[1]:
        logger.warning('Paired read names do not match! Check %s and %s!' % (first_read_names[0], first_read_names[1]))
        return False
    return True
