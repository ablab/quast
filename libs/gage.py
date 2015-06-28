############################################################################
# Copyright (c) 2011-2015 Saint-Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################

import logging
import os
import shutil
from libs import reporting, qutils
import qconfig

from libs.log import get_logger
logger = get_logger(qconfig.LOGGER_DEFAULT_NAME)


def run_gage(i, contigs_fpath, gage_results_dirpath, gage_tool_path, reference, tmp_dir):
    assembly_label = qutils.label_from_fpath_for_fname(contigs_fpath)

    logger.info('  ' + qutils.index_to_str(i) + assembly_label + '...')

    # run gage tool
    log_out_fpath = os.path.join(gage_results_dirpath, 'gage_' + assembly_label + '.stdout')
    log_err_fpath = os.path.join(gage_results_dirpath, 'gage_' + assembly_label + '.stderr')
    logger.info('  ' + qutils.index_to_str(i) + 'Logging to files ' +
                os.path.basename(log_out_fpath) + ' and ' +
                os.path.basename(log_err_fpath) + '...')
    log_out_f = open(log_out_fpath, 'w')
    log_err_f = open(log_err_fpath, 'w')

    return_code = qutils.call_subprocess(
        ['sh', gage_tool_path, reference, contigs_fpath, tmp_dir, str(qconfig.min_contig)],
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


def do(ref_fpath, contigs_fpaths, output_dirpath):
    gage_results_dirpath = os.path.join(output_dirpath, 'gage')

    # suffixes for files with report tables in plain text and tab separated formats
    if not os.path.isdir(gage_results_dirpath):
        os.mkdir(gage_results_dirpath)

    ########################################################################
    gage_tool_path = os.path.join(qconfig.LIBS_LOCATION, 'gage', 'getCorrectnessStats.sh')

    ########################################################################
    logger.print_timestamp()
    logger.info('Running GAGE...')

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

    n_jobs = min(len(contigs_fpaths), qconfig.max_threads)
    from joblib import Parallel, delayed
    return_codes = Parallel(n_jobs=n_jobs)(delayed(run_gage)(i, contigs_fpath, gage_results_dirpath, gage_tool_path, ref_fpath, tmp_dirpath)
        for i, contigs_fpath in enumerate(contigs_fpaths))

    if 0 not in return_codes:
        logger.warning('Error occurred while GAGE was processing assemblies.'
                       ' See GAGE error logs for details: %s' %
                os.path.join(gage_results_dirpath, 'gage_*.stderr'))
        return

    ## find metrics for total report:
    for i, contigs_fpath in enumerate(contigs_fpaths):
        assembly_label = qutils.label_from_fpath_for_fname(contigs_fpath)

        report = reporting.get(contigs_fpath)

        log_out_fpath = os.path.join(
            gage_results_dirpath, 'gage_' + assembly_label + '.stdout')
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

    logger.info('Done.')
