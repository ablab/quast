#!/usr/bin/env python

############################################################################
# Copyright (c) 2011-2014 Saint-Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################

import sys
import os
import shutil
import getopt
import re

quast_dirpath = os.path.abspath(os.path.dirname(os.path.realpath(__file__)))
sys.path.append(os.path.join(quast_dirpath, 'libs'))
from libs import qconfig
qconfig.check_python_version()

from libs import qutils, fastaparser
from libs.qutils import assert_file_exists
from libs.html_saver import json_saver

from libs.log import get_logger
logger = get_logger(qconfig.LOGGER_DEFAULT_NAME)
logger.set_up_console_handler()

from site import addsitedir
addsitedir(os.path.join(quast_dirpath, 'libs', 'site_packages'))


def _set_up_output_dir(output_dirpath, json_outputpath,
                       make_latest_symlink, save_json):
    existing_alignments = False

    if output_dirpath:  # 'output dir was specified with -o option'
        if os.path.isdir(output_dirpath):
            existing_alignments = True
    else:  # output dir was not specified, creating our own one
        output_dirpath = os.path.join(os.path.abspath(
            qconfig.default_results_root_dirname), qconfig.output_dirname)

        # in case of starting two instances of QUAST in the same second
        if os.path.isdir(output_dirpath):
            if make_latest_symlink:
                i = 2
                base_dirpath = output_dirpath
                while os.path.isdir(output_dirpath):
                    output_dirpath = str(base_dirpath) + '__' + str(i)
                    i += 1

    if not os.path.isdir(output_dirpath):
        os.makedirs(output_dirpath)

    # 'latest' symlink
    if make_latest_symlink:
        prev_dirpath = os.getcwd()
        os.chdir(qconfig.default_results_root_dirname)

        latest_symlink = 'latest'
        if os.path.islink(latest_symlink):
            os.remove(latest_symlink)
        os.symlink(output_dirpath, latest_symlink)

        os.chdir(prev_dirpath)

    # Json directory
    if save_json:
        if json_outputpath:
            if not os.path.isdir(json_outputpath):
                os.makedirs(json_outputpath)
        else:
            json_outputpath = os.path.join(output_dirpath, qconfig.default_json_dirname)
            if not os.path.isdir(json_outputpath):
                os.makedirs(json_outputpath)

    return output_dirpath, json_outputpath, existing_alignments


def correct_fasta(original_fpath, corrected_fpath, min_contig,
                  is_reference=False):
    modified_fasta_entries = []
    for first_line, seq in fastaparser.read_fasta(original_fpath):
        if (len(seq) >= min_contig) or is_reference:
            corr_name = qutils.correct_name(first_line)

            # seq to uppercase, because we later looking only uppercase letters
            corr_seq = seq.upper()

            # correcting alternatives (gage can't work with alternatives)
            # dic = {'M': 'A', 'K': 'G', 'R': 'A', 'Y': 'C', 'W': 'A', 'S': 'C', 'V': 'A', 'B': 'C', 'H': 'A', 'D': 'A'}
            dic = {'M': 'N', 'K': 'N', 'R': 'N', 'Y': 'N', 'W': 'N', 'S': 'N', 'V': 'N', 'B': 'N', 'H': 'N', 'D': 'N'}
            pat = "(%s)" % "|".join(map(re.escape, dic.keys()))
            corr_seq = re.sub(pat, lambda m: dic[m.group()], corr_seq)

            # make sure that only A, C, G, T or N are in the sequence
            if re.compile(r'[^ACGTN]').search(corr_seq):
                logger.warning('Skipping ' + original_fpath + ' because it contains non-ACGTN characters.',
                        indent='    ')
                return False

            modified_fasta_entries.append((corr_name, corr_seq))

    fastaparser.write_fasta(corrected_fpath, modified_fasta_entries)

    if is_reference:
        ref_len = sum(len(chr_seq) for (chr_name, chr_seq) in modified_fasta_entries)
        if ref_len > qconfig.MAX_REFERENCE_LENGTH:
            _, fasta_ext = os.path.splitext(corrected_fpath)
            splitted_ref_dirpath = os.path.join(os.path.dirname(corrected_fpath), 'splitted_ref')
            os.makedirs(splitted_ref_dirpath)

            for i, (chr_name, chr_seq) in enumerate(modified_fasta_entries):
                if len(chr_seq) > qconfig.MAX_REFERENCE_LENGTH:
                    logger.warning("Skipping chromosome " + chr_name + " because it length is greater than " +
                            str(qconfig.MAX_REFERENCE_LENGTH) + " (Nucmer's constraint).")
                    continue

                splitted_ref_fpath = os.path.join(splitted_ref_dirpath, "chr_" + str(i + 1)) + fasta_ext
                qconfig.splitted_ref.append(splitted_ref_fpath)
                fastaparser.write_fasta(splitted_ref_fpath, [(chr_name, chr_seq)])

            if len(qconfig.splitted_ref) == 0:
                logger.warning("Skipping reference because all of its chromosomes exceeded Nucmer's constraint.")
                return False
    return True


# Correcting fasta and reporting stats
def _handle_fasta(contigs_fpath, corr_fpath, reporting):
    lengths = fastaparser.get_lengths_from_fastafile(contigs_fpath)

    if not sum(l for l in lengths if l >= qconfig.min_contig):
        logger.warning("Skipping %s because it doesn't contain contigs >= %d bp."
                % (qutils.label_from_fpath(corr_fpath), qconfig.min_contig))
        return False

    # correcting
    if not correct_fasta(contigs_fpath, corr_fpath, qconfig.min_contig):
        return False

    ## filling column "Assembly" with names of assemblies
    report = reporting.get(corr_fpath)

    ## filling columns "Number of contigs >=110 bp", ">=200 bp", ">=500 bp"
    report.add_field(reporting.Fields.CONTIGS__FOR_THRESHOLDS,
                     [sum(1 for l in lengths if l >= threshold)
                      for threshold in qconfig.contig_thresholds])
    report.add_field(reporting.Fields.TOTALLENS__FOR_THRESHOLDS,
                     [sum(l for l in lengths if l >= threshold)
                      for threshold in qconfig.contig_thresholds])
    return True


def _correct_contigs(contigs_fpaths, corrected_dirpath, reporting, labels):
    ## removing from contigs' names special characters because:
    ## 1) Some embedded tools can fail on some strings with "...", "+", "-", etc
    ## 2) Nucmer fails on names like "contig 1_bla_bla", "contig 2_bla_bla" (it interprets as a contig's name only the first word of caption and gets ambiguous contigs names)
    corrected_contigs_fpaths = []

    for i, contigs_fpath in enumerate(contigs_fpaths):
        contigs_fname = os.path.basename(contigs_fpath)
        fname, fasta_ext = qutils.splitext_for_fasta_file(contigs_fname)

        label = labels[i]
        corr_fpath = qutils.unique_corrected_fpath(os.path.join(corrected_dirpath, label + fasta_ext))
        qconfig.assembly_labels_by_fpath[corr_fpath] = label
        logger.info('  %s ==> %s' % (contigs_fpath, label))

        # if option --scaffolds is specified QUAST adds splitted version of assemblies to the comparison
        if qconfig.scaffolds:
            logger.info("  breaking scaffolds into contigs:")
            corr_fpath_wo_ext = os.path.join(corrected_dirpath, qutils.name_from_fpath(corr_fpath))
            broken_scaffolds_fpath = corr_fpath_wo_ext + '_broken' + fasta_ext
            broken_scaffolds_fasta = []
            contigs_counter = 0

            for i, (name, seq) in enumerate(fastaparser.read_fasta(contigs_fpath)):
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
                        broken_scaffolds_fasta.append(
                            (name.split()[0] + "_" +
                             str(cur_contig_number),
                             seq[cur_contig_start:start]))
                        cur_contig_number += 1
                        cur_contig_start = end

                broken_scaffolds_fasta.append(
                    (name.split()[0] + "_" +
                     str(cur_contig_number),
                     seq[cur_contig_start:]))

                contigs_counter += cur_contig_number

            fastaparser.write_fasta(broken_scaffolds_fpath, broken_scaffolds_fasta)
            qconfig.assembly_labels_by_fpath[broken_scaffolds_fpath] = label + ' broken'
            logger.info("      %d scaffolds (%s) were broken into %d contigs (%s)" %
                        (i + 1,
                         qutils.name_from_fpath(corr_fpath),
                         contigs_counter,
                         qutils.name_from_fpath(broken_scaffolds_fpath)))

            if _handle_fasta(broken_scaffolds_fpath, broken_scaffolds_fpath, reporting):
                corrected_contigs_fpaths.append(broken_scaffolds_fpath)
                qconfig.list_of_broken_scaffolds.append(qutils.name_from_fpath(broken_scaffolds_fpath))

        if _handle_fasta(contigs_fpath, corr_fpath, reporting):
            corrected_contigs_fpaths.append(corr_fpath)

    return corrected_contigs_fpaths


def _correct_reference(ref_fpath, corrected_dirpath):
    ref_fname = os.path.basename(ref_fpath)
    name, fasta_ext = qutils.splitext_for_fasta_file(ref_fname)
    corr_fpath = qutils.unique_corrected_fpath(
        os.path.join(corrected_dirpath, name + fasta_ext))

    if not correct_fasta(ref_fpath, corr_fpath, qconfig.min_contig, is_reference=True):
        ref_fpath = ''
    else:
        logger.info('  %s ==> %s' % (ref_fpath, qutils.name_from_fpath(corr_fpath)))
        ref_fpath = corr_fpath

    return ref_fpath


def parse_labels(line, contigs_fpaths):
    def remove_quotes(line):
        if line:
            if line[0] == '"':
                line = line[1:]
            if line[-1] == '"':
                line = line[:-1]
            return line

    # '"Assembly 1, "Assembly 2",Assembly3"'
    labels = remove_quotes(line).split(',')
    # 'Assembly 1 '
    # '"Assembly 2"'
    # 'Assembly3'
    labels = [label.strip() for label in labels]

    if len(labels) != len(contigs_fpaths):
        logger.error('Number of labels does not match the number of files with contigs', 11, to_stderr=True)
        return []
    else:
        for i, label in enumerate(labels):
            labels[i] = remove_quotes(label.strip())
        return labels


def get_label_from_par_dir(contigs_fpath):
    label = os.path.basename(os.path.dirname(os.path.abspath(contigs_fpath)))
    return label


def get_label_from_par_dir_and_fname(contigs_fpath):
    abspath = os.path.abspath(contigs_fpath)
    name = qutils.rm_extentions_for_fasta_file(os.path.basename(contigs_fpath))
    label = os.path.basename(os.path.dirname(abspath)) + '_' + name
    return label


def get_duplicated(labels):
    # check duplicates
    occurences = {}
    for label in labels:
        if label in occurences:
            occurences[label] += 1
        else:
            occurences[label] = 1

    dupls = [dup_label for dup_label, occurs_num in occurences.items() if occurs_num > 1]
    return dupls


def get_labels_from_par_dirs(contigs_fpaths):
    labels = []
    for fpath in contigs_fpaths:
        labels.append(get_label_from_par_dir(fpath))

    for duplicated_label in get_duplicated(labels):
        for i, (label, fpath) in enumerate(zip(labels, contigs_fpaths)):
            if label == duplicated_label:
                labels[i] = get_label_from_par_dir_and_fname(fpath)

    return labels


def process_labels(contigs_fpaths, labels, all_labels_from_dirs):
    # 1. labels if the provided by -l options
    if labels:
        # process duplicates, empties
        for i, label in enumerate(labels):
            if not label:
                labels[i] = get_label_from_par_dir_and_fname(contigs_fpaths[i])

    # 2. labels from parent directories if -L flag was privided
    elif all_labels_from_dirs:
        labels = get_labels_from_par_dirs(contigs_fpaths)

    # 3. otherwise, labels from fnames
    else:
        # labels from fname
        labels = [qutils.rm_extentions_for_fasta_file(os.path.basename(fpath)) for fpath in contigs_fpaths]

        for duplicated_label in get_duplicated(labels):
            for i, (label, fpath) in enumerate(zip(labels, contigs_fpaths)):
                if label == duplicated_label:
                    labels[i] = get_label_from_par_dir_and_fname(contigs_fpaths[i])

    # fixing remaining duplicates by adding index
    for duplicated_label in get_duplicated(labels):
        j = 0
        for i, (label, fpath) in enumerate(zip(labels, contigs_fpaths)):
            if label == duplicated_label:
                if j == 0:
                    labels[i] = label
                else:
                    labels[i] = label + ' ' + str(j)
                j += 1

    return labels


def main(args):
    if ' ' in quast_dirpath:
        logger.error('QUAST does not support spaces in paths. \n'
                     'You are trying to run it from ' + str(quast_dirpath) + '\n'
                     'Please, put QUAST in a different directory, then try again.\n',
                     to_stderr=True,
                     exit_with_code=3)

    if not args:
        qconfig.usage()
        sys.exit(0)

    reload(qconfig)

    try:
        options, contigs_fpaths = getopt.gnu_getopt(args, qconfig.short_options, qconfig.long_options)
    except getopt.GetoptError:
        _, exc_value, _ = sys.exc_info()
        print >> sys.stderr, exc_value
        print >> sys.stderr
        qconfig.usage()
        sys.exit(2)

    for opt, arg in options[:]:
        if opt in ('-d', '--debug'):
            options.remove((opt, arg))
            qconfig.debug = True
            logger.set_up_console_handler(debug=True)

        if opt == '--test':
            options.remove((opt, arg))
            options += [('-o', 'quast_test_output'),
                        ('-R', 'test_data/reference.fasta.gz'),   # for compiling MUMmer
                        ('-O', 'test_data/operons.gff'),
                        ('-G', 'test_data/genes.gff'),
                        ('--gene-finding',''), ('--eukaryote','')] # for compiling GlimmerHMM
            contigs_fpaths += ['test_data/contigs_1.fasta',
                               'test_data/contigs_2.fasta']
            qconfig.test = True

        if opt.startswith('--help'):
            qconfig.usage(opt == "--help-hidden")
            sys.exit(0)

    if not contigs_fpaths:
        logger.error("You should specify at least one file with contigs!\n")
        qconfig.usage()
        sys.exit(2)

    json_output_dirpath = None
    output_dirpath = None

    labels = None
    all_labels_from_dirs = False

    ref_fpath = ''
    genes_fpaths = []
    operons_fpaths = []

    # Yes, this is a code duplicating. But OptionParser is deprecated since version 2.7.
    for opt, arg in options:
        if opt in ('-o', "--output-dir"):
            output_dirpath = os.path.abspath(arg)
            qconfig.make_latest_symlink = False

        elif opt in ('-G', "--genes"):
            genes_fpaths.append(assert_file_exists(arg, 'genes'))

        elif opt in ('-O', "--operons"):
            operons_fpaths.append(assert_file_exists(arg, 'operons'))

        elif opt in ('-R', "--reference"):
            ref_fpath = assert_file_exists(arg, 'reference')

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
            json_output_dirpath = arg

        elif opt in ('-s', "--scaffolds"):
            qconfig.scaffolds = True

        elif opt == "--gage":
            qconfig.with_gage = True

        elif opt in ('-e', "--eukaryote"):
            qconfig.prokaryote = False

        elif opt in ('-f', "--gene-finding"):
            qconfig.gene_finding = True

        elif opt in ('-a', "--ambiguity-usage"):
            if arg in ["none", "one", "all"]:
                qconfig.ambiguity_usage = arg

        elif opt in ('-u', "--use-all-alignments"):
            qconfig.use_all_alignments = True

        elif opt in ('-n', "--strict-NA"):
            qconfig.strict_NA = True

        elif opt == '--no-plots':
            qconfig.draw_plots = False

        elif opt == '--no-html':
            qconfig.html_report = False

        elif opt in ('-m', '--meta'):
            qconfig.meta = True

        elif opt in ('-l', '--labels'):
            labels = parse_labels(arg, contigs_fpaths)

        elif opt == '-L':
            all_labels_from_dirs = True
        else:
            logger.error('Unknown option: %s. Use -h for help.' % (opt + ' ' + arg), to_stderr=True, exit_with_code=2)

    for contigs_fpath in contigs_fpaths:
        assert_file_exists(contigs_fpath, 'contigs')

    labels = process_labels(contigs_fpaths, labels, all_labels_from_dirs)

    output_dirpath, json_output_dirpath, existing_alignments = \
        _set_up_output_dir(output_dirpath, json_output_dirpath, qconfig.make_latest_symlink, qconfig.save_json)

    corrected_dirpath = os.path.join(output_dirpath, qconfig.corrected_dirname)

    logger.set_up_file_handler(output_dirpath)
    logger.print_command_line([os.path.realpath(__file__)] + args, wrap_after=None)
    logger.start()

    if existing_alignments:
        logger.info()
        logger.notice("Output directory already exists. Existing Nucmer alignments can be used.")
        qutils.remove_reports(output_dirpath)

    if qconfig.contig_thresholds == "None":
        qconfig.contig_thresholds = []
    else:
        qconfig.contig_thresholds = map(int, qconfig.contig_thresholds.split(","))
    if qconfig.genes_lengths == "None":
        qconfig.genes_lengths = []
    else:
        qconfig.genes_lengths = map(int, qconfig.genes_lengths.split(","))

    # Threading
    if qconfig.max_threads is None:
        try:
            import multiprocessing
            qconfig.max_threads = multiprocessing.cpu_count()
        except:
            logger.warning('Failed to determine the number of CPUs')
            qconfig.max_threads = qconfig.DEFAULT_MAX_THREADS

        logger.info()
        logger.notice('Maximum number of threads is set to ' + str(qconfig.max_threads) + ' (use --threads option to set it manually)')


    ########################################################################
    from libs import reporting
    reload(reporting)

    if os.path.isdir(corrected_dirpath):
        shutil.rmtree(corrected_dirpath)
    os.mkdir(corrected_dirpath)

    # PROCESSING REFERENCE
    if ref_fpath:
        logger.info()
        logger.info('Reference:')
        ref_fpath = _correct_reference(ref_fpath, corrected_dirpath)
    else:
        ref_fpath = ''

    # PROCESSING CONTIGS
    logger.info()
    logger.info('Contigs:')
    contigs_fpaths = _correct_contigs(contigs_fpaths, corrected_dirpath, reporting, labels)
    for contigs_fpath in contigs_fpaths:
        report = reporting.get(contigs_fpath)
        report.add_field(reporting.Fields.NAME, qutils.label_from_fpath(contigs_fpath))

    qconfig.assemblies_num = len(contigs_fpaths)

    if not contigs_fpaths:
        logger.error("None of the assembly files contains correct contigs. "
              "Please, provide different files or decrease --min-contig threshold.",
              fake_if_nested_run=True)
        return 4

    if qconfig.with_gage:
        ########################################################################
        ### GAGE
        ########################################################################
        if not ref_fpath:
            logger.warning("GAGE can't be run without a reference and will be skipped.")
        else:
            from libs import gage
            gage.do(ref_fpath, contigs_fpaths, output_dirpath)

    # Where all pdfs will be saved
    all_pdf_fpath = os.path.join(output_dirpath, qconfig.plots_fname)
    all_pdf_file = None

    if qconfig.draw_plots:
        from libs import plotter  # Do not remove this line! It would lead to a warning in matplotlib.
        try:
            from matplotlib.backends.backend_pdf import PdfPages
            all_pdf_file = PdfPages(all_pdf_fpath)
        except:
            all_pdf_file = None

    ########################################################################
    ### Stats and plots
    ########################################################################
    from libs import basic_stats
    basic_stats.do(ref_fpath, contigs_fpaths, os.path.join(output_dirpath, 'basic_stats'),
                   json_output_dirpath, output_dirpath)

    aligned_contigs_fpaths = []
    aligned_lengths_lists = []
    contig_alignment_plot_fpath = None
    if ref_fpath:
        ########################################################################
        ### former PLANTAKOLYA, PLANTAGORA
        ########################################################################
        from libs import contigs_analyzer
        nucmer_statuses, aligned_lengths_per_fpath = contigs_analyzer.do(
            ref_fpath, contigs_fpaths, qconfig.prokaryote, os.path.join(output_dirpath, 'contigs_reports'))
        for contigs_fpath in contigs_fpaths:
            if nucmer_statuses[contigs_fpath] == contigs_analyzer.NucmerStatus.OK:
                aligned_contigs_fpaths.append(contigs_fpath)
                aligned_lengths_lists.append(aligned_lengths_per_fpath[contigs_fpath])

    # Before continue evaluating, check if nucmer didn't skip all of the contigs files.
    detailed_contigs_reports_dirpath = None
    if len(aligned_contigs_fpaths) and ref_fpath:
        detailed_contigs_reports_dirpath = os.path.join(output_dirpath, 'contigs_reports')

        ########################################################################
        ### NAx and NGAx ("aligned Nx and NGx")
        ########################################################################
        from libs import aligned_stats
        aligned_stats.do(
            ref_fpath, aligned_contigs_fpaths, output_dirpath, json_output_dirpath,
            aligned_lengths_lists, os.path.join(output_dirpath, 'aligned_stats'))

        ########################################################################
        ### GENOME_ANALYZER
        ########################################################################
        from libs import genome_analyzer
        genome_analyzer.do(
            ref_fpath, aligned_contigs_fpaths, output_dirpath, json_output_dirpath,
            genes_fpaths, operons_fpaths, detailed_contigs_reports_dirpath, os.path.join(output_dirpath, 'genome_stats'))

    if qconfig.gene_finding:
        if qconfig.prokaryote or qconfig.meta:
            ########################################################################
            ### GeneMark
            ########################################################################
            from libs import genemark
            genemark.do(contigs_fpaths, qconfig.genes_lengths, os.path.join(output_dirpath, 'predicted_genes'),
                        qconfig.meta)
        else:
            ########################################################################
            ### Glimmer
            ########################################################################
            from libs import glimmer
            glimmer.do(contigs_fpaths, qconfig.genes_lengths, os.path.join(output_dirpath, 'predicted_genes'))
    else:
        logger.info("")
        logger.notice("Genes are not predicted by default. Use --gene-finding option to enable it.")
    ########################################################################
    reports_fpaths, transposed_reports_fpaths = reporting.save_total(output_dirpath)

    ########################################################################
    ### LARGE DRAWING TASKS
    ########################################################################
    if qconfig.draw_plots:
        logger.print_timestamp()
        logger.info('Drawing large plots...')
        logger.info('This may take a while: press Ctrl-C to skip this step..')
        try:
            number_of_steps = sum([int(bool(value)) for value in [detailed_contigs_reports_dirpath, all_pdf_file]])
            if detailed_contigs_reports_dirpath:
                ########################################################################
                ### VISUALIZE CONTIG ALIGNMENT
                ########################################################################
                logger.info('  1 of %d: Creating contig alignment plot...' % number_of_steps)
                from libs import contig_alignment_plotter
                contig_alignment_plot_fpath = contig_alignment_plotter.do(
                    contigs_fpaths, os.path.join(detailed_contigs_reports_dirpath, 'contigs_report_%s.stdout'),
                    output_dirpath, ref_fpath, similar=True)

            if all_pdf_file:
                # full report in PDF format: all tables and plots
                logger.info('  %d of %d: Creating PDF with all tables and plots...' % (number_of_steps, number_of_steps))
                plotter.fill_all_pdf_file(all_pdf_file)
            logger.info('Done')
        except KeyboardInterrupt:
            logger.info('..step skipped!')
            os.remove(all_pdf_fpath)

    ########################################################################
    ### TOTAL REPORT
    ########################################################################
    logger.print_timestamp()
    logger.info('RESULTS:')
    logger.info('  Text versions of total report are saved to ' + reports_fpaths)
    logger.info('  Text versions of transposed total report are saved to ' + transposed_reports_fpaths)

    if json_output_dirpath:
        json_saver.save_total_report(json_output_dirpath, qconfig.min_contig)

    if qconfig.html_report:
        from libs.html_saver import html_saver
        html_saver.save_total_report(output_dirpath, qconfig.min_contig)

    if os.path.isfile(all_pdf_fpath):
        logger.info('  PDF version (tables and plots) saved to ' + all_pdf_fpath)

    if contig_alignment_plot_fpath:
        logger.info('  Contig alignment plot: %s' % contig_alignment_plot_fpath)

    _cleanup(corrected_dirpath)
    logger.finish_up(check_test=qconfig.test)
    return 0


def _cleanup(corrected_dirpath):
    # removing correcting input contig files
    if not qconfig.debug:
        shutil.rmtree(corrected_dirpath)


if __name__ == '__main__':
    try:
        return_code = main(sys.argv[1:])
        exit(return_code)
    except Exception:
        _, exc_value, _ = sys.exc_info()
        logger.exception(exc_value)
        logger.error('exception caught!', exit_with_code=1)

