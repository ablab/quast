############################################################################
# Copyright (c) 2011-2012 Saint-Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################

import os
import subprocess
from libs import reporting
from qutils import id_to_str


def do(reference, contigs, output_dirpath, min_contig, lib_dir):
    gage_results_path = os.path.join(output_dirpath, 'gage')

    # suffixes for files with report tables in plain text and tab separated formats
    if not os.path.isdir(gage_results_path):
        os.mkdir(gage_results_path)

    ########################################################################
    gage_tool_path = os.path.join(lib_dir, 'gage/getCorrectnessStats.sh')

    ########################################################################

    print 'Running GAGE tool...'
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

    tmp_dir = gage_results_path + '/tmp/'
    for id, filename in enumerate(contigs):
        report = reporting.get(filename)
        print ' ', id_to_str(id), os.path.basename(filename), '...'
        # run gage tool
        logfilename_out = gage_results_path + '/gage_' + os.path.basename(filename) + '.stdout'
        logfilename_err = gage_results_path + '/gage_' + os.path.basename(filename) + '.stderr'
        print '    Logging to files', logfilename_out, 'and', os.path.basename(logfilename_err), '...',
        logfile_out = open(logfilename_out, 'w')
        logfile_err = open(logfilename_err, 'w')

        subprocess.call(
            ['sh', gage_tool_path, reference, filename, tmp_dir, str(min_contig)],
            stdout=logfile_out, stderr=logfile_err)

        logfile_out.close()
        logfile_err.close()
        print '  Done.'

        ## find metrics for total report:
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

    print '  Done'

    reporting.save_gage(output_dirpath)

    print '  Done.'
