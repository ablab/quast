############################################################################
# Copyright (c) 2015-2016 Saint Petersburg State University
# Copyright (c) 2011-2015 Saint Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################

from __future__ import with_statement
import os
from copy import copy
from optparse import OptionParser, Option
from os.path import join, abspath

import sys

from quast_libs import qconfig, qutils
from quast_libs.metautils import remove_from_quast_py_args
from quast_libs.qutils import assert_file_exists, set_up_output_dir


test_data_dir = join(qconfig.QUAST_HOME, 'test_data')

test_reference           = join(test_data_dir, 'reference.fasta.gz')
test_forward_reads       = join(test_data_dir, 'reads1.fastq.gz')
test_reverse_reads       = join(test_data_dir, 'reads2.fastq.gz')
test_genes               = [join(test_data_dir, 'genes.gff')]
test_operons             = [join(test_data_dir, 'operons.gff')]
test_contigs_fpaths      = [join(test_data_dir, 'contigs_1.fasta'),
                            join(test_data_dir, 'contigs_2.fasta')]

meta_test_references     = [join(test_data_dir, 'meta_ref_1.fasta'),
                            join(test_data_dir, 'meta_ref_2.fasta'),
                            join(test_data_dir, 'meta_ref_3.fasta')]
meta_test_contigs_fpaths = [join(test_data_dir, 'meta_contigs_1.fasta'),
                            join(test_data_dir, 'meta_contigs_2.fasta')]


class QuastOption(Option):
    def check_file(option, opt, value):
        files = value.split(',')
        for f in files:
            assert_file_exists(f, option.dest)
        return value
    TYPES = Option.TYPES + ('file',)
    TYPE_CHECKER = copy(Option.TYPE_CHECKER)
    TYPE_CHECKER['file'] = check_file

    ACTIONS = Option.ACTIONS + ('extend',)
    STORE_ACTIONS = Option.STORE_ACTIONS + ('extend',)
    TYPED_ACTIONS = Option.TYPED_ACTIONS + ('extend',)
    ALWAYS_TYPED_ACTIONS = Option.ALWAYS_TYPED_ACTIONS + ('extend',)

    def take_action(self, action, dest, opt, value, values, parser):
        if action == 'extend':
            split_value = value.split(',')
            ensure_value(qconfig, dest, []).extend(split_value)
        else:
            Option.take_action(
                self, action, dest, opt, value, qconfig, parser)


def ensure_value(values, attr, value):
    if not hasattr(values, attr) or getattr(values, attr) is None:
        setattr(values, attr, value)
    return getattr(values, attr)


def check_output_dir(option, opt_str, value, parser, logger):
    output_dirpath = os.path.abspath(value)
    setattr(qconfig, option.dest, output_dirpath)
    if ' ' in output_dirpath:
        logger.error('QUAST does not support spaces in paths. \n'
                     'You have specified ' + str(output_dirpath) + ' as an output path.\n'
                     'Please, use a different directory.',
                     to_stderr=True,
                     exit_with_code=3)


def set_extensive_mis_size(option, opt_str, value, parser, logger):
    if value <= qconfig.MAX_INDEL_LENGTH:
        logger.error("--extensive-mis-size should be greater than maximum indel length (%d)!"
                     % qconfig.MAX_INDEL_LENGTH, 1, to_stderr=True, exit_with_code=2)
    setattr(qconfig, option.dest, value)


def set_multiple_variables(option, opt_str, value, parser, store_true_values=None, store_false_values=None):
    if store_true_values is not None:
        for v in store_true_values:
            setattr(qconfig, v, True)
    if store_false_values is not None:
        for v in store_false_values:
            setattr(qconfig, v, False)


def check_str_arg_value(option, opt_str, value, parser, logger, available_values):
    if value.lower() in available_values:
        setattr(qconfig, option.dest, value.lower())
    else:
        logger.error("incorrect value for " + opt_str + " (" + str(value) + ")! "
                     "Please use one of the following values: " + ', '.join(available_values),
                     to_stderr=True, exit_with_code=2)


def check_arg_value(option, opt_str, value, parser, logger, default_value=None, min_value=0, max_value=float('Inf')):
    if min_value <= float(value) <= max_value:
        setattr(qconfig, option.dest, value)
        setattr(parser.values, option.dest, value)
    elif default_value:
        setattr(qconfig, option.dest, default_value)
    else:
        if max_value:
            logger.error("incorrect value for " + opt_str + " (" + str(value) + ")! "
                         "Please specify a number between " + str(min_value) + " and " + str(max_value),
                         to_stderr=True, exit_with_code=2)
        else:
            logger.error("incorrect value for " + opt_str + " (" + str(value) + ")! "
                         "Please specify a number greater than " + str(min_value),
                         to_stderr=True, exit_with_code=2)


def parse_meta_references(option, opt_str, value, parser, logger):
    ref_fpaths = []
    ref_values = value.split(',')
    for i, ref_value in enumerate(ref_values):
        if os.path.isdir(ref_value):
            references = [join(path, file) for (path, dirs, files) in os.walk(ref_value) for file in files
                               if qutils.check_is_fasta_file(file, logger=logger)]
            ref_fpaths.extend(sorted(references))
        else:
            assert_file_exists(ref_value, 'reference')
            ref_fpaths.append(ref_value)
    ensure_value(qconfig, option.dest, []).extend(ref_fpaths)


def wrong_test_option(logger, msg, is_metaquast):
    logger.error(msg)
    qconfig.usage(meta=is_metaquast)
    sys.exit(2)


def clean_metaquast_args(quast_py_args, contigs_fpaths):
    opts_with_args_to_remove = ['-o', '--output-dir', '-R', '--reference', '--max-ref-number', '-l', '--labels',
                                '-1', '--reads1', '-2', '--reads2', '--references-list']
    opts_to_remove = ['-L', '--test', '--test-no-ref', '--unique-mapping']
    for contigs_fpath in contigs_fpaths:
        if contigs_fpath in quast_py_args:
            quast_py_args.remove(contigs_fpath)
    for opt in opts_with_args_to_remove:
        remove_from_quast_py_args(quast_py_args, opt, arg=True)

    for opt in opts_to_remove:
        remove_from_quast_py_args(quast_py_args, opt)
    return quast_py_args


def parse_options(logger, quast_args, is_metaquast=False):
    if '-h' in quast_args or '--help' in quast_args or '--help-hidden' in quast_args:
        qconfig.usage('--help-hidden' in quast_args, meta=is_metaquast, short=False)
        sys.exit(0)

    if '-v' in quast_args or '--version' in quast_args:
        qconfig.print_version(meta=is_metaquast)
        sys.exit(0)

    quast_py_args = quast_args[1:]

    options = [
        (['--debug'], dict(
             dest='debug',
             action='store_true')
         ),
        (['--test'], dict(
             dest='test',
             action='store_true')
         ),
        (['--test-sv'], dict(
             dest='test_sv',
             action='store_true')
         ),
        (['--test-no-ref'], dict(
             dest='test_no_ref',
             action='store_true')
         ),
        (['-o', '--output-dir'], dict(
             dest='output_dirpath',
             type='string',
             action='callback',
             callback=check_output_dir,
             callback_args=(logger,))
         ),
        (['-t', '--threads'], dict(
             dest='max_threads',
             type='int',
             action='callback',
             callback=check_arg_value,
             callback_args=(logger,),
             callback_kwargs={'default_value': 1, 'min_value': 1})
         ),
        (['-R', '--reference'], dict(
             dest='reference',
             type='string' if is_metaquast else 'file',
             action='callback' if is_metaquast else 'store',
             callback_args=(logger,) if is_metaquast else None,
             callback=parse_meta_references if is_metaquast else None)
         ),
        (['-G', '--genes'], dict(
             dest='genes',
             type='file',
             action='extend')
         ),
        (['-O', '--operons'], dict(
             dest='operons',
             type='file',
             action='extend')
         ),
        (['-1', '--reads1'], dict(
             dest='forward_reads',
             type='file')
         ),
        (['-2', '--reads2'], dict(
             dest='reverse_reads',
             type='file')
         ),
        (['--sam'], dict(
             dest='sam',
             type='file')
         ),
        (['--bam'], dict(
             dest='bam',
             type='file')
         ),
        (['--sv-bedpe'], dict(
             dest='bed',
             type='file')
         ),
        (['-l', '--labels'], dict(
             dest='labels',
             type='string')
         ),
        (['-L'], dict(
             dest='all_labels_from_dirs',
             action='store_true')
         ),
        (['--meta'], dict(
             dest='meta',
             action='store_true')
         ),
        (['-s', '--scaffolds'], dict(
             dest='scaffolds',
             action='store_true')
         ),
        (['-e', '--eukaryote'], dict(
             dest='prokaryote',
             action='store_false')
         ),
        (['-f', '--gene-finding'], dict(
             dest='gene_finding',
             action='store_true')
         ),
        (['--fragmented'], dict(
             dest='check_for_fragmented_ref',
             action='store_true')
         ),
        (['-a', '--ambiguity-usage'], dict(
             dest='ambiguity_usage',
             type='string',
             default=qconfig.ambiguity_usage,
             action='callback',
             callback=check_str_arg_value,
             callback_args=(logger,),
             callback_kwargs={'available_values': ['none', 'one', 'all']})
         ),
        (['--ambiguity-score'], dict(
             dest='ambiguity_score',
             type='float',
             action='callback',
             callback=check_arg_value,
             callback_args=(logger,),
             callback_kwargs={'min_value': 0.8, 'max_value': 1.0})
         ),
        (['-u', '--use-all-alignments'], dict(
             dest='use_all_alignments',
             action='store_true')
         ),
        (['--strict-NA'], dict(
             dest='strict_NA',
             action='store_true')
         ),
        (['--significant-part-size'], dict(
             dest='significant_part_size',
             type=int)
         ),
        (['-x', '--extensive-mis-size'], dict(
             dest='extensive_misassembly_threshold',
             type='int',
             default=qconfig.extensive_misassembly_threshold,
             action='callback',
             callback=set_extensive_mis_size,
             callback_args=(logger,))
         ),
        (['-m', '--min-contig'], dict(
             dest='min_contig',
             type='int')
         ),
        (['-c', '--min-cluster'], dict(
             dest='min_cluster',
             type='int')
         ),
        (['-i', '--min-alignment'], dict(
             dest='min_alignment',
             type='int')
         ),
        (['--min-identity'], dict(
             dest='min_IDY',
             type='float',
             default=qconfig.min_IDY,
             action='callback',
             callback=check_arg_value,
             callback_args=(logger,),
             callback_kwargs={'min_value': 80.0, 'max_value': 100.0})
         ),
        (['--est-ref-size'], dict(
             dest='estimated_reference_size',
             type='int')
         ),
        (['--contig-thresholds'], dict(
             dest='contig_thresholds')
         ),
        (['--gene-thresholds'], dict(
             dest='genes_lengths')
         ),
        (['--gage'], dict(
             dest='with_gage',
             action='store_true')
         ),
        (['--glimmer'], dict(
             dest='glimmer',
             action='callback',
             callback=set_multiple_variables,
             callback_kwargs={'store_true_values': ['gene_finding', 'glimmer']},
             default=False)
         ),
        (['--plots-format'], dict(
             dest='plot_extension',
             type='string',
             action='callback',
             callback=check_str_arg_value,
             callback_args=(logger,),
             callback_kwargs={'available_values': qconfig.supported_plot_extensions})
         ),
        (['--svg'], dict(
             dest='draw_svg',
             action='store_true')
         ),
        (['--fast'], dict(
             dest='fast',
             action='callback',
             callback=set_multiple_variables,
             callback_kwargs={'store_true_values': ['no_gc', 'no_sv', 'no_gzip'],
                              'store_false_values': ['show_snps', 'draw_plots', 'html_report', 'create_icarus_html']},
             default=False)
         ),
        (['--no-gzip'], dict(
             dest='no_gzip',
             action='store_true')
         ),
        (['--no-check'], dict(
             dest='no_check',
             action='store_true')
         ),
        (['--no-check-meta'], dict(
             dest='no_check_meta',
             action='callback',
             callback=set_multiple_variables,
             callback_kwargs={'store_true_values': ['no_check', 'no_check_meta']})
         ),
        (['--no-snps'], dict(
             dest='show_snps',
             action='store_false')
         ),
        (['--no-plots'], dict(
             dest='draw_plots',
             action='store_false')
         ),
        (['--no-html'], dict(
             dest='html_report',
             action='callback',
             callback=set_multiple_variables,
             callback_kwargs={'store_false_values': ['html_report', 'create_icarus_html']})
         ),
        (['--no-icarus'], dict(
             dest='create_icarus_html',
             action='store_false')
         ),
        (['--no-gc'], dict(
             dest='no_gc',
             action='store_true')
         ),
        (['--no-sv'], dict(
             dest='no_sv',
             action='store_true')
         ),
        (['--memory-efficient'], dict(
             dest='memory_efficient',
             action='store_true')
         ),
        (['--silent'], dict(
             dest='silent',
             action='store_true')
         ),
        (['--combined-ref'], dict(
             dest='is_combined_ref',
             action='store_true')
         ),
        (['--colors'], dict(
             dest='used_colors',
             action='extend')
         ),
        (['--ls'], dict(
             dest='used_ls',
             action='extend')
         ),
        (['-j', '--save-json'], dict(
             dest='save_json',
             action='store_true')
         ),
        (['-J', '--save-json-to'], dict(
             dest='json_output_dirpath')
         ),
        (['--err-fpath'], dict(
             dest='error_log_fpath')
         ),
        (['--read-support'], dict(
             dest='calculate_read_support',
             action='store_true')
         )
    ]
    if is_metaquast:
        options += [
            (['--unique-mapping'], dict(
                 dest='unique_mapping',
                 action='store_true')
             ),
            (['--max-ref-number'], dict(
                 dest='max_references',
                 type='int',
                 action='callback',
                 callback=check_arg_value,
                 callback_args=(logger,),
                 callback_kwargs={'default_value': qconfig.max_references, 'min_value': 0})
             ),
            (['--references-list'], dict(
                 dest='references_txt')
             )
        ]

    parser = OptionParser(option_class=QuastOption)
    for args, kwargs in options:
        parser.add_option(*args, **kwargs)
    (opts, contigs_fpaths) = parser.parse_args(quast_args[1:])

    if qconfig.test_sv and is_metaquast:
        msg = "Option --test-sv can be used for QUAST only\n"
        wrong_test_option(logger, msg, is_metaquast)
    if qconfig.test_no_ref and not is_metaquast:
        msg = "Option --test-no-ref can be used for MetaQUAST only\n"
        wrong_test_option(logger, msg, is_metaquast)

    if qconfig.test or qconfig.test_no_ref or qconfig.test_sv:
        qconfig.output_dirpath = abspath(qconfig.test_output_dirname)
        if qconfig.test or qconfig.test_sv:
            qconfig.reference = meta_test_references if is_metaquast else test_reference
            if not is_metaquast:
                qconfig.genes = test_genes
                qconfig.operons = test_operons
                qconfig.with_gage = True
                qconfig.glimmer = True
                qconfig.gene_finding = True
                qconfig.prokaryote = False
        if qconfig.test_sv:
            qconfig.forward_reads = test_forward_reads
            qconfig.reverse_reads = test_reverse_reads
        contigs_fpaths += meta_test_contigs_fpaths if is_metaquast else test_contigs_fpaths
        qconfig.test = True

    if not contigs_fpaths:
        logger.error("You should specify at least one file with contigs!\n")
        qconfig.usage(meta=is_metaquast)
        sys.exit(2)

    logger.set_up_console_handler(debug=qconfig.debug)

    for c_fpath in contigs_fpaths:
        assert_file_exists(c_fpath, 'contigs')

    if qconfig.json_output_dirpath:
        qconfig.save_json = True

    qconfig.output_dirpath, qconfig.json_output_dirpath, existing_alignments = \
        set_up_output_dir(qconfig.output_dirpath, qconfig.json_output_dirpath, not qconfig.output_dirpath,
                          qconfig.save_json if not is_metaquast else None)

    logger.set_up_file_handler(qconfig.output_dirpath, qconfig.error_log_fpath)
    logger.print_command_line(quast_args, wrap_after=None, is_main=True)
    logger.start()

    if existing_alignments and not is_metaquast:
        logger.notice("Output directory already exists. Existing Nucmer alignments can be used")
        qutils.remove_reports(qconfig.output_dirpath)

    if qconfig.labels:
        qconfig.labels = qutils.parse_labels(qconfig.labels, contigs_fpaths)
    qconfig.labels = qutils.process_labels(contigs_fpaths, qconfig.labels, qconfig.all_labels_from_dirs)

    if qconfig.contig_thresholds == "None":
        qconfig.contig_thresholds = []
    else:
        qconfig.contig_thresholds = map(int, qconfig.contig_thresholds.split(","))
    if qconfig.genes_lengths == "None":
        qconfig.genes_lengths = []
    else:
        qconfig.genes_lengths = map(int, qconfig.genes_lengths.split(","))

    qconfig.set_max_threads(logger)

    if parser.values.ambiguity_score:
        if qconfig.ambiguity_usage != 'all':
            qconfig.ambiguity_usage = 'all'
            logger.notice("--ambiguity-usage was set to 'all' because not default --ambiguity-score was specified")

    if is_metaquast:
        quast_py_args = clean_metaquast_args(quast_py_args, contigs_fpaths)

    return quast_py_args, contigs_fpaths


