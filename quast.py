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
import subprocess

__location__ = os.path.realpath(os.path.join(os.getcwd(), os.path.dirname(__file__)))

#sys.path.append(os.path.join(os.path.abspath(sys.path[0]), 'libs'))
#sys.path.append(os.path.join(os.path.abspath(sys.path[0]), '../spades_pipeline'))

from libs import qconfig
from libs import json_saver

RELEASE_MODE=True

def usage():
    print >> sys.stderr, 'QUAST: a quality assessment tool.'
    print >> sys.stderr, 'Usage: python', sys.argv[0], '[options] contig files'
    print >> sys.stderr, ""

    if RELEASE_MODE:
        print >> sys.stderr, "Options:"
        print >> sys.stderr, "-o           <dirname>       directory to store all result files [default: quast_results/results_<datetime>]"
        print >> sys.stderr, "-R           <filename>      reference genome file"
        print >> sys.stderr, "-G/--genes   <filename>      annotated genes file"
        print >> sys.stderr, "-O/--operons <filename>      annotated operons file"
        print >> sys.stderr, "--min-contig <int>           lower threshold for contig length [default: %s]" % qconfig.min_contig
        print >> sys.stderr, ""
        print >> sys.stderr, "Advanced options:"
        print >> sys.stderr, "--gage                              this flag starts QUAST in \"GAGE mode\""
        print >> sys.stderr, "--contig-thresholds   <int,int,..>  comma-separated list of contig length thresholds [default is %s]" % qconfig.contig_thresholds
        print >> sys.stderr, "--genemark-thresholds <int,int,..>  comma-separated list of threshold lengths of genes to search with GeneMark [default is %s]" % qconfig.genes_lengths
        print >> sys.stderr, '--not-circular                      this flag should be set if the genome is not circular (e.g., it is an eukaryote)'
        print >> sys.stderr, ""
        print >> sys.stderr, "-h/--help           print this usage message"
    else:
        print >> sys.stderr, 'Options with arguments'
        print >> sys.stderr, "-o  --output-dir             directory to store all result files [default: quast_results/results_<datetime>]"
        print >> sys.stderr, "-R           <filename>      reference genome file"
        print >> sys.stderr, "-G/--genes   <filename>      annotated genes file"
        print >> sys.stderr, "-O/--operons <filename>      annotated operons file"
        print >> sys.stderr, "-M  --min-contig             lower threshold for contig length [default is %s]" % qconfig.min_contig
        print >> sys.stderr, "-t  --contig-thresholds      comma-separated list of contig length thresholds [default is %s]" % qconfig.contig_thresholds
        print >> sys.stderr, "-e  --genemark-thresholds    comma-separated list of threshold lengths of genes to search with GeneMark [default is %s]" % qconfig.genes_lengths
        print >> sys.stderr, ""
        print >> sys.stderr, 'Options without arguments'
        print >> sys.stderr, '-g  --gage                   use Gage (results are in gage_report.txt)'
        print >> sys.stderr, '-n  --not-circular           genome is not circular (e.g., it is an eukaryote)'
        print >> sys.stderr, "-d  --disable-rc             reverse complementary contig should NOT be counted as misassembly"
        print >> sys.stderr, "-j  --save-json              save the output also in the JSON format"
        print >> sys.stderr, "-J  --save-json-to <path>    save the JSON-output to a particular path"
        print >> sys.stderr, "-p  --plain-report-no-plots  plain text report only, don't draw plots (to make quast faster)"
        print >> sys.stderr, ""
        print >> sys.stderr, "-h  --help                   print this usage message"

def check_file(f, message=''):
    if not os.path.isfile(f):
        print >> sys.stderr, "\nERROR! File not found (%s): %s\n" % (message, f)
        sys.exit(2)
    return f


def main(args, lib_dir=os.path.join(__location__, 'libs')): # os.path.join(os.path.abspath(sys.path[0]), 'libs')):
    ######################
    ### ARGS
    ######################    
    reload(qconfig)

    try:
        options, contigs = getopt.gnu_getopt(args, qconfig.short_options, qconfig.long_options)
    except getopt.GetoptError, err:
        print >> sys.stderr, err
        print >> sys.stderr
        usage()
        sys.exit(1)

    if not contigs:
        usage()
        sys.exit(1)

    json_outputpath = None
    output_dirpath = os.path.join(os.path.abspath(qconfig.default_results_root_dirname), qconfig.output_dirname)

    for opt, arg in options:
        # Yes, this is doubling the code. Python's getopt is non well-thought!!
        if opt in ('-o', "--output-dir"):
            output_dirpath = os.path.abspath(arg)
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
        elif opt in ('-e', "--genemark-thresholds"):
            qconfig.genes_lengths = arg
        elif opt in ('-j', '--save-json'):
            qconfig.save_json = True
        elif opt in ('-J', '--save-json-to'):
            qconfig.save_json = True
            json_outputpath = arg
        elif opt in ('-g', "--gage"):
            qconfig.with_gage = True
        elif opt in ('-n', "--not-circular"):
            qconfig.cyclic = False
        elif opt in ('-d', "--disable-rc"):
            qconfig.rc = True
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
    qconfig.genes_lengths =  map(int, qconfig.genes_lengths.split(","))

    ########################################################################
    ### CONFIG & CHECKS
    ########################################################################

    if os.path.isdir(output_dirpath):  # in case of starting two instances of QUAST in the same second
        i = 2
        base_dirpath = output_dirpath
        while os.path.isdir(output_dirpath):
            output_dirpath = str(base_dirpath) + '__' + str(i)
            i += 1
        print "\nWARNING! Output directory already exists! Results will be saved in " + output_dirpath + "\n"

    if not os.path.isdir(output_dirpath):
        os.makedirs(output_dirpath)

    if qconfig.make_latest_symlink:
        prev_dirpath = os.getcwd()
        os.chdir(qconfig.default_results_root_dirname)

        latest_symlink = 'latest'
        if os.path.islink(latest_symlink):
            os.remove(latest_symlink)
        os.symlink(output_dirpath, latest_symlink)

        os.chdir(prev_dirpath)


    # Json directory
    if qconfig.save_json:
        if json_outputpath:
            if not os.path.isdir(json_outputpath):
                os.makedirs(json_outputpath)
        else:
            json_outputpath = os.path.join(output_dirpath, qconfig.default_json_dirname)
            if not os.path.isdir(json_outputpath):
                os.makedirs(json_outputpath)


    # Where log will be saved
    logfile = os.path.join(output_dirpath, qconfig.logfile)

    # Where corrected contigs will be saved
    corrected_dir = os.path.join(output_dirpath, qconfig.corrected_dir)

    # Where all pdfs will be saved
    all_pdf_filename = os.path.join(output_dirpath, qconfig.plots_filename)
    all_pdf = None

    ########################################################################

    # duplicating output to log file
    from libs import support
    if os.path.isfile(logfile):
        os.remove(logfile)

    tee = support.Tee(logfile, 'w', console=True) # not pure, changes sys.stdout and sys.stderr

    ########################################################################

    from libs import reporting
    reload(reporting)

    print 'Correcting contig files...'
    if os.path.isdir(corrected_dir):
        shutil.rmtree(corrected_dir)
    os.mkdir(corrected_dir)
    from libs import fastaparser

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
    ## 1) Some embedded tools can fail on some strings with "...", "+", "-", etc
    ## 2) Nucmer fails on names like "contig 1_bla_bla", "contig 2_bla_bla" (it interprets as a contig's name only the first word of caption and gets ambiguous contigs names)
    newcontigs = []
    for id, filename in enumerate(contigs):
        outfilename = os.path.splitext( os.path.join(corrected_dir, os.path.basename(filename).replace(' ','_')) )[0]
        if os.path.isfile(outfilename):  # in case of files with the same names
            i = 1
            basename = os.path.splitext(filename)[0]
            while os.path.isfile(outfilename):
                i += 1
                outfilename = os.path.join(corrected_dir, os.path.basename(basename + '__' + str(i)))

        lengths = fastaparser.get_lengths_from_fastafile(filename)
        if not sum(1 for l in lengths if l >= qconfig.min_contig):
            print "\n  WARNING! %s will not be processed because it doesn't have contigs >= %d bp.\n" % (os.path.basename(filename), qconfig.min_contig)
            continue

        ## filling column "Assembly" with names of assemblies
        report = reporting.get(outfilename)
        ## filling columns "Number of contigs >=110 bp", ">=200 bp", ">=500 bp"
        report.add_field(reporting.Fields.CONTIGS,   [sum(1 for l in lengths if l >= threshold) for threshold in qconfig.contig_thresholds])
        report.add_field(reporting.Fields.TOTALLENS, [sum(l for l in lengths if l >= threshold) for threshold in qconfig.contig_thresholds])

        modified_fasta_entries = []
        for name, seq in fastaparser.read_fasta(filename): # in tuples: (name, seq)
            if len(seq) >= qconfig.min_contig:
                corr_name = re.sub(r'\W', '', re.sub(r'\s', '_', name))
                # mauve and gage can't work with alternatives
                dic = {'M': 'A', 'K': 'G', 'R': 'A', 'Y': 'C', 'W': 'A', 'S': 'C', 'V': 'A', 'B': 'C', 'H': 'A', 'D': 'A'}
                pat = "(%s)" % "|".join( map(re.escape, dic.keys()) )
                corr_seq = re.sub(pat, lambda m:dic[m.group()], seq)
                modified_fasta_entries.append((corr_name, corr_seq))
        fastaparser.write_fasta_to_file(outfilename, modified_fasta_entries)

        print '  %s ==> %s' % (filename, os.path.basename(outfilename))
        newcontigs.append(os.path.join(__location__, outfilename))

    print '  Done.'
    contigs = newcontigs

    if not contigs:
        usage()
        sys.exit(1)

    if qconfig.with_gage:
        ########################################################################
        ### GAGE
        ########################################################################        
        if not qconfig.reference:
            print >> sys.stderr, "\nERROR! GAGE can't be run without reference!\n"
        else:
            from libs import gage
            gage.do(qconfig.reference, contigs, output_dirpath, qconfig.min_contig, lib_dir)

    if qconfig.draw_plots:
        from libs import plotter  # Do not remove this line! It would lead to a warning in matplotlib.
        try:
            from matplotlib.backends.backend_pdf import PdfPages
            all_pdf = PdfPages(all_pdf_filename)
        except:
            all_pdf = None

    ########################################################################
    ### Stats and plots
    ########################################################################
    from libs import basic_stats
    basic_stats.do(qconfig.reference, contigs, output_dirpath + '/basic_stats', all_pdf, qconfig.draw_plots, json_outputpath, output_dirpath)

    if qconfig.reference:
        ########################################################################
        ### PLANTAKOLYA
        ########################################################################
        from libs import plantakolya
        plantakolya.do(qconfig.reference, contigs, qconfig.cyclic, qconfig.rc, output_dirpath + '/plantakolya', lib_dir, qconfig.draw_plots)

        ########################################################################
        ### NA and NGA ("aligned N and NG")
        ########################################################################
        from libs import aligned_stats
        aligned_stats.do(qconfig.reference, contigs, output_dirpath + '/plantakolya', output_dirpath + '/aligned_stats', all_pdf, qconfig.draw_plots, json_outputpath, output_dirpath)

        ########################################################################
        ### GENOME_ANALYZER
        ########################################################################
        from libs import genome_analyzer
        genome_analyzer.do(qconfig.reference, contigs, output_dirpath + '/genome_analyzer', output_dirpath + '/plantakolya', qconfig.genes, qconfig.operons, all_pdf, qconfig.draw_plots, json_outputpath, output_dirpath)

    if not qconfig.genes:
        ########################################################################
        ### GeneMark
        ########################################################################
        from libs import genemark
        genemark.do(contigs, qconfig.genes_lengths, output_dirpath + '/genemark', lib_dir)
    else:
        # TODO: make it nicer (not output predicted genes if annotations are provided
        for id, filename in enumerate(contigs):
            report = reporting.get(filename)
            report.add_field(reporting.Fields.GENEMARKUNIQUE, "")
            report.add_field(reporting.Fields.GENEMARK, [""] * len(qconfig.genes_lengths))

    ########################################################################
    ### TOTAL REPORT
    ########################################################################
    reporting.save(output_dirpath, qconfig.min_contig)

    if json_outputpath:
        json_saver.save_total_report(json_outputpath, qconfig.min_contig)

    from libs.html_saver import html_saver
    html_saver.save_total_report(output_dirpath, qconfig.min_contig)

    if qconfig.draw_plots and all_pdf:
        print '  All pdf files are merged to', all_pdf_filename
        all_pdf.close()

    ########################################################################

    print 'Done.'

    ## removing correcting input contig files
    shutil.rmtree(corrected_dir)

    tee.free() # free sys.stdout and sys.stderr from logfile

    return 0

if __name__ == '__main__':
    main(sys.argv[1:])
