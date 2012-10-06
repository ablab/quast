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
from libs.html_saver import json_saver

RELEASE_MODE=False

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
        print >> sys.stderr, "--contig-thresholds   <int,int,..>  comma-separated list of contig length thresholds [default: %s]" % qconfig.contig_thresholds
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
        print >> sys.stderr, "-M  --min-contig             lower threshold for contig length [default: %s]" % qconfig.min_contig
        print >> sys.stderr, "-t  --contig-thresholds      comma-separated list of contig length thresholds [default: %s]" % qconfig.contig_thresholds
        print >> sys.stderr, "-e  --genemark-thresholds    comma-separated list of threshold lengths of genes to search with GeneMark [default: %s]" % qconfig.genes_lengths
        print >> sys.stderr, ""
        print >> sys.stderr, 'Options without arguments'
        print >> sys.stderr, '-g  --gage                   use Gage (results are in gage_report.txt)'
        print >> sys.stderr, '-n  --not-circular           genome is not circular (e.g., it is an eukaryote)'
        print >> sys.stderr, "-j  --save-json              save the output also in the JSON format"
        print >> sys.stderr, "-J  --save-json-to <path>    save the JSON-output to a particular path"
        print >> sys.stderr, "-p  --plain-report-no-plots  plain text report only, don't draw plots (to make quast faster)"
        print >> sys.stderr, ""
        print >> sys.stderr, "-d  --debug                  run in debug mode"
        print >> sys.stderr, "-h  --help                   print this usage message"

def check_file_existence(fpath, message=''):
    if not os.path.isfile(fpath):
        print >> sys.stderr, "\nERROR! File not found (%s): %s\n" % (message, fpath)
        sys.exit(2)
    return fpath


def corrected_fname_for_nucmer(fpath):
    dirpath = os.path.dirname(fpath)
    fname = os.path.basename(fpath)

    corr_fname = fname
    corr_fname = re.sub('[^\w\._-]', '_', corr_fname).strip()

    if corr_fname != fname:
        if os.path.isfile(os.path.join(dirpath, corr_fname)):
            i = 1
            base_corr_fname = corr_fname
            while os.path.isfile(os.path.join(dirpath, corr_fname)):
                str_i = ''
                if i > 1:
                    str_i = str(i)

                corr_fname = 'copy_' +  str_i + '_' + str(base_corr_fname)
                i += 1

    return os.path.join(dirpath, corr_fname)


def check_dir_name(fpath):
    dirname = os.path.dirname(fpath)

    if ' ' in dirname:
        print 'Error: QUAST can\'t use data with spaces in path. Please, replace' + fpath + '.'
        return False
    else:
        return True


def main(args, lib_dir=os.path.join(__location__, 'libs')): # os.path.join(os.path.abspath(sys.path[0]), 'libs')):
    if lib_dir == os.path.join(__location__, 'libs'):
        if ' ' in __location__:
            print >> sys.stderr, 'Error: we does not support spaces in paths. '
            print >> sys.stderr, 'You are trying to run it from', __location__
            print >> sys.stderr, 'Please, put QUAST in a different folder, then run it again.'
            return 1
    else:
        if ' ' in lib_dir:
            print >> sys.stderr, 'Error: we does not support spaces in paths. '
            print >> sys.stderr, 'You are trying to use libs from', lib_dir
            print >> sys.stderr, 'Please, put libs in a different folder, then run it again.'
            return 1


    ######################
    ### ARGS
    ######################
    reload(qconfig)

    try:
        options, contigs_fpaths = getopt.gnu_getopt(args, qconfig.short_options, qconfig.long_options)
    except getopt.GetoptError, err:
        print >> sys.stderr, err
        print >> sys.stderr
        usage()
        sys.exit(1)

    if not contigs_fpaths:
        usage()
        sys.exit(1)

    json_outputpath = None
    output_dirpath = os.path.join(os.path.abspath(qconfig.default_results_root_dirname), qconfig.output_dirname)

    for opt, arg in options:
        # Yes, this is a code duplicating. Python's getopt is non well-thought!!
        if opt in ('-o', "--output-dir"):
            output_dirpath = os.path.abspath(arg)
            qconfig.make_latest_symlink = False

        elif opt in ('-G', "--genes"):
            qconfig.genes = check_file_existence(arg, 'genes')

        elif opt in ('-O', "--operons"):
            qconfig.operons = check_file_existence(arg, 'operons')

        elif opt in ('-R', "--reference"):
            qconfig.reference = check_file_existence(arg, 'reference')

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
            qconfig.make_latest_symlink = False
            json_outputpath = arg

        elif opt in ('-g', "--gage"):
            qconfig.with_gage = True

        elif opt in ('-n', "--not-circular"):
            qconfig.cyclic = False

        elif opt in ('-p', '--plain-report-no-plots'):
            qconfig.draw_plots = False

        elif opt in ('-d', "--debug"):
            qconfig.debug = True

        elif opt in ('-h', "--help"):
            usage()
            sys.exit(0)

        else:
            raise ValueError

    for c_fpath in contigs_fpaths:
        check_file_existence(c_fpath, 'contigs')

#    old_contigs_fpaths = contigs_fpaths[:]
#    contigs_fpaths = []
#    for old_c_fpath in old_contigs_fpaths:
#        contigs_fpaths.append(rename_file_for_nucmer(old_c_fpath))

#    # For renaming back: contigs_fpaths are to be changed further.
#    new_contigs_fpaths = contigs_fpaths[:]


    qconfig.contig_thresholds = map(int, qconfig.contig_thresholds.split(","))
    qconfig.genes_lengths =  map(int, qconfig.genes_lengths.split(","))

    ########################################################################
    ### CONFIG & CHECKS
    ########################################################################

    # in case of starting two instances of QUAST in the same second
    if os.path.isdir(output_dirpath):
        # if user starts QUAST with -o <existing dir> then qconfig.make_latest_symlink will be False
        if qconfig.make_latest_symlink:
            i = 2
            base_dirpath = output_dirpath
            while os.path.isdir(output_dirpath):
                output_dirpath = str(base_dirpath) + '__' + str(i)
                i += 1
        else:
            print "\nWarning! Output directory already exists! Existing Nucmer aligns will be used!\n"

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
    corrected_dirpath = os.path.join(output_dirpath, qconfig.corrected_dirname)

    # Where all pdfs will be saved
    all_pdf_filename = os.path.join(output_dirpath, qconfig.plots_filename)
    all_pdf = None

    ########################################################################

    # duplicating output to a log file
    from libs import support
    if os.path.isfile(logfile):
        os.remove(logfile)

    tee = support.Tee(logfile, 'w', console=True) # not pure, changes sys.stdout and sys.stderr

    ########################################################################

    from libs import reporting
    reload(reporting)
    reporting.min_contig = qconfig.min_contig

    print 'Correcting contig files...'
    if os.path.isdir(corrected_dirpath):
        shutil.rmtree(corrected_dirpath)
    os.mkdir(corrected_dirpath)

    from libs import fastaparser
    # if reference in .gz format we should unzip it
    if qconfig.reference:
        ref_basename, ref_extension = os.path.splitext(qconfig.reference)
        if ref_extension == ".gz":
            unziped_reference_name = os.path.join(corrected_dirpath, os.path.basename(ref_basename))
            # Renaming, because QUAST does not support non-alphabetic symbols or non-digits in names of files with contigs or references.
            unziped_reference_name = corrected_fname_for_nucmer(unziped_reference_name)
            unziped_reference = open(unziped_reference_name, 'w')
            subprocess.call(['gunzip', qconfig.reference, '-c'], stdout=unziped_reference)
            unziped_reference.close()
            qconfig.reference = unziped_reference_name

    # we should remove input files with no contigs (e.g. if ll contigs are less than "min_contig" value)
    contigs_to_remove = []
    ## removing from contigs' names special characters because:
    ## 1) Some embedded tools can fail on some strings with "...", "+", "-", etc
    ## 2) Nucmer fails on names like "contig 1_bla_bla", "contig 2_bla_bla" (it interprets as a contig's name only the first word of caption and gets ambiguous contigs names)
    new_contigs_fpaths = []
    for id, contigs_fpath in enumerate(contigs_fpaths):
        corr_fname = corrected_fname_for_nucmer(os.path.basename(contigs_fpath))
        corr_fpath = os.path.splitext(os.path.join(corrected_dirpath, corr_fname))[0]
        if os.path.isfile(corr_fpath):  # in case of files with the same names
            i = 1
            basename = os.path.splitext(corr_fname)[0]
            while os.path.isfile(corr_fpath):
                i += 1
                corr_fpath = os.path.join(corrected_dirpath, os.path.basename(basename + '__' + str(i)))

        lengths = fastaparser.get_lengths_from_fastafile(contigs_fpath)
        if not sum(1 for l in lengths if l >= qconfig.min_contig):
            print "\n  Warning! %s will not be processed because it doesn't have contigs >= %d bp.\n" % (os.path.basename(contigs_fpath), qconfig.min_contig)
            continue

        ## filling column "Assembly" with names of assemblies
        report = reporting.get(corr_fpath)
        ## filling columns "Number of contigs >=110 bp", ">=200 bp", ">=500 bp"
        report.add_field(reporting.Fields.CONTIGS,   [sum(1 for l in lengths if l >= threshold) for threshold in qconfig.contig_thresholds])
        report.add_field(reporting.Fields.TOTALLENS, [sum(l for l in lengths if l >= threshold) for threshold in qconfig.contig_thresholds])

        modified_fasta_entries = []
        for name, seq in fastaparser.read_fasta(contigs_fpath): # in tuples: (name, seq)
            if len(seq) >= qconfig.min_contig:
                corr_name = re.sub(r'\W', '', re.sub(r'\s', '_', name))
                # mauve and gage can't work with alternatives
                dic = {'M': 'A', 'K': 'G', 'R': 'A', 'Y': 'C', 'W': 'A', 'S': 'C', 'V': 'A', 'B': 'C', 'H': 'A', 'D': 'A'}
                pat = "(%s)" % "|".join( map(re.escape, dic.keys()) )
                corr_seq = re.sub(pat, lambda m:dic[m.group()], seq)
                modified_fasta_entries.append((corr_name, corr_seq))
        fastaparser.write_fasta_to_file(corr_fpath, modified_fasta_entries)

        print '  %s ==> %s' % (contigs_fpath, os.path.basename(corr_fpath))
        new_contigs_fpaths.append(os.path.join(__location__, corr_fpath))

    print '  Done.'
    old_contigs_fpaths = contigs_fpaths
    contigs_fpaths = new_contigs_fpaths

    if not contigs_fpaths:
        usage()
        sys.exit(1)

    if qconfig.with_gage:
        ########################################################################
        ### GAGE
        ########################################################################
        if not qconfig.reference:
            print >> sys.stderr, "\nError! GAGE can't be run without reference!\n"
        else:
            from libs import gage
            gage.do(qconfig.reference, contigs_fpaths, output_dirpath, qconfig.min_contig, lib_dir)

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
    basic_stats.do(qconfig.reference, contigs_fpaths, output_dirpath + '/basic_stats', all_pdf, qconfig.draw_plots,
        json_outputpath, output_dirpath)

    nucmer_statuses = {}

    if qconfig.reference:
        ########################################################################
        ### former PLANTAKOLYA, PLANTAGORA
        ########################################################################
        from libs import contigs_analyzer
        nucmer_statuses = contigs_analyzer.do(qconfig.reference, contigs_fpaths, qconfig.cyclic, output_dirpath + '/contigs_reports', lib_dir, qconfig.draw_plots)
        for contigs_fpath, nucmer_status in nucmer_statuses.items():
            if nucmer_status == contigs_analyzer.NucmerStatus.FAILED:
                reporting.delete(contigs_fpath)
                contigs_fpaths.remove(contigs_fpath)
#            if nucmer_status == contigs_analyzer.NucmerStatus.NOT_ALIGNED:
#                qconfig.reference = None


    # Before continue evaluating, check if nucmer didn't skip all of the contigs files.
    if len(contigs_fpaths) != 0:
        if qconfig.reference:
            aligned_contigs_fpaths = filter(lambda c_fpath: nucmer_statuses[c_fpath] == contigs_analyzer.NucmerStatus.OK, contigs_fpaths)

            ##################################################
            # ######################
            ### NA and NGA ("aligned N and NG")
            ########################################################################
            from libs import aligned_stats
            aligned_stats.do(qconfig.reference, aligned_contigs_fpaths, output_dirpath + '/contigs_reports',
                output_dirpath + '/aligned_stats', all_pdf, qconfig.draw_plots, json_outputpath, output_dirpath)

            ########################################################################
            ### GENOME_ANALYZER
            ########################################################################
            from libs import genome_analyzer
            genome_analyzer.do(qconfig.reference, aligned_contigs_fpaths, output_dirpath + '/contigs_reports',
                output_dirpath + '/genome_stats', qconfig.genes, qconfig.operons, all_pdf, qconfig.draw_plots, json_outputpath, output_dirpath)

        def add_empty_predicted_genes_fields():
            # TODO: make it nicer (not output predicted genes if annotations are provided
            for id, contigs_fpath in enumerate(contigs_fpaths):
                report = reporting.get(contigs_fpath)
                report.add_field(reporting.Fields.GENEMARKUNIQUE, "")
                report.add_field(reporting.Fields.GENEMARK, [""] * len(qconfig.genes_lengths))

        if not qconfig.genes:
            ########################################################################
            ### GeneMark
            ########################################################################
            import platform
            if platform.system() == 'Darwin':
                print >> sys.stderr, 'Warning! GeneMark tool for gene prediction doesn\'t work on Mac OS X.'
                add_empty_predicted_genes_fields()
            else:
                from libs import genemark
                genemark.do(contigs_fpaths, qconfig.genes_lengths, output_dirpath + '/predicted_genes', lib_dir)

        else:
            add_empty_predicted_genes_fields()

        ########################################################################
        ### TOTAL REPORT
        ########################################################################
        reporting.save_total(output_dirpath)

        if json_outputpath:
            json_saver.save_total_report(json_outputpath, qconfig.min_contig)

        if qconfig.html_report:
            from libs.html_saver import html_saver
            html_saver.save_total_report(output_dirpath, qconfig.min_contig)

        if qconfig.draw_plots and all_pdf:
            print '  All pdf files are merged to', all_pdf_filename
            all_pdf.close()

        from libs import contigs_analyzer
        for contigs_fpath, nucmer_status in nucmer_statuses.items():
            if nucmer_status == contigs_analyzer.NucmerStatus.FAILED:
                print 'Warning!', '\'' + os.path.basename(contigs_fpath) + '\'', 'skipped. Nucmer failed processing this contigs.'
            if nucmer_status == contigs_analyzer.NucmerStatus.NOT_ALIGNED:
                print 'Warning! Contigs in', '\'' + os.path.basename(contigs_fpath) + '\'', 'are not aligned on the reference. Most of the metrics are impossible to evaluate and going to be skipped.'

        print 'Done.'
        cleanup(corrected_dirpath, tee)
        return 0

    else:
        print 'Error! Nucmer failed processing the file%s with contigs. ' \
              'Check out if the contigs and the reference used are correct.%s' \
                % ('s' if len(old_contigs_fpaths) > 1 else '',
                   ' The problem concern all the files provided.' if len(old_contigs_fpaths) > 1 else '')
        cleanup(corrected_dirpath, tee)
        return 1



def cleanup(corrected_dirpath, tee):
    ## removing correcting input contig files
    if not qconfig.debug:
        shutil.rmtree(corrected_dirpath)

    tee.free() # free sys.stdout and sys.stderr from logfile


if __name__ == '__main__':
    main(sys.argv[1:])
