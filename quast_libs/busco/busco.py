#!/usr/bin/env python
# coding: utf-8
"""
.. module:: run_BUSCO
   :synopsis: BUSCO - Benchmarking Universal Single-Copy Orthologs.
.. versionadded:: 3.0.0
.. versionchanged:: 3.0.1

This is the BUSCO main script.

To get help, ``python run_BUSCO.py -h``. See also the user guide.

And visit our website `<http://busco.ezlab.org/>`_

Copyright (c) 2016-2017, Evgeny Zdobnov (ez@ezlab.org)
Licensed under the MIT license. See LICENSE.md file.

"""

import time
import traceback
import sys
import os
import argparse
import logging

from quast_libs.busco.BuscoConfig import BuscoConfig
from argparse import RawTextHelpFormatter

from os.path import isdir

from quast_libs.qutils import is_non_empty_file


def _parse_args():
    """
    This function parses the arguments provided by the user
    :return: a dictionary having a key for each arguments
    :rtype: dict
    """
    # small hack to get sub-parameters with dash and pass it to Augustus
    #for i, arg in enumerate(sys.argv):
    #    if (arg[0] == '-' or arg[0] == '--') and (sys.argv[i - 1] == '-a' or sys.argv[i - 1] == '--augustus'):
    #        sys.argv[i] = ' ' + arg

    parser = argparse.ArgumentParser(
        description='Welcome to BUSCO %s: the Benchmarking Universal Single-Copy Ortholog assessment tool.\n'
                    'For more detailed usage information, please review the README file provided with '
                    'this distribution and the BUSCO user guide.' % BuscoConfig.VERSION,
        usage='python BUSCO.py -i [SEQUENCE_FILE] -l [LINEAGE] -o [OUTPUT_NAME] -m [MODE] [OTHER OPTIONS]',
        formatter_class=RawTextHelpFormatter, add_help=False)

    optional = parser.add_argument_group('optional arguments')

    optional.add_argument(
        '-i', '--in', dest='in', required=False, metavar='FASTA FILE', help='Input sequence file in FASTA format. '
        'Can be an assembled genome or transcriptome (DNA), or protein sequences from an annotated gene set.')

    optional.add_argument(
        '-c', '--cpu', dest='cpu', required=False, metavar='N', help='Specify the number (N=integer) '
                                                                     'of threads/cores to use.')
    optional.add_argument(
        '-o', '--out', dest='out', required=False, metavar='OUTPUT',
        help='Give your analysis run a recognisable short name. '
             'Output folders and files will be labelled with this name. WARNING: do not provide a path')

    optional.add_argument(
        '-e', '--evalue', dest='evalue', required=False, metavar='N', type=float,
        help='E-value cutoff for BLAST searches. '
             'Allowed formats, 0.001 or 1e-03 (Default: %.0e)' % BuscoConfig.DEFAULT_ARGS_VALUES['evalue'])

    optional.add_argument(
        '-m', '--mode', dest='mode', required=False, metavar='MODE',
        help='Specify which BUSCO analysis mode to run.\n'
             'There are three valid modes:\n- geno or genome, for genome assemblies (DNA)\n- tran or '
             'transcriptome, '
             'for transcriptome assemblies (DNA)\n- prot or proteins, for annotated gene sets (protein)')
    optional.add_argument(
        '-l', '--lineage_path', dest='lineage_path', required=False, metavar='LINEAGE',
        help='Specify location of the BUSCO lineage data to be used.\n'
             'Visit http://busco.ezlab.org for available lineages.')

    optional.add_argument(
        '-f', '--force', action='store_true', required=False, dest='force',
        help='Force rewriting of existing files. '
             'Must be used when output files with the provided name already exist.')

    optional.add_argument(
        '-r', '--restart', action='store_true', required=False, dest='restart',
        help='Restart an uncompleted run. Not available for the protein mode')

    optional.add_argument(
        '-sp', '--species', required=False, dest='species', metavar='SPECIES',
        help='Name of existing Augustus species gene finding parameters. '
             'See Augustus documentation for available options.')

    optional.add_argument('--augustus_parameters', required=False, dest='augustus_parameters',
                          help='Additional parameters for the fine-tuning of Augustus run. '
                               'For the species, do not use this option.\n'
                               'Use single quotes as follow: \'--param1=1 --param2=2\', '
                               'see Augustus documentation for available options.')

    optional.add_argument(
        '-t', '--tmp_path', metavar='PATH', required=False, dest='tmp_path',
        help='Where to store temporary files (Default: %s)' % BuscoConfig.DEFAULT_ARGS_VALUES['tmp_path'])

    optional.add_argument(
        '--limit', dest='limit', metavar='REGION_LIMIT', required=False,
        type=int, help='How many candidate regions (contig or transcript) to consider per BUSCO (default: %s)'
                       % str(BuscoConfig.DEFAULT_ARGS_VALUES['limit']))

    optional.add_argument(
        '--long', action='store_true', required=False, dest='long',
        help='Optimization mode Augustus '
             'self-training (Default: Off) adds considerably to the run time, '
             'but can improve results for some non-model organisms')

    optional.add_argument(
        '-q', '--quiet', dest='quiet', required=False, help='Disable the info logs, displays only errors',
        action="store_true")

    optional.add_argument(
        '-z', '--tarzip', dest='tarzip', required=False, help='Tarzip the output folders likely to '
                                                              'contain thousands of files',
        action="store_true")

    optional.add_argument(
        '--blast_single_core', dest='blast_single_core', required=False,
        help='Force tblastn to run on a single core and ignore the --cpu argument for this step only. '
             'Useful if inconsistencies when using multiple threads are noticed',
        action="store_true")

    optional.add_argument('-v', '--version', action='version', help="Show this version and exit",
                          version='BUSCO %s' % BuscoConfig.VERSION)

    optional.add_argument('-h', '--help', action="help", help="Show this help message and exit")

    return vars(parser.parse_args())


def main(in_fpath, out_fname):
    """
    This function runs a BUSCO analysis according to the provided parameters.
    See the help for more details:
    ``python run_BUSCO.py -h``
    :raises SystemExit: if any errors occur
    """
    start_time = time.time()
    # 1) Load a busco config file that will figure out all the params from all sources
    # i.e. provided config file, dataset cfg, and user args
    if os.environ.get('BUSCO_CONFIG_FILE') and os.access(os.environ.get('BUSCO_CONFIG_FILE'), os.R_OK):
        config_file = os.environ.get('BUSCO_CONFIG_FILE')
    else:
        config_file = '%s//config.ini.default' % os.path.dirname(os.path.realpath(__file__))
    config = BuscoConfig(config_file, args={'in': in_fpath, 'out': out_fname})
    # Define a logger, the config is passed to tell the logger if you required the quiet mode

    assembly_dirpath = os.path.join(config.get('busco', 'out_path'), 'run_%s' % out_fname)
    if not isdir(assembly_dirpath):
        os.makedirs(assembly_dirpath)
    summary_path = os.path.join(assembly_dirpath, 'short_summary_%s.txt' % out_fname)

    from quast_libs.busco import pipebricks
    pipebricks.PipeLogger.run_dirpath = assembly_dirpath
    from quast_libs.busco.GenomeAnalysis import GenomeAnalysis
    from quast_libs.busco.BuscoAnalysis import BuscoAnalysis
    from quast_libs.busco.pipebricks.Toolset import ToolException
    BuscoAnalysis._logger.reload_log()
    logger = BuscoAnalysis._logger
    if is_non_empty_file(summary_path):
        logger.info('Using existing BUSCO files for ' + out_fname + '...')
        return summary_path

    try:
        try:
            logger.info(
                '****************** Start a BUSCO %s analysis, current time: %s **'
                '****************' % (BuscoConfig.VERSION, time.strftime('%m/%d/%Y %H:%M:%S')))
            logger.info('Configuration loaded from %s' % config_file)
            # 2) Load the analysis, this will check the dependencies and return the appropriate analysis object
            analysis = GenomeAnalysis(config)

            # 3) Run the analysis
            analysis.run_analysis()

            if not logger.has_warning():
                logger.info('BUSCO analysis done. Total running time: %s seconds' % str(time.time() - start_time))
            else:
                logger.info('BUSCO analysis done with WARNING(s). Total running time: %s seconds'
                            % str(time.time() - start_time))

            logger.info('Results written in %s\n' % analysis.mainout)

        except ToolException as e:
            #
            logger.error(e)
            raise SystemExit

    except SystemExit:
        logger.error('BUSCO analysis failed !')
        logger.error(
            'Check the logs, read the user guide, if you still need technical '
            'support, then please contact %s\n' % BuscoConfig.CONTACT)
        raise SystemExit

    except KeyboardInterrupt:
        logger.error('A signal was sent to kill the process')
        logger.error('BUSCO analysis failed !')
        logger.error(
            'Check the logs, read the user guide, if you still need technical '
            'support, then please contact %s\n' % BuscoConfig.CONTACT)
        raise SystemExit

    except BaseException:
        exc_type, exc_value, exc_traceback = sys.exc_info()
        logger.critical('Unhandled exception occurred: %s\n' % traceback.format_exception(
            exc_type, exc_value, exc_traceback))
        logger.error('BUSCO analysis failed !')
        logger.error(
            'Check the logs, read the user guide, if you still need technical '
            'support, then please contact %s\n' % BuscoConfig.CONTACT)
        raise SystemExit
    return summary_path


# Entry point
if __name__ == "__main__":
    main()
