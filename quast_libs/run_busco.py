############################################################################
# Copyright (c) 2015-2022 Saint Petersburg State University
# Copyright (c) 2011-2015 Saint Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################
from __future__ import with_statement
import os
from os.path import join, dirname, realpath

import shutil

from quast_libs.ra_utils.misc import download_unpack_compressed_tar

from quast_libs import reporting, qconfig, qutils
from quast_libs.busco import busco
from quast_libs.log import get_logger
from quast_libs.qutils import download_blast_binaries, run_parallel, compile_tool, get_dir_for_download, \
    check_prev_compilation_failed, get_blast_fpath

logger = get_logger(qconfig.LOGGER_DEFAULT_NAME)

augustus_version = '3.2.3'
augustus_max_allowed_version = '3.3'
augustus_url = 'http://bioinf.uni-greifswald.de/augustus/binaries/old/augustus-' + augustus_version + '.tar.gz'
bacteria_db_url = 'https://busco-archive.ezlab.org/v3/datasets/bacteria_odb9.tar.gz'
fungi_db_url = 'https://busco-archive.ezlab.org/v3/datasets/fungi_odb9.tar.gz'
eukaryota_db_url = 'https://busco-archive.ezlab.org/v3/datasets/eukaryota_odb9.tar.gz'
blast_filenames = ['tblastn', 'makeblastdb']
default_config_fname = 'config.ini.default'
config_fname = 'config.ini'


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
        logger.main_info('  Downloading BUSCO database...')
        download_unpack_compressed_tar(clade + ' database', url, downloaded_fpath, db_dirpath, logger)

        if not os.path.exists(db_dirpath):
            logger.warning('Failed to download ' + clade + ' database from ' + url + ' and unpack it into ' + dirpath)
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
        logger.main_info('  Downloading third-party tools...')
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


def get_augustus_config_dir(core_dirpath):
    expected_path = os.path.join(core_dirpath, 'config')
    if os.path.isfile(os.path.join(expected_path, 'cgp', 'log_reg_parameters_default.cfg')):
        return expected_path
    return None


def get_augustus_scripts_dir(core_dirpath):
    expected_options = ['scripts', 'bin']
    for opt in expected_options:
        expected_path = os.path.join(core_dirpath, opt)
        if os.path.isfile(os.path.join(expected_path, 'augustus2browser.pl')):
            return expected_path
    return None


def download_augustus(logger, only_clean=False):
    def __check_preinstalled_augustus_completeness(dirpath):
        etraining_path = os.path.join(dirpath, 'bin', 'etraining')
        if not os.path.isfile(etraining_path) or not os.access(etraining_path, os.X_OK):
            return False
        if get_augustus_config_dir(dirpath) and get_augustus_scripts_dir(dirpath):
            return True
        return False

    preinstalled_augustus = qutils.get_path_to_program('augustus', min_version=augustus_version,
                                                       recommend_version=augustus_version,
                                                       max_allowed_version=augustus_max_allowed_version)
    if preinstalled_augustus is not None:
        preinstalled_augustus_dirpath = os.path.dirname(os.path.dirname(preinstalled_augustus))
        if __check_preinstalled_augustus_completeness(preinstalled_augustus_dirpath):
            return preinstalled_augustus_dirpath
    return download_tool('augustus', augustus_version, ['bin'], logger, augustus_url, only_clean=only_clean)


def make_config(output_dirpath, tmp_dirpath, threads, clade_dirpath, augustus_dirpath):
    busco_dirpath = join(dirname(realpath(__file__)), 'busco')
    domain = 'prokaryota' if qconfig.prokaryote else 'eukaryota'
    values = {'out_path': output_dirpath,
              'lineage_path': clade_dirpath,
              'domain': domain,
              'tmp_dir': tmp_dirpath,
              'threads': str(threads),
              'tblastn_path': dirname(get_blast_fpath('tblastn')),
              'makeblastdb_path': dirname(get_blast_fpath('makeblastdb')),
              'augustus_path': join(augustus_dirpath, 'bin'),
              'etraining_path': join(augustus_dirpath, 'bin'),
              'augustus_scripts_path': get_augustus_scripts_dir(augustus_dirpath),
              'hmmsearch_path': busco_dirpath
    }
    default_config_fpath = join(busco_dirpath, default_config_fname)
    config_fpath = join(output_dirpath, config_fname)
    with open(default_config_fpath) as f_in:
        with open(config_fpath, 'w') as f_out:
            for line in f_in:
                fs = line.strip().split()
                if not fs:
                    continue
                keyword = fs[-1]
                if keyword in values:
                    fs[-1] = values[keyword]
                f_out.write(' '.join(fs) + '\n')
    return config_fpath


def busco_main_handler(*busco_args):
    try:
        return busco.main(*busco_args)
    except SystemExit:
        return None


def copy_augustus_configs(augustus_dirpath, output_dirpath):
    input_basedir = get_augustus_config_dir(augustus_dirpath)
    output_basedir = join(output_dirpath, 'config')
    if not input_basedir or not os.path.isdir(input_basedir):
        return None
    if not os.path.isdir(output_basedir):
        os.makedirs(output_basedir)
    for dirpath, dirnames, files in os.walk(input_basedir):
        for dirname in dirnames:
            if dirname != 'species':
                if not os.path.isdir(join(output_basedir, dirname)):
                    shutil.copytree(join(dirpath, dirname), join(output_basedir, dirname))
        break  # note: we use for-break instead of os.walk().next() or next(os.walk()) due to support of both Python 2.5 and Python 3.*
    # species -- special case, we need to copy only generic/template subdirs
    species_input_dir = join(input_basedir, 'species')
    species_output_dir = join(output_basedir, 'species')
    if not os.path.isdir(species_input_dir):
        return None
    if not os.path.isdir(species_output_dir):
        os.makedirs(species_output_dir)
    for dirpath, dirnames, files in os.walk(species_input_dir):
        for dirname in dirnames:
            if 'generic' in dirname or 'template' in dirname or 'fly' in dirname:  # 'fly' is the default species
                if not os.path.isdir(join(species_output_dir, dirname)):
                    shutil.copytree(join(dirpath, dirname), join(species_output_dir, dirname))
        break
    return output_basedir


def cleanup(busco_output_dir):
    do_not_remove_exts = ['.log', '.txt']
    for dirpath, dirnames, files in os.walk(busco_output_dir):
        for dirname in dirnames:
            shutil.rmtree(join(dirpath, dirname))
        for filename in files:
            if os.path.splitext(filename)[1] not in do_not_remove_exts:
                os.remove(join(dirpath, filename))
        break


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

    config_fpath = make_config(output_dir, tmp_dir, busco_threads, clade_dirpath, augustus_dirpath)
    logger.info('  running BUSCO with augustus from ' + augustus_dirpath)
    logger.info('Logs and results will be saved under ' + output_dir + '...')

    os.environ['BUSCO_CONFIG_FILE'] = config_fpath
    os.environ['AUGUSTUS_CONFIG_PATH'] = copy_augustus_configs(augustus_dirpath, tmp_dir)
    if not os.environ['AUGUSTUS_CONFIG_PATH']:
        logger.error('Augustus configs not found, failed to run BUSCO without them.')
    busco_args = [[contigs_fpath, qutils.label_from_fpath_for_fname(contigs_fpath)] for contigs_fpath in contigs_fpaths]
    summary_fpaths = run_parallel(busco_main_handler, busco_args, qconfig.max_threads)
    if not any(fpath for fpath in summary_fpaths):
        logger.error('Failed running BUSCO for all the assemblies. See log files in ' + output_dir + ' for information '
                     '(rerun with --debug to keep all intermediate files).')
        return

    # saving results
    zero_output_for_all = True
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
            if complete_buscos + part_buscos > 0:
                zero_output_for_all = False
            shutil.copy(summary_fpaths[i], output_dir)
        else:
            logger.error(
                'Failed running BUSCO for ' + contigs_fpath + '. See the log for detailed information'
                                                              ' (rerun with --debug to keep all intermediate files).')
    if zero_output_for_all:
        logger.warning('BUSCO did not fail explicitly but found nothing for all assemblies! '
                       'Possible reasons and workarounds:\n'
                       '  1. Provided assemblies are so small that they do not contain even a single partial BUSCO gene. Not likely but may happen -- nothing to worry then.\n'
                       '  2. Incorrect lineage database was used. To run with fungi DB use --fungus, to run with eukaryota DB use --eukaryote, otherwise BUSCO uses bacteria DB.\n'
                       '  3. Problem with BUSCO dependencies, most likely Augustus. Check that the binaries in ' + augustus_dirpath + '/bin/ are working properly.\n'
                       '     If something is wrong with Augustus, you may try to install it yourself '
                             '(https://github.com/Gaius-Augustus/Augustus or `conda install -c bioconda augustus`) and make sure "augustus" binary is in PATH.\n'
                       '     Please install the PROPER VERSION of Augustus, we tested BUSCO with augustus-' + augustus_version +
                             ', it may also work with augustus-' + augustus_max_allowed_version + ', the newer/older versions are not supported.\n'
                       '  4. Some other problem with BUSCO. Check the logs (you may need to rerun QUAST with --debug to see all intermediate files).\n'
                       '     If you cannot solve the problem yourself, post an issue at https://github.com/ablab/quast/issues or write to quast.support@cab.spbu.ru')
    if not qconfig.debug:
        cleanup(output_dir)
    logger.info('Done.')