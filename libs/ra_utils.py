import os
import socket
import urllib2
from os.path import exists, join, isfile

import shutil

from libs import qconfig, qutils
from libs.qutils import compile_tool, check_prev_compilation_failed

bowtie_dirpath = join(qconfig.LIBS_LOCATION, 'bowtie2')
sambamba_dirpath = join(qconfig.LIBS_LOCATION, 'sambamba')
bedtools_dirpath = join(qconfig.LIBS_LOCATION, 'bedtools')
bedtools_bin_dirpath = join(qconfig.LIBS_LOCATION, 'bedtools', 'bin')
manta_dirpath = join(qconfig.LIBS_LOCATION, 'manta')
manta_build_dirpath = join(qconfig.LIBS_LOCATION, 'manta', 'build')
manta_bin_dirpath = join(qconfig.LIBS_LOCATION, 'manta', 'build/bin')
config_manta_fpath = join(manta_bin_dirpath, 'configManta.py')
manta_linux_download_path = 'https://raw.githubusercontent.com/ablab/quast/master/external_tools/manta/manta_linux.tar.bz2'
manta_osx_download_path = 'https://raw.githubusercontent.com/ablab/quast/master/external_tools/manta/manta_osx.tar.bz2'


def bowtie_fpath(fname):
    return join(bowtie_dirpath, fname)


def sambamba_fpath(fname):
    platform_suffix = '_osx' if qconfig.platform_name == 'macosx' else '_linux'
    return join(sambamba_dirpath, fname + platform_suffix)


def bedtools_fpath(fname):
    return join(bedtools_bin_dirpath, fname)


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
        response = urllib2.urlopen(download_path)
        content = response.read()
    except socket.error:
        logger.main_info('  Failed to establish connection!')
    if content:
        logger.main_info('  ' + name + ' successfully downloaded!')
        f = open(downloaded_fpath + '.download', 'w')
        f.write(content)
        f.close()
        if exists(downloaded_fpath + '.download'):
            logger.info('  Unpacking ' + name + '...')
            shutil.move(downloaded_fpath + '.download', downloaded_fpath)
            import tarfile
            tar = tarfile.open(downloaded_fpath, "r:bz2")
            tar.extractall(final_dirpath)
            tar.close()
            temp_dirpath = join(final_dirpath, tar.members[0].name)
            from distutils.dir_util import copy_tree
            copy_tree(temp_dirpath, final_dirpath)
            shutil.rmtree(temp_dirpath)
            os.remove(downloaded_fpath)
            logger.main_info('  Done')
            return True
        else:
            logger.main_info('  Failed downloading %s from %s!' % (name, download_path))
            return False


def compile_reads_analyzer_tools(logger, bed_fpath=None, only_clean=False):
    for name, dirpath, requirements in [
            ('Bowtie2', bowtie_dirpath, ['bowtie2-align-l']),
            ('BEDtools', bedtools_dirpath, [join('bin', 'bedtools')])]:
        success_compilation = compile_tool(name, dirpath, requirements, only_clean=only_clean)
        if not success_compilation:
            return False

    if only_clean:
        return True

    if not qconfig.no_sv and bed_fpath is None and not isfile(config_manta_fpath):
        failed_compilation_flag = join(manta_dirpath, 'make.failed')
        if check_prev_compilation_failed('Manta', failed_compilation_flag):
            print_manta_warning(logger)
            return True
        # making
        if not exists(manta_build_dirpath):
            os.mkdir(manta_build_dirpath)
        manta_downloaded_fpath = join(manta_build_dirpath, 'manta.tar.bz2')
        logger.main_info('  Downloading binary distribution of Manta...')
        if qconfig.platform_name == 'linux_64':
            download_unpack_tar_bz('Manta', manta_linux_download_path, manta_downloaded_fpath, manta_build_dirpath, logger)
        elif qconfig.platform_name == 'macosx':
            download_unpack_tar_bz('Manta', manta_osx_download_path, manta_downloaded_fpath, manta_build_dirpath, logger)
        else:
            logger.warning('Manta is not available for your platform.')

        if not isfile(config_manta_fpath):
            logger.warning('Failed to download binary distribution from https://github.com/ablab/quast/external_tools/manta '
                             'and unpack it into ' + join(manta_dirpath, 'build/'))
            print_manta_warning(logger)
    return True
