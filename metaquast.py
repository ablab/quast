#!/usr/bin/env python

############################################################################
# Copyright (c) 2011-2015 Saint-Petersburg Academic University
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
from libs import search_references_meta
from libs.qutils import assert_file_exists

from libs.log import get_logger
logger = get_logger(qconfig.LOGGER_META_NAME)
logger.set_up_console_handler()

from site import addsitedir
addsitedir(os.path.join(qconfig.LIBS_LOCATION, 'site_packages'))

import quast
from libs import contigs_analyzer

COMBINED_REF_FNAME = 'combined_reference.fasta'


class Assembly:
    def __init__(self, fpath, label):
        self.fpath = fpath
        self.label = label
        self.name = os.path.splitext(os.path.basename(self.fpath))[0]


def parallel_partition_contigs(asm, assemblies_by_ref, corrected_dirpath, alignments_fpath_template):
    logger.info('  ' + 'processing ' + asm.name)
    added_ref_asm = []
    not_aligned_fname = asm.name + '_not_aligned_anywhere.fasta'
    not_aligned_fpath = os.path.join(corrected_dirpath, not_aligned_fname)
    contigs = {}
    aligned_contig_names = set()
    aligned_contigs_for_each_ref = {}
    contigs_seq = fastaparser.read_fasta_one_time(asm.fpath)
    if os.path.exists(alignments_fpath_template % asm.name):
        for line in open(alignments_fpath_template % asm.name):
            values = line.split()
            if values[0] in contigs_analyzer.ref_labels_by_chromosomes.keys():
                ref_name = contigs_analyzer.ref_labels_by_chromosomes[values[0]]
                ref_contigs_names = values[1:]
                ref_contigs_fpath = os.path.join(
                    corrected_dirpath, asm.name + '_to_' + ref_name[:40] + '.fasta')
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

                ref_asm = Assembly(ref_contigs_fpath, asm.label)
                if ref_asm.name not in added_ref_asm:
                    if ref_name in assemblies_by_ref:
                        assemblies_by_ref[ref_name].append(ref_asm)
                        added_ref_asm.append(ref_asm.name)

    # Exctraction not aligned contigs
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
    assemblies_by_ref = {}
    for k in assemblies_dicts[0].keys():
        assemblies_by_ref[k] = []
        not_sorted_assemblies = set([val for sublist in (assemblies_dicts[i][k] for i in range(len(assemblies_dicts))) for val in sublist])
        for label in labels:  # sort by label
            for assembly in not_sorted_assemblies:
                if assembly.label == label:
                    assemblies_by_ref[k].append(assembly)
                    break
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
        output_dirpath=None, exit_on_exception=True, num_notifications_tuple=None):
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


def _correct_contigs(contigs_fpaths, output_dirpath, labels):
    assemblies = [Assembly(contigs_fpaths[i], labels[i]) for i in range(len(contigs_fpaths))]
    corr_assemblies = []
    for (contigs_fpath, label) in zip(contigs_fpaths, labels):
        contigs_fname = os.path.basename(contigs_fpath)
        fname, ctg_fasta_ext = qutils.splitext_for_fasta_file(contigs_fname)

        corr_fpath = qutils.unique_corrected_fpath(
            os.path.join(output_dirpath, qconfig.corrected_dirname, label + ctg_fasta_ext))

        corr_assemblies.append(Assembly(corr_fpath, label))

    return assemblies, corr_assemblies


def get_label_from_par_dir_and_fname(contigs_fpath):
    abspath = os.path.abspath(contigs_fpath)
    name = qutils.rm_extentions_for_fasta_file(os.path.basename(contigs_fpath))
    label = os.path.basename(os.path.dirname(abspath)) + '_' + name
    return label


def _correct_references(ref_fpaths, corrected_dirpath):
    common_ref_fasta_ext = ''

    corrected_ref_fpaths = []

    combined_ref_fpath = os.path.join(corrected_dirpath, COMBINED_REF_FNAME)

    chromosomes_by_refs = {}

    def correct_seq(seq_name, seq, ref_name, ref_fasta_ext, total_references, ref_fpath):
        seq_fname = ref_name
        seq_fname += ref_fasta_ext

        if total_references > 1:
            corr_seq_fpath = corrected_ref_fpaths[-1]
        else:
            corr_seq_fpath = qutils.unique_corrected_fpath(os.path.join(corrected_dirpath, seq_fname))
            corrected_ref_fpaths.append(corr_seq_fpath)
        corr_seq_name = qutils.name_from_fpath(corr_seq_fpath)
        if total_references > 1:
            corr_seq_name += '_' + qutils.correct_name(seq_name[:20])
        if not qconfig.no_check:
            corr_seq = seq.upper()
            dic = {'M': 'N', 'K': 'N', 'R': 'N', 'Y': 'N', 'W': 'N', 'S': 'N', 'V': 'N', 'B': 'N', 'H': 'N', 'D': 'N'}
            pat = "(%s)" % "|".join(map(re.escape, dic.keys()))
            corr_seq = re.sub(pat, lambda m: dic[m.group()], corr_seq)
            if re.compile(r'[^ACGTN]').search(corr_seq):
                logger.warning('Skipping ' + ref_fpath + ' because it contains non-ACGTN characters.',
                        indent='    ')
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
            ref_name = get_label_from_par_dir_and_fname(ref_fpath)

        common_ref_fasta_ext = ref_fasta_ext
        chromosomes_by_refs[ref_name] = []

        corr_seq_fpath = None
        for i, (seq_name, seq) in enumerate(fastaparser.read_fasta(ref_fpath)):
            total_references += 1
            corr_seq_name, corr_seq_fpath = correct_seq(seq_name, seq, ref_name, ref_fasta_ext, total_references, ref_fpath)
            if not corr_seq_name:
                break
        if corr_seq_fpath:
            logger.info('  ' + ref_fpath + ' ==> ' + qutils.name_from_fpath(corr_seq_fpath) + '')

    logger.info('  All references combined in ' + COMBINED_REF_FNAME)

    return corrected_ref_fpaths, common_ref_fasta_ext, combined_ref_fpath, chromosomes_by_refs, ref_fpaths


def remove_unaligned_downloaded_refs(output_dirpath, ref_fpaths, chromosomes_by_refs):
    genome_info_dirpath = os.path.join(output_dirpath, 'combined_quast_output', 'genome_stats')
    genome_info_fpath = os.path.join(genome_info_dirpath, 'genome_info.txt')
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
    draw_plots = qconfig.draw_plots
    html_report = qconfig.html_report
    make_latest_symlink = True

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

        elif opt.startswith('--help'):
            qconfig.usage(opt == "--help-hidden", meta=True)
            sys.exit(0)

    if not contigs_fpaths:
        logger.error("You should specify at least one file with contigs!\n")
        qconfig.usage(meta=True)
        sys.exit(2)

    ref_fpaths = []
    combined_ref_fpath = ''

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
                ref_fpaths = [os.path.join(arg,file) for (path, dirs, files) in os.walk(arg) for file in files if qutils.check_is_fasta_file(file)]
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

        elif opt in ('-M', "--min-contig"):
            qconfig.min_contig = int(arg)

        elif opt in ('-T', "--threads"):
            qconfig.max_threads = int(arg)
            if qconfig.max_threads < 1:
                qconfig.max_threads = 1

        elif opt in ('-l', '--labels'):
            quast_py_args = __remove_from_quast_py_args(quast_py_args, opt, arg)
            labels = quast.parse_labels(arg, contigs_fpaths)

        elif opt == '-L':
            quast_py_args = __remove_from_quast_py_args(quast_py_args, opt)
            all_labels_from_dirs = True

        elif opt in ('-j', '--save-json'):
            pass
        elif opt in ('-J', '--save-json-to'):
            pass
        elif opt in ('-t', "--contig-thresholds"):
            pass
        elif opt in ('-c', "--mincluster"):
            pass
        elif opt == "--est-ref-size":
            pass
        elif opt in ('-S', "--gene-thresholds"):
            pass
        elif opt in ('-s', "--scaffolds"):
            pass
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
        elif opt in ('-u', "--use-all-alignments"):
            pass
        elif opt in ('-n', "--strict-NA"):
            pass
        elif opt in ("-x", "--extensive-mis-size"):
            pass
        elif opt in ("-m", "--meta"):
            pass
        elif opt == '--glimmer':
            pass
        elif opt == '--no-snps':
            pass
        elif opt == '--no-check':
            pass
        elif opt == '--no-gc':
            pass
        elif opt == '--no-plots':
            draw_plots = False
        elif opt == '--no-html':
            html_report = False
        elif opt == '--fast':  # --no-check, --no-gc, --no-snps will automatically set in QUAST runs
            draw_plots = False
            html_report = False
        else:
            logger.error('Unknown option: %s. Use -h for help.' % (opt + ' ' + arg), to_stderr=True, exit_with_code=2)

    for c_fpath in contigs_fpaths:
        assert_file_exists(c_fpath, 'contigs')

    labels = quast.process_labels(contigs_fpaths, labels, all_labels_from_dirs)

    for contigs_fpath in contigs_fpaths:
        if contigs_fpath in quast_py_args:
            quast_py_args.remove(contigs_fpath)

    # Directories
    output_dirpath, _, _ = quast._set_up_output_dir(
        output_dirpath, None, make_latest_symlink,
        save_json=False)

    corrected_dirpath = os.path.join(output_dirpath, qconfig.corrected_dirname)

    logger.set_up_file_handler(output_dirpath)
    args = [os.path.realpath(__file__)]
    for k, v in options: args.extend([k, v])
    args.extend(contigs_fpaths)
    logger.print_command_line(args, wrap_after=None)
    logger.start()

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

    common_ref_fasta_ext = ''

    # PROCESSING REFERENCES

    if ref_fpaths:
        logger.info()
        logger.info('Reference(s):')

        corrected_ref_fpaths, common_ref_fasta_ext, combined_ref_fpath, chromosomes_by_refs, ref_names =\
            _correct_references(ref_fpaths, corrected_dirpath)

    # PROCESSING CONTIGS
    logger.info()
    logger.info('Contigs:')
    assemblies, correct_assemblies = _correct_contigs(contigs_fpaths, output_dirpath, labels)
    if not assemblies:
        logger.error("None of the assembly files contains correct contigs. "
                     "Please, provide different files or decrease --min-contig threshold.")
        return 4

    # Running QUAST(s)
    quast_py_args += ['--meta']
    quast_py_args += ['--combined-ref']
    downloaded_refs = False

    # SEARCHING REFERENCES
    if not ref_fpaths:
        logger.info()
        if qconfig.max_references == 0:
            logger.notice("Maximum number of references (--max-ref-number) is set to 0, search in SILVA rRNA database is disabled")
        else:
            logger.info("No references are provided, starting to search for reference genomes in SILVA rRNA database "
                        "and to download them from NCBI...")
            downloaded_dirpath = os.path.join(output_dirpath, qconfig.downloaded_dirname)
            if not os.path.isdir(downloaded_dirpath):
                os.mkdir(downloaded_dirpath)
            ref_fpaths = search_references_meta.do(assemblies, downloaded_dirpath)
            if ref_fpaths:
                search_references_meta.is_quast_first_run = True
                downloaded_refs = True
                logger.info()
                logger.info('Downloaded reference(s):')
                corrected_ref_fpaths, common_ref_fasta_ext, combined_ref_fpath, chromosomes_by_refs, ref_names =\
                    _correct_references(ref_fpaths, corrected_dirpath)
            elif test_mode:
                logger.error('Failed to download or setup SILVA rRNA database for working without '
                             'references on metagenome datasets!', to_stderr=True, exit_with_code=4)

    if not ref_fpaths:
        # No references, running regular quast with MetaGenemark gene finder
        logger.info()
        logger.notice('No references are provided, starting quast.py with MetaGeneMark gene finder')
        _start_quast_main(
            None,
            quast_py_args,
            assemblies=assemblies,
            output_dirpath=os.path.join(output_dirpath, 'quast_output'),
            exit_on_exception=True)
        exit(0)

    # Running combined reference
    run_name = 'for the combined reference'
    logger.info()
    logger.info('Starting quast.py ' + run_name + '...')
    total_num_notices = 0
    total_num_warnings = 0
    total_num_nf_errors = 0
    total_num_notifications = (total_num_notices, total_num_warnings, total_num_nf_errors)
    if qconfig.html_report:
        from libs.html_saver import json_saver
        json_texts = []
    else:
        json_texts = None
    return_code, total_num_notifications = _start_quast_main(run_name, quast_py_args + ["--ambiguity-usage"] + ['all'],
        assemblies=assemblies,
        reference_fpath=combined_ref_fpath,
        output_dirpath=os.path.join(output_dirpath, 'combined_quast_output'),
        num_notifications_tuple=total_num_notifications)

    if json_texts is not None:
        json_texts.append(json_saver.json_text)
    search_references_meta.is_quast_first_run = False

    if downloaded_refs:
        logger.info()
        logger.info('Excluding downloaded references with low genome fraction from further analysis..')
        corr_ref_fpaths = remove_unaligned_downloaded_refs(output_dirpath, ref_fpaths, chromosomes_by_refs)
        if corr_ref_fpaths and corr_ref_fpaths != ref_fpaths:
            logger.info()
            logger.info('Filtered reference(s):')
            os.remove(combined_ref_fpath)
            corrected_ref_fpaths, common_ref_fasta_ext, combined_ref_fpath, chromosomes_by_refs, ref_names =\
                    _correct_references(corr_ref_fpaths, corrected_dirpath)
            run_name = 'for the corrected combined reference'
            logger.info()
            logger.info('Starting quast.py ' + run_name + '...')
            return_code, total_num_notifications = _start_quast_main(run_name, quast_py_args + ["--ambiguity-usage"] + ['all'],
                assemblies=assemblies,
                reference_fpath=combined_ref_fpath,
                output_dirpath=os.path.join(output_dirpath, 'combined_quast_output'),
                num_notifications_tuple=total_num_notifications)
            if json_texts is not None:
                json_texts = json_texts[:-1]
                json_texts.append(json_saver.json_text)
        elif corr_ref_fpaths == ref_fpaths:
            logger.info('All downloaded references have genome fraction more than 10%. Nothing was excluded.')
        else:
            logger.info('All downloaded references have low genome fraction. Nothing was excluded for now.')

    quast_py_args += ['--no-check-meta']
    assemblies = correct_assemblies
    qconfig.contig_thresholds = [str(threshold) for threshold in qconfig.contig_thresholds if threshold > qconfig.min_contig]
    if not qconfig.contig_thresholds:
        qconfig.contig_thresholds = ['None']
    quast_py_args += ['-t']
    quast_py_args += qconfig.contig_thresholds
    quast_py_args.remove('--combined-ref')

    logger.info()
    logger.info('Partitioning contigs into bins aligned to each reference..')

    assemblies_by_reference, not_aligned_assemblies = _partition_contigs(
        assemblies, corrected_ref_fpaths, corrected_dirpath,
        os.path.join(output_dirpath, 'combined_quast_output', 'contigs_reports', 'alignments_%s.tsv'), labels)

    ref_names = []
    for ref_name, ref_assemblies in assemblies_by_reference.iteritems():
        logger.info('')
        if not ref_assemblies:
            logger.info('No contigs were aligned to the reference ' + ref_name + ', skipping..')
        else:
            ref_names.append(ref_name)
            run_name = 'for the contigs aligned to ' + ref_name
            logger.info('Starting quast.py ' + run_name)

            return_code, total_num_notifications = _start_quast_main(run_name, quast_py_args,
                assemblies=ref_assemblies,
                reference_fpath=os.path.join(corrected_dirpath, ref_name) + common_ref_fasta_ext,
                output_dirpath=os.path.join(output_dirpath, ref_name + '_quast_output'),
                exit_on_exception=False, num_notifications_tuple=total_num_notifications)
            if json_texts is not None:
                json_texts.append(json_saver.json_text)

    # Finally running for the contigs that has not been aligned to any reference
    run_name = 'for the contigs not aligned anywhere'
    logger.info()
    logger.info('Starting quast.py ' + run_name + '...')

    return_code, total_num_notifications = _start_quast_main(run_name, quast_py_args,
        assemblies=not_aligned_assemblies,
        output_dirpath=os.path.join(output_dirpath, qconfig.not_aligned_name + '_quast_output'),
        exit_on_exception=False, num_notifications_tuple=total_num_notifications)
    if json_texts is not None:
        json_texts.append(json_saver.json_text)

    if return_code not in [0, 4]:
        logger.error('Error running quast.py for the contigs not aligned anywhere')

    if ref_names:
        logger.print_timestamp()
        logger.info("Summarizing results...")
        summary_dirpath = os.path.join(output_dirpath, 'summary')
        if not os.path.isdir(summary_dirpath):
            os.mkdir(summary_dirpath)
        if draw_plots:
            from libs import create_meta_summary
            metrics_for_plots = reporting.Fields.main_metrics
            misassembl_metrics = [reporting.Fields.MIS_RELOCATION, reporting.Fields.MIS_TRANSLOCATION, reporting.Fields.MIS_INVERTION,
                               reporting.Fields.MIS_ISTRANSLOCATIONS]
            create_meta_summary.do(output_dirpath, summary_dirpath, labels, metrics_for_plots, misassembl_metrics, ref_names)
        if html_report and json_texts:
            from libs.html_saver import html_saver
            html_saver.create_meta_report(summary_dirpath, json_texts)

    quast._cleanup(corrected_dirpath)
    logger.info('')
    logger.info('MetaQUAST finished.')
    logger.finish_up(numbers=tuple(total_num_notifications), check_test=test_mode)


if __name__ == '__main__':
    try:
        return_code = main(sys.argv[1:])
        exit(return_code)
    except Exception:
        _, exc_value, _ = sys.exc_info()
        logger.exception(exc_value)
        logger.error('exception caught!', exit_with_code=1, to_stderr=True)


















