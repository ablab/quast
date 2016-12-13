############################################################################
# Copyright (c) 2015-2016 Saint Petersburg State University
# Copyright (c) 2011-2015 Saint Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################

import logging
import os
import shutil
from quast_libs import reporting, qconfig, qutils, ca_utils
from quast_libs.ca_utils.misc import compile_aligner
from .qutils import get_path_to_program, is_python2
from os.path import join, abspath

from quast_libs.log import get_logger
logger = get_logger(qconfig.LOGGER_DEFAULT_NAME)

required_java_fnames = ['Utils', 'SizeFasta', 'GetFastaStats']


def all_required_java_classes_exist(dirpath):
    for required_name in required_java_fnames:
        if not os.path.isfile(os.path.join(dirpath, required_name + '.class')):
            return False
    return True


def run_gage(i, contigs_fpath, gage_results_dirpath, gage_tool_path, reference, tmp_dir):
    assembly_label = qutils.label_from_fpath(contigs_fpath)
    corr_assembly_label = qutils.label_from_fpath_for_fname(contigs_fpath)

    logger.info('  ' + qutils.index_to_str(i) + assembly_label + '...')

    # run gage tool
    log_out_fpath = os.path.join(gage_results_dirpath, 'gage_' + corr_assembly_label + '.stdout')
    log_err_fpath = os.path.join(gage_results_dirpath, 'gage_' + corr_assembly_label + '.stderr')
    logger.info('  ' + qutils.index_to_str(i) + 'Logging to files ' +
                os.path.basename(log_out_fpath) + ' and ' +
                os.path.basename(log_err_fpath) + '...')
    log_out_f = open(log_out_fpath, 'w')
    log_err_f = open(log_err_fpath, 'w')

    return_code = qutils.call_subprocess(
        ['sh', gage_tool_path, abspath(ca_utils.misc.contig_aligner_dirpath), reference,
         contigs_fpath, tmp_dir, str(qconfig.min_contig)],
        stdout=log_out_f,
        stderr=log_err_f,
        indent='  ' + qutils.index_to_str(i),
        only_if_debug=False)
    if return_code != 0:
        logger.info('  ' + qutils.index_to_str(i) + 'Failed.')
    else:
        logger.info('  ' + qutils.index_to_str(i) + 'Done.')

    log_out_f.close()
    log_err_f.close()

    return return_code


gage_dirpath = os.path.join(qconfig.LIBS_LOCATION, 'gage')


def compile_gage(only_clean=False):
    if only_clean:
        for required_name in required_java_fnames:
            fpath = os.path.join(gage_dirpath, required_name + '.class')
            if os.path.isfile(fpath):
                os.remove(fpath)
        return True

    javac_path = get_path_to_program('javac')
    if javac_path is None:
        logger.error('Java compiler not found (javac)! '
                     'Please install it or compile GAGE java classes manually (' + gage_dirpath + '/*.java)!')
        return False

    cur_dir = os.getcwd()
    os.chdir(gage_dirpath)
    # making
    logger.main_info('Compiling JAVA classes (details are in ' + os.path.join(gage_dirpath, 'make.log') + ' and make.err)')
    return_codes = [qutils.call_subprocess(
        ['javac', os.path.join(gage_dirpath, java_fname + '.java')],
        stdout=open(os.path.join(gage_dirpath, 'make.log'), 'w'),
        stderr=open(os.path.join(gage_dirpath, 'make.err'), 'w'),) for java_fname in required_java_fnames]
    os.chdir(cur_dir)

    if any(return_code != 0 for return_code in return_codes) or not all_required_java_classes_exist(gage_dirpath):
        logger.error('Error occurred during compilation of java classes (' + gage_dirpath + '/*.java)! '
                     'Try to compile it manually. ' + ('You can restart Quast with the --debug flag '
                     'to see the command line.' if not qconfig.debug else ''))
        return False
    return True


def do(ref_fpath, contigs_fpaths, output_dirpath):
    gage_results_dirpath = os.path.join(output_dirpath, 'gage')

    # suffixes for files with report tables in plain text and tab separated formats
    if not os.path.isdir(gage_results_dirpath):
        os.mkdir(gage_results_dirpath)

    ########################################################################
    gage_tool_path = os.path.join(gage_dirpath, 'getCorrectnessStats.sh')

    ########################################################################
    logger.print_timestamp()
    logger.main_info('Running GAGE...')

    metrics = ['Total units', 'Min', 'Max', 'N50', 'Genome Size', 'Assembly Size', 'Chaff bases',
               'Missing Reference Bases', 'Missing Assembly Bases', 'Missing Assembly Contigs',
               'Duplicated Reference Bases', 'Compressed Reference Bases', 'Bad Trim', 'Avg Idy', 'SNPs', 'Indels < 5bp',
               'Indels >= 5', 'Inversions', 'Relocation', 'Translocation',
               'Total units', 'BasesInFasta', 'Min', 'Max', 'N50']
    metrics_in_reporting = [reporting.Fields.GAGE_NUMCONTIGS, reporting.Fields.GAGE_MINCONTIG, reporting.Fields.GAGE_MAXCONTIG, 
                            reporting.Fields.GAGE_N50, reporting.Fields.GAGE_GENOMESIZE, reporting.Fields.GAGE_ASSEMBLY_SIZE,
                            reporting.Fields.GAGE_CHAFFBASES, reporting.Fields.GAGE_MISSINGREFBASES, reporting.Fields.GAGE_MISSINGASMBLYBASES, 
                            reporting.Fields.GAGE_MISSINGASMBLYCONTIGS, reporting.Fields.GAGE_DUPREFBASES, 
                            reporting.Fields.GAGE_COMPRESSEDREFBASES, reporting.Fields.GAGE_BADTRIM, reporting.Fields.GAGE_AVGIDY, 
                            reporting.Fields.GAGE_SNPS, reporting.Fields.GAGE_SHORTINDELS, reporting.Fields.GAGE_LONGINDELS, 
                            reporting.Fields.GAGE_INVERSIONS, reporting.Fields.GAGE_RELOCATION, reporting.Fields.GAGE_TRANSLOCATION, 
                            reporting.Fields.GAGE_NUMCORCONTIGS, reporting.Fields.GAGE_CORASMBLYSIZE, reporting.Fields.GAGE_MINCORCONTIG, 
                            reporting.Fields.GAGE_MAXCORCOTING, reporting.Fields.GAGE_CORN50]

    tmp_dirpath = os.path.join(gage_results_dirpath, 'tmp')
    if not os.path.exists(tmp_dirpath):
        os.makedirs(tmp_dirpath)

    if not compile_aligner(logger) or (not all_required_java_classes_exist(gage_dirpath) and not compile_gage()):
        logger.error('GAGE module was not installed properly, so it is disabled and you cannot use --gage.')
        return

    n_jobs = min(len(contigs_fpaths), qconfig.max_threads)
    if is_python2():
        from joblib import Parallel, delayed
    else:
        from joblib3 import Parallel, delayed
    return_codes = Parallel(n_jobs=n_jobs)(delayed(run_gage)(i, contigs_fpath, gage_results_dirpath, gage_tool_path, ref_fpath, tmp_dirpath)
        for i, contigs_fpath in enumerate(contigs_fpaths))

    if 0 not in return_codes:
        logger.error('Error occurred while GAGE was processing assemblies.'
                     ' See GAGE error logs for details: %s' % os.path.join(gage_results_dirpath, 'gage_*.stderr'))
        return

    ## find metrics for total report:
    for i, contigs_fpath in enumerate(contigs_fpaths):
        corr_assembly_label = qutils.label_from_fpath_for_fname(contigs_fpath)

        report = reporting.get(contigs_fpath)

        log_out_fpath = os.path.join(
            gage_results_dirpath, 'gage_' + corr_assembly_label + '.stdout')
        logfile_out = open(log_out_fpath, 'r')
        cur_metric_id = 0
        for line in logfile_out:
            if metrics[cur_metric_id] in line:
                if (metrics[cur_metric_id].startswith('N50')):
                    report.add_field(metrics_in_reporting[cur_metric_id], line.split(metrics[cur_metric_id] + ':')[1].strip())
                else:
                    report.add_field(metrics_in_reporting[cur_metric_id], line.split(':')[1].strip())
                cur_metric_id += 1
                if cur_metric_id == len(metrics):
                    break
        logfile_out.close()

    reporting.save_gage(output_dirpath)

    if not qconfig.debug:
        shutil.rmtree(tmp_dirpath)

    logger.main_info('Done.')
