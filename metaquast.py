#!/usr/bin/env python

############################################################################
# Copyright (c) 2011-2013 Saint-Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################

from __future__ import with_statement

RELEASE_MODE = False

import getopt
import os
import shutil
import sys
from libs import qconfig, qutils, fastaparser
from libs.qutils import assert_file_exists

from libs.log import get_logger
logger = get_logger('metaquast')
logger.set_up_console_handler(debug=not RELEASE_MODE)

import quast

from site import addsitedir
addsitedir(os.path.join(qconfig.LIBS_LOCATION, 'site_packages'))

COMBINED_REF_FNAME = 'combined_reference.fasta'


class Assembly:
    def __init__(self, fpath, label):
        self.fpath = fpath
        self.label = label
        self.name = os.path.splitext(os.path.basename(self.fpath))[0]


def usage():
    print >> sys.stderr, "Options:"
    print >> sys.stderr, "-o            <dirname>      Directory to store all result file. Default: quast_results/results_<datetime>"
    print >> sys.stderr, "-R            <filename>     Reference genomes (accepts multiple fasta files with multiple sequences each)"
    print >> sys.stderr, "-G  --genes   <filename>     Annotated genes file"
    print >> sys.stderr, "-O  --operons <filename>     Annotated operons file"
    print >> sys.stderr, "--min-contig  <int>          Lower threshold for contig length [default: %s]" % qconfig.min_contig
    print >> sys.stderr, ""
    print >> sys.stderr, "Advanced options:"
    print >> sys.stderr, "-t  --threads <int>               Maximum number of threads [default: number of CPUs]"
    print >> sys.stderr, "-l  --labels \"label, label, ...\"  Names of assemblies to use in reports, comma-separated."
    print >> sys.stderr, "--gage                            Starts GAGE inside QUAST (\"GAGE mode\")"
    print >> sys.stderr, "--contig-thresholds <int,int,..>  Comma-separated list of contig length thresholds [default: %s]" % qconfig.contig_thresholds
    print >> sys.stderr, "--gene-finding                    Uses MetaGeneMark for gene finding"
    print >> sys.stderr, "--gene-thresholds <int,int,..>    Comma-separated list of threshold lengths of genes to search with Gene Finding module"
    print >> sys.stderr, "                                  [default is %s]" % qconfig.genes_lengths
    print >> sys.stderr, "--eukaryote                       Genome is an eukaryote"
    print >> sys.stderr, "--est-ref-size <int>              Estimated reference size (for computing NGx metrics without a reference)"
    print >> sys.stderr, "--scaffolds                       Provided assemblies are scaffolds"
    print >> sys.stderr, "--use-all-alignments              Computes Genome fraction, # genes, # operons metrics in compatible with QUAST v.1.* mode."
    print >> sys.stderr, "                                  By default, QUAST filters Nucmer\'s alignments to keep only best ones"
    print >> sys.stderr, "--ambiguity-usage <none|one|all>  Uses none, one, or all alignments of a contig with multiple equally good alignments."
    print >> sys.stderr, "                                  [default is %s]" % qconfig.ambiguity_usage
    print >> sys.stderr, "--strict-NA                       Breaks contigs by any misassembly event to compute NAx and NGAx."
    print >> sys.stderr, "                                  By default, QUAST breaks contigs only by extensive misassemblies (not local ones)"
    print >> sys.stderr, ""
    print >> sys.stderr, "--test                            Runs QUAST with the data in the test_data folder."
    print >> sys.stderr, "-h  --help                        Prints this message"


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

        with open(alignments_fpath_template % asm.name) as alignments_tsv_f:
            for line in alignments_tsv_f:
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
        output_dirpath=None, exit_on_exception=True):
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
    quast.logger.set_up_console_handler(debug=not RELEASE_MODE, indent_val=1)

    # nested_quast_console_handler = logging.StreamHandler(sys.stdout)
    # nested_quast_console_handler.setFormatter(
    #     LoggingIndentFormatter('%(message)s'))
    # nested_quast_console_handler.setLevel(logging.DEBUG)
    # log.addHandler(nested_quast_console_handler)

    # print 'quast.py ' + ' '.join(args)

    logger.info_to_file('(logging to ' +
                        os.path.join(output_dirpath,
                                     qconfig.LOGGER_DEFAULT_NAME + '.log)'))
    # try:
    return quast.main(args)

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
        label, ctg_fasta_ext = qutils.splitext_for_fasta_file(contigs_fname)

        if labels:
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
    libs_dir = os.path.dirname(qconfig.LIBS_LOCATION)
    if ' ' in libs_dir:
        logger.error(
            'QUAST does not support spaces in paths. \n' + \
            'You are trying to run it from ' + str(libs_dir) + '\n' + \
            'Please, put QUAST in a different directory, then try again.\n',
            to_stderr=True,
            exit_with_code=3)

    min_contig = qconfig.min_contig
    genes = ''
    operons = ''
    draw_plots = qconfig.draw_plots
    html_report = qconfig.html_report
    make_latest_symlink = True

    try:
        options, contigs_fpaths = getopt.gnu_getopt(args, qconfig.short_options, qconfig.long_options)
    except getopt.GetoptError, err:
        print >> sys.stderr, err
        print >> sys.stderr
        usage()
        sys.exit(2)

    for opt, arg in options:
        if opt == '--test':
            test_options = ['test_data/meta_contigs_1.fasta',
                  'test_data/meta_contigs_2.fasta',
                  '-R', 'test_data/meta_ref_1.fasta,test_data/meta_ref_2.fasta,test_data/meta_ref_3.fasta',
                  '-o', 'test_meta_output']

            main(test_options)
            sys.exit(0)

    if not contigs_fpaths:
        usage()
        sys.exit(2)

    ref_fpaths = []
    combined_ref_fpath = ''

    output_dirpath = None

    labels = None

    quast_py_args = args[:]

    for opt, arg in options:
        # Yes, this is a code duplicating. Python's getopt is non well-thought!!
        if opt in ('-o', "--output-dir"):
            # Removing output dir arg in order to further
            # construct other quast calls from this options
            quast_py_args.remove(opt)
            quast_py_args.remove(arg)

            output_dirpath = os.path.abspath(arg)
            make_latest_symlink = False

        elif opt in ('-G', "--genes"):
            assert_file_exists(arg, 'genes')
            genes = arg

        elif opt in ('-O', "--operons"):
            assert_file_exists(arg, 'operons')
            operons = arg

        elif opt in ('-R', "--reference"):
            # Removing reference args in order to further
            # construct quast calls from this args with other reference options
            quast_py_args.remove(opt)
            quast_py_args.remove(arg)

            ref_fpaths = arg.split(',')
            for i, ref_fpath in enumerate(ref_fpaths):
                assert_file_exists(ref_fpath, 'reference')
                ref_fpaths[i] = ref_fpath

        elif opt in ('-M', "--min-contig"):
            min_contig = int(arg)

        elif opt in ('-h', "--help"):
            usage()
            sys.exit(0)

        elif opt in ('-T', "--threads"):
            pass

        elif opt in ('-d', "--debug"):
            RELEASE_MODE = False

        elif opt in ('-l', '--labels'):
            quast_py_args.remove(opt)
            quast_py_args.remove(arg)
            labels = quast.parse_labels(arg, contigs_fpaths)

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
            logger.error('Unknown option: %s. Use -h for help.' % (opt + (' ' + arg) if arg else ''), to_stderr=True)
            sys.exit(2)

    for c_fpath in contigs_fpaths:
        assert_file_exists(c_fpath, 'contigs')

    for contigs_fpath in contigs_fpaths:
        quast_py_args.remove(contigs_fpath)

    # # Removing outout dir if exists
    # if output_dirpath:  # 'output dir was specified with -o option'
    #     if os.path.isdir(output_dirpath):
    #         shutil.rmtree(output_dirpath)

    # Directories
    output_dirpath, _, _ = quast._set_up_output_dir(
        output_dirpath, None, make_latest_symlink,
        save_json=False, remove_old=True)

    corrected_dirpath = os.path.join(output_dirpath, qconfig.corrected_dirname)

    logger.set_up_file_handler(output_dirpath)
    logger.info(' '.join(['metaquast.py'] + args))
    logger.start()

    # Where all pdfs will be saved
    all_pdf_fpath = os.path.join(output_dirpath, qconfig.plots_fname)
    all_pdf = None

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
        logger.info('Reference' + ('s' if len(ref_fpaths) > 0 else '') + ':')

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
        logger.info('No references provided, starting quast.py with MetaGeneMark gene finder')
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

    _start_quast_main(run_name, quast_py_args,
        assemblies=assemblies,
        reference_fpath=combined_ref_fpath,
        output_dirpath=os.path.join(output_dirpath, 'combined_quast_output'))

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

            _start_quast_main(run_name, quast_py_args,
                assemblies=ref_assemblies,
                reference_fpath=os.path.join(corrected_dirpath, ref_name) + common_ref_fasta_ext,
                output_dirpath=os.path.join(output_dirpath, ref_name + '_quast_output'),
                exit_on_exception=False)

    # Finally running for the contigs that has not been aligned to any reference
    run_name = 'for the contigs not alined anywhere'
    logger.info()
    logger.info('Starting quast.py ' + run_name + '...')

    return_code = _start_quast_main(run_name, quast_py_args,
        assemblies=not_aligned_assemblies,
        output_dirpath=os.path.join(output_dirpath, 'not_aligned_quast_output'),
        exit_on_exception=False)

    if return_code not in [0, 4]:
        logger.error('Error running quast.py for the contigs not aligned anywhere')

    quast._cleanup(corrected_dirpath)

    logger.info('')
    logger.info('MetaQUAST finished.')
    logger.finish_up()


if __name__ == '__main__':
    try:
        return_code = main(sys.argv[1:])
        exit(return_code)
    except Exception, e:
        logger.exception(e)


















