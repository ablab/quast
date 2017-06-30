############################################################################
# Copyright (c) 2015-2017 Saint Petersburg State University
# Copyright (c) 2011-2015 Saint Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################
from __future__ import with_statement
import os
from os.path import join

import shutil

from quast_libs.busco.busco import set_augustus_dir
from quast_libs.ra_utils.misc import download_unpack_compressed_tar

from quast_libs import reporting, qconfig, qutils
from quast_libs.busco import busco
from quast_libs.log import get_logger
from quast_libs.qutils import download_blast_binaries, run_parallel, compile_tool, get_dir_for_download, \
    check_prev_compilation_failed

logger = get_logger(qconfig.LOGGER_DEFAULT_NAME)

augustus_version = '3.1'
augustus_url = 'http://bioinf.uni-greifswald.de/augustus/binaries/old/augustus-' + augustus_version + '.tar.gz'
bacteria_db_url = 'http://busco.ezlab.org/datasets/bacteria_odb9.tar.gz'
fungi_db_url = 'http://busco.ezlab.org/datasets/fungi_odb9.tar.gz'
eukaryota_db_url = 'http://busco.ezlab.org/datasets/eukaryota_odb9.tar.gz'
blast_filenames = ['tblastn', 'makeblastdb']


def download_db(logger, is_prokaryote, is_fungus=False, only_clean=False):
    if is_prokaryote:
        url = bacteria_db_url
        clade = 'bacteria'
    elif is_fungus:
        url = fungi_db_url
        clade = 'fungi'
    else:
        url = eukaryota_db_url
        clade = 'eukaryota'
    dirpath = get_dir_for_download('busco', 'Busco databases', [clade], logger, only_clean=only_clean)
    if not dirpath:
        return None

    db_dirpath = join(dirpath, clade)
    if only_clean:
        if os.path.isdir(db_dirpath):
            shutil.rmtree(db_dirpath, ignore_errors=True)
        return True

    if not os.path.exists(db_dirpath):
        downloaded_fpath = join(dirpath, clade + '.tar.gz')
        logger.main_info('  Downloading ' + clade + ' database...')
        download_unpack_compressed_tar(clade + ' database', url, downloaded_fpath, db_dirpath, logger)

        if not os.path.exists(db_dirpath):
            logger.warning('Failed to download ' + clade + ' database from ' + url + 'and unpack it into ' + dirpath)
            return None
    return db_dirpath


def download_tool(tool, tool_version, required_files, logger, url, only_clean=False):
    tool_dirpath = get_dir_for_download(tool + tool_version, tool, required_files, logger, only_clean=only_clean)
    if not tool_dirpath:
        return None

    if only_clean:
        if os.path.isdir(tool_dirpath):
            shutil.rmtree(tool_dirpath, ignore_errors=True)
        return tool_dirpath

    failed_compilation_flag = join(tool_dirpath, 'make.failed')
    if not all(os.path.exists(join(tool_dirpath, fpath)) for fpath in required_files) and not \
            check_prev_compilation_failed(tool, failed_compilation_flag):
        downloaded_fpath = join(tool_dirpath, tool + '.tar.gz')
        logger.main_info('  Downloading ' + tool + '...')
        download_unpack_compressed_tar(tool, url, downloaded_fpath, tool_dirpath, logger)

        if not all(os.path.exists(join(tool_dirpath, fpath)) for fpath in required_files):
            logger.warning('Failed to download ' + tool + ' from ' + url + 'and unpack it into ' + tool_dirpath)
            return None
    return tool_dirpath


def download_all_db(logger, only_clean=False):
    bacteria_db = download_db(logger, is_prokaryote=True, only_clean=only_clean)
    eukaryota_db = download_db(logger, is_prokaryote=False, only_clean=only_clean)
    fungi_db = download_db(logger, is_prokaryote=False, is_fungus=True, only_clean=only_clean)
    return bacteria_db and eukaryota_db and fungi_db


def download_augustus(logger, only_clean=False):
    return download_tool('augustus', augustus_version, ['bin'], logger, augustus_url, only_clean=only_clean)


def do(contigs_fpaths, output_dir, logger):
    logger.print_timestamp()
    logger.info('Running BUSCO...')

    compilation_success = True

    augustus_dirpath = download_augustus(logger)
    if not augustus_dirpath:
        compilation_success = False
    elif not compile_tool('Augustus', augustus_dirpath, [join('bin', 'augustus')], logger=logger):
        compilation_success = False

    if compilation_success and not download_blast_binaries(logger=logger, filenames=blast_filenames):
        compilation_success = False

    if not compilation_success:
        logger.info('Failed finding conservative genes.')
        return

    set_augustus_dir(augustus_dirpath)
    if not os.path.isdir(output_dir):
        os.makedirs(output_dir)
    tmp_dir = join(output_dir, 'tmp')
    if not os.path.isdir(tmp_dir):
        os.makedirs(tmp_dir)

    n_jobs = min(len(contigs_fpaths), qconfig.max_threads)
    busco_threads = max(1, qconfig.max_threads // n_jobs)

    clade_dirpath = download_db(logger, is_prokaryote=qconfig.prokaryote, is_fungus=qconfig.is_fungus)
    if not clade_dirpath:
        logger.info('Failed finding conservative genes.')
        return

    log_fpath = join(output_dir, 'busco.log')
    logger.info('Logging to ' + log_fpath + '...')
    busco_args = [(['-i', contigs_fpath, '-o', qutils.label_from_fpath_for_fname(contigs_fpath), '-l', clade_dirpath,
                    '-m', 'genome', '-f', '-z', '-c', str(busco_threads), '-t', tmp_dir,
                    '--augustus_parameters=\'--AUGUSTUS_CONFIG_PATH=' + join(augustus_dirpath, 'config') + '\'' ], output_dir)
                    for contigs_fpath in contigs_fpaths]
    summary_fpaths = run_parallel(busco.main, busco_args, qconfig.max_threads)
    if not any(fpath for fpath in summary_fpaths):
        logger.error('Failed running BUSCO for all the assemblies. See ' + log_fpath + ' for information.')
        return

    # saving results
    for i, contigs_fpath in enumerate(contigs_fpaths):
        report = reporting.get(contigs_fpath)

        if summary_fpaths[i] and os.path.isfile(summary_fpaths[i]):
            total_buscos, part_buscos, complete_buscos = 0, 0, 0
            with open(summary_fpaths[i]) as f:
                for line in f:
                    if 'Complete BUSCOs' in line:
                        complete_buscos = int(line.split()[0])
                    elif 'Fragmented' in line:
                        part_buscos = int(line.split()[0])
                    elif 'Total' in line:
                        total_buscos = int(line.split()[0])
            if total_buscos != 0:
                report.add_field(reporting.Fields.BUSCO_COMPLETE, ('%.2f' % (float(complete_buscos) * 100.0 / total_buscos)))
                report.add_field(reporting.Fields.BUSCO_PART, ('%.2f' % (float(part_buscos) * 100.0 / total_buscos)))
        else:
            logger.error(
                'Failed running BUSCO for ' + contigs_fpath + '. See ' + log_fpath + ' for information.')
    logger.info('Done.')