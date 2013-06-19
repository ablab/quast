from __future__ import with_statement
from __future__ import with_statement
import getopt
import os
import shutil
import sys
from libs import qconfig, qutils, fastaparser
from libs.qutils import assert_file_exists, print_version, print_system_info, print_timestamp, notice, warning, error
import quast

from site import addsitedir
addsitedir(os.path.join(qconfig.LIBS_LOCATION, 'site_packages'))

import logging
log = logging.getLogger('meta_quast')

combined_ref_fname = 'combined_reference.fasta'


def usage():
    print >> sys.stderr, "Options:"
    print >> sys.stderr, "-o            <dirname>      Directory to store all result file. Default: quast_results/results_<datetime>"
    print >> sys.stderr, "-R            <filename>     Reference genomes (accepts multiple fasta files with multiple sequences each)"
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
    print >> sys.stderr, "--gene-finding                    Uses MetaGeneMark for gene finding"
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
    print >> sys.stderr, "--ambiguity-usage <none|one|all>  Uses none, one, or all alignments of a contig with multiple equally good alignments."
    print >> sys.stderr, "                                  [default is %s]" % qconfig.ambiguity_usage
    print >> sys.stderr, ""
    print >> sys.stderr, "--strict-NA                       Breaks contigs by any misassembly event to compute NAx and NGAx."
    print >> sys.stderr, "                                  By default, QUAST breaks contigs only by extensive misassemblies (not local ones)"
    print >> sys.stderr, ""
    print >> sys.stderr, "-h  --help                        Prints this message"


def partition_contigs(contigs_fpaths, ref_fpaths, corrected_dirpath, alignments_fpath):
    # not_aligned_anywhere_dirpath = os.path.join(output_dirpath, 'contigs_not_aligned_anywhere')
    # if os.path.isdir(not_aligned_anywhere_dirpath):
    #     os.rmdir(not_aligned_anywhere_dirpath)
    # os.mkdir(not_aligned_anywhere_dirpath)

    not_aligned_fpaths = []
    # array of fpaths for each reference
    partitions = dict([(os.path.basename(ref_fpath), []) for ref_fpath in ref_fpaths])

    for contigs_fpath in contigs_fpaths:
        contigs_path, ext = os.path.splitext(contigs_fpath)
        contigs_name = os.path.basename(contigs_path)
        not_aligned_fpath = contigs_path + '_not_aligned_anywhere' + ext
        contigs = {}
        aligned_contig_names = set()

        alignments_tsv_fpath = alignments_fpath % os.path.splitext(os.path.basename(contigs_fpath))[0]
        with open(alignments_tsv_fpath) as f:
            for line in f.readlines():
                values = line.split()
                ref_name = values[0]
                ref_contigs_names = values[1:]
                ref_contigs_fpath = os.path.join(corrected_dirpath, contigs_name + '_to_' + ref_name)

                for (cont_name, seq) in fastaparser.read_fasta(contigs_fpath):
                    if not cont_name in contigs.keys():
                        contigs[cont_name] = seq

                    if cont_name in ref_contigs_names:
                        # Collecting all aligned contigs names in order to futher extract not-aligned
                        aligned_contig_names.add(cont_name)
                        fastaparser.write_fasta(ref_contigs_fpath, [(cont_name, seq)], 'a')

                partitions[ref_name].append(ref_contigs_fpath)

        # Exctraction not aligned contigs
        all_contigs_names = set(contigs.keys())
        not_aligned_contigs_names = all_contigs_names - aligned_contig_names
        print 'not_aligned_contigs_names'
        print not_aligned_contigs_names
        fastaparser.write_fasta(not_aligned_fpath, [(name, contigs[name]) for name in not_aligned_contigs_names])

        not_aligned_fpaths.append(not_aligned_fpath)

    return partitions, not_aligned_fpaths


def main(args):
    quast_dir = os.path.dirname(qconfig.LIBS_LOCATION)
    if ' ' in quast_dir:
        qutils.error('QUAST does not support spaces in paths. \n' + \
                     'You are trying to run it from ' + str(quast_dir) + '\n' + \
                     'Please, put QUAST in a different directory, then try again.\n',
                     console_output=True,
                     exit_with_code=3)

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

    ref_fpaths = []
    combined_ref_fpath = ''

    json_outputpath = None
    output_dirpath = None

    for opt, arg in options:
        # Yes, this is a code duplicating. Python's getopt is non well-thought!!
        if opt in ('-o', "--output-dir"):
            # Removing output dir arg in order to further
            # construct other quast calls from this options
            args.remove('-o')
            args.remove(arg)

            output_dirpath = os.path.abspath(arg)
            qconfig.make_latest_symlink = False

        elif opt in ('-G', "--genes"):
            assert_file_exists(arg, 'genes')
            qconfig.genes = arg

        elif opt in ('-O', "--operons"):
            assert_file_exists(arg, 'operons')
            qconfig.operons = arg

        elif opt in ('-R', "--reference"):
            # Removing reference args in order to further
            # construct quast calls from this args with other reference options
            args.remove('-R')
            args.remove(arg)

            ref_fpaths = arg.split(',')
            for i, ref_fpath in enumerate(ref_fpaths):
                assert_file_exists(ref_fpath, 'reference')
                ref_fpaths[i] = ref_fpath

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

        elif opt in ('-d', "--debug"):
            qconfig.debug = True

        elif opt in ('-h', "--help"):
            usage()
            sys.exit(0)
        else:
            raise ValueError

    for c_fpath in contigs_fpaths:
        assert_file_exists(c_fpath, 'contigs')

    for contigs_fpath in contigs_fpaths:
        args.remove(contigs_fpath)

    # # Removing outout dir if exists
    # if output_dirpath:  # 'output dir was specified with -o option'
    #     if os.path.isdir(output_dirpath):
    #         shutil.rmtree(output_dirpath)

    # Directories
    output_dirpath, json_outputpath, existing_alignments = \
        quast.set_up_output_dir(output_dirpath, json_outputpath)

    corrected_dirpath = os.path.join(output_dirpath, qconfig.corrected_dirname)

    logfile, handlers = quast.set_up_log(log, output_dirpath)

    command_line = ''
    for v in sys.argv:
        command_line += str(v) + ' '
    print_version(log)
    log.info('')
    print_system_info(log)

    start_time = print_timestamp('Started: ', log)
    log.info('')
    log.info('Logging to ' + logfile)
    log.info('')

    qconfig.contig_thresholds = map(int, qconfig.contig_thresholds.split(","))

    qconfig.genes_lengths = map(int, qconfig.genes_lengths.split(","))

    # Threadding
    if qconfig.max_threads is None:
        try:
            import multiprocessing
            qconfig.max_threads = multiprocessing.cpu_count()
        except:
            warning('Failed to determine the number of CPUs', log=log)
            qconfig.max_threads = qconfig.DEFAULT_MAX_THREADS
        notice('Maximum number of threads was set to ' +
               str(qconfig.max_threads) +
               ' (use --threads option to set it manually)',
               log=log)

    # Where all pdfs will be saved
    all_pdf_filename = os.path.join(output_dirpath, qconfig.plots_filename)
    all_pdf = None

    ########################################################################

    from libs import reporting
    reload(reporting)

    if os.path.isdir(corrected_dirpath):
        shutil.rmtree(corrected_dirpath)
    os.mkdir(corrected_dirpath)

    # PROCESSING REFERENCES
    if ref_fpaths:
        log.info('')
        log.info('Processing references...')

        corrected_ref_fpaths = []

        combined_ref_fpath = os.path.join(corrected_dirpath, combined_ref_fname)

        for ref_fpath in ref_fpaths:
            ref_fname = os.path.basename(ref_fpath)
            ref_name, ext = os.path.splitext(ref_fname)
            corr_name = qutils.correct_name(ref_name)

            for i, (name, seq) in enumerate(fastaparser.read_fasta(ref_fpath)):
                corr_fname = corr_name + '_' + qutils.correct_name(name) + ext
                corr_fpath = os.path.join(corrected_dirpath, corr_fname)
                corrected_ref_fpaths.append(corr_fpath)

                fastaparser.write_fasta(corr_fpath, [(corr_fname, seq)], 'a')
                fastaparser.write_fasta(combined_ref_fpath, [(corr_fname, seq)], 'a')
                log.info('\t' + corr_fname + '\n')

        log.info('\tAll references combined in ' + combined_ref_fname)
        ref_fpaths = corrected_ref_fpaths

    ## removing from contigs' names special characters because:
    ## 1) Some embedded tools can fail on some strings with "...", "+", "-", etc
    ## 2) Nucmer fails on names like "contig 1_bla_bla", "contig 2_bla_bla" (it interprets as a contig's name only the first word of caption and gets ambiguous contigs names)
    log.info('')
    log.info('Processing contigs...')
    new_contigs_fpaths = []
    for id, contigs_fpath in enumerate(contigs_fpaths):
        contigs_fname = os.path.basename(contigs_fpath)
        corr_fname = quast.corrected_fname_for_nucmer(contigs_fname)
        corr_fpath = os.path.join(corrected_dirpath, corr_fname)
        if os.path.isfile(corr_fpath):  # in case of files with equal names
            i = 1
            basename, ext = os.path.splitext(corr_fname)
            while os.path.isfile(corr_fpath):
                i += 1
                corr_fpath = os.path.join(corrected_dirpath, os.path.basename(basename + '__' + str(i)) + ext)

        log.info('\t%s ==> %s' % (contigs_fpath, os.path.basename(corr_fpath)))

        # Handle fasta
        lengths = fastaparser.get_lengths_from_fastafile(contigs_fpath)
        if not sum(l for l in lengths if l >= qconfig.min_contig):
            warning("Skipping %s because it doesn't contain contigs >= %d bp."
                    % (os.path.basename(contigs_fpath), qconfig.min_contig),
                    log=log)
            continue

        # correcting
        if not quast.correct_fasta(contigs_fpath, corr_fpath):
            continue

        new_contigs_fpaths.append(corr_fpath)

        log.info('')

    contigs_fpaths = new_contigs_fpaths

    qconfig.assemblies_num = len(contigs_fpaths)

    if not contigs_fpaths:
        error("None of assembly file contain correct contigs. "
              "Please, provide different files or decrease --min-contig threshold.",
              exit_with_code=4)

    # End of processing
    log.info('Done.')

    # Running QUAST(s)
    args += ['--meta']

    if not ref_fpaths:
        # No references, running regular quast with MetaGenemark gene finder
        reload(quast)
        quast.main(args)
        args.append('-o')
        args.append(output_dirpath)
        log.info('No references provided, running quast.py '
                 'with MetaGenemark gene finder')
        quast.main(args)
        exit(0)

    # Running combined reference
    comb_args = args[:]
    comb_args.append('-o')
    comb_args.append(os.path.join(output_dirpath, 'combined_quast_output'))
    comb_args.append('-R')
    comb_args.append(combined_ref_fpath)
    comb_args.extend(contigs_fpaths)
    log.info('')
    log.info('Starting quast.py for the combined reference')
    reload(quast)
    print 'quast.py ' + ' '.join(args)
    try:
        quast.main(comb_args)
    except Exception, e:
        print e.message
        exit(10)

    # Partitioning contigs into bins aligned to each reference
    partitions, not_aligned_fpaths = partition_contigs(
        contigs_fpaths, ref_fpaths, corrected_dirpath,
        os.path.join(output_dirpath, 'combined_quast_output', 'contigs_reports', 'alignments_%s.tsv'))

    for partition_contigs_fpaths, ref_fpath in zip(partitions, ref_fpaths):
        log.info('')
        partition_name = os.path.splitext(os.path.basename(ref_fpath))[0]

        log.info('Starting quast.py for the reference aligned to ' + partition_name)
        partition_args = args[:]
        partition_args.append('-o')
        partition_args.append(os.path.join(output_dirpath, partition_name + '_quast_output'))
        partition_args.append('-R')
        partition_args.append(ref_fpath)
        ref_fname = os.path.basename(ref_fpath)
        partition_args.extend(partitions[ref_fname])
        reload(quast)
        print 'quast.py' + ' '.join(args)
        try:
            quast.main(partition_args)
        except Exception, e:
            print e.message

    log.info('Starting quast.py for not alined contigs')
    not_aligned_args = args[:]
    not_aligned_args.append('-o')
    not_aligned_args.append(os.path.join(output_dirpath, 'not_aligned_quast_output'))
    not_aligned_args.extend(not_aligned_fpaths)
    reload(quast)
    print 'quast.py' + ' '.join(args)
    try:
        quast.main(not_aligned_args)
    except:
        pass

    # quast.cleanup(corrected_dirpath)
    log.info('')
    log.info('MetaQUAST finished.')

    log.info('')
    log.info("MetaQUAST log was saved to " + logfile)

    finish_time = print_timestamp("Finished: ")
    log.info("Elapsed time: " + str(finish_time - start_time))

if __name__ == '__main__':
    main(sys.argv[1:])


















