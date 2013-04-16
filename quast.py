#!/usr/bin/env python

############################################################################
# Copyright (c) 2011-2013 Saint-Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################

import logging
log = logging.getLogger('quast')
import sys
import os
import shutil
import re
import getopt
from site import addsitedir
from libs import qconfig, qutils
from libs.qutils import notice, warning, error, assert_file_exists
from libs.qutils import print_timestamp, print_system_info, print_version, uncompress
from libs import fastaparser
from libs.html_saver import json_saver

addsitedir(os.path.join(qconfig.LIBS_LOCATION, 'site_packages'))

RELEASE_MODE=False


def usage():
    print >> sys.stderr, 'QUAST: QUality ASsessment Tool for Genome Assemblies'
    print_version(True)

    print >> sys.stderr, ""
    print >> sys.stderr, 'Usage: python', sys.argv[0], '[options] <files_with_contigs>'
    print >> sys.stderr, ""

    if RELEASE_MODE:
        print >> sys.stderr, "Options:"
        print >> sys.stderr, "-o            <dirname>      Directory to store all result file. Default: quast_results/results_<datetime>"
        print >> sys.stderr, "-R            <filename>     Reference genome file"
        print >> sys.stderr, "-G  --genes   <filename>     Annotated genes file"
        print >> sys.stderr, "-O  --operons <filename>     Annotated operons file"
        print >> sys.stderr, "--min-contig  <int>          Lower threshold for contig length [default: %s]" % qconfig.min_contig
        print >> sys.stderr, ""
        print >> sys.stderr, ""
        print >> sys.stderr, "Advanced options:"
        print >> sys.stderr, "--threads <int>                   Maximum number of threads [default: number of CPUs]"
        print >> sys.stderr, ""
        print >> sys.stderr, "--gage                            Starts GAGE inside QUAST (\"GAGE mode\")"
        print >> sys.stderr, ""
        print >> sys.stderr, "--contig-thresholds <int,int,..>  Comma-separated list of contig length thresholds [default: %s]" % qconfig.contig_thresholds
        print >> sys.stderr, ""
        print >> sys.stderr, "--gene-finding                    Uses Gene Finding module"
        print >> sys.stderr, ""
        print >> sys.stderr, "--gene-thresholds <int,int,..>    Comma-separated list of threshold lengths of genes to search with Gene Finding module"
        print >> sys.stderr, "                                  [default is %s]" % qconfig.genes_lengths
        print >> sys.stderr, ""
        print >> sys.stderr, "--eukaryote                       Genome is an eukaryote"
        print >> sys.stderr, ""
        print >> sys.stderr, "--est-ref-size <int>              Estimated reference size (for computing NGx metrics without a reference)"
        print >> sys.stderr, ""
        print >> sys.stderr, "--scaffolds                       Provided assemblies are scaffolds"
        print >> sys.stderr, ""
        print >> sys.stderr, "--use-all-alignments              Computes Genome fraction, # genes, # operons metrics in compatible with QUAST v.1.* mode."
        print >> sys.stderr, "                                  By default, QUAST filters Nucmer\'s alignments to keep only best ones"
        print >> sys.stderr, ""
        print >> sys.stderr, "--allow-ambiguity                 Uses all alignments of a contig with multiple equally good alignments (probably a repeat)."
        print >> sys.stderr, "                                  By default, QUAST skips all alignments of such contigs"
        print >> sys.stderr, ""
        print >> sys.stderr, "--strict-NA                       Breaks contigs by any misassembly event to compute NAx and NGAx."
        print >> sys.stderr, "                                  By default, QUAST breaks contigs only by extensive misassemblies (not local ones)"
        print >> sys.stderr, ""
        print >> sys.stderr, "-m  --meta                        Metagenomic assembly. Uses MetaGeneMark for gene prediction. "
        print >> sys.stderr, "                                  Accepts multiple reference files (comma-separated list of filenames after -R option)"
        print >> sys.stderr, ""
        print >> sys.stderr, "-h  --help                        Prints this message"

    else:
        print >> sys.stderr, 'Options with arguments'
        print >> sys.stderr, "-o  --output-dir   <dirname>     directory to store all result files [default: quast_results/results_<datetime>]"
        print >> sys.stderr, "-R                 <filename>    reference genome file"
        print >> sys.stderr, "-G  --genes        <filename>    annotated genes file"
        print >> sys.stderr, "-O  --operons      <filename>    annotated operons file"
        print >> sys.stderr, "-M  --min-contig   <int>         lower threshold for contig length [default: %s]" % qconfig.min_contig
        print >> sys.stderr, "-t  --contig-thresholds          comma-separated list of contig length thresholds [default: %s]" % qconfig.contig_thresholds
        print >> sys.stderr, "-S  --gene-thresholds            comma-separated list of threshold lengths of genes to search with Gene Finding module [default: %s]" % qconfig.genes_lengths
        print >> sys.stderr, "-T  --threads      <int>         maximum number of threads [default: number of CPUs]"
        print >> sys.stderr, "-c  --mincluster   <int>         Nucmer's parameter -- the minimum length of a cluster of matches [default: %s]" % qconfig.mincluster
        print >> sys.stderr, "    --est-ref-size <int>         estimated reference size (for calculating NG)"
        print >> sys.stderr, ""
        print >> sys.stderr, 'Options without arguments'
        print >> sys.stderr, "-f  --gene-finding               uses Gene Finding module"
        print >> sys.stderr, "-s  --scaffolds                  this flag informs QUAST that provided assemblies are scaffolds"
        print >> sys.stderr, "    --gage                       uses GAGE (results are in gage_report.txt)"
        print >> sys.stderr, "-e  --eukaryote                  genome is eukaryotic"
        print >> sys.stderr, "-a  --allow-ambiguity            uses all alignments of contigs covering repeats (ambiguous)"
        print >> sys.stderr, "-u  --use-all-alignments         computes Genome fraction, # genes, # operons in v.1.0-1.3 style"
        print >> sys.stderr, "-n  --strict-NA                  breaks contigs by any misassembly event to compute NAx and NGAx."
        print >> sys.stderr, "-j  --save-json                  saves the output also in the JSON format"
        print >> sys.stderr, "-J  --save-json-to <path>        saves the JSON-output to a particular path"
        print >> sys.stderr, "    --no-html                    doesn't build html report"
        print >> sys.stderr, "    --no-plots                   doesn't draw plots (to make quast faster)"
        print >> sys.stderr, ""
        print >> sys.stderr, "-d  --debug                      runs in debug mode"
        print >> sys.stderr, "-h  --help                       prints this message"


def corrected_fname_for_nucmer(fpath):
    dirpath = os.path.dirname(fpath)
    fname = os.path.basename(fpath)

    corr_fname = qutils.correct_name(fname)

    if corr_fname != fname:
        if os.path.isfile(os.path.join(dirpath, corr_fname)):
            i = 1
            base_corr_fname = corr_fname
            while os.path.isfile(os.path.join(dirpath, corr_fname)):
                str_i = ''
                if i > 1:
                    str_i = str(i)

                corr_fname = 'copy_' + str_i + '_' + str(base_corr_fname)
                i += 1

    return os.path.join(dirpath, corr_fname)


def correct_fasta(original_fpath, corrected_fpath, is_reference=False):
    modified_fasta_entries = []
    for first_line, seq in fastaparser.read_fasta(original_fpath):
        if (len(seq) >= qconfig.min_contig) or is_reference:
            corr_name = qutils.correct_name(first_line)

            # seq to uppercase, because we later looking only uppercase letters
            corr_seq = seq.upper()

            # removing \r (Nucmer fails on such sequences)
            corr_seq = corr_seq.strip('\r')

            # correcting alternatives (gage can't work with alternatives)
            # dic = {'M': 'A', 'K': 'G', 'R': 'A', 'Y': 'C', 'W': 'A', 'S': 'C', 'V': 'A', 'B': 'C', 'H': 'A', 'D': 'A'}
            dic = {'M': 'N', 'K': 'N', 'R': 'N', 'Y': 'N', 'W': 'N', 'S': 'N', 'V': 'N', 'B': 'N', 'H': 'N', 'D': 'N'}
            pat = "(%s)" % "|".join(map(re.escape, dic.keys()))
            corr_seq = re.sub(pat, lambda m: dic[m.group()], corr_seq)

            # make sure that only A, C, G, T or N are in the sequence
            if re.compile(r'[^ACGTN]').search(corr_seq):
                warning('Skipping ' + original_fpath + ' because it contains non-ACGTN characters.',
                        indent='    ')
                return False

            modified_fasta_entries.append((corr_name, corr_seq))

    fastaparser.write_fasta(corrected_fpath, modified_fasta_entries)

    if is_reference:
        ref_len = sum(len(chr_seq) for (chr_name, chr_seq) in modified_fasta_entries)
        if ref_len > qconfig.MAX_REFERENCE_LENGTH:
            dir_for_splitted_ref = os.path.join(os.path.dirname(corrected_fpath), 'splitted_ref')
            os.makedirs(dir_for_splitted_ref)
            for id, (chr_name, chr_seq) in enumerate(modified_fasta_entries):
                if len(chr_seq) > qconfig.MAX_REFERENCE_LENGTH:
                    warning("Skipping chromosome " + chr_name + " because it length is greater than " +
                            str(qconfig.MAX_REFERENCE_LENGTH) + " (Nucmer's constraint).")
                    continue
                qconfig.splitted_ref.append(os.path.join(dir_for_splitted_ref, "chr_" + str(id + 1)))
                fastaparser.write_fasta(qconfig.splitted_ref[-1], [(chr_name, chr_seq)])
            if len(qconfig.splitted_ref) == 0:
                warning("Skipping reference because all of its chromosomes exceeded Nucmer's constraint.")
                return False
    return True


def main(args):
    quast_dir = os.path.dirname(qconfig.LIBS_LOCATION)
    if ' ' in quast_dir:
        error('QUAST does not support spaces in paths. \n' + \
              'You are trying to run it from ' + str(quast_dir) + '\n' + \
              'Please, put QUAST in a different directory, then try again.\n',
              console_output=True,
              exit_with_code=3)

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
        sys.exit(2)

    if not contigs_fpaths:
        usage()
        sys.exit(2)

    json_outputpath = None
    output_dirpath = None

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
            qconfig.max_threads = int(arg)
            if qconfig.max_threads < 1:
                qconfig.max_threads = 1

        elif opt in ('-c', "--mincluster"):
            qconfig.mincluster = int(arg)

        elif opt == "--est-ref-size":
            qconfig.estimated_reference_size = int(arg)

        elif opt in ('-S', "--gene-thresholds"):
            qconfig.genes_lengths = arg

        elif opt in ('-j', '--save-json'):
            qconfig.save_json = True

        elif opt in ('-J', '--save-json-to'):
            qconfig.save_json = True
            qconfig.make_latest_symlink = False
            json_outputpath = arg

        elif opt in ('-s', "--scaffolds"):
            qconfig.scaffolds = True

        elif opt == "--gage":
            qconfig.with_gage = True

        elif opt in ('-e', "--eukaryote"):
            qconfig.prokaryote = False

        elif opt in ('-f', "--gene-finding"):
            qconfig.gene_finding = True

        elif opt in ('-a', "--allow-ambiguity"):
            qconfig.allow_ambiguity = True

        elif opt in ('-u', "--use-all-alignments"):
            qconfig.use_all_alignments = True

        elif opt in ('-n', "--strict-NA"):
            qconfig.strict_NA = True

        elif opt == '--no-plots':
            qconfig.draw_plots = False

        elif opt == '--no-html':
            qconfig.html_report = False

        elif opt in ("-m", "--meta"):
            qconfig.meta = True

        elif opt in ('-d', "--debug"):
            qconfig.debug = True

        elif opt in ('-h', "--help"):
            usage()
            sys.exit(0)

        else:
            raise ValueError

    for c_fpath in contigs_fpaths:
        assert_file_exists(c_fpath, 'contigs')

    ########################################################################
    ### CONFIG & CHECKS
    ########################################################################
    existing_alignments = False

    if output_dirpath:  # 'output dir was specified with -o option'
        if os.path.isdir(output_dirpath):
            existing_alignments = True

    else:  # output dir was not specified, creating our own one
        output_dirpath = os.path.join(os.path.abspath(qconfig.default_results_root_dirname), qconfig.output_dirname)

        # in case of starting two instances of QUAST in the same second
        if os.path.isdir(output_dirpath):
            if qconfig.make_latest_symlink:
                i = 2
                base_dirpath = output_dirpath
                while os.path.isdir(output_dirpath):
                    output_dirpath = str(base_dirpath) + '__' + str(i)
                    i += 1

    if not os.path.isdir(output_dirpath):
        os.makedirs(output_dirpath)

    # duplicating output to a log file
    logfile = os.path.join(output_dirpath, qconfig.logfile)
    log.setLevel(logging.DEBUG)

    console = logging.StreamHandler(sys.stdout)
    console.setFormatter(logging.Formatter('%(message)s'))
    console.setLevel(logging.DEBUG)
    log.addHandler(console)
    log_handler = logging.FileHandler(logfile, mode='w')
    log.addHandler(log_handler)

    ########################################################################

    command_line = ""
    for v in sys.argv:
        command_line += str(v) + ' '
    log.info("QUAST started: " + command_line)
    print_version()
    log.info("")
    print_system_info()

    start_time = print_timestamp("Started: ")
    log.info("")
    log.info("Logging to " + logfile)
    log.info("")

    ########################################################################

    if existing_alignments:
        notice("Output directory already exists. Existing Nucmer alignments can be used.")
        qutils.remove_reports(output_dirpath)

    qconfig.contig_thresholds = map(int, qconfig.contig_thresholds.split(","))
    qconfig.genes_lengths = map(int, qconfig.genes_lengths.split(","))
    if qconfig.max_threads is None:
        try:
            import multiprocessing
            qconfig.max_threads = multiprocessing.cpu_count()
        except:
            warning('Failed to determine the number of CPUs')
            qconfig.max_threads = qconfig.DEFAULT_MAX_THREADS
        notice('Maximum number of threads was set to ' + str(qconfig.max_threads) + ' (use --threads option to set it manually)')

    # 'latest' symlink
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

    # Where corrected contigs will be saved
    corrected_dirpath = os.path.join(output_dirpath, qconfig.corrected_dirname)

    # Where all pdfs will be saved
    all_pdf_filename = os.path.join(output_dirpath, qconfig.plots_filename)
    all_pdf = None

    ########################################################################

    from libs import reporting
    reload(reporting)

    message = "Processing contig files"
    if qconfig.reference:
        message += " and reference"
    message += "..."
    log.info(message)

    if os.path.isdir(corrected_dirpath):
        shutil.rmtree(corrected_dirpath)
    os.mkdir(corrected_dirpath)

    # Processing contigs
    def handle_fasta(contigs_fpath, corr_fpath):
        lengths = fastaparser.get_lengths_from_fastafile(contigs_fpath)
        if not sum(l for l in lengths if l >= qconfig.min_contig):
            warning("Skipping %s because it doesn't contain contigs >= %d bp." % (os.path.basename(contigs_fpath), qconfig.min_contig))
            return False

        # correcting
        if not correct_fasta(contigs_fpath, corr_fpath):
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

        log.info('  %s ==> %s' % (contigs_fpath, os.path.basename(corr_fpath)))

        # if option --scaffolds is specified QUAST adds splitted version of assemblies to the comparison
        if qconfig.scaffolds:
            log.info("  breaking scaffolds into contigs:")
            broken_scaffolds_path = corr_fpath + '_broken'

            broken_scaffolds_fasta = []
            contigs_counter = 0
            for id, (name, seq) in enumerate(fastaparser.read_fasta(contigs_fpath)):
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
                        broken_scaffolds_fasta.append((name.split()[0] + "_" + str(cur_contig_number), seq[cur_contig_start:start]))
                        cur_contig_number += 1
                        cur_contig_start = end

                broken_scaffolds_fasta.append((name.split()[0] + "_" + str(cur_contig_number), seq[cur_contig_start:]))
                contigs_counter += cur_contig_number

            fastaparser.write_fasta(broken_scaffolds_path, broken_scaffolds_fasta)
            log.info("      %d scaffolds (%s) were broken into %d contigs (%s)"\
                  % (id + 1, os.path.basename(corr_fpath), contigs_counter, os.path.basename(broken_scaffolds_path)))
            if handle_fasta(broken_scaffolds_path, broken_scaffolds_path):
                new_contigs_fpaths.append(broken_scaffolds_path)
                qconfig.list_of_broken_scaffolds.append(os.path.basename(broken_scaffolds_path))

        if not handle_fasta(contigs_fpath, corr_fpath):
            continue
        new_contigs_fpaths.append(corr_fpath)

    contigs_fpaths = new_contigs_fpaths

    qconfig.assemblies_num = len(contigs_fpaths)

    if not contigs_fpaths:
        error("None of assembly file contain correct contigs. "
              "Please, provide different files or decrease --min-contig threshold.",
              exit_with_code=4)

    # Processing reference
    if qconfig.reference:
        ref_basename, ref_extension = os.path.splitext(qconfig.reference)
        corrected_and_unziped_reference_fname = os.path.join(corrected_dirpath, os.path.basename(ref_basename))
        corrected_and_unziped_reference_fname = corrected_fname_for_nucmer(corrected_and_unziped_reference_fname)

        # unzipping (if needed)
        if uncompress(qconfig.reference, corrected_and_unziped_reference_fname):
            qconfig.reference = corrected_and_unziped_reference_fname

        # correcting
        if not correct_fasta(qconfig.reference, corrected_and_unziped_reference_fname, True):
            qconfig.reference = ""
        else:
            qconfig.reference = corrected_and_unziped_reference_fname

    # End of processing
    log.info('Done.')

    if qconfig.with_gage:
        ########################################################################
        ### GAGE
        ########################################################################
        if not qconfig.reference:
            warning("GAGE can't be run without a reference and will be skipped.")
        else:
            from libs import gage
            gage.do(qconfig.reference, contigs_fpaths, output_dirpath)

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
    aligned_lengths_lists = []
    if qconfig.reference:
        ########################################################################
        ### former PLANTAKOLYA, PLANTAGORA
        ########################################################################
        from libs import contigs_analyzer
        nucmer_statuses, aligned_lengths_per_fpath = contigs_analyzer.do(qconfig.reference, contigs_fpaths, qconfig.prokaryote, output_dirpath + '/contigs_reports')
        for contigs_fpath in contigs_fpaths:
            if nucmer_statuses[contigs_fpath] == contigs_analyzer.NucmerStatus.OK:
                aligned_fpaths.append(contigs_fpath)
                aligned_lengths_lists.append(aligned_lengths_per_fpath[contigs_fpath])

    # Before continue evaluating, check if nucmer didn't skip all of the contigs files.
    if len(aligned_fpaths) and qconfig.reference:
        ########################################################################
        ### NA and NGA ("aligned N and NG")
        ########################################################################
        from libs import aligned_stats
        aligned_stats.do(qconfig.reference, aligned_fpaths, aligned_lengths_lists, output_dirpath + '/contigs_reports',
                         output_dirpath + '/aligned_stats', all_pdf, qconfig.draw_plots, json_outputpath, output_dirpath)

        ########################################################################
        ### GENOME_ANALYZER
        ########################################################################
        from libs import genome_analyzer
        genome_analyzer.do(qconfig.reference, aligned_fpaths, output_dirpath + '/contigs_reports',
                           output_dirpath + '/genome_stats', qconfig.genes, qconfig.operons, all_pdf, qconfig.draw_plots, json_outputpath, output_dirpath)

    # def add_empty_predicted_genes_fields():
    #     # TODO: make it in a more appropriate way (don't output predicted genes if annotations are provided)
    #     for id, contigs_fpath in enumerate(contigs_fpaths):
    #         report = reporting.get(contigs_fpath)
    #         report.add_field(reporting.Fields.PREDICTED_GENES_UNIQUE, "")
    #         report.add_field(reporting.Fields.PREDICTED_GENES, [""] * len(qconfig.genes_lengths))

    if qconfig.gene_finding:
        if qconfig.prokaryote:
            ########################################################################
            ### GeneMark
            ########################################################################
            from libs import genemark
            genemark.do(contigs_fpaths, qconfig.genes_lengths, output_dirpath + '/predicted_genes')
        else:
            ########################################################################
            ### Glimmer
            ########################################################################
            from libs import glimmer
            glimmer.do(contigs_fpaths, qconfig.genes_lengths, output_dirpath + '/predicted_genes')
    else:
        log.info("")
        notice("Genes are not predicted by default. Use --gene-finding option to enable it.")
        # add_empty_predicted_genes_fields()

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
        log.info('  All pdf files are merged to ' + all_pdf_filename)
        all_pdf.close()

    cleanup(corrected_dirpath)
    log.info('Done.')

    log.info('')
    log.info("Log saved to " + logfile)

    finish_time = print_timestamp("Finished: ")
    log.info("Elapsed time: " + str(finish_time - start_time))

    return 0


def cleanup(corrected_dirpath):
    ## removing correcting input contig files
    if not qconfig.debug:
        shutil.rmtree(corrected_dirpath)


if __name__ == '__main__':
    main(sys.argv[1:])
