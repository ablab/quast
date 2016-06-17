#!/usr/bin/env python

############################################################################
# Copyright (c) 2015-2016 Saint Petersburg State University
# Copyright (c) 2011-2015 Saint Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################

from __future__ import with_statement
import sys
import os
import shutil
import getopt
import re

from libs import qconfig
qconfig.check_python_version()
from libs import qutils, fastaparser
from libs import reporting
from libs import search_references_meta
from libs.qutils import assert_file_exists, correct_seq, cleanup

from libs.log import get_logger
logger = get_logger(qconfig.LOGGER_META_NAME)
logger.set_up_console_handler()

from site import addsitedir
addsitedir(os.path.join(qconfig.LIBS_LOCATION, 'site_packages'))

import quast
from libs import contigs_analyzer, reads_analyzer

COMBINED_REF_FNAME = 'combined_reference.fasta'


class Assembly:
    def __init__(self, fpath, label):
        self.fpath = fpath
        self.label = label
        self.name = os.path.splitext(os.path.basename(self.fpath))[0]


def parallel_partition_contigs(asm, assemblies_by_ref, corrected_dirpath, alignments_fpath_template):
    assembly_label = qutils.label_from_fpath(asm.fpath)
    corr_assembly_label = assembly_label.replace(' ', '_')
    logger.info('  ' + 'processing ' + assembly_label)
    added_ref_asm = []
    not_aligned_fname = corr_assembly_label + '_not_aligned_anywhere.fasta'
    not_aligned_fpath = os.path.join(corrected_dirpath, not_aligned_fname)
    contigs = {}
    aligned_contig_names = set()
    aligned_contigs_for_each_ref = {}
    contigs_seq = fastaparser.read_fasta_one_time(asm.fpath)
    if os.path.exists(alignments_fpath_template % corr_assembly_label):
        for line in open(alignments_fpath_template % corr_assembly_label):
            values = line.split()
            if values[0] in contigs_analyzer.ref_labels_by_chromosomes.keys():
                ref_name = contigs_analyzer.ref_labels_by_chromosomes[values[0]]
                ref_contigs_names = values[1:]
                ref_contigs_fpath = os.path.join(
                    corrected_dirpath, corr_assembly_label + '_to_' + ref_name + '.fasta')
                if ref_name not in aligned_contigs_for_each_ref:
                    aligned_contigs_for_each_ref[ref_name] = []

                for (cont_name, seq) in contigs_seq:
                    if not cont_name in contigs:
                        contigs[cont_name] = seq

                    if cont_name in ref_contigs_names and cont_name not in aligned_contigs_for_each_ref[ref_name]:
                        # Collecting all aligned contigs names in order to futher extract not-aligned
                        aligned_contig_names.add(cont_name)
                        aligned_contigs_for_each_ref[ref_name].append(cont_name)
                        fastaparser.write_fasta(ref_contigs_fpath, [(cont_name, seq)], 'a')

                ref_asm = Assembly(ref_contigs_fpath, assembly_label)
                if ref_asm.name not in added_ref_asm:
                    if ref_name in assemblies_by_ref:
                        assemblies_by_ref[ref_name].append(ref_asm)
                        added_ref_asm.append(ref_asm.name)

    # Extraction not aligned contigs
    all_contigs_names = set(contigs.keys())
    not_aligned_contigs_names = all_contigs_names - aligned_contig_names
    fastaparser.write_fasta(not_aligned_fpath, [(name, contigs[name]) for name in not_aligned_contigs_names])

    not_aligned_asm = Assembly(not_aligned_fpath, asm.label)
    return assemblies_by_ref, not_aligned_asm


def _partition_contigs(assemblies, ref_fpaths, corrected_dirpath, alignments_fpath_template, labels):
    # not_aligned_anywhere_dirpath = os.path.join(output_dirpath, 'contigs_not_aligned_anywhere')
    # if os.path.isdir(not_aligned_anywhere_dirpath):
    #     os.rmdir(not_aligned_anywhere_dirpath)
    # os.mkdir(not_aligned_anywhere_dirpath)

    # array of assemblies for each reference
    assemblies_by_ref = dict([(qutils.name_from_fpath(ref_fpath), []) for ref_fpath in ref_fpaths])
    n_jobs = min(qconfig.max_threads, len(assemblies))
    from joblib import Parallel, delayed
    assemblies = Parallel(n_jobs=n_jobs)(delayed(parallel_partition_contigs)(asm,
                                assemblies_by_ref, corrected_dirpath, alignments_fpath_template) for asm in assemblies)
    assemblies_dicts = [assembly[0] for assembly in assemblies]
    assemblies_by_ref = []
    for ref_fpath in ref_fpaths:
        ref_name = qutils.name_from_fpath(ref_fpath)
        not_sorted_assemblies = set([val for sublist in (assemblies_dicts[i][ref_name] for i in range(len(assemblies_dicts))) for val in sublist])
        sorted_assemblies = []
        for label in labels:  # sort by label
            for assembly in not_sorted_assemblies:
                if assembly.label == label:
                    sorted_assemblies.append(assembly)
                    break
        assemblies_by_ref.append((ref_fpath, sorted_assemblies))
    not_aligned_assemblies = [assembly[1] for assembly in assemblies]
    return assemblies_by_ref, not_aligned_assemblies


# class LoggingIndentFormatter(logging.Formatter):
#     def __init__(self, fmt):
#         logging.Formatter.__init__(self, fmt)
#
#     def format(self, record):
#         indent = '\t'
#         msg = logging.Formatter.format(self, record)
#         return '\n'.join([indent + x for x in msg.split('\n')])


def _start_quast_main(
        name, args, assemblies, reference_fpath=None,
        output_dirpath=None, exit_on_exception=True, num_notifications_tuple=None, is_first_run=None):
    args = args[:]

    args.extend([asm.fpath for asm in assemblies])

    if reference_fpath:
        args.append('-R')
        args.append(reference_fpath)

    if output_dirpath:
        args.append('-o')
        args.append(output_dirpath)

    args.append('--labels')

    def quote(line):
        if ' ' in line:
            line = '"%s"' % line
        return line

    args.append(quote(', '.join([asm.label for asm in assemblies])))

    import quast
    reload(quast)
    quast.logger.set_up_console_handler(indent_val=1, debug=qconfig.debug)
    quast.logger.set_up_metaquast()
    logger.info_to_file('(logging to ' +
                        os.path.join(output_dirpath,
                                     qconfig.LOGGER_DEFAULT_NAME + '.log)'))
    # try:
    return_code = quast.main(args)
    if num_notifications_tuple:
        cur_num_notifications = quast.logger.get_numbers_of_notifications()
        num_notifications_tuple = map(sum, zip(num_notifications_tuple,cur_num_notifications))

    if is_first_run:
        labels = [qconfig.assembly_labels_by_fpath[fpath] for fpath in qconfig.assemblies_fpaths]
        assemblies = [Assembly(fpath, qconfig.assembly_labels_by_fpath[fpath]) for fpath in qconfig.assemblies_fpaths]
        return return_code, num_notifications_tuple, assemblies, labels
    else:
        return return_code, num_notifications_tuple

    # except Exception, (errno, strerror):
    #     if exit_on_exception:
    #         logger.exception(e)
    #     else:
    #         msg = 'Error running quast.py' + (' ' + name if name else '')
    #         msg += ': ' + e.strerror
    #         if e.message:
    #             msg += ', ' + e.message
    #         logger.error(msg)


def _correct_assemblies(contigs_fpaths, output_dirpath, labels):
    corrected_dirpath = os.path.join(output_dirpath, qconfig.corrected_dirname)
    corrected_contigs_fpaths, old_contigs_fpaths = qutils.correct_contigs(contigs_fpaths, corrected_dirpath, reporting, labels)
    assemblies = [Assembly(fpath, qutils.label_from_fpath(fpath)) for fpath in old_contigs_fpaths]
    corrected_labels = [asm.label for asm in assemblies]

    if qconfig.draw_plots or qconfig.html_report:
        from libs import plotter
        corr_fpaths = [asm.fpath for asm in assemblies]
        corr_labels = [asm.label for asm in assemblies]
        plotter.save_colors_and_ls(corr_fpaths, labels=corr_labels)
    return assemblies, corrected_labels


def _correct_meta_references(ref_fpaths, corrected_dirpath):
    corrected_ref_fpaths = []

    combined_ref_fpath = os.path.join(corrected_dirpath, COMBINED_REF_FNAME)

    chromosomes_by_refs = {}

    def _proceed_seq(seq_name, seq, ref_name, ref_fasta_ext, total_references, ref_fpath):
        seq_fname = ref_name
        seq_fname += ref_fasta_ext

        if total_references > 1:
            corr_seq_fpath = corrected_ref_fpaths[-1]
        else:
            corr_seq_fpath = qutils.unique_corrected_fpath(os.path.join(corrected_dirpath, seq_fname))
            corrected_ref_fpaths.append(corr_seq_fpath)
        corr_seq_name = qutils.name_from_fpath(corr_seq_fpath)
        corr_seq_name += '_' + qutils.correct_name(seq_name[:20])
        if not qconfig.no_check:
            corr_seq = correct_seq(seq, ref_fpath)
            if not corr_seq:
                return None, None

        fastaparser.write_fasta(corr_seq_fpath, [(corr_seq_name, seq)], 'a')
        fastaparser.write_fasta(combined_ref_fpath, [(corr_seq_name, seq)], 'a')

        contigs_analyzer.ref_labels_by_chromosomes[corr_seq_name] = qutils.name_from_fpath(corr_seq_fpath)
        chromosomes_by_refs[ref_name].append((corr_seq_name, len(seq)))

        return corr_seq_name, corr_seq_fpath

    ref_fnames = [os.path.basename(ref_fpath) for ref_fpath in ref_fpaths]
    ref_names = []
    for ref_fname in ref_fnames:
        ref_name, ref_fasta_ext = qutils.splitext_for_fasta_file(ref_fname)
        ref_names.append(ref_name)
    dupl_ref_names = [ref_name for ref_name in ref_names if ref_names.count(ref_name) > 1]

    for ref_fpath in ref_fpaths:
        total_references = 0
        ref_fname = os.path.basename(ref_fpath)
        ref_name, ref_fasta_ext = qutils.splitext_for_fasta_file(ref_fname)
        if ref_name in dupl_ref_names:
            ref_name = qutils.get_label_from_par_dir_and_fname(ref_fpath)

        chromosomes_by_refs[ref_name] = []

        corr_seq_fpath = None
        for i, (seq_name, seq) in enumerate(fastaparser.read_fasta(ref_fpath)):
            total_references += 1
            corr_seq_name, corr_seq_fpath = _proceed_seq(seq_name, seq, ref_name, ref_fasta_ext, total_references, ref_fpath)
            if not corr_seq_name:
                break
            fastaparser.write_fasta(corr_seq_fpath, [(corr_seq_name, seq)], 'a')
            fastaparser.write_fasta(combined_ref_fpath, [(corr_seq_name, seq)], 'a')
        if corr_seq_fpath:
            logger.main_info('  ' + ref_fpath + ' ==> ' + qutils.name_from_fpath(corr_seq_fpath) + '')

    logger.main_info('  All references combined in ' + COMBINED_REF_FNAME)

    return corrected_ref_fpaths, combined_ref_fpath, chromosomes_by_refs, ref_fpaths


def remove_unaligned_downloaded_refs(genome_info_fpath, ref_fpaths, chromosomes_by_refs):
    refs_len = {}
    with open(genome_info_fpath, 'r') as report_file:
        report_file.readline()
        for line in report_file:
            if line == '\n' or not line:
                break
            line = line.split()
            refs_len[line[0]] = (line[3], line[8])

    corr_refs = []
    for ref_fpath in ref_fpaths:
        ref_fname = os.path.basename(ref_fpath)
        ref, ref_fasta_ext = qutils.splitext_for_fasta_file(ref_fname)
        aligned_len = 0
        all_len = 0
        for chromosome in chromosomes_by_refs[ref]:
            if chromosome[0] in refs_len:
                aligned_len += int(refs_len[chromosome[0]][1])
                all_len += int(refs_len[chromosome[0]][0])
        if aligned_len > all_len * 0.1 and aligned_len > 0:
            corr_refs.append(ref_fpath)
    return corr_refs


# safe remove from quast_py_args, e.g. removes correctly "--test-no" (full is "--test-no-ref") and corresponding argument
def __remove_from_quast_py_args(quast_py_args, opt, arg=None):
    opt_idx = None
    if opt in quast_py_args:
        opt_idx = quast_py_args.index(opt)
    if opt_idx is None:
        common_length = -1
        for idx, o in enumerate(quast_py_args):
            if opt.startswith(o):
                if len(o) > common_length:
                    opt_idx = idx
                    common_length = len(o)
    if opt_idx is not None:
        if arg:
            del quast_py_args[opt_idx + 1]
        del quast_py_args[opt_idx]
    return quast_py_args


def main(args):
    if ' ' in qconfig.QUAST_HOME:
        logger.error('QUAST does not support spaces in paths. \n'
                     'You are trying to run it from ' + str(qconfig.QUAST_HOME) + '\n'
                     'Please, put QUAST in a different directory, then try again.\n',
                     to_stderr=True,
                     exit_with_code=3)

    if not args:
        qconfig.usage(meta=True)
        sys.exit(0)

    genes = []
    operons = []
    html_report = qconfig.html_report
    make_latest_symlink = True
    ref_txt_fpath = None

    try:
        options, contigs_fpaths = getopt.gnu_getopt(args, qconfig.short_options, qconfig.long_options)
    except getopt.GetoptError:
        _, exc_value, _ = sys.exc_info()
        print >> sys.stderr, exc_value
        print >> sys.stderr
        qconfig.usage(meta=True)
        sys.exit(2)

    quast_py_args = args[:]
    test_mode = False

    for opt, arg in options:
        if opt in ('-d', '--debug'):
            options.remove((opt, arg))
            qconfig.debug = True
            logger.set_up_console_handler(debug=True)

        elif opt == '--test' or opt == '--test-no-ref':
            options.remove((opt, arg))
            quast_py_args = __remove_from_quast_py_args(quast_py_args, opt)
            options += [('-o', 'quast_test_output')]
            if opt == '--test':
                options += [('-R', ','.join([os.path.join(qconfig.QUAST_HOME, 'test_data', 'meta_ref_1.fasta'),
                            os.path.join(qconfig.QUAST_HOME, 'test_data', 'meta_ref_2.fasta'),
                            os.path.join(qconfig.QUAST_HOME, 'test_data', 'meta_ref_3.fasta')]))]
            contigs_fpaths += [os.path.join(qconfig.QUAST_HOME, 'test_data', 'meta_contigs_1.fasta'),
                               os.path.join(qconfig.QUAST_HOME, 'test_data', 'meta_contigs_2.fasta')]
            test_mode = True

        elif opt.startswith('--help') or opt == '-h':
            qconfig.usage(opt == "--help-hidden", meta=True, short=False)
            sys.exit(0)

        elif opt.startswith('--version') or opt == '-v':
            qconfig.print_version(meta=True)
            sys.exit(0)

    if not contigs_fpaths:
        logger.error("You should specify at least one file with contigs!\n")
        qconfig.usage(meta=True)
        sys.exit(2)

    ref_fpaths = []
    combined_ref_fpath = ''
    reads_fpath_f = ''
    reads_fpath_r = ''
    bed_fpath = None
    output_dirpath = None

    labels = None
    all_labels_from_dirs = False

    for opt, arg in options:
        if opt in ('-o', "--output-dir"):
            # Removing output dir arg in order to further
            # construct other quast calls from this options
            if opt in quast_py_args and arg in quast_py_args:
                quast_py_args = __remove_from_quast_py_args(quast_py_args, opt, arg)

            output_dirpath = os.path.abspath(arg)
            make_latest_symlink = False

        elif opt in ('-G', "--genes"):
            assert_file_exists(arg, 'genes')
            genes += arg

        elif opt in ('-O', "--operons"):
            assert_file_exists(arg, 'operons')
            operons += arg

        elif opt in ('-R', "--reference"):
            # Removing reference args in order to further
            # construct quast calls from this args with other reference options
            if opt in quast_py_args and arg in quast_py_args:
                quast_py_args = __remove_from_quast_py_args(quast_py_args, opt, arg)
            if os.path.isdir(arg):
                ref_fpaths = [os.path.join(path,file) for (path, dirs, files) in os.walk(arg) for file in files if qutils.check_is_fasta_file(file)]
                ref_fpaths.sort()
            else:
                ref_fpaths = arg.split(',')
                for i, ref_fpath in enumerate(ref_fpaths):
                    assert_file_exists(ref_fpath, 'reference')
                    ref_fpaths[i] = ref_fpath

        elif opt == '--max-ref-number':
            quast_py_args = __remove_from_quast_py_args(quast_py_args, opt, arg)
            qconfig.max_references = int(arg)
            if qconfig.max_references < 0:
                qconfig.max_references = 0

        elif opt in ('-m', "--min-contig"):
            qconfig.min_contig = int(arg)

        elif opt in ('-t', "--threads"):
            qconfig.max_threads = int(arg)
            if qconfig.max_threads < 1:
                qconfig.max_threads = 1

        elif opt in ('-l', '--labels'):
            quast_py_args = __remove_from_quast_py_args(quast_py_args, opt, arg)
            labels = qutils.parse_labels(arg, contigs_fpaths)

        elif opt == '-L':
            quast_py_args = __remove_from_quast_py_args(quast_py_args, opt)
            all_labels_from_dirs = True

        elif opt in ('-j', '--save-json'):
            pass
        elif opt in ('-J', '--save-json-to'):
            pass
        elif opt == '--err-fpath':  # for web-quast
            qconfig.save_error = True
            qconfig.error_log_fpath = arg
        elif opt == "--contig-thresholds":
            pass
        elif opt in ('-c', "--mincluster"):
            pass
        elif opt == "--est-ref-size":
            pass
        elif opt == "--gene-thresholds":
            pass
        elif opt in ('-s', "--scaffolds"):
            qconfig.scaffolds = True
        elif opt == "--gage":
            pass
        elif opt == "--debug":
            pass
        elif opt in ('-e', "--eukaryote"):
            pass
        elif opt in ('-f', "--gene-finding"):
            pass
        elif opt in ('-i', "--min-alignment"):
            pass
        elif opt in ('-c', "--min-cluster"):
            pass
        elif opt in ('-a', "--ambiguity-usage"):
            pass
        elif opt == '--ambiguity-score':
            pass
        elif opt == '--unique-mapping':
            qconfig.unique_mapping = True
            __remove_from_quast_py_args(quast_py_args, opt, arg)
        elif opt in ('-u', "--use-all-alignments"):
            pass
        elif opt == "--strict-NA":
            pass
        elif opt == '--fragmented':
            pass
        elif opt in ('-x', "--extensive-mis-size"):
            pass
        elif opt == "--significant-part-size":
            pass
        elif opt == "--meta":
            pass
        elif opt == '--references-list':
            ref_txt_fpath = arg
        elif opt == '--glimmer':
            pass
        elif opt == '--no-snps':
            pass
        elif opt == '--no-check':
            pass
        elif opt == '--no-gc':
            pass
        elif opt == '--no-sv':
            pass
        elif opt == '--no-plots':
            pass
        elif opt == '--no-html':
            html_report = False
        elif opt == '--no-icarus':
            qconfig.create_icarus_html = False
        elif opt == '--svg':
            pass
        elif opt == '--fast':  # --no-check, --no-gc, --no-snps will automatically set in QUAST runs
            html_report = False
        elif opt == '--plots-format':
            pass
        elif opt == '--memory-efficient':
            pass
        elif opt == '--silent':
            qconfig.silent = True
        elif opt in ('-1', '--reads1'):
            reads_fpath_f = arg
            quast_py_args = __remove_from_quast_py_args(quast_py_args, opt, arg)
        elif opt in ('-2', '--reads2'):
            reads_fpath_r = arg
            quast_py_args = __remove_from_quast_py_args(quast_py_args, opt, arg)
        elif opt == '--sv-bedpe':
            bed_fpath = assert_file_exists(arg, 'BEDPE file with structural variations')
        else:
            logger.error('Unknown option: %s. Use -h for help.' % (opt + ' ' + arg), to_stderr=True, exit_with_code=2)

    for c_fpath in contigs_fpaths:
        assert_file_exists(c_fpath, 'contigs')

    labels = qutils.process_labels(contigs_fpaths, labels, all_labels_from_dirs)

    for contigs_fpath in contigs_fpaths:
        if contigs_fpath in quast_py_args:
            quast_py_args.remove(contigs_fpath)

    # Directories
    output_dirpath, _, _ = qutils.set_up_output_dir(
        output_dirpath, None, make_latest_symlink,
        save_json=False)

    corrected_dirpath = os.path.join(output_dirpath, qconfig.corrected_dirname)

    logger.set_up_file_handler(output_dirpath, qconfig.error_log_fpath if qconfig.save_error else None)
    args = [os.path.realpath(__file__)]
    for k, v in options: args.extend([k, v])
    args.extend(contigs_fpaths)
    logger.print_command_line(args, wrap_after=None)
    logger.start()

    qconfig.set_max_threads(logger)

    ########################################################################

    from libs import reporting
    reload(reporting)
    from libs import plotter

    if os.path.isdir(corrected_dirpath):
        shutil.rmtree(corrected_dirpath)
    os.mkdir(corrected_dirpath)

    # PROCESSING REFERENCES
    if ref_fpaths:
        logger.main_info()
        logger.main_info('Reference(s):')

        corrected_ref_fpaths, combined_ref_fpath, chromosomes_by_refs, ref_names =\
            _correct_meta_references(ref_fpaths, corrected_dirpath)

    # PROCESSING CONTIGS
    logger.main_info()
    logger.main_info('Contigs:')
    assemblies, labels = _correct_assemblies(contigs_fpaths, output_dirpath, labels)
    if not assemblies:
        logger.error("None of the assembly files contains correct contigs. "
                     "Please, provide different files or decrease --min-contig threshold.")
        return 4

    # Running QUAST(s)
    quast_py_args += ['--meta']
    downloaded_refs = False

    # SEARCHING REFERENCES
    if not ref_fpaths:
        logger.main_info()
        if qconfig.max_references == 0:
            logger.notice("Maximum number of references (--max-ref-number) is set to 0, search in SILVA 16S rRNA database is disabled")
        else:
            if ref_txt_fpath:
                logger.main_info("List of references was provided, starting to download reference genomes from NCBI...")
            else:
                logger.main_info("No references are provided, starting to search for reference genomes in SILVA 16S rRNA database "
                        "and to download them from NCBI...")
            downloaded_dirpath = os.path.join(output_dirpath, qconfig.downloaded_dirname)
            if not os.path.isdir(downloaded_dirpath):
                os.mkdir(downloaded_dirpath)
            ref_fpaths = search_references_meta.do(assemblies, labels, downloaded_dirpath, ref_txt_fpath)
            if ref_fpaths:
                search_references_meta.is_quast_first_run = True
                if not ref_txt_fpath:
                    downloaded_refs = True
                logger.main_info()
                logger.main_info('Downloaded reference(s):')
                corrected_ref_fpaths, combined_ref_fpath, chromosomes_by_refs, ref_names =\
                    _correct_meta_references(ref_fpaths, corrected_dirpath)
            elif test_mode and ref_fpaths is None:
                logger.error('Failed to download or setup SILVA 16S rRNA database for working without '
                             'references on metagenome datasets!', to_stderr=True, exit_with_code=4)

    if not ref_fpaths:
        # No references, running regular quast with MetaGenemark gene finder
        logger.main_info()
        logger.notice('No references are provided, starting regular QUAST with MetaGeneMark gene finder')
        _start_quast_main(
            None,
            quast_py_args,
            assemblies=assemblies,
            output_dirpath=output_dirpath,
            exit_on_exception=True)
        exit(0)

    # Running combined reference
    combined_output_dirpath = os.path.join(output_dirpath, qconfig.combined_output_name)

    reads_fpaths = []
    if reads_fpath_f:
        reads_fpaths.append(reads_fpath_f)
    if reads_fpath_r:
        reads_fpaths.append(reads_fpath_r)
    if reads_fpaths:
        bed_fpath, cov_fpath = reads_analyzer.do(combined_ref_fpath, contigs_fpaths, reads_fpaths, corrected_ref_fpaths,
                                      os.path.join(combined_output_dirpath, qconfig.variation_dirname),
                                      external_logger=logger, bed_fpath=bed_fpath)
    if bed_fpath:
        quast_py_args += ['--sv-bed']
        quast_py_args += [bed_fpath]

    for arg in args:
        if arg in ('-s', "--scaffolds"):
            quast_py_args.remove(arg)

    quast_py_args += ['--combined-ref']
    if qconfig.draw_plots or qconfig.html_report:
        if plotter.dict_color_and_ls:
            colors_and_ls = [plotter.dict_color_and_ls[asm.label] for asm in assemblies]
            quast_py_args += ['--colors']
            quast_py_args += [','.join([style[0] for style in colors_and_ls])]
            quast_py_args += ['--ls']
            quast_py_args += [','.join([style[1] for style in colors_and_ls])]
    run_name = 'for the combined reference'
    logger.main_info()
    logger.main_info('Starting quast.py ' + run_name + '...')
    total_num_notices = 0
    total_num_warnings = 0
    total_num_nf_errors = 0
    total_num_notifications = (total_num_notices, total_num_warnings, total_num_nf_errors)
    if qconfig.html_report:
        from libs.html_saver import json_saver
        json_texts = []
    else:
        json_texts = None
    return_code, total_num_notifications, assemblies, labels = \
        _start_quast_main(run_name, quast_py_args + ([] if qconfig.unique_mapping else ["--ambiguity-usage", 'all']),
        assemblies=assemblies,
        reference_fpath=combined_ref_fpath,
        output_dirpath=combined_output_dirpath,
        num_notifications_tuple=total_num_notifications, is_first_run=True)

    if json_texts is not None:
        json_texts.append(json_saver.json_text)
    search_references_meta.is_quast_first_run = False

    genome_info_dirpath = os.path.join(output_dirpath, qconfig.combined_output_name, 'genome_stats')
    genome_info_fpath = os.path.join(genome_info_dirpath, 'genome_info.txt')
    if not os.path.exists(genome_info_fpath):
        logger.main_info('')
        logger.main_info('Failed aligning the contigs for all the references. ' + ('Try to restart MetaQUAST with another references.'
                                                        if not downloaded_refs else 'Try to use option --max-ref-number to change maximum number of references '
                                                                                    '(per each assembly) to download.'))
        logger.main_info('')
        cleanup(corrected_dirpath)
        logger.main_info('MetaQUAST finished.')
        logger.finish_up(numbers=tuple(total_num_notifications), check_test=test_mode)
        return

    if downloaded_refs:
        logger.main_info()
        logger.main_info('Excluding downloaded references with low genome fraction from further analysis..')
        corr_ref_fpaths = remove_unaligned_downloaded_refs(genome_info_fpath, ref_fpaths, chromosomes_by_refs)
        if corr_ref_fpaths and corr_ref_fpaths != ref_fpaths:
            logger.main_info()
            logger.main_info('Filtered reference(s):')
            os.remove(combined_ref_fpath)
            contigs_analyzer.ref_labels_by_chromosomes = {}
            corrected_ref_fpaths, combined_ref_fpath, chromosomes_by_refs, ref_names =\
                    _correct_meta_references(corr_ref_fpaths, corrected_dirpath)
            run_name = 'for the corrected combined reference'
            logger.main_info()
            logger.main_info('Starting quast.py ' + run_name + '...')
            return_code, total_num_notifications, assemblies, labels = \
                _start_quast_main(run_name, quast_py_args + ([] if qconfig.unique_mapping else ["--ambiguity-usage", 'all']),
                assemblies=assemblies,
                reference_fpath=combined_ref_fpath,
                output_dirpath=combined_output_dirpath,
                num_notifications_tuple=total_num_notifications, is_first_run=True)
            if json_texts is not None:
                json_texts = json_texts[:-1]
                json_texts.append(json_saver.json_text)
        elif corr_ref_fpaths == ref_fpaths:
            logger.main_info('All downloaded references have genome fraction more than 10%. Nothing was excluded.')
        else:
            logger.main_info('All downloaded references have low genome fraction. Nothing was excluded for now.')

    quast_py_args += ['--no-check-meta']
    qconfig.contig_thresholds = ','.join([str(threshold) for threshold in qconfig.contig_thresholds if threshold > qconfig.min_contig])
    if not qconfig.contig_thresholds:
        qconfig.contig_thresholds = 'None'
    quast_py_args = __remove_from_quast_py_args(quast_py_args, '--contig-thresholds', qconfig.contig_thresholds)
    quast_py_args += ['--contig-thresholds']
    quast_py_args += [qconfig.contig_thresholds]
    quast_py_args.remove('--combined-ref')

    logger.main_info()
    logger.main_info('Partitioning contigs into bins aligned to each reference..')

    assemblies_by_reference, not_aligned_assemblies = _partition_contigs(
        assemblies, corrected_ref_fpaths, corrected_dirpath,
        os.path.join(combined_output_dirpath, 'contigs_reports', 'alignments_%s.tsv'), labels)

    ref_names = []
    output_dirpath_per_ref = os.path.join(output_dirpath, qconfig.per_ref_dirname)
    for ref_fpath, ref_assemblies in assemblies_by_reference:
        ref_name = qutils.name_from_fpath(ref_fpath)
        logger.main_info('')
        if not ref_assemblies:
            logger.main_info('No contigs were aligned to the reference ' + ref_name + ', skipping..')
        else:
            ref_names.append(ref_name)
            run_name = 'for the contigs aligned to ' + ref_name
            logger.main_info('Starting quast.py ' + run_name)

            return_code, total_num_notifications = _start_quast_main(run_name, quast_py_args,
                assemblies=ref_assemblies,
                reference_fpath=ref_fpath,
                output_dirpath=os.path.join(output_dirpath_per_ref, ref_name),
                exit_on_exception=False, num_notifications_tuple=total_num_notifications)
            if json_texts is not None:
                json_texts.append(json_saver.json_text)

    # Finally running for the contigs that has not been aligned to any reference
    no_unaligned_contigs = True
    for assembly in not_aligned_assemblies:
        if os.path.isfile(assembly.fpath) and os.stat(assembly.fpath).st_size != 0:
            no_unaligned_contigs = False
            break

    run_name = 'for the contigs not aligned anywhere'
    logger.main_info()
    if no_unaligned_contigs:
        logger.main_info('Skipping quast.py ' + run_name + ' (everything is aligned!)')
    else:
        logger.main_info('Starting quast.py ' + run_name + '...')

        return_code, total_num_notifications = _start_quast_main(run_name, quast_py_args,
            assemblies=not_aligned_assemblies,
            output_dirpath=os.path.join(output_dirpath, qconfig.not_aligned_name),
            exit_on_exception=False, num_notifications_tuple=total_num_notifications)

        if return_code not in [0, 4]:
            logger.error('Error running quast.py for the contigs not aligned anywhere')
        elif return_code == 4:  # no unaligned contigs, i.e. everything aligned
            no_unaligned_contigs = True
        if not no_unaligned_contigs:
            if json_texts is not None:
                json_texts.append(json_saver.json_text)

    if ref_names:
        logger.print_timestamp()
        logger.main_info("Summarizing results...")

        summary_output_dirpath = os.path.join(output_dirpath, qconfig.meta_summary_dir)
        if not os.path.isdir(summary_output_dirpath):
            os.makedirs(summary_output_dirpath)
        if html_report and json_texts:
            from libs.html_saver import html_saver
            html_summary_report_fpath = html_saver.init_meta_report(output_dirpath)
        else:
            html_summary_report_fpath = None
        from libs import create_meta_summary
        metrics_for_plots = reporting.Fields.main_metrics
        misassembl_metrics = [reporting.Fields.MIS_RELOCATION, reporting.Fields.MIS_TRANSLOCATION, reporting.Fields.MIS_INVERTION,
                           reporting.Fields.MIS_ISTRANSLOCATIONS]
        create_meta_summary.do(html_summary_report_fpath, summary_output_dirpath, combined_output_dirpath, output_dirpath_per_ref, metrics_for_plots, misassembl_metrics,
                               ref_names if no_unaligned_contigs else ref_names + [qconfig.not_aligned_name])
        if html_report and json_texts:
            html_saver.save_colors(output_dirpath, contigs_fpaths, plotter.dict_color_and_ls, meta=True)
            if qconfig.create_icarus_html:
                icarus_html_fpath = html_saver.create_meta_icarus(output_dirpath, ref_names)
                logger.main_info('  Icarus (contig browser) is saved to %s' % icarus_html_fpath)
            html_saver.create_meta_report(output_dirpath, json_texts)

    cleanup(corrected_dirpath)
    logger.main_info('')
    logger.main_info('MetaQUAST finished.')
    return logger.finish_up(numbers=tuple(total_num_notifications), check_test=test_mode)


if __name__ == '__main__':
    try:
        return_code = main(sys.argv[1:])
        exit(return_code)
    except Exception:
        _, exc_value, _ = sys.exc_info()
        logger.exception(exc_value)
        logger.error('exception caught!', exit_with_code=1, to_stderr=True)


















