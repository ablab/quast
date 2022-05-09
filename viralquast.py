#!/usr/bin/env python3

import os
import shutil
import sys
import wget

from quast_libs import contigs_analyzer, qutils, qconfig, reporting
from quast_libs.qutils import cleanup, check_dirpath, check_reads_fpaths
from quast_libs.options_parser import QuastOption, OptionParser, check_output_dir, check_arg_value
from quast_libs.viralquast.preprocess import preprocess
from quast_libs.viralquast.mash_finder import MashReferenceFinder, MinimapReferenceFinder
from typing import Optional, Tuple

from quast_libs.log import get_logger
logger = get_logger(qconfig.LOGGER_DEFAULT_NAME)
logger.set_up_console_handler()

help_str = '''
Usage: 

    viralquast scaffolds.fasta      find best sample from precomputed set
                                    of reference samples, runs quast with
                                    found sample as reference

options:

    -r <reference.fasta>                use provided reference file as
                                        as set of reference samples

    --mash-ref <reference.msh>          path to custom mash reference

    --preprocess  <file>                preprocess custom reference file
                                        to then use it in -r option
    
    --no-mash                           uses alternative algorithm for
                                        for finding best reference sample,
                                        don't need preprocessing

    -o <output dir>                     directory to store all result files [default: viralquast_results]

    --download-default-reference        download prepared references
    
    --download-path <path>              specifies path where references wold be downloaded
    
    --mash-path <path>                  specifies path to mash executable
    
    --minimap-path <path>               specifies path to minimap executable
    
    --save-all-reports                  saves all reports
    
    -t                                  number of threads
    
    --quast-options "parameters"        parameters for internal quast runs
    
    --update-reference                  downloads newest reference and preprocess it
    
    --test                              run in test mode
'''



def parse_args(logger, quast_args):

    if '-h' in quast_args or '--help' in quast_args or '--help-hidden' in quast_args:
        stream = sys.stdout
        stream.write('ViralQUAST: Quality Assessment Tool for Viruse Assemblies\n')
        stream.write(help_str)
        sys.exit(0)


    if '-v' in quast_args or '--version' in quast_args:
        qconfig.print_version(mode)
        sys.exit(0)

    quast_py_args = quast_args[1:]

    options = [
        (['-o', '--output-dir'], dict(
             dest='output_dirpath',
             type='string',
             action='callback',
             callback=check_output_dir,
             callback_args=(logger,))
         ),
        (['-t', '--threads'], dict(
             dest='max_threads',
             type='int',
             action='callback',
             callback=check_arg_value,
             callback_args=(logger,),
             callback_kwargs={'default_value': 1, 'min_value': 1})
         ),
        (['-r', '-R', '--reference'], dict(
             dest='reference',
             type='file',
             action='store')
         ),
        (['--mash-reference'], dict(
             dest='mash_reference',
             type='file',
             action='store')
         ),
        (['--preprocess'], dict(
            dest='preprocess_file',
            type='file',
            action='store'
            )
         ),
        (['--no-mash'], dict(
            dest='no_mash',
            action='store_true'
            )
         ),
        (['--save-all-reports'], dict(
            dest='save_all_reports',
            action='store_true'
            )
         ),
        (['--quast-options'], dict(
            dest='quast_options',
            action='store'
            )
         ),
        (['--download-path'], dict(
            dest='download_path',
            action='store'
            )
         ),
        (['--download-default-reference'], dict(
            dest='download_default',
            action='store_true'
            )
         ),
        (['--test'], dict(
            dest='test',
            action='store_true'
        )
         ),
        (['--mash-path'], dict(
            dest='mash_path',
            action='store'
        )
         ),
        (['--minimap-path'], dict(
            dest='minimap_path',
            action='store'
        )
         ),
        (['--update-reference'], dict(
            dest='update_reference',
            action='callback',
            callback=update_reference,
            callback_args=(logger,))
         ),

    ]
    parser = OptionParser(option_class=QuastOption)
    for args, kwargs in options:
        parser.add_option(*args, **kwargs)
    (opts, contigs_fpaths) = parser.parse_args(quast_args[1:])

    return quast_py_args, contigs_fpaths


def check_reference() -> Tuple[str, str]:
    path = os.sep.join([os.path.abspath(os.curdir), 'quast_libs', 'viralquast', 'references'])
    alternative_path = os.sep.join(['~', '.quast', 'viralquast', 'references'])
    mash_reference_path = None
    if qconfig.no_mash is False:
        if qconfig.mash_reference is not None:
            mash_reference_path = qconfig.mash_reference
        elif os.path.exists(os.sep.join([path, 'reference.msh'])):
            mash_reference_path = os.sep.join([path, 'reference.msh'])
        elif os.path.exists(os.sep.join([alternative_path, 'reference.msh'])):
            mash_reference_path = os.sep.join([alternative_path, 'reference.msh'])
    reference_path = None
    if qconfig.reference is not None:
        reference_path = qconfig.reference
    elif os.path.exists(os.sep.join([path, 'reference.fasta.gz'])):
        reference_path = os.sep.join([path, 'reference.fasta.gz'])
    elif os.path.exists(os.sep.join([alternative_path, 'reference.fasta.gz'])):
        reference_path = os.sep.join([alternative_path, 'reference.fasta.gz'])
    return mash_reference_path, reference_path



def check_path(path: str, filename: str) -> bool:
    reference_path = os.sep.join([path, filename])
    if os.path.exists(reference_path):
        logger.info('Reference already exists, skipping...')
        return None
    try:
        os.makedirs(path, exist_ok=True)
    except:
        return False
    return True

def symlink_force(target, link_name):
    try:
        os.symlink(target, link_name)
    except:
        os.remove(link_name)
        os.symlink(target, link_name)

def get_reference(link: str, filename: str='reference.fasta.gz', path: str=None, replace: bool=False) -> Optional[str]:
    if path is None:
        path = os.sep.join([os.path.abspath(os.curdir), 'quast_libs', 'viralquast', 'references'])
        check = check_path(path, filename)
        if check is None and replace is False:
            return
        if check is False:
            path = os.sep.join(['~', '.quast', 'viralquast', 'references', filename])
            check = check_path(path, filename)
        if check is None and replace is False:
            return
        if check is False and replace is False:
            logger.error('Can not access default directories, error')
            return

    reference_path = os.sep.join([path, filename])

    logger.info('Reference would be downloaded to {}'.format(path))
    logger.info('Starting download...')

    downloaded_path = wget.download(link, path)

    symlink_force(downloaded_path, reference_path)

    logger.info('Download complete')
    return downloaded_path


def get_default_references():
    get_reference('https://rvdb.dbi.udel.edu/download/U-RVDBv22.0.fasta.gz', 'reference.fasta.gz', qconfig.download_path)
    get_reference('https://zenodo.org/record/6520127/files/U-RVDBv22.msh', 'reference.msh', qconfig.download_path)


def update_reference(option, opt_str, value, parser, logger):
    downloaded_path = get_reference('https://rvdb.dbi.udel.edu/download/U-RVDBvCurrent.fasta.gz', 'reference.fasta.gz',
                                   path=None, replace=True)
    new_path = preprocess(None, None, downloaded_path, logger, threads=qconfig.max_threads)
    path = os.sep.join([os.path.abspath(os.curdir), 'quast_libs', 'viralquast', 'references'])
    check = check_path(path, filename)
    if check is False:
        path = os.sep.join(['~', '.quast', 'viralquast', 'references', filename])
        check = check_path(path, filename)
    if check is False:
        logger.error('Can not access default directories, error')
        return
    reference_path = os.sep.join([path, 'reference.msh'])
    symlink_force(new_path, reference_path)


def main(args):
    check_dirpath(qconfig.QUAST_HOME, 'You are trying to run it from ' + str(qconfig.QUAST_HOME) + '\n.' +
                  'Please, put QUAST in a different directory, then try again.\n', exit_code=3)

    if not args:
        qconfig.usage(stream=sys.stderr)
        sys.exit(1)

    try:
        import importlib
        importlib.reload(qconfig)
        importlib.reload(qutils)
    except (ImportError, AttributeError):
        reload(qconfig)
        reload(qutils)

    try:
        locale.setlocale(locale.LC_ALL, 'en_US.utf8')
    except Exception:
        try:
            locale.setlocale(locale.LC_ALL, 'en_US.UTF-8')
        except Exception:
            print('Python locale settings can\'t be changed')
    quast_path = [__file__]
    quast_py_args, contigs_fpaths = parse_args(logger, quast_path + args)

    if qconfig.test:
        qconfig.mash_reference = os.sep.join(['quast_libs', 'viralquast', 'references', 'test_reference.msh'])
        qconfig.reference = os.sep.join(['quast_libs', 'viralquast', 'references', 'test_reference.fasta.gz'])
        contigs_fpaths = [os.sep.join(['quast_libs', 'viralquast', 'references', 'test_scaffolds.fasta'])]
        qconfig.output_dirpath = 'viralquast_test_output'

    if qconfig.preprocess_file is not None:
        new_path = preprocess(None, None, qconfig.preprocess_file, logger, threads=qconfig.max_threads)
        qconfig.mash_reference = new_path
        if not contigs_fpaths or len(contigs_fpaths) == 0:
            logger.info('Contigs not provided, exiting')
            sys.exit(0)

    if not contigs_fpaths:
        logger.error("You should specify at least one file with contigs!\n", to_stderr=True)
        qconfig.usage(stream=sys.stderr)
        sys.exit(2)

    if qconfig.download_default is True:
        get_default_references()

    mash_reference_path, reference_path = check_reference()

    if reference_path is None:
        logger.error('Reference file not found, try to run with --download-reference flag or use -r option')
        sys.exit(1)

    finder = MashReferenceFinder(logger, reference_path)
    if qconfig.no_mash:
        finder = MinimapReferenceFinder(logger, reference_path)
        finder.find_reference(contigs_fpaths[0])
    else:
        if mash_reference_path is None:
            logger.error('Mash reference file not found, try to run with --download-reference flag or use --mash-reference option')
            sys.exit(1)
        finder.find_reference(contigs_fpaths[0], mash_reference_path)

    logger.info('Finished!')


if __name__ == "__main__":
    try:
        return_code = main(sys.argv[1:])
        exit(return_code)
    except Exception:
        _, exc_value, _ = sys.exc_info()
        logger.exception(exc_value)
        logger.error('exception caught!', exit_with_code=1, to_stderr=True)
