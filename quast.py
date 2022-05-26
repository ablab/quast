#!/usr/bin/env python

############################################################################
# Copyright (c) 2015-2022 Saint Petersburg State University
# Copyright (c) 2011-2015 Saint Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################

import locale
import os
import sys
import shutil

from quast_libs import qconfig

qconfig.check_python_version()

from site import addsitedir
addsitedir(os.path.join(qconfig.LIBS_LOCATION, 'site_packages'))
from quast_libs import qutils, run_barrnap, plotter_data, unique_kmers
from quast_libs.qutils import cleanup, check_dirpath, check_reads_fpaths
from quast_libs.options_parser import parse_options

from quast_libs.log import get_logger
logger = get_logger(qconfig.LOGGER_DEFAULT_NAME)
logger.set_up_console_handler()

is_combined_ref = False


def main(args):
    check_dirpath(qconfig.QUAST_HOME, 'You are trying to run it from ' + str(qconfig.QUAST_HOME) + '\n.' +
                  'Please, put QUAST in a different directory, then try again.\n', exit_code=3)

    if not args:
        qconfig.usage(stream=sys.stderr)
        sys.exit(1)

    try:
        import importlib
        importlib.reload(qconfig)
        importlib.reload(qutils)
    except (ImportError, AttributeError):
        reload(qconfig)
        reload(qutils)

    try:
        locale.setlocale(locale.LC_ALL, 'en_US.utf8')
    except Exception:
        try:
            locale.setlocale(locale.LC_ALL, 'en_US.UTF-8')
        except Exception:
            logger.warning('Python locale settings can\'t be changed')
    quast_path = [__file__]
    quast_py_args, contigs_fpaths = parse_options(logger, quast_path + args)
    output_dirpath, ref_fpath, labels = qconfig.output_dirpath, qconfig.reference, qconfig.labels
    corrected_dirpath = os.path.join(output_dirpath, qconfig.corrected_dirname)
    logger.main_info()
    logger.print_params()

    ########################################################################
    from quast_libs import reporting
    reports = reporting.reports
    try:
        import importlib
        importlib.reload(reporting)
    except (ImportError, AttributeError):
        reload(reporting)
    reporting.reports = reports
    reporting.assembly_fpaths = []
    from quast_libs import plotter  # Do not remove this line! It would lead to a warning in matplotlib.

    if qconfig.is_combined_ref:
        corrected_dirpath = os.path.join(output_dirpath, '..', qconfig.corrected_dirname)
    else:
        if os.path.isdir(corrected_dirpath):
            shutil.rmtree(corrected_dirpath)
        os.mkdir(corrected_dirpath)

    qconfig.set_max_threads(logger)
    check_reads_fpaths(logger)
    # PROCESSING REFERENCE
    if ref_fpath:
        logger.main_info()
        logger.main_info('Reference:')
        original_ref_fpath = ref_fpath
        ref_fpath = qutils.correct_reference(ref_fpath, corrected_dirpath)
        if qconfig.optimal_assembly:
            if not qconfig.pacbio_reads and not qconfig.nanopore_reads and not qconfig.mate_pairs:
                logger.warning('Upper Bound Assembly cannot be created. It requires mate-pairs or long reads (Pacbio SMRT or Oxford Nanopore).')
            else:
                from quast_libs import optimal_assembly
                optimal_assembly_fpath = optimal_assembly.do(ref_fpath, original_ref_fpath,
                                                             os.path.join(output_dirpath, qconfig.optimal_assembly_basename))
                if optimal_assembly_fpath is not None:
                    contigs_fpaths.insert(0, optimal_assembly_fpath)
                    labels.insert(0, 'UpperBound')
                    labels = qutils.process_labels(contigs_fpaths, labels)
    else:
        ref_fpath = ''

    # PROCESSING CONTIGS
    logger.main_info()
    logger.main_info('Contigs:')

    contigs_fpaths, old_contigs_fpaths = qutils.correct_contigs(contigs_fpaths, corrected_dirpath, labels, reporting)
    for contigs_fpath in contigs_fpaths:
        report = reporting.get(contigs_fpath)
        report.add_field(reporting.Fields.NAME, qutils.label_from_fpath(contigs_fpath))

    qconfig.assemblies_num = len(contigs_fpaths)

    cov_fpath = qconfig.cov_fpath
    physical_cov_fpath = qconfig.phys_cov_fpath
    if qconfig.reads_fpaths or qconfig.reference_sam or qconfig.reference_sam or qconfig.sam_fpaths or qconfig.bam_fpaths:
        from quast_libs import reads_analyzer
        bed_fpath, cov_fpath, physical_cov_fpath = reads_analyzer.do(ref_fpath, contigs_fpaths,
                                                                     os.path.join(output_dirpath, qconfig.reads_stats_dirname),
                                                                     external_logger=logger)
        qconfig.bed = bed_fpath

    if not contigs_fpaths:
        logger.error("None of the assembly files contains correct contigs. "
              "Please, provide different files or decrease --min-contig threshold.",
              fake_if_nested_run=True)
        return 4

    if qconfig.used_colors and qconfig.used_ls:
        for i, label in enumerate(labels):
            plotter_data.dict_color_and_ls[label] = (qconfig.used_colors[i], qconfig.used_ls[i])

    qconfig.assemblies_fpaths = contigs_fpaths

    # Where all pdfs will be saved
    all_pdf_fpath = None
    if qconfig.draw_plots and plotter.can_draw_plots:
        all_pdf_fpath = os.path.join(output_dirpath, qconfig.plots_fname)

    if qconfig.json_output_dirpath:
        from quast_libs.html_saver import json_saver
        if json_saver.simplejson_error:
            qconfig.json_output_dirpath = None

    ########################################################################
    ### Stats and plots
    ########################################################################
    from quast_libs import basic_stats
    icarus_gc_fpath, circos_gc_fpath = basic_stats.do(ref_fpath, contigs_fpaths, os.path.join(output_dirpath, 'basic_stats'), output_dirpath)

    if qconfig.use_kmc and ref_fpath:
        unique_kmers.do(os.path.join(output_dirpath, 'k_mer_stats'), ref_fpath, contigs_fpaths, logger)

    aligned_contigs_fpaths = []
    aligned_lengths_lists = []
    icarus_html_fpath = None
    circos_png_fpath = None
    if ref_fpath:
        ########################################################################
        ### former PLANTAKOLYA, PLANTAGORA
        ########################################################################
        from quast_libs import contigs_analyzer
        is_cyclic = qconfig.prokaryote and not qconfig.check_for_fragmented_ref
        aligner_statuses, aligned_lengths_per_fpath = contigs_analyzer.do(
            ref_fpath, contigs_fpaths, is_cyclic, os.path.join(output_dirpath, qconfig.detailed_contigs_reports_dirname),
            old_contigs_fpaths, qconfig.bed)
        for contigs_fpath in contigs_fpaths:
            if aligner_statuses[contigs_fpath] == contigs_analyzer.AlignerStatus.OK:
                aligned_contigs_fpaths.append(contigs_fpath)
                aligned_lengths_lists.append(aligned_lengths_per_fpath[contigs_fpath])

    if qconfig.is_agb_mode:  # AGB needs only alignments information
        sys.exit(0)

    # Before continue evaluating, check if aligner didn't skip all of the contigs files.
    detailed_contigs_reports_dirpath = None
    features_containers = None
    if len(aligned_contigs_fpaths) and ref_fpath:
        detailed_contigs_reports_dirpath = os.path.join(output_dirpath, qconfig.detailed_contigs_reports_dirname)

        ########################################################################
        ### NAx and NGAx ("aligned Nx and NGx")
        ########################################################################
        from quast_libs import aligned_stats
        aligned_stats.do(
            ref_fpath, aligned_contigs_fpaths, output_dirpath,
            aligned_lengths_lists, os.path.join(output_dirpath, 'aligned_stats'))

        ########################################################################
        ### GENOME_ANALYZER
        ########################################################################
        from quast_libs import genome_analyzer
        features_containers = genome_analyzer.do(
            ref_fpath, aligned_contigs_fpaths, output_dirpath,
            qconfig.features, qconfig.operons, detailed_contigs_reports_dirpath,
            os.path.join(output_dirpath, 'genome_stats'))

    genes_by_labels = None
    if qconfig.glimmer:
        ########################################################################
        ### Glimmer
        ########################################################################
        from quast_libs import glimmer
        genes_by_labels = glimmer.do(contigs_fpaths, qconfig.genes_lengths, os.path.join(output_dirpath, 'predicted_genes'))
    if qconfig.gene_finding:
        ########################################################################
        ### GeneMark
        ########################################################################
        from quast_libs import genemark
        genes_by_labels = genemark.do(contigs_fpaths, qconfig.genes_lengths, os.path.join(output_dirpath, 'predicted_genes'),
                    qconfig.prokaryote, qconfig.metagenemark)
    if genes_by_labels is None:
        logger.main_info("")
        logger.notice("Genes are not predicted by default. Use --gene-finding or --glimmer option to enable it.")

    if qconfig.rna_gene_finding:
        run_barrnap.do(contigs_fpaths, os.path.join(output_dirpath, 'predicted_genes'), logger)

    if qconfig.run_busco and not qconfig.is_combined_ref:
        if qconfig.platform_name == 'macosx':
            logger.main_info("")
            logger.warning("BUSCO can be run on Linux only")
        elif sys.version[0:3] < '2.7':
            logger.main_info("")
            logger.warning("BUSCO does not support Python versions earlier than 2.7.")
        else:
            from quast_libs import run_busco
            run_busco.do(contigs_fpaths, os.path.join(output_dirpath, qconfig.busco_dirname), logger)
    ########################################################################
    reports_fpaths, transposed_reports_fpaths = reporting.save_total(output_dirpath)

    ########################################################################
    ### LARGE DRAWING TASKS
    ########################################################################
    if qconfig.draw_plots or qconfig.create_icarus_html or qconfig.draw_circos:
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
            draw_alignment_plots = qconfig.create_icarus_html
            draw_circos_plot = qconfig.draw_circos and ref_fpath and len(aligned_contigs_fpaths)
            number_of_steps = sum([int(bool(value)) for value in [draw_alignment_plots, draw_circos_plot, all_pdf_fpath]])
            if all_pdf_fpath:
                # full report in PDF format: all tables and plots
                logger.main_info(
                    '  1 of %d: Creating PDF with all tables and plots...' % number_of_steps)
                plotter.fill_all_pdf_file(all_pdf_fpath)

            if draw_alignment_plots:
                ########################################################################
                ### VISUALIZE CONTIG ALIGNMENT
                ########################################################################
                logger.main_info('  %d of %d: Creating Icarus viewers...' % (2 if all_pdf_fpath else 1, number_of_steps))
                from quast_libs import icarus
                icarus_html_fpath = icarus.do(
                    contigs_fpaths, report_for_icarus_fpath_pattern, output_dirpath, ref_fpath,
                    stdout_pattern=stdout_pattern, features=features_containers,
                    cov_fpath=cov_fpath, physical_cov_fpath=physical_cov_fpath, gc_fpath=icarus_gc_fpath,
                    json_output_dir=qconfig.json_output_dirpath, genes_by_labels=genes_by_labels)

            if draw_circos_plot:
                logger.main_info('  %d of %d: Creating Circos plot...' % (number_of_steps, number_of_steps))
                from quast_libs import circos
                circos_png_fpath, circos_legend_fpath = circos.do(ref_fpath, contigs_fpaths, report_for_icarus_fpath_pattern, circos_gc_fpath,
                                                                  features_containers, cov_fpath, os.path.join(output_dirpath, 'circos'), logger)

            logger.main_info('Done')
        except KeyboardInterrupt:
            logger.main_info('..step skipped!')
            if all_pdf_fpath and os.path.isfile(all_pdf_fpath):
                os.remove(all_pdf_fpath)

    ########################################################################
    ### TOTAL REPORT
    ########################################################################
    logger.print_timestamp()
    logger.main_info('RESULTS:')
    logger.main_info('  Text versions of total report are saved to ' + reports_fpaths)
    logger.main_info('  Text versions of transposed total report are saved to ' + transposed_reports_fpaths)

    if qconfig.html_report:
        from quast_libs.html_saver import html_saver
        html_saver.save_colors(output_dirpath, contigs_fpaths, plotter_data.dict_color_and_ls)
        html_saver.save_total_report(output_dirpath, qconfig.min_contig, ref_fpath)

    if all_pdf_fpath and os.path.isfile(all_pdf_fpath):
        logger.main_info('  PDF version (tables and plots) is saved to ' + all_pdf_fpath)

    if circos_png_fpath:
        logger.main_info('  Circos plot is saved to %s (legend is in %s, configuration file is in %s)' %
                         (circos_png_fpath, os.path.basename(circos_legend_fpath),
                          os.path.basename(circos_png_fpath.replace('.png', '.conf'))))

    if icarus_html_fpath:
        logger.main_info('  Icarus (contig browser) is saved to %s' % icarus_html_fpath)

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
