import os
import socket
import urllib2
from os.path import exists, join, isfile

import shutil

from libs import qconfig, qutils
from libs.qutils import compile_tool, check_prev_compilation_failed

bowtie_dirpath = join(qconfig.LIBS_LOCATION, 'bowtie2')
samtools_dirpath = join(qconfig.LIBS_LOCATION, 'samtools')
bedtools_dirpath = join(qconfig.LIBS_LOCATION, 'bedtools')
bedtools_bin_dirpath = join(qconfig.LIBS_LOCATION, 'bedtools', 'bin')
manta_dirpath = join(qconfig.LIBS_LOCATION, 'manta')
manta_build_dirpath = join(qconfig.LIBS_LOCATION, 'manta', 'build')
manta_bin_dirpath = join(qconfig.LIBS_LOCATION, 'manta', 'build/bin')
config_manta_fpath = join(manta_bin_dirpath, 'configManta.py')
manta_download_path = 'https://github.com/Illumina/manta/releases/download/v0.29.6/manta-0.29.6.centos5_x86_64.tar.bz2'


def bowtie_fpath(fname):
    return join(bowtie_dirpath, fname)


def samtools_fpath(fname):
    return join(samtools_dirpath, fname)


def bedtools_fpath(fname):
    return join(bedtools_bin_dirpath, fname)


def print_manta_warning(logger):
    logger.main_info('Failed searching structural variations. QUAST will search trivial deletions only.')


def manta_compilation_failed():
    failed_compilation_flag = join(manta_dirpath, 'make.failed')
    if check_prev_compilation_failed('Manta', failed_compilation_flag):
        return True
    return False


def compile_reads_analyzer_tools(logger, bed_fpath=None):
    tools_to_try = [('Bowtie2', bowtie_dirpath, ['bowtie2-align-l']),
                    ('SAMtools', samtools_dirpath, ['samtools']),
                    ('BEDtools', bedtools_dirpath, [join('bin', 'bedtools')])]

    for name, dirpath, requirements in tools_to_try:
        success_compilation = compile_tool(name, dirpath, requirements)
        if not success_compilation:
            return False

    if not qconfig.no_sv and bed_fpath is None and not isfile(config_manta_fpath):
        failed_compilation_flag = join(manta_dirpath, 'make.failed')
        if check_prev_compilation_failed('Manta', failed_compilation_flag):
            print_manta_warning(logger)
            return True
        # making
        if not exists(manta_build_dirpath):
            os.mkdir(manta_build_dirpath)
        if qconfig.platform_name == 'linux_64':
            logger.main_info('  Downloading binary distribution of Manta...')
            manta_downloaded_fpath = join(manta_build_dirpath, 'manta.tar.bz2')
            content = None
            try:
                response = urllib2.urlopen(manta_download_path)
                content = response.read()
            except socket.error:
                logger.main_info('  Failed to establish connection!')
            if content:
                logger.main_info('  Manta successfully downloaded!')
                f = open(manta_downloaded_fpath + '.download', 'w' )
                f.write(content)
                f.close()
                if exists(manta_downloaded_fpath + '.download'):
                    logger.info('  Unpacking Manta...')
                    shutil.move(manta_downloaded_fpath + '.download', manta_downloaded_fpath)
                    import tarfile
                    tar = tarfile.open(manta_downloaded_fpath, "r:bz2")
                    tar.extractall(manta_build_dirpath)
                    tar.close()
                    manta_temp_dirpath = join(manta_build_dirpath, tar.members[0].name)
                    from distutils.dir_util import copy_tree
                    copy_tree(manta_temp_dirpath, manta_build_dirpath)
                    shutil.rmtree(manta_temp_dirpath)
                    os.remove(manta_downloaded_fpath)
                    logger.main_info('  Done')
            else:
                logger.main_info('  Failed downloading Manta from %s!' % manta_download_path)

        if not isfile(config_manta_fpath):
            logger.main_info('Compiling Manta (details are in ' + join(manta_dirpath, 'make.log') + ' and make.err)')
            prev_dir = os.getcwd()
            os.chdir(manta_build_dirpath)
            return_code = qutils.call_subprocess(
                [join(manta_dirpath, 'source', 'configure'), '--prefix=' + join(manta_dirpath, 'build'),
                 '--jobs=' + str(qconfig.max_threads)],
                stdout=open(join(manta_dirpath, 'make.log'), 'w'),
                stderr=open(join(manta_dirpath, 'make.err'), 'w'), logger=logger)
            if return_code == 0:
                return_code = qutils.call_subprocess(
                    ['make', '-j' + str(qconfig.max_threads), 'install'],
                    stdout=open(join(manta_dirpath, 'make.log'), 'a'),
                    stderr=open(join(manta_dirpath, 'make.err'), 'a'), logger=logger)
            os.chdir(prev_dir)
            if return_code != 0 or not isfile(config_manta_fpath):
                logger.warning('Failed to compile Manta (' + manta_dirpath + ')! Try to compile it manually ' + (
                                 'or download binary distribution from https://github.com/Illumina/manta/releases '
                                 'and unpack it into ' + join(manta_dirpath, 'build/') if qconfig.platform_name == 'linux_64' else '') + (
                                 '. You can restart QUAST with the --debug flag '
                                 'to see the command line.' if not qconfig.debug else '.'))
                open(failed_compilation_flag, 'w').close()
                print_manta_warning(logger)
    return True
