#!/usr/bin/env python

############################################################################
# Copyright (c) 2011-2012 Saint-Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################
import gzip

import sys
import bz2
import os
import shutil
import re
import getopt
import subprocess
from site import addsitedir
import zipfile
from libs.qutils import uncompress

__location__ = os.path.realpath(os.path.join(os.getcwd(), os.path.dirname(__file__)))

addsitedir(os.path.join(__location__, 'libs/site_packages'))

import simplejson as json


#sys.path.append(os.path.join(os.path.abspath(sys.path[0]), 'libs'))
#sys.path.append(os.path.join(os.path.abspath(sys.path[0]), '../spades_pipeline'))


from libs import qconfig
from libs import fastaparser
from libs.html_saver import json_saver

RELEASE_MODE=False

def print_version(stream=sys.stdout):
    version_filename = os.path.join(__location__, 'VERSION')
    version = "unknown"
    build = "unknown"
    if os.path.isfile(version_filename):
        version_file = open(version_filename)
        version = version_file.readline()
        if version:
            version = version.strip()
        else:
            version = "unknown"
        build = version_file.readline()
        if build:
            build = build.split()[1].strip()
        else:
            build = "unknown"

    print >> stream, "Version:", version,
    print >> stream, "Build:", build


def usage():
    print >> sys.stderr, 'QUAST: QUality ASsessment Tool for Genome Assemblies.'
    print_version(sys.stderr)

    print >> sys.stderr, ""
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
        print >> sys.stderr, "--threads    <int>                  maximum number of threads [default: number of provided assemblies]"
        print >> sys.stderr, "--gage                              this flag starts QUAST in \"GAGE mode\""
        print >> sys.stderr, "--contig-thresholds   <int,int,..>  comma-separated list of contig length thresholds [default: %s]" % qconfig.contig_thresholds
        print >> sys.stderr, "--genemark-thresholds <int,int,..>  comma-separated list of threshold lengths of genes to search with GeneMark [default is %s]" % qconfig.genes_lengths
        print >> sys.stderr, "--scaffolds                         this flag informs QUAST that provided assemblies are scaffolds"
        print >> sys.stderr, '--eukaryote                         this flag should be set if the genome is an eukaryote'
        print >> sys.stderr, '--only-best-alignments              this flag forces QUAST to use only one alignment of contigs covering repeats'
        print >> sys.stderr, ""
        print >> sys.stderr, "-h/--help           print this usage message"
    else:
        print >> sys.stderr, 'Options with arguments'
        print >> sys.stderr, "-o  --output-dir             directory to store all result files [default: quast_results/results_<datetime>]"
        print >> sys.stderr, "-R           <filename>      reference genome file"
        print >> sys.stderr, "-G/--genes   <filename>      annotated genes file"
        print >> sys.stderr, "-O/--operons <filename>      annotated operons file"
        print >> sys.stderr, "-M  --min-contig <int>       lower threshold for contig length [default: %s]" % qconfig.min_contig
        print >> sys.stderr, "-t  --contig-thresholds      comma-separated list of contig length thresholds [default: %s]" % qconfig.contig_thresholds
        print >> sys.stderr, "-k  --genemark-thresholds    comma-separated list of threshold lengths of genes to search with GeneMark [default: %s]" % qconfig.genes_lengths
        print >> sys.stderr, "-T  --threads    <int>       maximum number of threads [default: number of provided assemblies]"
        print >> sys.stderr, "-c  --mincluster <int>       Nucmer's parameter -- the minimum length of a cluster of matches [default: %s]" % qconfig.mincluster
        print >> sys.stderr, "-r  --est-ref-size <int>     Estimated reference size (for calculating NG)"
        print >> sys.stderr, ""
        print >> sys.stderr, 'Options without arguments'
        print >> sys.stderr, "-s  --scaffolds              this flag informs QUAST that provided assemblies are scaffolds"
        print >> sys.stderr, '-g  --gage                   use Gage (results are in gage_report.txt)'
        print >> sys.stderr, '-e  --eukaryote              genome is an eukaryote'
        print >> sys.stderr, '-b  --only-best-alignments   QUAST use only one alignment of contigs covering repeats (ambiguous)'
        print >> sys.stderr, "-j  --save-json              save the output also in the JSON format"
        print >> sys.stderr, "-J  --save-json-to <path>    save the JSON-output to a particular path"
        print >> sys.stderr, "`   --no-html                don't build html report"
        print >> sys.stderr, "`   --no-plots               don't draw plots (to make quast faster)"
        print >> sys.stderr, ""
        print >> sys.stderr, "-d  --debug                  run in debug mode"
        print >> sys.stderr, "-h  --help                   print this usage message"

def assert_file_exists(fpath, message=''):
    if not os.path.isfile(fpath):
        print >> sys.stderr, "\nERROR! File not found (%s): %s\n" % (message, fpath)
        sys.exit(2)
    return fpath


def corrected_fname_for_nucmer(fpath):
    dirpath = os.path.dirname(fpath)
    fname = os.path.basename(fpath)

    corr_fname = fname
    corr_fname = re.sub(r'[^\w\._\-+]', '_', corr_fname).strip()

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


def correct_fasta(original_fpath, corrected_fpath, is_reference=False):
    modified_fasta_entries = []
    for name, seq in fastaparser.read_fasta(original_fpath): # in tuples: (name, seq)
        if (len(seq) >= qconfig.min_contig) or is_reference:
            corr_name = re.sub(r'[^\w\._\-+]', '_', name)
            # seq to uppercase, because we later looking only uppercase letters
            corr_seq = seq.upper()
            # removing \r (Nucmer fails on such sequences)
            corr_seq = corr_seq.strip('\r')
            # correcting alternatives (gage can't work with alternatives)
            #dic = {'M': 'A', 'K': 'G', 'R': 'A', 'Y': 'C', 'W': 'A', 'S': 'C', 'V': 'A', 'B': 'C', 'H': 'A', 'D': 'A'}
            dic = {'M': 'N', 'K': 'N', 'R': 'N', 'Y': 'N', 'W': 'N', 'S': 'N', 'V': 'N', 'B': 'N', 'H': 'N', 'D': 'N'}
            pat = "(%s)" % "|".join( map(re.escape, dic.keys()) )
            corr_seq = re.sub(pat, lambda m:dic[m.group()], corr_seq)
            # checking that only A,C,G,T or N presented in sequence
            if re.compile(r'[^ACGTN]').search(corr_seq):
                return False
            modified_fasta_entries.append((corr_name, corr_seq))
    fastaparser.write_fasta(corrected_fpath, modified_fasta_entries)
    return True


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
            qconfig.genes = assert_file_exists(arg, 'genes')

        elif opt in ('-O', "--operons"):
            qconfig.operons = assert_file_exists(arg, 'operons')

        elif opt in ('-R', "--reference"):
            qconfig.reference = assert_file_exists(arg, 'reference')

        elif opt in ('-t', "--contig-thresholds"):
            qconfig.contig_thresholds = arg

        elif opt in ('-M', "--min-contig"):
            qconfig.min_contig = int(arg)

        elif opt in ('-T', "--threads"):
            qconfig.threads = int(arg)
            if qconfig.threads < 1:
                qconfig.threads = 1

        elif opt in ('-c', "--mincluster"):
            qconfig.mincluster = int(arg)

        elif opt in ('-r', "--est-ref-size"):
            qconfig.estimated_reference_size = int(arg)

        elif opt in ('-k', "--genemark-thresholds"):
            qconfig.genes_lengths = arg

        elif opt in ('-j', '--save-json'):
            qconfig.save_json = True

        elif opt in ('-J', '--save-json-to'):
            qconfig.save_json = True
            qconfig.make_latest_symlink = False
            json_outputpath = arg

        elif opt in ('-s', "--scaffolds"):
            qconfig.scaffolds = True

        elif opt in ('-g', "--gage"):
            qconfig.with_gage = True

        elif opt in ('-e', "--eukaryote"):
            qconfig.prokaryote = False

        elif opt in ('-b', "--only-best-alignments"):
            qconfig.only_best_alignments = True

        elif opt == '--no-plots':
            qconfig.draw_plots = False

        elif opt == '--no-html':
            qconfig.html_report = False

        elif opt in ('-d', "--debug"):
            qconfig.debug = True

        elif opt in ('-h', "--help"):
            usage()
            sys.exit(0)

        else:
            raise ValueError

    for c_fpath in contigs_fpaths:
        assert_file_exists(c_fpath, 'contigs')

    #    old_contigs_fpaths = contigs_fpaths[:]
    #    contigs_fpaths = []
    #    for old_c_fpath in old_contigs_fpaths:
    #        contigs_fpaths.append(rename_file_for_nucmer(old_c_fpath))

    #    # For renaming back: contigs_fpaths are to be changed further.
    #    new_contigs_fpaths = contigs_fpaths[:]


    qconfig.contig_thresholds = map(int, qconfig.contig_thresholds.split(","))
    qconfig.genes_lengths = map(int, qconfig.genes_lengths.split(","))

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
            print "\nWarning! Output directory already exists! Existing Nucmer alignments can be used!\n"

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

    # saving info about command line and options into log file
    lfile = open(logfile,'w')
    print >> lfile, "QUAST started: ",
    for v in sys.argv:
        print >> lfile, v,
    print >> lfile, ""
    print_version(lfile)
    print >> lfile, ""
    lfile.close()

    tee = support.Tee(logfile, 'a', console=True) # not pure, changes sys.stdout and sys.stderr

    ########################################################################

    from libs import reporting
    reload(reporting)
    reporting.min_contig = qconfig.min_contig

    print 'Correcting contig files...'
    if os.path.isdir(corrected_dirpath):
        shutil.rmtree(corrected_dirpath)
    os.mkdir(corrected_dirpath)

    if qconfig.reference:
        ref_basename, ref_extension = os.path.splitext(qconfig.reference)
        corrected_and_unziped_reference_fname = os.path.join(corrected_dirpath, os.path.basename(ref_basename))
        corrected_and_unziped_reference_fname = corrected_fname_for_nucmer(corrected_and_unziped_reference_fname)

        # unzipping (if needed)
        if uncompress(qconfig.reference, corrected_and_unziped_reference_fname, sys.stderr):
            qconfig.reference = corrected_and_unziped_reference_fname

        # correcting
        if not correct_fasta(qconfig.reference, corrected_and_unziped_reference_fname, True):
            print "\n  Warning! Provided reference will be skipped because it contains non-ACGTN characters.\n"
            qconfig.reference = ""
        else:
            qconfig.reference = corrected_and_unziped_reference_fname

    def handle_fasta(contigs_fpath, corr_fpath):
        lengths = fastaparser.get_lengths_from_fastafile(contigs_fpath)
        if not sum(1 for l in lengths if l >= qconfig.min_contig):
            print "\n  Warning! %s will be skipped because it doesn't have contigs >= %d bp.\n" % (os.path.basename(contigs_fpath), qconfig.min_contig)
            return False

        # correcting
        if not correct_fasta(contigs_fpath, corr_fpath):
            print "\n  Warning! %s will be skipped because it contains non-ACGTN characters.\n" % (os.path.basename(contigs_fpath))
            return False

        ## filling column "Assembly" with names of assemblies
        report = reporting.get(corr_fpath)
        ## filling columns "Number of contigs >=110 bp", ">=200 bp", ">=500 bp"
        report.add_field(reporting.Fields.CONTIGS,   [sum(1 for l in lengths if l >= threshold) for threshold in qconfig.contig_thresholds])
        report.add_field(reporting.Fields.TOTALLENS, [sum(l for l in lengths if l >= threshold) for threshold in qconfig.contig_thresholds])
        return True


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

        if not handle_fasta(contigs_fpath, corr_fpath):
            continue

        print '  %s ==> %s' % (contigs_fpath, os.path.basename(corr_fpath))
        new_contigs_fpaths.append(os.path.join(__location__, corr_fpath))

        # if option --scaffolds is specified QUAST adds splitted version of assemblies to the comparison
        if qconfig.scaffolds:
            print "  splitting scaffolds into contigs:"
            splitted_path = corr_fpath + '_splitted'
            shutil.copy(corr_fpath, splitted_path)

            splitted_fasta = []
            contigs_counter = 0
            for id, (name, seq) in enumerate(fastaparser.read_fasta(corr_fpath)):
                i = 0
                cur_contig_number = 1
                cur_contig_start = 0
                while (i < len(seq)) and (seq.find("N", i) != -1):
                    start = seq.find("N", i)
                    end = start + 1
                    while (end != len(seq)) and (seq[end] == 'N'):
                        end += 1

                    i = end + 1
                    if (end - start) >= qconfig.Ns_break_threshold:
                        splitted_fasta.append((name.split()[0] + "_" + str(cur_contig_number), seq[cur_contig_start:start]))
                        cur_contig_number += 1
                        cur_contig_start = end

                splitted_fasta.append((name.split()[0] + "_" + str(cur_contig_number), seq[cur_contig_start:]))
                contigs_counter += cur_contig_number

            fastaparser.write_fasta(splitted_path, splitted_fasta)
            print "  %d scaffolds (%s) were broken into %d contigs (%s)"\
                  % (id + 1, os.path.basename(corr_fpath), contigs_counter, os.path.basename(splitted_path))
            if handle_fasta(splitted_path, splitted_path):
                new_contigs_fpaths.append(os.path.join(__location__, splitted_path))

    print '  Done.'
    old_contigs_fpaths = contigs_fpaths
    contigs_fpaths = new_contigs_fpaths

    from libs import qutils
    qutils.assemblies_number = len(contigs_fpaths)

    if not contigs_fpaths:
        usage()
        sys.exit(1)

    if qconfig.with_gage:
        ########################################################################
        ### GAGE
        ########################################################################
        if not qconfig.reference:
            print >> sys.stderr, "\nError! GAGE can't be run without a reference!\n"
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

    aligned_fpaths = []
    if qconfig.reference:
        ########################################################################
        ### former PLANTAKOLYA, PLANTAGORA
        ########################################################################
        from libs import contigs_analyzer
        nucmer_statuses = contigs_analyzer.do(qconfig.reference, contigs_fpaths, qconfig.prokaryote, output_dirpath + '/contigs_reports', lib_dir, qconfig.draw_plots)
        for contigs_fpath in contigs_fpaths:
            if nucmer_statuses[contigs_fpath] == contigs_analyzer.NucmerStatus.OK:
                aligned_fpaths.append(contigs_fpath)

    # Before continue evaluating, check if nucmer didn't skip all of the contigs files.
    if len(aligned_fpaths) != 0 and qconfig.reference:
        ##################################################
        # ######################
        ### NA and NGA ("aligned N and NG")
        ########################################################################
        from libs import aligned_stats
        aligned_stats.do(qconfig.reference, aligned_fpaths, output_dirpath + '/contigs_reports',
                         output_dirpath + '/aligned_stats', all_pdf, qconfig.draw_plots, json_outputpath, output_dirpath)

        ########################################################################
        ### GENOME_ANALYZER
        ########################################################################
        from libs import genome_analyzer
        genome_analyzer.do(qconfig.reference, aligned_fpaths, output_dirpath + '/contigs_reports',
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
        from libs import genemark
        genemark.do(contigs_fpaths, qconfig.genes_lengths, output_dirpath + '/predicted_genes', lib_dir)

    else:
        add_empty_predicted_genes_fields()

    ########################################################################
    ### TOTAL REPORT
    ########################################################################

    # changing names of assemblies to more human-readable versions if provided
    if qconfig.legend_names and len(contigs_fpaths) == len(qconfig.legend_names):
        for id, contigs_fpath in enumerate(contigs_fpaths):
            report = reporting.get(contigs_fpath)
            report.add_field(reporting.Fields.NAME, qconfig.legend_names[id])

    reporting.save_total(output_dirpath)

    if json_outputpath:
        json_saver.save_total_report(json_outputpath, qconfig.min_contig)

    if qconfig.html_report:
        from libs.html_saver import html_saver
        html_saver.save_total_report(output_dirpath, qconfig.min_contig)

    if qconfig.draw_plots and all_pdf:
        print '  All pdf files are merged to', all_pdf_filename
        all_pdf.close()

    print 'Done.'
    cleanup(corrected_dirpath, tee)
    return 0

#    else:
#        print 'Warning! No contigs were aligned to the reference. Most of the metrics are impossible to evaluate and going to be skipped.'
#        print 'Done.'
#        print 'Warning! Nucmer failed processing the file%s with contigs. ' \
#              'Check out if the contigs and the reference are correct.%s' \
#                % ('s' if len(old_contigs_fpaths) > 1 else '',
#                   ' The problem concerns all the files provided.' if len(old_contigs_fpaths) > 1 else '')
#        cleanup(corrected_dirpath, tee)
#        return 1


def cleanup(corrected_dirpath, tee):
    ## removing correcting input contig files
    if not qconfig.debug:
        shutil.rmtree(corrected_dirpath)

    tee.free() # free sys.stdout and sys.stderr from logfile


if __name__ == '__main__':
    main(sys.argv[1:])
