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

quast_dirpath = os.path.abspath(os.path.dirname(os.path.realpath(__file__)))
sys.path.append(os.path.join(quast_dirpath, 'libs'))
from libs import qconfig
qconfig.check_python_version()

from libs import qutils, fastaparser
from libs.qutils import assert_file_exists

from libs.log import get_logger
logger = get_logger('metaquast')
logger.set_up_console_handler()

from site import addsitedir
addsitedir(os.path.join(quast_dirpath, 'libs', 'site_packages'))

import quast

COMBINED_REF_FNAME = 'combined_reference.fasta'


class Assembly:
    def __init__(self, fpath, label):
        self.fpath = fpath
        self.label = label
        self.name = os.path.splitext(os.path.basename(self.fpath))[0]


def _partition_contigs(assemblies, ref_fpaths, corrected_dirpath, alignments_fpath_template):
    # not_aligned_anywhere_dirpath = os.path.join(output_dirpath, 'contigs_not_aligned_anywhere')
    # if os.path.isdir(not_aligned_anywhere_dirpath):
    #     os.rmdir(not_aligned_anywhere_dirpath)
    # os.mkdir(not_aligned_anywhere_dirpath)

    not_aligned_assemblies = []
    # array of assemblies for each reference
    assemblies_by_ref = dict([(qutils.name_from_fpath(ref_fpath), []) for ref_fpath in ref_fpaths])

    for asm in assemblies:
        not_aligned_fname = asm.name + '_not_aligned_anywhere.fasta'
        not_aligned_fpath = os.path.join(corrected_dirpath, not_aligned_fname)
        contigs = {}
        aligned_contig_names = set()

        for line in open(alignments_fpath_template % asm.name):
            values = line.split()
            ref_name = values[0]
            ref_contigs_names = values[1:]
            ref_contigs_fpath = os.path.join(
                corrected_dirpath, asm.name + '_to_' + ref_name[:40] + '.fasta')

            for (cont_name, seq) in fastaparser.read_fasta(asm.fpath):
                if not cont_name in contigs.keys():
                    contigs[cont_name] = seq

                if cont_name in ref_contigs_names:
                    # Collecting all aligned contigs names in order to futher extract not-aligned
                    aligned_contig_names.add(cont_name)
                    fastaparser.write_fasta(ref_contigs_fpath, [(cont_name, seq)], 'a')

            ref_asm = Assembly(ref_contigs_fpath, asm.label)
            assemblies_by_ref[ref_name].append(ref_asm)

        # Exctraction not aligned contigs
        all_contigs_names = set(contigs.keys())
        not_aligned_contigs_names = all_contigs_names - aligned_contig_names
        fastaparser.write_fasta(not_aligned_fpath, [(name, contigs[name]) for name in not_aligned_contigs_names])

        not_aligned_asm = Assembly(not_aligned_fpath, asm.label)
        not_aligned_assemblies.append(not_aligned_asm)

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


def _correct_contigs(contigs_fpaths, corrected_dirpath, min_contig, labels):
    assemblies = []

    for i, contigs_fpath in enumerate(contigs_fpaths):
        contigs_fname = os.path.basename(contigs_fpath)
        fname, ctg_fasta_ext = qutils.splitext_for_fasta_file(contigs_fname)

        label = labels[i]

        corr_fpath = qutils.unique_corrected_fpath(
            os.path.join(corrected_dirpath, label + ctg_fasta_ext))

        assembly = Assembly(corr_fpath, label)

        logger.info('  %s ==> %s' % (contigs_fpath, label))

        # Handle fasta
        lengths = fastaparser.get_lengths_from_fastafile(contigs_fpath)
        if not sum(l for l in lengths if l >= min_contig):
            logger.warning("Skipping %s because it doesn't contain contigs >= %d bp."
                           % (os.path.basename(contigs_fpath), min_contig))
            continue

        # correcting
        if not quast.correct_fasta(contigs_fpath, corr_fpath, min_contig):
            continue

        assemblies.append(assembly)

    return assemblies


def _correct_refrences(ref_fpaths, corrected_dirpath):
    common_ref_fasta_ext = ''

    corrected_ref_fpaths = []

    combined_ref_fpath = os.path.join(corrected_dirpath, COMBINED_REF_FNAME)

    def correct_seq(seq_name, seq, ref_name, ref_fasta_ext, total_references):
        seq_fname = ref_name
        if total_references > 1:
            seq_fname += '_' + qutils.correct_name(seq_name[:20])
        seq_fname += ref_fasta_ext

        corr_seq_fpath = qutils.unique_corrected_fpath(os.path.join(corrected_dirpath, seq_fname))
        corr_seq_name = qutils.name_from_fpath(corr_seq_fpath)

        corrected_ref_fpaths.append(corr_seq_fpath)

        fastaparser.write_fasta(corr_seq_fpath, [(corr_seq_name, seq)], 'a')
        fastaparser.write_fasta(combined_ref_fpath, [(corr_seq_name, seq)], 'a')

        return corr_seq_name

    for ref_fpath in ref_fpaths:
        total_references = 0
        for _ in fastaparser.read_fasta(ref_fpath):
            total_references += 1

        if total_references > 1:
            logger.info('  ' + ref_fpath + ':')

        ref_fname = os.path.basename(ref_fpath)
        ref_name, ref_fasta_ext = qutils.splitext_for_fasta_file(ref_fname)
        common_ref_fasta_ext = ref_fasta_ext

        for i, (seq_name, seq) in enumerate(fastaparser.read_fasta(ref_fpath)):
            corr_seq_name = correct_seq(seq_name, seq, ref_name, ref_fasta_ext, total_references)
            if total_references > 1:
                logger.info('    ' + corr_seq_name + '\n')
            else:
                logger.info('  ' + ref_fpath + ' ==> ' + corr_seq_name + '')

    logger.info('  All references combined in ' + COMBINED_REF_FNAME)

    return corrected_ref_fpaths, common_ref_fasta_ext, combined_ref_fpath


def main(args):
    if ' ' in quast_dirpath:
        logger.error('QUAST does not support spaces in paths. \n'
                     'You are trying to run it from ' + str(quast_dirpath) + '\n'
                     'Please, put QUAST in a different directory, then try again.\n',
                     to_stderr=True,
                     exit_with_code=3)

    if not args:
        qconfig.usage(meta=True)
        sys.exit(0)

    min_contig = qconfig.min_contig
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

        elif opt == '--test':
            options.remove((opt, arg))
            quast_py_args.remove(opt)
            options += [('-o', 'quast_test_output'),
                        ('-R', 'test_data/meta_ref_1.fasta,'
                               'test_data/meta_ref_2.fasta,'
                               'test_data/meta_ref_3.fasta')]
            contigs_fpaths += ['test_data/meta_contigs_1.fasta',
                               'test_data/meta_contigs_2.fasta']
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
                quast_py_args.remove(opt)
                quast_py_args.remove(arg)

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
                quast_py_args.remove(opt)
                quast_py_args.remove(arg)

            ref_fpaths = arg.split(',')
            for i, ref_fpath in enumerate(ref_fpaths):
                assert_file_exists(ref_fpath, 'reference')
                ref_fpaths[i] = ref_fpath

        elif opt in ('-M', "--min-contig"):
            min_contig = int(arg)

        elif opt in ('-T', "--threads"):
            pass

        elif opt in ('-l', '--labels'):
            quast_py_args.remove(opt)
            quast_py_args.remove(arg)
            labels = quast.parse_labels(arg, contigs_fpaths)

        elif opt == '-L':
            quast_py_args.remove(opt)
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
        elif opt in ('-a', "--ambiguity-usage"):
            pass
        elif opt in ('-u', "--use-all-alignments"):
            pass
        elif opt in ('-n', "--strict-NA"):
            pass
        elif opt in ("-m", "--meta"):
            pass
        elif opt in ["--no-plots"]:
            pass
        elif opt in ["--no-html"]:
            pass
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
    logger.print_command_line([os.path.realpath(__file__)] + args, wrap_after=None)
    logger.start()

    ########################################################################

    from libs import reporting
    reload(reporting)

    if os.path.isdir(corrected_dirpath):
        shutil.rmtree(corrected_dirpath)
    os.mkdir(corrected_dirpath)

    # PROCESSING REFERENCES
    common_ref_fasta_ext = ''

    if ref_fpaths:
        logger.info()
        logger.info('Reference(s):')

        ref_fpaths, common_ref_fasta_ext, combined_ref_fpath =\
            _correct_refrences(ref_fpaths, corrected_dirpath)

    # PROCESSING CONTIGS
    logger.info()
    logger.info('Contigs:')
    assemblies = _correct_contigs(contigs_fpaths, corrected_dirpath, min_contig, labels)

    if not assemblies:
        logger.error("None of the assembly files contains correct contigs. "
                     "Please, provide different files or decrease --min-contig threshold.")
        return 4

    # Running QUAST(s)
    quast_py_args += ['--meta']

    if not ref_fpaths:
        # No references, running regular quast with MetaGenemark gene finder
        logger.info()
        logger.notice('No references provided, starting quast.py with MetaGeneMark gene finder')
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

    return_code, total_num_notifications = _start_quast_main(run_name, quast_py_args,
        assemblies=assemblies,
        reference_fpath=combined_ref_fpath,
        output_dirpath=os.path.join(output_dirpath, 'combined_quast_output'),
        num_notifications_tuple=total_num_notifications)

    # Partitioning contigs into bins aligned to each reference
    assemblies_by_reference, not_aligned_assemblies = _partition_contigs(
        assemblies, ref_fpaths, corrected_dirpath,
        os.path.join(output_dirpath, 'combined_quast_output', 'contigs_reports', 'alignments_%s.tsv'))

    for ref_name, ref_assemblies in assemblies_by_reference.iteritems():
        logger.info('')
        if not ref_assemblies:
            logger.info('No contigs were aligned to the reference ' + ref_name)
        else:
            run_name = 'for the contigs aligned to ' + ref_name
            logger.info('Starting quast.py ' + run_name)

            return_code, total_num_notifications = _start_quast_main(run_name, quast_py_args,
                assemblies=ref_assemblies,
                reference_fpath=os.path.join(corrected_dirpath, ref_name) + common_ref_fasta_ext,
                output_dirpath=os.path.join(output_dirpath, ref_name + '_quast_output'),
                exit_on_exception=False, num_notifications_tuple=total_num_notifications)

    # Finally running for the contigs that has not been aligned to any reference
    run_name = 'for the contigs not alined anywhere'
    logger.info()
    logger.info('Starting quast.py ' + run_name + '...')

    return_code, total_num_notifications = _start_quast_main(run_name, quast_py_args,
        assemblies=not_aligned_assemblies,
        output_dirpath=os.path.join(output_dirpath, 'not_aligned_quast_output'),
        exit_on_exception=False, num_notifications_tuple=total_num_notifications)

    if return_code not in [0, 4]:
        logger.error('Error running quast.py for the contigs not aligned anywhere')

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
        logger.error('exception caught!')


















