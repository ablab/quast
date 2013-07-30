#!/usr/bin/env python

############################################################################
# Copyright (c) 2011-2013 Saint-Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################

RELEASE_MODE = False

import sys
import os
import shutil
import re
import getopt
from site import addsitedir
from libs import qconfig, qutils

from libs.log import get_logger
logger = get_logger(qconfig.LOGGER_DEFAULT_NAME)
logger.set_up_console_handler(debug=not RELEASE_MODE)

from libs.qutils import assert_file_exists
from libs import fastaparser
from libs.html_saver import json_saver

addsitedir(os.path.join(qconfig.LIBS_LOCATION, 'site_packages'))


def _usage():
    print >> sys.stderr, 'QUAST: QUality ASsessment Tool for Genome Assemblies'
    logger.print_version(to_stderr=True)

    print >> sys.stderr, ""
    print >> sys.stderr, 'Usage: python', sys.argv[0], '[options] <files_with_contigs>'
    print >> sys.stderr, ""

    if RELEASE_MODE:
        print >> sys.stderr, "Options:"
        print >> sys.stderr, "-o  --output-dir  <dirname>   Directory to store all result files [default: quast_results/results_<datetime>]"
        print >> sys.stderr, "-R                <filename>  Reference genome file"
        print >> sys.stderr, "-G  --genes       <filename>  File with gene coordiantes in the reference"
        print >> sys.stderr, "-O  --operons     <filename>  File with operon coordiantes in the reference"
        print >> sys.stderr, "    --min-contig  <int>       Lower threshold for contig length [default: %s]" % qconfig.min_contig
        print >> sys.stderr, ""
        print >> sys.stderr, "Advanced options:"
        print >> sys.stderr, "-T  --threads      <int>              Maximum number of threads [default: number of CPUs]"
        print >> sys.stderr, "-l  --labels \"label, label, ...\"      Names of assemblies to use in reports, comma-separated. If contain spaces, use quotes"
        print >> sys.stderr, "-L                                    Take assembly names from their parent directory names"
        print >> sys.stderr, "-f  --gene-finding                    Predict genes (with GeneMarkS from prokaryotes (default), GlimmerHMM"
        print >> sys.stderr, "                                      for eukaryotes (--eukaryote), or MetaGeneMark for metagenomes (--meta)"
        print >> sys.stderr, "-S  --gene-thresholds                 Comma-separated list of threshold lengths of genes to search with Gene Finding module"
        print >> sys.stderr, "                                      [default is %s]" % qconfig.genes_lengths
        print >> sys.stderr, "-e  --eukaryote                       Genome is eukaryotic"
        print >> sys.stderr, "-m  --meta                            Metagenomic assembly. Use MetaGeneMark for gene prediction. "
        print >> sys.stderr, "    --est-ref-size <int>              Estimated reference size (for computing NGx metrics without a reference)"
        print >> sys.stderr, "    --gage                            Use GAGE (results are in gage_report.txt)"
        print >> sys.stderr, "-t  --contig-thresholds               Comma-separated list of contig length thresholds [default: %s]" % qconfig.contig_thresholds
        print >> sys.stderr, "-s  --scaffolds                       Assemblies are scaffolds, split them and add contigs to the comparison"
        print >> sys.stderr, "-u  --use-all-alignments              Compute genome fraction, # genes, # operons in the v.1.0-1.3 style."
        print >> sys.stderr, "                                      By default, QUAST filters Nucmer\'s alignments to keep only best ones"
        print >> sys.stderr, "-a  --ambiguity-usage <none|one|all>  Use none, one, or all alignments of a contig with multiple equally "
        print >> sys.stderr, "                                      good alignments [default is %s]" % qconfig.ambiguity_usage
        print >> sys.stderr, "-n  --strict-NA                       Break contigs in any misassembly event when compute NAx and NGAx"
        print >> sys.stderr, "                                      By default, QUAST breaks contigs only by extensive misassemblies (not local ones)"
        print >> sys.stderr, ""
        print >> sys.stderr, "    --test                            Run QUAST on the data from the test_data folder, output to test_output"
        print >> sys.stderr, "-h  --help                            Print this message"

    else:
        print >> sys.stderr, 'Options with arguments'
        print >> sys.stderr, "-o  --output-dir   <dirname>          Directory to store all result files [default: quast_results/results_<datetime>]"
        print >> sys.stderr, "-R                 <filename>         Reference genome file"
        print >> sys.stderr, "-G  --genes        <filename>         File with gene coordiantes in the reference"
        print >> sys.stderr, "-O  --operons      <filename>         File with operon coordiantes in the reference"
        print >> sys.stderr, "-M  --min-contig   <int>              Lower threshold for contig length [default: %s]" % qconfig.min_contig
        print >> sys.stderr, "-t  --contig-thresholds               Comma-separated list of contig length thresholds [default: %s]" % qconfig.contig_thresholds
        print >> sys.stderr, "-a  --ambiguity-usage <none|one|all>  Use none, one, or all alignments of a contig with multiple equally "
        print >> sys.stderr, "                                      good alignments [default is %s]" % qconfig.ambiguity_usage
        print >> sys.stderr, "-S  --gene-thresholds                 Comma-separated list of threshold lengths of genes to search with Gene Finding module "
        print >> sys.stderr, "                                      [default: %s]" % qconfig.genes_lengths
        print >> sys.stderr, "-T  --threads      <int>              Maximum number of threads [default: number of CPUs]"
        print >> sys.stderr, "-l  --labels \"label, label, ...\"      Names of assemblies to use in reports, comma-separated. If contain spaces, use quotes"
        print >> sys.stderr, "-L                                    Take assembly names from their parent directory names"
        print >> sys.stderr, "-c  --mincluster   <int>              Nucmer's parameter: the minimum length of a cluster of matches [default: %s]" % qconfig.mincluster
        print >> sys.stderr, "    --est-ref-size <int>              Estimated reference size (for computing NGx metrics without a reference)"
        print >> sys.stderr, "-J  --save-json-to <path>             Save the JSON output to a particular path"
        print >> sys.stderr, ""
        print >> sys.stderr, "Flags"
        print >> sys.stderr, "-f  --gene-finding        Predict genes (with GeneMarkS from prokaryotes (default), GlimmerHMM"
        print >> sys.stderr, "                          for eukaryotes (--eukaryote), or MetaGeneMark for metagenomes (--meta)"
        print >> sys.stderr, "-s  --scaffolds           Assemblies are scaffolds, split them and add contigs to the comparison"
        print >> sys.stderr, "    --gage                Use GAGE (results are in gage_report.txt)"
        print >> sys.stderr, "-e  --eukaryote           Genome is eukaryotic"
        print >> sys.stderr, "-m  --meta                Metagenomic assembly. Use MetaGeneMark for gene prediction. "
        print >> sys.stderr, "-u  --use-all-alignments  Compute genome fraction, # genes, # operons in the v.1.0-1.3 style"
        print >> sys.stderr, "-n  --strict-NA           Break contigs in any misassembly event when compute NAx and NGAx"
        print >> sys.stderr, "-j  --save-json           Save the output also in the JSON format"
        print >> sys.stderr, "    --no-html             Do not build html report"
        print >> sys.stderr, "    --no-plots            Do not draw plots (to make quast faster)"
        print >> sys.stderr, ""
        print >> sys.stderr, "-d  --debug               Run in a debug mode"
        print >> sys.stderr, "    --test                Run QUAST on the data from the test_data folder, output to test_output"
        print >> sys.stderr, "-h  --help                Print this message"



def _set_up_output_dir(output_dirpath, json_outputpath,
                       make_latest_symlink, save_json, remove_old=False):
    existing_alignments = False

    if output_dirpath:  # 'output dir was specified with -o option'
        if os.path.isdir(output_dirpath):
            if remove_old:
                try:
                    shutil.rmtree(output_dirpath)
                except OSError:
                    pass
            else:
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

            # removing \r (Nucmer fails on such sequences)
            corr_seq = corr_seq.strip('\r')

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

    # fixing remaning duplicates by adding index
    for duplicated_label in get_duplicated(labels):
        j = 0
        for i, (label, fpath) in enumerate(zip(labels, contigs_fpaths)):
            if label == duplicated_label:
                labels[i] = label if j == 0 else label + ' ' + str(j)
                j += 1

    return labels


def main(args):
    quast_dirpath = os.path.dirname(qconfig.LIBS_LOCATION)
    if ' ' in quast_dirpath:
        logger.error('QUAST does not support spaces in paths. \n' + \
              'You are trying to run it from ' + str(quast_dirpath) + '\n' + \
              'Please, put QUAST in a different directory, then try again.\n',
              to_stderr=True,
              exit_with_code=3)

    ######################
    ### ARGS
    ######################
    reload(qconfig)

    try:
        options, contigs_fpaths = getopt.gnu_getopt(args, qconfig.short_options, qconfig.long_options)
    except getopt.GetoptError, err:
        print >> sys.stderr, err
        print >> sys.stderr
        _usage()
        sys.exit(2)

    for opt, arg in options:
        if opt == '--test':
            main(['test_data/contigs_10k_1.fasta',
                  'test_data/contigs_10k_2.fasta',
                  '-R', 'test_data/reference_10k.fasta.gz',
                  '-O', 'test_data/operons_10k.gff',
                  '-G', 'test_data/genes_10k.gff',
                  '-o', 'test_output'])
            sys.exit(0)

    if not contigs_fpaths:
        _usage()
        sys.exit(2)

    json_outputpath = None
    output_dirpath = None

    labels = None
    all_labels_from_dirs = False

    for opt, arg in options:
        # Yes, this is a code duplicating. But OptionParser is deprecated since version 2.7.
        if opt in ('-o', "--output-dir"):
            output_dirpath = os.path.abspath(arg)
            qconfig.make_latest_symlink = False

        elif opt in ('-G', "--genes"):
            qconfig.genes = assert_file_exists(arg, 'genes')

        elif opt in ('-O', "--operons"):
            qconfig.operons = assert_file_exists(arg, 'operons')

        elif opt in ('-R', "--reference"):
            qconfig.ref_fpath = assert_file_exists(arg, 'reference')

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

        elif opt in ('-m', '--meta'):
            qconfig.meta = True

        elif opt in ('-d', '--debug'):
            qconfig.debug = True
            RELEASE_MODE = False

        elif opt in ('-l', '--labels'):
            labels = parse_labels(arg, contigs_fpaths)

        elif opt == '-L':
            all_labels_from_dirs = True

        elif opt in ('-h', "--help"):
            _usage()
            sys.exit(0)
        else:
            logger.error('Unknown option: %s. Use -h for help.' % (opt + (' ' + arg) if arg else ''), to_stderr=True)
            sys.exit(2)

    for contigs_fpath in contigs_fpaths:
        assert_file_exists(contigs_fpath, 'contigs')

    labels = process_labels(contigs_fpaths, labels, all_labels_from_dirs)

    output_dirpath, json_outputpath, existing_alignments = \
        _set_up_output_dir(output_dirpath, json_outputpath, qconfig.make_latest_symlink, qconfig.min_contig)

    corrected_dirpath = os.path.join(output_dirpath, qconfig.corrected_dirname)

    logger.set_up_file_handler(output_dirpath)
    logger.info(' '.join([os.path.realpath(__file__)] + args))
    logger.start()

    if existing_alignments:
        logger.notice()
        logger.notice("Output directory already exists. Existing Nucmer alignments can be used.")
        qutils.remove_reports(output_dirpath)

    qconfig.contig_thresholds = map(int, qconfig.contig_thresholds.split(","))

    qconfig.genes_lengths = map(int, qconfig.genes_lengths.split(","))

    # Threading
    if qconfig.max_threads is None:
        try:
            import multiprocessing
            qconfig.max_threads = multiprocessing.cpu_count()
        except:
            logger.warning('Failed to determine the number of CPUs')
            qconfig.max_threads = qconfig.DEFAULT_MAX_THREADS

        logger.notice()
        logger.notice('Maximum number of threads is set to ' + str(qconfig.max_threads) + ' (use --threads option to set it manually)')

    # Where all pdfs will be saved
    all_pdf_fpath = os.path.join(output_dirpath, qconfig.plots_fname)
    all_pdf = None

    ########################################################################

    from libs import reporting
    reload(reporting)

    if os.path.isdir(corrected_dirpath):
        shutil.rmtree(corrected_dirpath)
    os.mkdir(corrected_dirpath)

    # PROCESSING REFERENCE
    if qconfig.ref_fpath:
        logger.info()
        logger.info('Reference:')
        ref_fpath = _correct_reference(qconfig.ref_fpath, corrected_dirpath)
    else:
        ref_fpath = ''

    # PROCESSING CONTIGS
    logger.info()
    logger.info('Contigs:')
    contigs_fpaths = _correct_contigs(contigs_fpaths, corrected_dirpath, reporting, labels)

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

    if qconfig.draw_plots:
        from libs import plotter  # Do not remove this line! It would lead to a warning in matplotlib.
        try:
            from matplotlib.backends.backend_pdf import PdfPages
            all_pdf = PdfPages(all_pdf_fpath)
        except:
            all_pdf = None

    ########################################################################
    ### Stats and plots
    ########################################################################
    from libs import basic_stats
    basic_stats.do(ref_fpath, contigs_fpaths, os.path.join(output_dirpath, 'basic_stats'), all_pdf, qconfig.draw_plots,
                   json_outputpath, output_dirpath)

    aligned_fpaths = []
    aligned_lengths_lists = []
    if ref_fpath:
        ########################################################################
        ### former PLANTAKOLYA, PLANTAGORA
        ########################################################################
        from libs import contigs_analyzer
        nucmer_statuses, aligned_lengths_per_fpath = contigs_analyzer.do(
            ref_fpath, contigs_fpaths, qconfig.prokaryote, os.path.join(output_dirpath, 'contigs_reports'))
        for contigs_fpath in contigs_fpaths:
            if nucmer_statuses[contigs_fpath] == contigs_analyzer.NucmerStatus.OK:
                aligned_fpaths.append(contigs_fpath)
                aligned_lengths_lists.append(aligned_lengths_per_fpath[contigs_fpath])

    # Before continue evaluating, check if nucmer didn't skip all of the contigs files.
    if len(aligned_fpaths) and ref_fpath:
        ########################################################################
        ### NAx and NGAx ("aligned Nx and NGx")
        ########################################################################
        from libs import aligned_stats
        aligned_stats.do(ref_fpath, aligned_fpaths, aligned_lengths_lists,
                         os.path.join(output_dirpath, 'contigs_reports'),
                         os.path.join(output_dirpath, 'aligned_stats'), all_pdf,
                         qconfig.draw_plots, json_outputpath, output_dirpath)

        ########################################################################
        ### GENOME_ANALYZER
        ########################################################################
        from libs import genome_analyzer
        genome_analyzer.do(ref_fpath, aligned_fpaths, os.path.join(output_dirpath, 'contigs_reports'),
                           os.path.join(output_dirpath, 'genome_stats'),
                           qconfig.genes, qconfig.operons, all_pdf, qconfig.draw_plots,
                           json_outputpath, output_dirpath)

    # def add_empty_predicted_genes_fields():
    #     # TODO: make it in a more appropriate way (don't output predicted genes if annotations are provided)
    #     for id, contigs_fpath in enumerate(contigs_fpaths):
    #         report = reporting.get(contigs_fpath)
    #         report.add_field(reporting.Fields.PREDICTED_GENES_UNIQUE, "")
    #         report.add_field(reporting.Fields.PREDICTED_GENES, [""] * len(qconfig.genes_lengths))

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
        # add_empty_predicted_genes_fields()

    ########################################################################
    ### TOTAL REPORT
    ########################################################################
    for contigs_fpath in contigs_fpaths:
        report = reporting.get(contigs_fpath)
        report.add_field(reporting.Fields.NAME, qutils.label_from_fpath(contigs_fpath))

    reporting.save_total(output_dirpath)

    if json_outputpath:
        json_saver.save_total_report(json_outputpath, qconfig.min_contig)

    if qconfig.html_report:
        from libs.html_saver import html_saver
        html_saver.save_total_report(output_dirpath, qconfig.min_contig)

    if qconfig.draw_plots and all_pdf:
        logger.info('  All pdf files are merged to ' + all_pdf_fpath)
        all_pdf.close()

    # if RELEASE_MODE:
    _cleanup(corrected_dirpath)

    logger.finish_up()

    return 0


def _cleanup(corrected_dirpath):
    # removing correcting input contig files
    if not qconfig.debug:
        shutil.rmtree(corrected_dirpath)


if __name__ == '__main__':
    # try:
    return_code = main(sys.argv[1:])
    exit(return_code)
    # except Exception, e:
    #     logger.exception(e)
