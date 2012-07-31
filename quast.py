#!/usr/bin/python

############################################################################
# Copyright (c) 2011-2012 Saint-Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################


import sys
import os
import shutil
import re
import getopt
import datetime
import subprocess
import glob

sys.path.append(os.path.join(os.path.abspath(sys.path[0]), 'libs'))
#sys.path.append(os.path.join(os.path.abspath(sys.path[0]), '../spades_pipeline'))

import qconfig
import support
import json_saver

RELEASE_MODE=False

def usage():
    print >>sys.stderr, 'A tool for estimating assembly quality with various metrics and tools.'
    print >>sys.stderr, 'Usage:', sys.argv[0], '[options] contig files'
    print >>sys.stderr, ""

    if RELEASE_MODE:
        print >>sys.stderr, "Options:"
        print >>sys.stderr, "-o           <dirname>       directory to store all result files [default: results_<datetime>]"
        print >>sys.stderr, "-R           <filename>      file with a reference genome"
        print >>sys.stderr, "-G/--genes   <filename>      file with genes for a given species"
        print >>sys.stderr, "-O/--operons <filename>      file with operons for a given species"
        print >>sys.stderr, "--min-contig <int>           lower threshold for contig length [default: %s]" % (qconfig.min_contig)
        print >>sys.stderr, ""
        print >>sys.stderr, "Advanced options:"
        print >>sys.stderr, "--contig-thresholds <int,int,...>   comma-separated list of contig length thresholds [default is %s]" % (qconfig.contig_thresholds)
        print >>sys.stderr, "--orf               <int,int,...>   comma-separated list of threshold lengths of ORFs to search for [default is %s]" % (qconfig.orf_lengths)
        print >>sys.stderr, '--not-circular                      this flag should be set if the genome is not circular (e.g., eukaryote)'
        print >>sys.stderr, ""
        print >>sys.stderr, "-h/--help           print this usage message"
    else:
        print >>sys.stderr, 'Options with arguments'
        print >>sys.stderr, "-o  --output-dir             directory to store all result files [default: results_<datetime>]"
        print >>sys.stderr, "-R  --reference              file with a reference genome"
        print >>sys.stderr, "-G  --genes                  file with genes for a given species"
        print >>sys.stderr, "-O  --operons                file with operons for a given species"
        print >>sys.stderr, "-M  --min-contig             lower threshold for contig length [default: %s]" % (qconfig.min_contig)
        print >>sys.stderr, "-t  --contig-thresholds      comma-separated list of contig length thresholds [default is %s]" % (qconfig.contig_thresholds)
        print >>sys.stderr, "-f  --orf                    comma-separated list of threshold lengths of ORFs to search for [default is %s]" % (qconfig.orf_lengths)
        print >>sys.stderr, "-e  --genemark-thresholds    comma-separated list of threshold lengths of genes to search with GeneMark (default is %s)" % (qconfig.genes_lengths)
        print >>sys.stderr, ""
        print >>sys.stderr, 'Options without arguments'
        print >>sys.stderr, '-m  --mauve                  use Mauve'
        print >>sys.stderr, '-g  --gage                   use Gage only'
        print >>sys.stderr, '-n  --not-circular           genome is not circular (e.g., eukaryote)'
        print >>sys.stderr, "-d  --disable-rc             reverse complementary contig should NOT be counted as misassembly"
        print >>sys.stderr, "-k  --genemark               use GeneMark"
        print >>sys.stderr, "-x  --extra-report           generate an extra report (extra_report.txt)"
        print >>sys.stderr, "-j  --save-json              save the output also in the JSON format"
        print >>sys.stderr, "-J  --save-json-to <path>    save the JSON-output to a particular path"
        print >>sys.stderr, "-p  --plain-report-no-plots  plain text report only, don't draw plots (to make quast faster)"
        print >>sys.stderr, ""
        print >>sys.stderr, "-h  --help                   print this usage message"

def check_file(f, message=''):
    if not os.path.isfile(f):
        print >>sys.stderr, "Error. File not found (%s): %s" % (message, f)
        sys.exit(2)
    return f

def main(args, lib_dir=os.path.join(os.path.abspath(sys.path[0]), 'libs')):

    ######################
    ### ARGS
    ######################    

    try:
        options, contigs = getopt.gnu_getopt(args, qconfig.short_options, qconfig.long_options)
    except getopt.GetoptError, err:
        print >>sys.stderr, err
        print >>sys.stderr
        usage()
        sys.exit(1)

    if not contigs:
        usage()
        sys.exit(1)

    json_output_dir = None

    for opt, arg in options:
        # Yes, this is doubling the code. Python's getopt is non well-thought!!
        if opt in ('-o', "--output-dir"):
            qconfig.output_dir = arg
            qconfig.make_latest_symlink = False
        elif opt in ('-G', "--genes"):
            qconfig.genes = check_file(arg, 'genes')
        elif opt in ('-O', "--operons"):
            qconfig.operons = check_file(arg, 'operons')
        elif opt in ('-R', "--reference"):
            qconfig.reference = check_file(arg, 'reference')
        elif opt in ('-t', "--contig-thresholds"):
            qconfig.contig_thresholds = arg
        elif opt in ('-M', "--min-contig"):
            qconfig.min_contig = int(arg)
        elif opt in ('-f', "--orf"):
            qconfig.orf_lengths = arg
        elif opt in ('-e', "--genemark-thresholds"):
            qconfig.genes_lengths = arg
        elif opt in ('-j', '--save-json'):
            qconfig.save_json = True
        elif opt in ('-J', '--save-json-to'):
            qconfig.save_json = True
            json_output_dir = arg
        elif opt in ('-m', "--mauve"):
            qconfig.with_mauve = True
        elif opt in ('-g', "--gage"):
            qconfig.with_gage = True
        elif opt in ('-n', "--not-circular"):
            qconfig.cyclic = False
        elif opt in ('-d', "--disable-rc"):
            qconfig.rc = True
        elif opt in ('-k', "--genemark"):
            qconfig.with_genemark = True
        elif opt in ('-x', "--extra-report"):
            qconfig.extra_report = True
        elif opt in ('-p', '--plain-report-no-plots'):
            qconfig.draw_plots = False
        elif opt in ('-h', "--help"):
            usage()
            sys.exit(0)
        else:
            raise ValueError

    for c in contigs:
        check_file(c, 'contigs')

    qconfig.contig_thresholds = map(int, qconfig.contig_thresholds.split(","))
    qconfig.orf_lengths = map(int, qconfig.orf_lengths.split(","))
    qconfig.output_dir = os.path.abspath(qconfig.output_dir)
    output_dir = qconfig.output_dir

    ########################################################################
    ### CONFIG & CHECKS
    ########################################################################

    if os.path.isdir(output_dir):  # in case of starting two instances of QUAST in the same second
        i = 2
        base_dir_name = output_dir
        while os.path.isdir(output_dir):
            output_dir = base_dir_name + '__' + str(i)
            i += 1
        print "\nWarning! Output directory already exists! Results will be saved in " + output_dir + "\n"

    if not os.path.isdir(output_dir):
        os.makedirs(output_dir)

    if qconfig.make_latest_symlink:
        latest_symlink = 'latest'
        if os.path.islink(latest_symlink):
            os.remove(latest_symlink)
        os.symlink(output_dir, latest_symlink)

    # Json directory
    if qconfig.save_json:
        if json_output_dir:
            if not os.path.isdir(json_output_dir):
                os.makedirs(json_output_dir)
        else:
            json_output_dir = os.path.join(output_dir, qconfig.default_json_dir_name)
            if not os.path.isdir(json_output_dir):
                os.makedirs(json_output_dir)

    # Where log will be saved
    logfile = os.path.join(output_dir, qconfig.logfile)

    # Where corrected contigs will be saved
    corrected_dir = os.path.join(output_dir, qconfig.corrected_dir)

    # Where total report will be saved
    total_report = os.path.join(output_dir, "transposed_" + qconfig.report_basename)
    total_report_tr = os.path.join(output_dir, qconfig.report_basename)

    # Where gage report will be saved (option --gage)
    gage_report = os.path.join(output_dir, qconfig.gage_report_basename)

    # Where all pdfs will be saved
    all_pdf_filename = os.path.join(output_dir, qconfig.plots_filename)
    all_pdf = None

    # Where Single Cell paper-like table will be saved
    extra_report_filename = os.path.join(output_dir, qconfig.extra_report_filename)

    ########################################################################

    def extend_report_dict(report_dict, new_data_dict):
        for contig, value_list in new_data_dict.iteritems():
            report_dict[contig].extend(value_list)
        return report_dict

    ########################################################################

    # duplicating output to log file
    import support
    if os.path.isfile(logfile):
        os.remove(logfile)

    tee = support.Tee(logfile, 'w', console=True) # not pure, changes sys.stdout and sys.stderr

    ########################################################################

    # dict with the main metrics (for total report)
    report_dict = {'header': ['id', 'Assembly']}
    for threshold in qconfig.contig_thresholds:
        report_dict['header'].append('# contigs >= ' + str(threshold))
    for threshold in qconfig.contig_thresholds:
        report_dict['header'].append('Total length (>= ' + str(threshold) + ')')

    print 'Correcting contig files...'
    if os.path.isdir(corrected_dir):
        shutil.rmtree(corrected_dir)
    os.mkdir(corrected_dir)
    import fastaparser

    # if reference in .gz format we should unzip it
    if qconfig.reference:
        ref_basename, ref_extension = os.path.splitext(qconfig.reference)
        if ref_extension == ".gz":
            unziped_reference_name = os.path.join(corrected_dir, os.path.basename(ref_basename))
            unziped_reference = open(unziped_reference_name, 'w')
            subprocess.call(['gunzip', qconfig.reference, '-c'], stdout=unziped_reference)
            unziped_reference.close()
            qconfig.reference = unziped_reference_name

    # we should remove input files with no contigs (e.g. if ll contigs are less than "min_contig" value)
    contigs_to_remove = []
    ## removing from contigs' names special characters because:
    ## 1) Mauve fails on some strings with "...", "+", "-", etc
    ## 2) nummer fails on names like "contig 1_bla_bla", "contig 2_bla_bla" (it interprets as a contig's name only the first word of caption and gets ambiguous contigs names)
    for id, filename in enumerate(contigs):
        outfilename = os.path.splitext( os.path.join(corrected_dir, os.path.basename(filename).replace(' ','_')) )[0]
        if os.path.isfile(outfilename):  # in case of files with the same names
            i = 1
            basename = os.path.splitext(filename)[0]
            while os.path.isfile(outfilename):
                i += 1
                outfilename = os.path.join(corrected_dir, os.path.basename(basename + '__' + str(i)))

        ## filling column "Assembly" with names of assemblies
        report_dict[os.path.basename(outfilename)] = [id, os.path.basename(outfilename)]
        ## filling columns "Number of contigs >=110 bp", ">200 bp", ">500 bp"
        lengths = fastaparser.get_lengths_from_fastafile(filename)
        for threshold in qconfig.contig_thresholds:
            cur_lengths = [l for l in lengths if l >= threshold]
            report_dict[os.path.basename(outfilename)].append(len(cur_lengths))
        for threshold in qconfig.contig_thresholds:
            cur_lengths = [l for l in lengths if l >= threshold]
            report_dict[os.path.basename(outfilename)].append(sum(cur_lengths))

        fasta_entries = fastaparser.read_fasta(filename) # in tuples: (name, seq)
        modified_fasta_entries = []
        to_remove = True
        for entry in fasta_entries:
            if (len(entry[1]) >= qconfig.min_contig) or (qconfig.with_gage):
                to_remove = False
                corr_name = '>' + re.sub(r'\W', '', re.sub(r'\s', '_', entry[0]))
                # mauve and gage can't work with alternatives
                dic = {'M': 'A', 'K': 'G', 'R': 'A', 'Y': 'C', 'W': 'A', 'S': 'C', 'V': 'A', 'B': 'C', 'H': 'A', 'D': 'A'}
                pat = "(%s)" % "|".join( map(re.escape, dic.keys()) )
                corr_seq = re.sub(pat, lambda m:dic[m.group()], entry[1])
                modified_fasta_entries.append((corr_name, corr_seq))

        fastaparser.write_fasta_to_file(outfilename, modified_fasta_entries)

        print '  ' + filename + ' ==> ' + os.path.basename(outfilename)
        contigs[id] = os.path.join(os.path.abspath(sys.path[0]), outfilename)
        if to_remove:
            contigs_to_remove.append(contigs[id])
    print '  Done.'
    ########################################################################
    if contigs_to_remove:
        print "\nWarning! These files will not processed because they don't have contigs >= " + str(qconfig.min_contig) + " bp!\n"
        for contig_to_remove in contigs_to_remove:
            print "  ", contig_to_remove
            contigs.remove(contig_to_remove)
            del report_dict[os.path.basename(contig_to_remove)]
        print ""
        ########################################################################

    if not contigs:
        usage()
        sys.exit(1)

    if qconfig.with_gage:
        ########################################################################
        ### GAGE
        ########################################################################        
        if not qconfig.reference:   
            print "\nError! GAGE can't be run without reference!\n"
            sys.exit(1)

        import gage
        gage.do(qconfig.reference, contigs, output_dir + '/gage', gage_report, qconfig.min_contig, lib_dir)
    else:
        if qconfig.draw_plots:
            import plotter
            try:
                from matplotlib.backends.backend_pdf import PdfPages
                all_pdf = PdfPages(all_pdf_filename)
            except:
                all_pdf = None

        ########################################################################	
        ### Stats and plots
        ########################################################################	
        import basic_stats
        cur_results_dict = basic_stats.do(qconfig.reference, contigs, output_dir + '/basic_stats', all_pdf, qconfig.draw_plots, json_output_dir, output_dir)
        report_dict = extend_report_dict(report_dict, cur_results_dict)

        if qconfig.reference:
            ########################################################################
            ### PLANTAKOLYA
            ########################################################################
            import plantakolya
            cur_results_dict = plantakolya.do(qconfig.reference, contigs, qconfig.cyclic, qconfig.rc, output_dir + '/plantakolya', lib_dir, qconfig.draw_plots)
            report_dict = extend_report_dict(report_dict, cur_results_dict)

            ########################################################################
            ### PLANTAGORA
            ########################################################################
            import plantagora
            cur_results_dict = plantagora.do(qconfig.reference, contigs, qconfig.cyclic, qconfig.rc, output_dir + '/plantagora', lib_dir, qconfig.draw_plots)
            report_dict = extend_report_dict(report_dict, cur_results_dict)

            ########################################################################
            ### SympAlign segments
            ########################################################################
            import sympalign
            sympalign.do(2, output_dir + '/plantakolya/sympalign.segments', [output_dir + '/plantakolya'])

            ########################################################################
            ### NA and NGA ("aligned N and NG")
            ########################################################################
            import aligned_stats
            cur_results_dict = aligned_stats.do(qconfig.reference, contigs, output_dir + '/plantakolya', output_dir + '/aligned_stats', all_pdf, qconfig.draw_plots, json_output_dir, output_dir)
            report_dict = extend_report_dict(report_dict, cur_results_dict)

            ########################################################################
            ### GENOME_ANALYZER
            ########################################################################
            import genome_analyzer
            cur_results_dict = genome_analyzer.do(qconfig.reference, contigs, output_dir + '/genome_analyzer', output_dir + '/plantakolya', qconfig.genes, qconfig.operons, all_pdf, qconfig.draw_plots, json_output_dir, output_dir)
            report_dict = extend_report_dict(report_dict, cur_results_dict)

            ########################################################################
            ### MAUVE
            ########################################################################
            if qconfig.with_mauve:
                import mauve
                cur_results_dict = mauve.do(qconfig.reference, contigs, output_dir + '/plantakolya', output_dir + '/mauve', lib_dir)
                report_dict = extend_report_dict(report_dict, cur_results_dict)


        if not qconfig.genes:
            ########################################################################
            ### ORFs
            ########################################################################
            import orfs
            for orf_length in qconfig.orf_lengths:
                cur_results_dict = orfs.do(contigs, orf_length)
                report_dict = extend_report_dict(report_dict, cur_results_dict)

            if qconfig.with_genemark:
                ########################################################################
                ### GeneMark
                ########################################################################    
                import genemark
                cur_results_dict = genemark.do(contigs, qconfig.genes_lengths, output_dir + '/genemark', lib_dir)
                report_dict = extend_report_dict(report_dict, cur_results_dict)

        ########################################################################
        ### TOTAL REPORT
        ########################################################################
        if json_output_dir:
            json_saver.save_total_report(json_output_dir, report_dict)

        import report_maker
        report_maker.do(report_dict, total_report, total_report_tr, qconfig.min_contig, output_dir)
        from libs.html_saver import html_saver
        html_saver.save_total_report(output_dir, report_dict)
        if qconfig.draw_plots and all_pdf:
            print '  All pdf files are merged to', all_pdf_filename
            all_pdf.close()

        ## and Single Cell paper table
        if qconfig.reference and qconfig.extra_report:
            import extra_report_maker
            extra_report_maker.do(total_report, output_dir + '/genome_analyzer/genome_info.txt', extra_report_filename, qconfig.min_contig, json_output_dir)

            ########################################################################

    print 'Done.'

    ## removing correcting input contig files
    shutil.rmtree(corrected_dir)

    tee.free() # free sys.stdout and sys.stderr from logfile

if __name__ == '__main__':
    main(sys.argv[1:])
