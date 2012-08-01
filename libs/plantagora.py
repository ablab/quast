############################################################################
# Copyright (c) 2011-2012 Saint-Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################

import os
import subprocess
import re
import fastaparser
import platform
from qutils import id_to_str

def do(reference, filenames, cyclic, rc, output_dir, lib_dir, draw_plots):
    if not os.path.isdir(output_dir):
        os.mkdir(output_dir)

    ########################################################################
    assess_assembly_path1 = os.path.join(lib_dir, 'plantagora/assess_assembly1.pl')
    assess_assembly_path2 = os.path.join(lib_dir, 'plantagora/assess_assembly2.pl')
    mummer_path           = os.path.join(lib_dir, 'MUMmer3.23-linux')
    if platform.system() == 'Darwin':
        mummer_path       = os.path.join(lib_dir, 'MUMmer3.23-osx')

    ########################################################################
    report_dict = {'header' : []}
    for filename in filenames:
        report_dict[os.path.basename(filename)] = []

    # for running our MUMmer 
    myenv = os.environ.copy()
    myenv['PATH'] = mummer_path + ':' + myenv['PATH']
    # making if needed
    if not os.path.exists(os.path.join(mummer_path, 'nucmer')):
        print ("Making MUMmer...")
        subprocess.call(
            ['make', '-C', mummer_path],
            stdout=open(os.path.join(mummer_path, 'make.log'), 'w'), stderr=open(os.path.join(mummer_path, 'make.err'), 'w'))

    print 'Running plantagora tool (assess_assemply.pl)...'
    metrics = ['Misassemblies', 'Misassembled contigs', 'Misassembled contig bases',
               'Unaligned contigs', 'Unaligned contig bases', 'Ambiguous contigs', 'Ambiguous contig bases']
    report_dict['header'] += metrics

    for id, filename in enumerate(filenames):
        print ' ', id_to_str(id), os.path.basename(filename), '...'
        nucmerfilename = output_dir + '/nucmer_' + os.path.basename(filename)
        # remove old nucmer coords file
        if os.path.isfile(nucmerfilename + '.coords'):
            os.remove(nucmerfilename + '.coords')
        # run plantagora tool
        logfilename_out = output_dir + '/plantagora_' + os.path.basename(filename) + '.stdout'
        logfilename_err = output_dir + '/plantagora_' + os.path.basename(filename) + '.stderr'
        print '    Logging to files', logfilename_out, 'and', os.path.basename(logfilename_err), '...',

        cyclic_option = ''
        if cyclic :
            cyclic_option = '--cyclic'
        rc_option = ''
        if rc :
            rc_option = '--rc'

        subprocess.call(
            ['perl', assess_assembly_path1, reference, filename, nucmerfilename, '--verbose', cyclic_option, rc_option],
            stdout=open(logfilename_out, 'w'), stderr=open(logfilename_err, 'w'), env=myenv)

        import sympalign
        sympalign.do(1, nucmerfilename + '.coords', [nucmerfilename + '.coords.btab'])

        subprocess.call(
            ['perl', assess_assembly_path2, reference, filename, nucmerfilename, '--verbose', cyclic_option, rc_option],
            stdout=open(logfilename_out, 'a'), stderr=open(logfilename_err, 'a'), env=myenv)


        print 'done.'


        if draw_plots:
            # draw reference coverage plot
            print '    Drawing reference coverage plot...',
            plotfilename = output_dir + '/mummerplot_' + os.path.basename(filename)
            plot_logfilename_out = output_dir + '/mummerplot_' + os.path.basename(filename) + '.stdout'
            plot_logfilename_err = output_dir + '/mummerplot_' + os.path.basename(filename) + '.stderr'
            plot_logfile_out = open(plot_logfilename_out, 'w')
            plot_logfile_err = open(plot_logfilename_err, 'w')
            subprocess.call(
                ['mummerplot', '--coverage', '--postscript', '--prefix', plotfilename, nucmerfilename + '.delta'],
                stdout=plot_logfile_out, stderr=plot_logfile_err, env=myenv)
            plot_logfile_out.close()
            plot_logfile_err.close()
            print 'saved to', plotfilename + '.ps'

        # compute nucmer average % IDY
        if os.path.isfile(nucmerfilename + '.coords'):
            file = open(nucmerfilename + '.coords')
            sum = 0.0
            num = 0
            for line in file:
                arr = line.split('|')
                if len(arr) > 4:
                    x = arr[3].strip()
                    if x[0] != '[': # not [% IDY]
                        sum += float(x)
                        num += 1
            if num:
                avg = sum / num
            else:
                avg = 0
            file.close()
        else:
            print '  ERROR: nucmer coord file (' + nucmerfilename + ') not found, skipping...'
            avg = 'N/A'
        print '    Average %IDY = ', avg
        # report_dict[os.path.basename(filename)].append('%3.2f' % avg)
        # delete temporary files
        for ext in ['.delta', '.mgaps', '.ntref', '.gp']:
            if os.path.isfile(nucmerfilename + ext):
                os.remove(nucmerfilename + ext)
        if draw_plots:
            for ext in ['.gp', '.rplot', '.fplot']:
                if os.path.isfile(plotfilename + ext):
                    os.remove(plotfilename + ext)
        if os.path.isfile('nucmer.error'):
            os.remove('nucmer.error')
        if os.path.isfile(filename + '.clean'):
            os.remove(filename + '.clean')

        ## find metrics for total report:

        logfile_out = open(logfilename_out, 'r')
        cur_metric_id = 0
        for line in logfile_out:
            if line.lower().strip().startswith(metrics[cur_metric_id].lower()):
                report_dict[os.path.basename(filename)].append( line.split(':')[1].strip() )
                cur_metric_id += 1
                if cur_metric_id == len(metrics):
                    break
        logfile_out.close()
        report_dict[os.path.basename(filename)] += ['N/A'] * (len(report_dict['header']) - len(report_dict[os.path.basename(filename)]))

        ## outputting misassembled contigs in separate file

        logfile_out = open(logfilename_out, 'r')
        mis_contigs_ids = []
        # skipping prologue
        for line in logfile_out:
            if line.startswith("Analyzing contigs..."):
                break
        # main part of plantagora output
        cur_contig_id = ""
        for line in logfile_out:
            if line.startswith("	CONTIG:"):
                cur_contig_id = line.split("	CONTIG:")[1].strip()
            if (line.find("Extensive misassembly") != -1) and (cur_contig_id != ""):
                mis_contigs_ids.append(cur_contig_id.split()[0])
                cur_contig_id = ""
            if line.startswith("Analyzing coverage..."):
                break
        logfile_out.close()

        # outputting misassembled contigs
        input_contigs = fastaparser.read_fasta(filename)
        mis_contigs = open(output_dir + '/' + os.path.basename(filename) + '.mis_contigs', "w")

        for (name, seq) in input_contigs:
            corr_name = re.sub(r'\W', '', re.sub(r'\s', '_', name))
            if mis_contigs_ids.count(corr_name) != 0:
                mis_contigs.write(name + '\n')
                for i in xrange(0, len(seq), 60):
                    mis_contigs.write(seq[i:i+60] + '\n')
        mis_contigs.close()

    print '  Done'

    return report_dict
