############################################################################
# Copyright (c) 2015-2022 Saint Petersburg State University
# Copyright (c) 2011-2015 Saint Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################

from __future__ import with_statement
import os
from copy import copy
from optparse import OptionParser, Option
from os.path import join, abspath, isfile, isdir

import sys

from quast_libs import qconfig, qutils
from quast_libs.qutils import assert_file_exists, set_up_output_dir, check_dirpath, is_non_empty_file
from quast_libs.qconfig import get_mode

test_data_dir_basename = 'test_data'
test_data_dir = join(qconfig.QUAST_HOME, test_data_dir_basename)
if not isdir(test_data_dir) and isdir(test_data_dir_basename):  # special case: test_data in CWD
    test_data_dir = abspath(test_data_dir_basename)

test_reference           = join(test_data_dir, 'reference.fasta.gz')
test_forward_reads       = [join(test_data_dir, 'reads1.fastq.gz')]
test_reverse_reads       = [join(test_data_dir, 'reads2.fastq.gz')]
test_features            = dict([('gene', join(test_data_dir, 'genes.gff'))])
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
        assert_file_exists(value, option.dest)
        return abspath(value)
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
    check_dirpath(qconfig.output_dirpath, 'You have specified ' + str(output_dirpath) + ' as an output path.\n'
                     'Please, use a different directory.')


def set_extensive_mis_size(option, opt_str, value, parser, logger):
    if value < qconfig.local_misassembly_min_length:
        logger.error("--extensive-mis-size should be equal to or greater than minimal local misassembly length (%d)!"
                     % qconfig.local_misassembly_min_length, to_stderr=True, exit_with_code=2)
    setattr(qconfig, option.dest, value)


def get_current_extensive_misassembly_threshold():
    if qconfig.extensive_misassembly_threshold is None:
        return qconfig.LARGE_EXTENSIVE_MIS_THRESHOLD if qconfig.large_genome else qconfig.DEFAULT_EXT_MIS_SIZE
    return qconfig.extensive_misassembly_threshold


def set_local_mis_size(option, opt_str, value, parser, logger):
    if value <= qconfig.SHORT_INDEL_THRESHOLD or value > get_current_extensive_misassembly_threshold():
        logger.error("--local-mis-size should be between short indel size (>%d) and --extensive-mis-size (<=%d)!"
                     % (qconfig.SHORT_INDEL_THRESHOLD, get_current_extensive_misassembly_threshold()),
                     to_stderr=True, exit_with_code=2)
    setattr(qconfig, option.dest, value)


def check_fragmented_max_indent(logger):
    if qconfig.fragmented_max_indent < 0 or qconfig.fragmented_max_indent > qconfig.extensive_misassembly_threshold:
        logger.error("--fragmented-max-indent should be between 0 and --extensive-mis-size (%d)!"
                     % qconfig.extensive_misassembly_threshold, to_stderr=True, exit_with_code=2)


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


# safe remove from quast_py_args, e.g. removes correctly "--test-no" (full is "--test-no-ref") and corresponding argument
def remove_from_quast_py_args(quast_py_args, opt, arg=None):
    opt_idxs = []
    common_length = -1
    for idx, o in enumerate(quast_py_args):
        if o == opt:
            opt_idxs.append(idx)
        elif opt.startswith(o):
            if len(o) > common_length:
                opt_idxs.append(idx)
                common_length = len(o)
    for opt_idx in sorted(opt_idxs, reverse=True):
        if arg:
            del quast_py_args[opt_idx + 1]
        del quast_py_args[opt_idx]
    return quast_py_args


def parse_features(option, opt_str, value, parser, logger, is_old_format=False):
    if is_old_format:
        fpath = value
        assert_file_exists(fpath, 'genomic feature')
        features = dict([('gene', fpath)])
        logger.warning('Option -G is deprecated! Please use --features (or -g) to specify a file with genomic features.\n'
                       'If you want QUAST to extract only a specific genomic feature from the file, \n'
                       'you should prepend the filepath with the feature name and a colon, for example:\n'
                       '--features CDS:genes.gff --features transcript:transcripts.bed\n'
                       'Otherwise, all features would be counted:\n'
                       '--features genes.gff\n')
    else:
        if ':' in value:
            feature, fpath = value.split(':')
        else:
            feature, fpath = qconfig.ALL_FEATURES_TYPE, value  # special case -- read all features
        assert_file_exists(fpath, 'genomic feature')
        features = dict([(feature, fpath)])
    ensure_value(qconfig, 'features', dict()).update(features)


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


def parse_files_list(option, opt_str, value, parser, extension, logger):
    fpaths = []
    values = value.split(',')
    for i, value in enumerate(values):
        if value.endswith(extension):
            assert_file_exists(value, extension.upper() + ' file')
            fpaths.append(value)
        else:
            logger.error("incorrect extension for " + extension.upper() + " file (" + str(value) + ")! ",
                         to_stderr=True, exit_with_code=2)

    ensure_value(qconfig, option.dest, []).extend(fpaths)


def check_sam_bam_files(contigs_fpaths, sam_fpaths, bam_fpaths, logger):
    if sam_fpaths and len(contigs_fpaths) != len(sam_fpaths):
        logger.error('Number of SAM files does not match the number of files with contigs', to_stderr=True, exit_with_code=11)
    if bam_fpaths and len(contigs_fpaths) != len(bam_fpaths):
        logger.error('Number of BAM files does not match the number of files with contigs', to_stderr=True, exit_with_code=11)


def set_large_genome_parameters():
    qconfig.prokaryote = False
    qconfig.analyze_gaps = False
    qconfig.show_snps = False


def wrong_test_option(logger, msg):
    logger.error(msg)
    qconfig.usage(stream=sys.stderr)
    sys.exit(2)


def clean_metaquast_args(quast_py_args, contigs_fpaths):
    opts_with_args_to_remove = ['-o', '--output-dir', '-r', '-R', '--reference', '--max-ref-number', '-l', '--labels',
                                '--references-list', '--blast-db']
    opts_to_remove = ['-L', '--test', '--test-no-ref', '--unique-mapping', '--reuse-combined-alignments']
    for contigs_fpath in contigs_fpaths:
        if contigs_fpath in quast_py_args:
            quast_py_args.remove(contigs_fpath)
    for opt in opts_with_args_to_remove:
        remove_from_quast_py_args(quast_py_args, opt, arg=True)
    for opt in opts_to_remove:
        remove_from_quast_py_args(quast_py_args, opt)
    return quast_py_args


def prepare_regular_quast_args(quast_py_args, combined_output_dirpath, reuse_combined_alignments=False):
    opts_with_args_to_remove = ['--contig-thresholds', '--sv-bed',]
    opts_to_remove = ['-s', '--split-scaffolds', '--combined-ref']
    for opt in opts_with_args_to_remove:
        remove_from_quast_py_args(quast_py_args, opt, arg=True)
    for opt in opts_to_remove:
        remove_from_quast_py_args(quast_py_args, opt)

    quast_py_args += ['--no-check-meta']
    qconfig.contig_thresholds = ','.join([str(threshold) for threshold in qconfig.contig_thresholds if threshold >= qconfig.min_contig])
    if not qconfig.contig_thresholds:
        qconfig.contig_thresholds = 'None'
    quast_py_args += ['--contig-thresholds']
    quast_py_args += [qconfig.contig_thresholds]

    reads_stats_dirpath = os.path.join(combined_output_dirpath, qconfig.reads_stats_dirname)
    reference_name = qutils.name_from_fpath(qconfig.combined_ref_name)
    qconfig.bed = qconfig.bed or os.path.join(reads_stats_dirpath, reference_name + '.bed')
    qconfig.cov_fpath = qconfig.cov_fpath or os.path.join(reads_stats_dirpath, reference_name + '.cov')
    qconfig.phys_cov_fpath = qconfig.phys_cov_fpath or os.path.join(reads_stats_dirpath, reference_name + '.physical.cov')
    if qconfig.bed and is_non_empty_file(qconfig.bed):
        quast_py_args += ['--sv-bed']
        quast_py_args += [qconfig.bed]
    if qconfig.cov_fpath and is_non_empty_file(qconfig.cov_fpath):
        quast_py_args += ['--cov']
        quast_py_args += [qconfig.cov_fpath]
    if qconfig.phys_cov_fpath and is_non_empty_file(qconfig.phys_cov_fpath):
        quast_py_args += ['--phys-cov']
        quast_py_args += [qconfig.phys_cov_fpath]

    alignments_for_reuse_dirpath = os.path.join(combined_output_dirpath, qconfig.detailed_contigs_reports_dirname,
                                                qconfig.aligner_output_dirname)
    if reuse_combined_alignments and os.path.isdir(alignments_for_reuse_dirpath):
        quast_py_args += ['--aligns-for-reuse', alignments_for_reuse_dirpath]


def parse_options(logger, quast_args):
    mode = get_mode(quast_args[0])
    is_metaquast = True if mode == 'meta' else False
    qconfig.large_genome = True if mode == 'large' else False

    if '-h' in quast_args or '--help' in quast_args or '--help-hidden' in quast_args:
        qconfig.usage('--help-hidden' in quast_args, mode=mode, short=False)
        sys.exit(0)

    if '-v' in quast_args or '--version' in quast_args:
        qconfig.print_version(mode)
        sys.exit(0)

    quast_py_args = quast_args[1:]

    options = [
        (['--debug'], dict(
             dest='debug',
             action='store_true')
         ),
        (['--no-portable-html'], dict(
             dest='portable_html',
             action='store_false')
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
        (['-r', '-R', '--reference'], dict(
             dest='reference',
             type='string' if is_metaquast else 'file',
             action='callback' if is_metaquast else 'store',
             callback_args=(logger,) if is_metaquast else None,
             callback=parse_meta_references if is_metaquast else None)
         ),
        (['-O', '--operons'], dict(
             dest='operons',
             type='file',
             action='extend')
         ),
        (['-G', '--genes'], dict(
             dest='genes',
             type='string',
             action='callback',
             callback_args=(logger, True),
             callback=parse_features)
         ),
        (['-g', '--features'], dict(
             dest='features',
             type='string',
             action='callback',
             callback_args=(logger,),
             callback=parse_features)
         ),
        (['-1', '--reads1'], dict(
             dest='forward_reads',
             type='file',
             action='extend')
         ),
        (['-2', '--reads2'], dict(
             dest='reverse_reads',
             type='file',
             action='extend')
         ),
        (['--pe1'], dict(
             dest='forward_reads',
             type='file',
             action='extend')
         ),
        (['--pe2'], dict(
             dest='reverse_reads',
             type='file',
             action='extend')
         ),
        (['--mp1'], dict(
             dest='mp_forward_reads',
             type='file',
             action='extend')
         ),
        (['--mp2'], dict(
             dest='mp_reverse_reads',
             type='file',
             action='extend')
         ),
        (['--12'], dict(
             dest='interlaced_reads',
             type='file',
             action='extend')
         ),
        (['--pe12'], dict(
             dest='interlaced_reads',
             type='file',
             action='extend')
         ),
        (['--mp12'], dict(
             dest='mp_interlaced_reads',
             type='file',
             action='extend')
         ),
        (['--single'], dict(
             dest='unpaired_reads',
             type='file',
             action='extend')
         ),
        (['--pacbio'], dict(
             dest='pacbio_reads',
             type='file',
             action='extend')
         ),
        (['--nanopore'], dict(
             dest='nanopore_reads',
             type='file',
             action='extend')
         ),
        (['--ref-sam'], dict(
            dest='reference_sam',
            type='file')
         ),
        (['--ref-bam'], dict(
            dest='reference_bam',
            type='file')
         ),
        (['--sam'], dict(
            dest='sam_fpaths',
            type='string',
            action='callback',
            callback_args=('.sam', logger),
            callback=parse_files_list)
         ),
        (['--bam'], dict(
            dest='bam_fpaths',
            type='string',
            action='callback',
            callback_args=('.bam', logger),
            callback=parse_files_list)
         ),
        (['--sv-bedpe'], dict(
             dest='bed',
             type='file')
         ),
        (['--cov'], dict(
             dest='cov_fpath',
             type='file')
         ),
        (['--phys-cov'], dict(
             dest='phys_cov_fpath',
             type='file')
         ),
        (['--aligns-for-reuse'], dict(
             dest='alignments_for_reuse_dirpath',
             type='string')
         ),
        (['-l', '--labels'], dict(
             dest='labels',
             type='string')
         ),
        (['-L'], dict(
             dest='all_labels_from_dirs',
             action='store_true')
         ),
        (['--mgm'], dict(
             dest='metagenemark',
             action='callback',
             callback=set_multiple_variables,
             callback_kwargs={'store_true_values': ['gene_finding', 'metagenemark']},
             default=False)
         ),
        (['-s', '--split-scaffolds'], dict(
             dest='split_scaffolds',
             action='store_true')
         ),
        (['-e', '--eukaryote'], dict(
             dest='prokaryote',
             action='store_false')
         ),
        (['--fungus'], dict(
             dest='is_fungus',
             action='callback',
             callback=set_multiple_variables,
             callback_kwargs={'store_true_values': ['is_fungus'],
                              'store_false_values': ['prokaryote']})
         ),
        (['--large'], dict(
             dest='large_genome',
             action='store_true')
         ),
        (['-f', '--gene-finding'], dict(
             dest='gene_finding',
             action='store_true')
         ),
        (['--rna-finding'], dict(
             dest='rna_gene_finding',
             action='store_true')
         ),
        (['--fragmented'], dict(
             dest='check_for_fragmented_ref',
             action='store_true')
         ),
        (['--fragmented-max-indent'], dict(
             dest='fragmented_max_indent',
             type='int')
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
        (['--unaligned-part-size'], dict(
             dest='unaligned_part_size',
             type=int)
         ),
        (['--skip-unaligned-mis-contigs'], dict(
            dest='unaligned_mis_threshold',
            action="store_const",
            const=0.0)
         ),
        (['-x', '--extensive-mis-size'], dict(
             dest='extensive_misassembly_threshold',
             type='int',
             default=qconfig.extensive_misassembly_threshold,
             action='callback',
             callback=set_extensive_mis_size,
             callback_args=(logger,))
         ),
        (['--local-mis-size'], dict(
            dest='local_misassembly_min_length',
            type='int',
            default=qconfig.local_misassembly_min_length,
            action='callback',
            callback=set_local_mis_size,
            callback_args=(logger,))
         ),
        (['--scaffold-gap-max-size'], dict(
             dest='scaffolds_gap_threshold',
             type=int)
         ),
        (['-m', '--min-contig'], dict(
             dest='min_contig',
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
        (['--x-for-Nx'], dict(
            dest='x_for_additional_Nx',
            type='int',
            default=qconfig.x_for_additional_Nx,
            action='callback',
            callback=check_arg_value,
            callback_args=(logger,),
            callback_kwargs={'min_value': 0, 'max_value': 100})
         ),
        (['--gene-thresholds'], dict(
             dest='genes_lengths')
         ),
        (['--glimmer'], dict(
             dest='glimmer',
             action='store_true',
             default=False)
         ),
        (['-b', '--conserved-genes-finding'], dict(
             dest='run_busco',
             action='store_true',
             default=False)
         ),
        (['-k', '--k-mer-stats'], dict(
             dest='use_kmc',
             action='store_true',
             default=False)
         ),
        (['--k-mer-size'], dict(
             dest='unique_kmer_len',
             type='int')
         ),
        (['--upper-bound-assembly'], dict(
             dest='optimal_assembly',
             action='store_true')
         ),
        (['--upper-bound-min-con'], dict(
             dest='upperbound_min_connections',
             type='int',
             action='callback',
             callback=check_arg_value,
             callback_args=(logger,),
             callback_kwargs={'min_value': 1})
         ),
        (['--est-insert-size'], dict(
             dest='optimal_assembly_insert_size',
             type='int',
             action='callback',
             callback=check_arg_value,
             callback_args=(logger,),
             callback_kwargs={'min_value': qconfig.optimal_assembly_min_IS,
                              'max_value': qconfig.optimal_assembly_max_IS})
         ),
        (['--report-all-metrics'], dict(
            dest='report_all_metrics',
            action='store_true')
         ),
        (['--plots-format'], dict(
             dest='plot_extension',
             type='string',
             action='callback',
             callback=check_str_arg_value,
             callback_args=(logger,),
             callback_kwargs={'available_values': qconfig.supported_plot_extensions})
         ),
        (['--use-input-ref-order'], dict(
             dest='use_input_ref_order',
             action='store_true')
         ),
        (['--circos'], dict(
             dest='draw_circos',
             action='store_true')
         ),
        (['--no-read-stats'], dict(
             dest='no_read_stats',
             action='store_true')
         ),
        (['--fast'], dict(
             dest='fast',
             action='callback',
             callback=set_multiple_variables,
             callback_kwargs={'store_true_values': ['no_gc', 'no_sv', 'no_read_stats'],
                              'store_false_values': ['show_snps', 'draw_plots', 'html_report', 'create_icarus_html', 'analyze_gaps']},
             default=False)
         ),
        # (['--no-gzip'], dict(
        #      dest='no_gzip',
        #      action='store_true')
        #  ),
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
        (['--space-efficient'], dict(
             dest='space_efficient',
             action='callback',
             callback=set_multiple_variables,
             callback_kwargs={'store_true_values': ['space_efficient'],
                              'store_false_values': ['show_snps', 'create_icarus_html']},)
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
         ),
        (['--agb'], dict(
             dest='is_agb_mode',
             action='store_true')
         )
    ]
    if is_metaquast:
        options += [
            (['--unique-mapping'], dict(
                 dest='unique_mapping',
                 action='store_true')
             ),
            (['--reuse-combined-alignments'], dict(
                 dest='reuse_combined_alignments',
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
             ),
            (['--blast-db'], dict(
                 dest='custom_blast_db_fpath')
             )
        ]

    parser = OptionParser(option_class=QuastOption)
    for args, kwargs in options:
        parser.add_option(*args, **kwargs)
    (opts, contigs_fpaths) = parser.parse_args(quast_args[1:])

    if qconfig.test_sv and is_metaquast:
        msg = "Option --test-sv can be used for QUAST only\n"
        wrong_test_option(logger, msg)
    if qconfig.test_no_ref and not is_metaquast:
        msg = "Option --test-no-ref can be used for MetaQUAST only\n"
        wrong_test_option(logger, msg)

    if qconfig.glimmer and qconfig.gene_finding:
        logger.error("You cannot use --glimmer and " + ("--mgm" if qconfig.metagenemark else "--gene-finding") + \
                     " simultaneously!", exit_with_code=3)
    if qconfig.use_all_alignments and qconfig.reuse_combined_alignments:
        logger.error("You cannot use --use-all-alignments and --reuse-combined-alignments simultaneously! " + \
                     "Reused alignments are always filtered, i.e. a subset of all alignments.", exit_with_code=3)
    if len(qconfig.forward_reads) != len(qconfig.reverse_reads):
        logger.error('Use the SAME number of files with forward and reverse reads for paired-end libraries '
                     '(-1 <filepath> -2 <filepath>).\n'
                     'Use --pe12 option to specify a file with interlaced forward and reverse paired-end reads.\n'
                     'Use --single option to specify a file with unpaired (single-end) reads.', exit_with_code=3)
    if len(qconfig.mp_forward_reads) != len(qconfig.mp_reverse_reads):
        logger.error('Use the SAME number of files with forward and reverse reads for mate-pair libraries '
                     '(--mp1 <filepath> --mp2 <filepath>).\n'
                     'Use --mp12 option to specify a file with interlaced forward and reverse mate-pair reads.',
                     exit_with_code=3)
    if qconfig.optimal_assembly:
        if not qconfig.reference:
            logger.error("UpperBound assembly is reference-based by design, so you cannot use --upper-bound-assembly"
                         " option without specifying a reference (-r)!", exit_with_code=3)
        if not qconfig.pacbio_reads and not qconfig.nanopore_reads and \
                not (qconfig.mp_forward_reads or qconfig.mp_interlaced_reads):
            logger.error("UpperBound assembly construction requires mate-pairs or long reads (Pacbio SMRT or"
                         " Oxford Nanopore), so you cannot use --upper-bound-assembly without specifying them!",
                         exit_with_code=3)

    if qconfig.test or qconfig.test_no_ref or qconfig.test_sv:
        qconfig.output_dirpath = abspath(qconfig.test_output_dirname)
        check_dirpath(qconfig.output_dirpath, 'You are trying to run QUAST from ' + str(os.path.dirname(qconfig.output_dirpath)) + '.\n' +
                      'Please, rerun QUAST from a different directory.')
        if qconfig.test or qconfig.test_sv:
            qconfig.reference = meta_test_references if is_metaquast else test_reference
            if not is_metaquast:
                qconfig.features = test_features
                qconfig.operons = test_operons
                qconfig.glimmer = True
                if not qconfig.large_genome:  # special case -- large mode imposes eukaryote gene finding (GeneMark-ES) and our test data is too small for it.
                    qconfig.gene_finding = True
        if qconfig.test_sv:
            qconfig.forward_reads = test_forward_reads
            qconfig.reverse_reads = test_reverse_reads
        contigs_fpaths += meta_test_contigs_fpaths if is_metaquast else test_contigs_fpaths
        qconfig.test = True

        if any(not isfile(fpath) for fpath in contigs_fpaths) or \
                any(not isfile(fpath) for fpath in qconfig.forward_reads) or any(not isfile(fpath) for fpath in qconfig.reverse_reads):
            logger.info(
                '\nYou are probably running QUAST installed via pip, which does not include test data.\n'
                'This is fine, just start using QUAST on your own data!\n\n'
                'If you still want to run tests, please download and unpack test data to CWD:\n'
                '  wget quast.sf.net/test_data.tar.gz && tar xzf test_data.tar.gz\n')
            sys.exit(2)

    if not contigs_fpaths:
        logger.error("You should specify at least one file with contigs!\n", to_stderr=True)
        qconfig.usage(stream=sys.stderr)
        sys.exit(2)

    if qconfig.large_genome:
        set_large_genome_parameters()

    qconfig.extensive_misassembly_threshold = get_current_extensive_misassembly_threshold()
    if qconfig.fragmented_max_indent:
        # TODO: write a warning message if --fragmented-max-indent is used while --fragmented is not set
        # or set it automatically, but not in the way as commented out below, since we did this ALL THE TIME
        # because qconfig.fragmented_max_indent is always True (there is the default value for it)
        # qconfig.check_for_fragmented_ref = True
        check_fragmented_max_indent(logger)

    if qconfig.min_contig is None:
        qconfig.min_contig = qconfig.LARGE_MIN_CONTIG if qconfig.large_genome else qconfig.DEFAULT_MIN_CONTIG
    if qconfig.min_alignment is None:
        qconfig.min_alignment = qconfig.LARGE_MIN_ALIGNMENT if qconfig.large_genome else qconfig.DEFAULT_MIN_ALIGNMENT
    if qconfig.min_IDY is None and not is_metaquast:
        qconfig.min_IDY = qconfig.DEFAULT_MIN_IDY

    for c_fpath in contigs_fpaths:
        assert_file_exists(c_fpath, 'contigs')

    if qconfig.json_output_dirpath:
        qconfig.save_json = True

    if not qconfig.output_dirpath:
        check_dirpath(os.getcwd(), 'An output path was not specified manually. You are trying to run QUAST from ' + str(os.getcwd()) + '.\n' +
                      'Please, specify a different directory using -o option.')
    qconfig.output_dirpath, qconfig.json_output_dirpath, existing_quast_dir = \
        set_up_output_dir(qconfig.output_dirpath, qconfig.json_output_dirpath, not qconfig.output_dirpath,
                          qconfig.save_json if not is_metaquast else None)

    logger.set_up_file_handler(qconfig.output_dirpath, qconfig.error_log_fpath)
    logger.set_up_console_handler(debug=qconfig.debug)
    logger.print_command_line(quast_args, wrap_after=None, is_main=True)
    logger.start()

    if existing_quast_dir:
        logger.notice("Output directory already exists and looks like a QUAST output dir. "
                      "Existing results can be reused (e.g. previously generated alignments)!")
        qutils.remove_reports(qconfig.output_dirpath)

    if qconfig.labels:
        qconfig.labels = qutils.parse_labels(qconfig.labels, contigs_fpaths)
    qconfig.labels = qutils.process_labels(contigs_fpaths, qconfig.labels, qconfig.all_labels_from_dirs)

    if qconfig.contig_thresholds == "None":
        qconfig.contig_thresholds = []
    else:
        qconfig.contig_thresholds = [int(x) for x in qconfig.contig_thresholds.split(",")]
    if qconfig.genes_lengths == "None":
        qconfig.genes_lengths = []
    else:
        qconfig.genes_lengths = [int(x) for x in qconfig.genes_lengths.split(",")]

    qconfig.set_max_threads(logger)

    if parser.values.ambiguity_score:
        if qconfig.ambiguity_usage != 'all':
            qconfig.ambiguity_usage = 'all'
            logger.warning("--ambiguity-usage was set to 'all' because not default --ambiguity-score was specified")

    if qconfig.memory_efficient and (qconfig.genes or qconfig.operons or qconfig.features):
        logger.warning("Analysis of genes and/or operons files (provided with -g and -O) requires extensive RAM usage, "
                       "consider running QUAST without them if memory consumption is critical.")

    if is_metaquast:
        quast_py_args = clean_metaquast_args(quast_py_args, contigs_fpaths)

    if qconfig.sam_fpaths or qconfig.bam_fpaths:
        check_sam_bam_files(contigs_fpaths, qconfig.sam_fpaths, qconfig.bam_fpaths, logger)

    return quast_py_args, contigs_fpaths


