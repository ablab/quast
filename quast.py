#!/usr/bin/env python

############################################################################
# Copyright (c) 2015-2016 Saint Petersburg State University
# Copyright (c) 2011-2015 Saint Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################

from os.path import basename
import sys
import os
import shutil
import getopt
import re

from libs import qconfig
qconfig.check_python_version()

from libs import qutils, reads_analyzer
from libs.qutils import assert_file_exists, set_up_output_dir, cleanup

from libs.log import get_logger
logger = get_logger(qconfig.LOGGER_DEFAULT_NAME)
logger.set_up_console_handler()

from site import addsitedir
addsitedir(os.path.join(qconfig.LIBS_LOCATION, 'site_packages'))
is_combined_ref = False


def main(args):
    if ' ' in qconfig.QUAST_HOME:
        logger.error('QUAST does not support spaces in paths. \n'
                     'You are trying to run it from ' + str(qconfig.QUAST_HOME) + '\n'
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

        if opt == '--test' or opt == '--test-sv':
            options.remove((opt, arg))
            options += [('-o', 'quast_test_output'),
                        ('-R', os.path.join(qconfig.QUAST_HOME, 'test_data', 'reference.fasta.gz')),  # for compiling MUMmer
                        ('-O', os.path.join(qconfig.QUAST_HOME, 'test_data', 'operons.gff')),
                        ('-G', os.path.join(qconfig.QUAST_HOME, 'test_data', 'genes.gff')),
                        ('--gage', ''),  # for compiling GAGE Java classes
                        ('--gene-finding', ''), ('--eukaryote', ''), ('--glimmer', '')]  # for compiling GlimmerHMM
            if opt == '--test-sv':
                options += [('-1', os.path.join(qconfig.QUAST_HOME, 'test_data', 'reads1.fastq.gz')),
                            ('-2', os.path.join(qconfig.QUAST_HOME, 'test_data', 'reads2.fastq.gz'))]
            contigs_fpaths += [os.path.join(qconfig.QUAST_HOME, 'test_data', 'contigs_1.fasta'),
                               os.path.join(qconfig.QUAST_HOME, 'test_data', 'contigs_2.fasta')]
            qconfig.test = True

        if opt.startswith('--help') or opt == '-h':
            qconfig.usage(opt == "--help-hidden", short=False)
            sys.exit(0)

        elif opt.startswith('--version') or opt == '-v':
            qconfig.print_version()
            sys.exit(0)

    if not contigs_fpaths:
        logger.error("You should specify at least one file with contigs!\n")
        qconfig.usage()
        sys.exit(2)

    json_output_dirpath = None
    output_dirpath = None

    labels = None
    all_labels_from_dirs = False
    qconfig.is_combined_ref = False

    ref_fpath = ''
    genes_fpaths = []
    operons_fpaths = []
    bed_fpath = None
    cov_fpath = None
    physical_cov_fpath = None
    reads_fpath_f = ''
    reads_fpath_r = ''
    default_ambiguity_score = True

    # Yes, this is a code duplicating. But OptionParser is deprecated since version 2.7.
    for opt, arg in options:
        if opt in ('-d', '--debug'):
            qconfig.debug = True
            logger.set_up_console_handler(debug=True)

        elif opt in ('-o', "--output-dir"):
            output_dirpath = os.path.abspath(arg)
            qconfig.make_latest_symlink = False
            if ' ' in output_dirpath:
                logger.error('QUAST does not support spaces in paths. \n'
                             'You have specified ' + str(output_dirpath) + ' as an output path.\n'
                             'Please, use a different directory.\n',
                             to_stderr=True,
                             exit_with_code=3)

        elif opt in ('-G', "--genes"):
            genes_fpaths.append(assert_file_exists(arg, 'genes'))

        elif opt in ('-O', "--operons"):
            operons_fpaths.append(assert_file_exists(arg, 'operons'))

        elif opt in ('-R', "--reference"):
            ref_fpath = assert_file_exists(arg, 'reference')

        elif opt == "--contig-thresholds":
            qconfig.contig_thresholds = arg

        elif opt in ('-m', "--min-contig"):
            qconfig.min_contig = int(arg)

        elif opt in ('-t', "--threads"):
            qconfig.max_threads = int(arg)
            if qconfig.max_threads < 1:
                qconfig.max_threads = 1

        elif opt in ('-c', "--min-cluster"):
            qconfig.min_cluster = int(arg)

        elif opt in ('-i', "--min-alignment"):
            qconfig.min_alignment = int(arg)

        elif opt == "--est-ref-size":
            qconfig.estimated_reference_size = int(arg)

        elif opt == "--gene-thresholds":
            qconfig.genes_lengths = arg

        elif opt in ('-j', '--save-json'):
            qconfig.save_json = True

        elif opt in ('-J', '--save-json-to'):
            qconfig.save_json = True
            qconfig.make_latest_symlink = False
            json_output_dirpath = arg

        elif opt == '--err-fpath':  # for web-quast
            qconfig.save_error = True
            qconfig.error_log_fpath = arg

        elif opt in ('-s', "--scaffolds"):
            qconfig.scaffolds = True

        elif opt == "--gage":
            qconfig.with_gage = True

        elif opt in ('-e', "--eukaryote"):
            qconfig.prokaryote = False

        elif opt in ('-f', "--gene-finding"):
            qconfig.gene_finding = True

        elif opt == "--fragmented":
            qconfig.check_for_fragmented_ref = True

        elif opt in ('-a', "--ambiguity-usage"):
            if arg in ["none", "one", "all"]:
                qconfig.ambiguity_usage = arg
            else:
                logger.error("incorrect value for --ambiguity-usage (%s)! "
                             "Please specify 'none', 'one', or 'all'." % arg,
                             to_stderr=True, exit_with_code=2)
        elif opt == '--ambiguity-score':
            if qutils.is_float(arg) and 0.0 <= float(arg) <= 1.0:
                qconfig.ambiguity_score = float(arg)
                default_ambiguity_score = False
            else:
                logger.error("incorrect value for --ambiguity-score (%s)! "
                             "Please specify a float number between 0.0 and 1.0." % arg,
                             to_stderr=True, exit_with_code=2)

        elif opt in ('-u', "--use-all-alignments"):
            qconfig.use_all_alignments = True

        elif opt == "--strict-NA":
            qconfig.strict_NA = True

        elif opt in ('-x', "--extensive-mis-size"):
            if int(arg) <= qconfig.MAX_INDEL_LENGTH:
                logger.error("--extensive-mis-size should be greater than maximum indel length (%d)!"
                             % qconfig.MAX_INDEL_LENGTH, 1, to_stderr=True, exit_with_code=2)
            qconfig.extensive_misassembly_threshold = int(arg)
        elif opt == "--significant-part-size":
            qconfig.significant_part_size = int(arg)

        elif opt == '--no-snps':
            qconfig.show_snps = False

        elif opt == '--no-plots':
            qconfig.draw_plots = False

        elif opt == '--no-html':
            qconfig.html_report = False
            qconfig.create_icarus_html = False

        elif opt == '--no-icarus':
            qconfig.create_icarus_html = False

        elif opt == '--no-check':
            qconfig.no_check = True

        elif opt == '--no-gc':
            qconfig.no_gc = True
        elif opt == '--no-sv':
            qconfig.no_sv = True

        elif opt == '--fast':  # --no-gc, --no-plots, --no-snps
            #qconfig.no_check = True  # too risky to include
            qconfig.no_gc = True
            qconfig.no_sv = True
            qconfig.show_snps = False
            qconfig.draw_plots = False
            qconfig.html_report = False
            qconfig.create_icarus_html = False

        elif opt == '--svg':
            qconfig.draw_svg = True

        elif opt == '--plots-format':
            if arg.lower() in qconfig.supported_plot_extensions:
                qconfig.plot_extension = arg.lower()
            else:
                logger.error('Format "%s" is not supported. Please, use one of the supported formats: %s.' %
                             (arg, ', '.join(qconfig.supported_plot_extensions)), to_stderr=True, exit_with_code=2)

        elif opt == '--meta':
            qconfig.meta = True

        elif opt == '--no-check-meta':
            qconfig.no_check = True
            qconfig.no_check_meta = True

        elif opt == '--references-list':
            pass

        elif opt in ('-l', '--labels'):
            labels = qutils.parse_labels(arg, contigs_fpaths)

        elif opt == '-L':
            all_labels_from_dirs = True

        elif opt == '--glimmer':
            qconfig.glimmer = True

        elif opt == '--combined-ref':
            qconfig.is_combined_ref = True

        elif opt == '--colors':
            qconfig.used_colors = arg.split(',')
        elif opt == '--ls':
            qconfig.used_ls = arg.split(',')

        elif opt == '--memory-efficient':
            qconfig.memory_efficient = True

        elif opt == '--silent':
            qconfig.silent = True

        elif opt in ('-1', '--reads1'):
            reads_fpath_f = arg
        elif opt in ('-2', '--reads2'):
            reads_fpath_r = arg
        elif opt == '--sv-bedpe':
            bed_fpath = assert_file_exists(arg, 'BEDPE file with structural variations')

        else:
            logger.error('Unknown option: %s. Use -h for help.' % (opt + ' ' + arg), to_stderr=True, exit_with_code=2)

    for contigs_fpath in contigs_fpaths:
        assert_file_exists(contigs_fpath, 'contigs')

    labels = qutils.process_labels(contigs_fpaths, labels, all_labels_from_dirs)

    output_dirpath, json_output_dirpath, existing_alignments = \
        set_up_output_dir(output_dirpath, json_output_dirpath, qconfig.make_latest_symlink, qconfig.save_json)

    corrected_dirpath = os.path.join(output_dirpath, qconfig.corrected_dirname)

    logger.set_up_file_handler(output_dirpath, qconfig.error_log_fpath if qconfig.save_error else None)
    args = [os.path.realpath(__file__)]
    for k, v in options: args.extend([k, v])
    args.extend(contigs_fpaths)
    logger.print_command_line(args, wrap_after=None, is_main=True)
    logger.start()

    if existing_alignments:
        logger.notice("Output directory already exists. Existing Nucmer alignments can be used")
        qutils.remove_reports(output_dirpath)

    if qconfig.contig_thresholds == "None":
        qconfig.contig_thresholds = []
    else:
        qconfig.contig_thresholds = map(int, qconfig.contig_thresholds.split(","))
    if qconfig.genes_lengths == "None":
        qconfig.genes_lengths = []
    else:
        qconfig.genes_lengths = map(int, qconfig.genes_lengths.split(","))

    qconfig.set_max_threads(logger)

    if not default_ambiguity_score:
        if qconfig.ambiguity_usage != 'all':
            qconfig.ambiguity_usage = 'all'
            logger.notice("--ambiguity-usage was set to 'all' because not default --ambiguity-score was specified")

    logger.main_info()
    logger.print_params()

    ########################################################################
    from libs import reporting
    reload(reporting)
    from libs import plotter  # Do not remove this line! It would lead to a warning in matplotlib.

    if qconfig.is_combined_ref:
        corrected_dirpath = os.path.join(output_dirpath, '..', qconfig.corrected_dirname)
    else:
        if os.path.isdir(corrected_dirpath):
            shutil.rmtree(corrected_dirpath)
        os.mkdir(corrected_dirpath)

    # PROCESSING REFERENCE
    if ref_fpath:
        logger.main_info()
        logger.main_info('Reference:')
        ref_fpath = qutils.correct_reference(ref_fpath, corrected_dirpath)
    else:
        ref_fpath = ''

    # PROCESSING CONTIGS
    logger.main_info()
    logger.main_info('Contigs:')

    contigs_fpaths, old_contigs_fpaths = qutils.correct_contigs(contigs_fpaths, corrected_dirpath, reporting, labels)
    for contigs_fpath in contigs_fpaths:
        report = reporting.get(contigs_fpath)
        report.add_field(reporting.Fields.NAME, qutils.label_from_fpath(contigs_fpath))

    qconfig.assemblies_num = len(contigs_fpaths)

    reads_fpaths = []
    if reads_fpath_f:
        reads_fpaths.append(reads_fpath_f)
    if reads_fpath_r:
        reads_fpaths.append(reads_fpath_r)
    if reads_fpaths and ref_fpath:
        bed_fpath, cov_fpath, physical_cov_fpath = reads_analyzer.do(ref_fpath, contigs_fpaths, reads_fpaths, None,
                                      os.path.join(output_dirpath, qconfig.variation_dirname),
                                      external_logger=logger, bed_fpath=bed_fpath)

    if not contigs_fpaths:
        logger.error("None of the assembly files contains correct contigs. "
              "Please, provide different files or decrease --min-contig threshold.",
              fake_if_nested_run=True)
        return 4

    if qconfig.used_colors and qconfig.used_ls:
        for i, label in enumerate(labels):
            plotter.dict_color_and_ls[label] = (qconfig.used_colors[i], qconfig.used_ls[i])

    qconfig.assemblies_fpaths = contigs_fpaths
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
        try:
            from matplotlib.backends.backend_pdf import PdfPages
            all_pdf_file = PdfPages(all_pdf_fpath)
        except:
            all_pdf_file = None

    if json_output_dirpath:
        from libs.html_saver import json_saver
        if json_saver.simplejson_error:
            json_output_dirpath = None


    ########################################################################
    ### Stats and plots
    ########################################################################
    from libs import basic_stats
    basic_stats.do(ref_fpath, contigs_fpaths, os.path.join(output_dirpath, 'basic_stats'),
                   json_output_dirpath, output_dirpath)

    aligned_contigs_fpaths = []
    aligned_lengths_lists = []
    contig_alignment_plot_fpath = None
    icarus_html_fpath = None
    if ref_fpath:
        ########################################################################
        ### former PLANTAKOLYA, PLANTAGORA
        ########################################################################
        from libs import contigs_analyzer
        nucmer_statuses, aligned_lengths_per_fpath = contigs_analyzer.do(
            ref_fpath, contigs_fpaths, qconfig.prokaryote, os.path.join(output_dirpath, 'contigs_reports'), old_contigs_fpaths, bed_fpath)
        for contigs_fpath in contigs_fpaths:
            if nucmer_statuses[contigs_fpath] == contigs_analyzer.NucmerStatus.OK:
                aligned_contigs_fpaths.append(contigs_fpath)
                aligned_lengths_lists.append(aligned_lengths_per_fpath[contigs_fpath])

    # Before continue evaluating, check if nucmer didn't skip all of the contigs files.
    detailed_contigs_reports_dirpath = None
    features_containers = None
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
        features_containers = genome_analyzer.do(
            ref_fpath, aligned_contigs_fpaths, output_dirpath, json_output_dirpath,
            genes_fpaths, operons_fpaths, detailed_contigs_reports_dirpath, os.path.join(output_dirpath, 'genome_stats'))

    if qconfig.gene_finding or qconfig.glimmer:
        if qconfig.glimmer:
            ########################################################################
            ### Glimmer
            ########################################################################
            from libs import glimmer
            glimmer.do(contigs_fpaths, qconfig.genes_lengths, os.path.join(output_dirpath, 'predicted_genes'))
        else:
            ########################################################################
            ### GeneMark
            ########################################################################
            from libs import genemark
            genemark.do(contigs_fpaths, qconfig.genes_lengths, os.path.join(output_dirpath, 'predicted_genes'), qconfig.prokaryote,
                        qconfig.meta)

    else:
        logger.main_info("")
        logger.notice("Genes are not predicted by default. Use --gene-finding option to enable it.")
    ########################################################################
    reports_fpaths, transposed_reports_fpaths = reporting.save_total(output_dirpath)

    ########################################################################
    ### LARGE DRAWING TASKS
    ########################################################################
    if qconfig.draw_plots or qconfig.create_icarus_html:
        logger.print_timestamp()
        logger.main_info('Creating large visual summaries...')
        logger.main_info('This may take a while: press Ctrl-C to skip this step..')
        try:
            if detailed_contigs_reports_dirpath:
                report_for_icarus_fpath_pattern = os.path.join(detailed_contigs_reports_dirpath, qconfig.icarus_report_fname_pattern)
                stdout_pattern = os.path.join(detailed_contigs_reports_dirpath, qconfig.contig_report_fname_pattern)
            else:
                report_for_icarus_fpath_pattern = None
                stdout_pattern = None
            draw_alignment_plots = qconfig.draw_svg or qconfig.create_icarus_html
            number_of_steps = sum([int(bool(value)) for value in [draw_alignment_plots, all_pdf_file]])
            if draw_alignment_plots:
                ########################################################################
                ### VISUALIZE CONTIG ALIGNMENT
                ########################################################################
                logger.main_info('  1 of %d: Creating Icarus viewers...' % number_of_steps)
                from libs import icarus
                icarus_html_fpath, contig_alignment_plot_fpath = icarus.do(
                    contigs_fpaths, report_for_icarus_fpath_pattern, output_dirpath, ref_fpath,
                    stdout_pattern=stdout_pattern, features=features_containers, cov_fpath=cov_fpath,
                    physical_cov_fpath=physical_cov_fpath, json_output_dir=json_output_dirpath)

            if all_pdf_file:
                # full report in PDF format: all tables and plots
                logger.main_info('  %d of %d: Creating PDF with all tables and plots...' % (number_of_steps, number_of_steps))
                plotter.fill_all_pdf_file(all_pdf_file)
            logger.main_info('Done')
        except KeyboardInterrupt:
            logger.main_info('..step skipped!')
            os.remove(all_pdf_fpath)

    ########################################################################
    ### TOTAL REPORT
    ########################################################################
    logger.print_timestamp()
    logger.main_info('RESULTS:')
    logger.main_info('  Text versions of total report are saved to ' + reports_fpaths)
    logger.main_info('  Text versions of transposed total report are saved to ' + transposed_reports_fpaths)

    if json_output_dirpath:
        json_saver.save_total_report(json_output_dirpath, qconfig.min_contig, ref_fpath)

    if qconfig.html_report:
        from libs.html_saver import html_saver
        html_saver.save_colors(output_dirpath, contigs_fpaths, plotter.dict_color_and_ls)
        html_saver.save_total_report(output_dirpath, qconfig.min_contig, ref_fpath)

    if os.path.isfile(all_pdf_fpath):
        logger.main_info('  PDF version (tables and plots) is saved to ' + all_pdf_fpath)

    if icarus_html_fpath:
        logger.main_info('  Icarus (contig browser) is saved to %s' % icarus_html_fpath)

    if qconfig.draw_svg and contig_alignment_plot_fpath:
        logger.main_info('  Contig alignment plot is saved to %s' % contig_alignment_plot_fpath)

    cleanup(corrected_dirpath)
    return logger.finish_up(check_test=qconfig.test)


if __name__ == '__main__':
    try:
        return_code = main(sys.argv[1:])
        exit(return_code)
    except Exception:
        _, exc_value, _ = sys.exc_info()
        logger.exception(exc_value)
        logger.error('exception caught!', exit_with_code=1, to_stderr=True)
