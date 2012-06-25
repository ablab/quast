############################################################################
# Copyright (c) 2011-2012 Saint-Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################

import os
import sys
from qutils import id_to_str
import shutil
import subprocess

## prepare using of VNC display if it is installed and configured
def prepare_display():    
    if os.path.isfile(os.path.expanduser('~/.vnc/passwd')):        
        output = subprocess.Popen(['vncserver'], stdout=subprocess.PIPE, stderr=subprocess.PIPE).communicate()[1]
        display_name = ''
        for line in output.split('\n'):
            if line.startswith('New'):
                return line[line.rindex(':'):]       
    else:
        return ''  # VNC is not installed or not configured

### remove unaligned contigs from assembly
def remove_unaligned(filename, nucmer_dir, tmpFolder):
    nucmer_prefix = os.path.join(os.path.abspath(sys.path[0]), nucmer_dir + '/nucmer_')
    unaligned_suffix = '.unaligned'
    unaligned_filename = nucmer_prefix + os.path.basename(filename) + unaligned_suffix
    if not os.path.isfile(unaligned_filename):
        print '  WARNING: nucmer unaligned file (' + unaligned_filename + ') not found, skipping...'
        return filename
    unaligned_file = open(unaligned_filename, 'r')
    ids = []
    for line in unaligned_file:
        ids.append(line)
    unaligned_file.close()

    in_contigs = open(filename, "r")
    without_unaligned_filename = tmpFolder + '/' + os.path.basename(filename)
    out_contigs = open(without_unaligned_filename, "w")

    line = in_contigs.readline()
    while (1):
        if not line:
            break

        if line.startswith(">"):
            is_unaligned = False
            for contigID in ids:
                if line.find(contigID) != -1:
                    is_unaligned = True
                    break
            if not is_unaligned:
                out_contigs.write(line)
                line = in_contigs.readline()
                while line and not line.startswith(">"):
                    out_contigs.write(line)
                    line = in_contigs.readline()
            else:
                line = in_contigs.readline()
        else:
            line = in_contigs.readline()

    in_contigs.close()
    out_contigs.close()
    return without_unaligned_filename
    
### main function ###
def do(reference, filenames, nucmer_dir, output_dir, lib_dir):
    print 'Running mauve...'

    output_dir = os.path.join(os.path.abspath(sys.path[0]), output_dir)
    reference = os.path.join(os.path.abspath(sys.path[0]), reference)
    log_dir = os.path.join(os.path.abspath(sys.path[0]), output_dir + '/logs/')
    common_log_file = log_dir + '/common_log.txt'        

    print '  Logging to ' + log_dir

    if os.path.isdir(output_dir):
        shutil.rmtree(output_dir)
    os.mkdir(output_dir)
    os.mkdir(log_dir)   

    ########################################################################
    report_dict = {'header' : []}
    for filename in filenames:
        report_dict[os.path.basename(filename)] = []    

    #creating tmp folder
    tmpFolder = os.path.join(os.path.abspath(sys.path[0]), output_dir, 'scores')
    if os.path.isdir(tmpFolder):
        shutil.rmtree(tmpFolder)
    os.mkdir(tmpFolder)

    # configuring VNC display (if VNC is configured)
    display_name = prepare_display()
    myenv = os.environ.copy()
    if display_name != '':
        myenv['DISPLAY'] = display_name

    # process all contig files with Mauve tool (aligner)
    no_display = False
    for id, filename in enumerate(filenames):
        print ' ', id_to_str(id), os.path.basename(filename), '...'  
        logfilename_out = log_dir + '/' + os.path.basename(filename) + '.stdout'
        logfilename_err = log_dir + '/' + os.path.basename(filename) + '.stderr'
        filename = remove_unaligned(filename, nucmer_dir, tmpFolder)
      
        log_out = open(logfilename_out, 'w')
        log_err = open(logfilename_err, 'w')        
        proc = subprocess.Popen(['java', '-cp', 'Mauve.jar', 'org.gel.mauve.assembly.ScoreAssembly', \
            '-reference', reference, '-assembly', filename,                                  \
            '-reorder', tmpFolder + '/' + os.path.basename(filename) + '_scores',            \
            '-outputDir', tmpFolder + '/' + os.path.basename(filename) + '_scores'],         \
            env=myenv, stdout=log_out, stderr=log_err, cwd=os.path.join(lib_dir, 'mauve'))        
        size_of_log_err = os.path.getsize(logfilename_err)
        while proc.poll() != 0:
            if size_of_log_err != os.path.getsize(logfilename_err): # if there are new errors
                size_of_log_err = os.path.getsize(logfilename_err)
                errfile = open(logfilename_err, 'r')
                lines = errfile.readlines()
                errfile.close()                
                if lines[-1].startswith('Exited with error'):
                    print '  Mauve doesn\'t finish. See error log for details: ' + logfilename_err + '\n'
                    proc.kill()
                    break
                elif lines[-1].startswith('No X11 DISPLAY variable was set'):
                    print '  No X11 DISPLAY variable was set. Mauve failed. See README for instruction\n'
                    proc.kill()
                    no_display = True
                    break					
        log_out.close()
        log_err.close()
        if no_display:
            break

    # if VNC was started then kill it
    if display_name != '':
        devnull = open('/dev/null', 'w')
        subprocess.call(['vncserver', '-kill', display_name], stdout=devnull, stderr=devnull)

    if not no_display:
        # plot
        print '  Generating plots to ' + output_dir
        logfile = open(common_log_file , 'w')
        logfile.write('Generating plots\n')
        subprocess.call(['perl', 'mauveAssemblyMetrics.pl', tmpFolder, output_dir], \
                stdout=logfile, stderr=logfile, cwd=os.path.join(lib_dir, 'mauve'))
        logfile.close()

    #removing tmp folder
    shutil.rmtree(tmpFolder)

    # find metrics for total report
    if os.path.isfile(output_dir + '/summaries.txt'):
        metrics = ['NumMisCalled', 'NumUnCalled', 'BasesMissed', 'ExtraBases']
        metrics_headers = ['Number of MisCalled', 'Number of UnCalled', 'Bases Missed', 'Extra Bases']        
        for metric_header in metrics_headers:
            report_dict['header'].append(metric_header)

        summaries = open(output_dir + '/summaries.txt', 'r')
        header = summaries.readline()
        metrics_positions = []
        cur_metric_id = 0
        for id, title in enumerate(header.split()):
            if metrics[cur_metric_id] in title:
                metrics_positions.append(id)
                cur_metric_id += 1
                if cur_metric_id == len(metrics):
                    break

        for line in summaries:
            fields = line.split()
            contig_name = fields[0]
            if contig_name.endswith('.fas'): # mauve renames ".fa"-files to ".fa.fas" ones
                contig_name = contig_name[:len(contig_name) - 4]
            for metric_pos in metrics_positions:
                report_dict[contig_name].append(fields[metric_pos])
        summaries.close()                

    print '  Done.'

    return report_dict
