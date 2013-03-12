############################################################################
# Copyright (c) 2011-2013 Saint-Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################

import logging
import os
import shutil
import subprocess
from libs import reporting
from qutils import id_to_str, print_timestamp, warning
import qconfig


def run_gage(id, filename, gage_results_path, gage_tool_path, reference, tmp_dir):
    log = logging.getLogger('quast')
    log.info('  ' + id_to_str(id) + os.path.basename(filename) + '...')

    # run gage tool
    logfilename_out = os.path.join(gage_results_path, 'gage_' + os.path.basename(filename) + '.stdout')
    logfilename_err = os.path.join(gage_results_path, 'gage_' + os.path.basename(filename) + '.stderr')
    log.info('  ' + id_to_str(id) + 'Logging to files ' + logfilename_out + ' and ' + os.path.basename(logfilename_err) + '...')
    logfile_out = open(logfilename_out, 'w')
    logfile_err = open(logfilename_err, 'w')

    return_code = subprocess.call(
        ['sh', gage_tool_path, reference, filename, tmp_dir, str(qconfig.min_contig)],
        stdout=logfile_out, stderr=logfile_err)

    logfile_out.close()
    logfile_err.close()
    log.info('  ' + id_to_str(id) + 'Done.')
    return return_code


def do(reference, contigs, output_dirpath):
    gage_results_path = os.path.join(output_dirpath, 'gage')

    # suffixes for files with report tables in plain text and tab separated formats
    if not os.path.isdir(gage_results_path):
        os.mkdir(gage_results_path)

    ########################################################################
    gage_tool_path = os.path.join(qconfig.LIBS_LOCATION, 'gage', 'getCorrectnessStats.sh')

    ########################################################################
    log = logging.getLogger('quast')
    print_timestamp()
    log.info('Running GAGE...')

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

    tmp_dir = os.path.join(gage_results_path, 'tmp')
    if not os.path.exists(tmp_dir):
        os.makedirs(tmp_dir)

    n_jobs = min(len(contigs), qconfig.max_threads)
    from joblib import Parallel, delayed
    return_codes = Parallel(n_jobs=n_jobs)(delayed(run_gage)(id, filename, gage_results_path, gage_tool_path, reference, tmp_dir)
        for id, filename in enumerate(contigs))

    error_occurred = False
    for return_code in return_codes:
        if return_code != 0:
            warning("Error occurred while GAGE was processing assemblies. See GAGE error logs for details (%s)" %
                    os.path.join(gage_results_path, 'gage_*.stderr'))
            error_occurred = True
            break

    if not error_occurred:
        ## find metrics for total report:
        for id, filename in enumerate(contigs):
            report = reporting.get(filename)

            logfilename_out = os.path.join(gage_results_path, 'gage_' + os.path.basename(filename) + '.stdout')
            logfile_out = open(logfilename_out, 'r')
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
            shutil.rmtree(tmp_dir)

        log.info('Done.')
