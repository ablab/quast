############################################################################
# Copyright (c) 2015-2016 Saint Petersburg State University
# Copyright (c) 2011-2015 Saint Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################

from __future__ import with_statement
import glob
import shutil
import subprocess
import os
import re
from collections import defaultdict

from . import qconfig
from os.path import basename, isfile, realpath

from quast_libs import fastaparser
from quast_libs.log import get_logger
logger = get_logger(qconfig.LOGGER_DEFAULT_NAME)

MAX_CONTIG_NAME = 1021  # Nucmer's constraint
MAX_CONTIG_NAME_GLIMMER = 298   # Glimmer's constraint


def set_up_output_dir(output_dirpath, json_outputpath,
                       make_latest_symlink, save_json):
    existing_alignments = False

    if output_dirpath:  # 'output dir was specified with -o option'
        if os.path.isdir(output_dirpath):
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
        os.symlink(basename(output_dirpath), latest_symlink)

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


def correct_seq(seq, original_fpath):
    corr_seq = seq.upper()

    # correcting alternatives (gage can't work with alternatives)
    # dic = {'M': 'A', 'K': 'G', 'R': 'A', 'Y': 'C', 'W': 'A', 'S': 'C', 'V': 'A', 'B': 'C', 'H': 'A', 'D': 'A'}
    dic = {'M': 'N', 'K': 'N', 'R': 'N', 'Y': 'N', 'W': 'N', 'S': 'N', 'V': 'N', 'B': 'N', 'H': 'N', 'D': 'N'}
    pat = "(%s)" % "|".join(map(re.escape, dic.keys()))
    corr_seq = re.sub(pat, lambda m: dic[m.group()], corr_seq)

    # make sure that only A, C, G, T or N are in the sequence
    if re.compile(r'[^ACGTN]').search(corr_seq):
        logger.warning('Skipping ' + original_fpath + ' because it contains non-ACGTN characters.',
                indent='    ')
        return None
    return corr_seq


def correct_fasta(original_fpath, corrected_fpath, min_contig,
                  is_reference=False):
    modified_fasta_entries = []
    used_seq_names = defaultdict(int)
    for first_line, seq in fastaparser.read_fasta(original_fpath):
        if (len(seq) >= min_contig) or is_reference:
            corr_name = correct_name(first_line)
            uniq_name = get_uniq_name(corr_name, used_seq_names)
            used_seq_names[corr_name] += 1

            if not qconfig.no_check:
                # seq to uppercase, because we later looking only uppercase letters
                corr_seq = correct_seq(seq, original_fpath)
                if not corr_seq:
                    return False
            else:
                corr_seq = seq
            modified_fasta_entries.append((uniq_name, corr_seq))

    fastaparser.write_fasta(corrected_fpath, modified_fasta_entries)

    if is_reference:
        ref_len = sum(len(chr_seq) for (chr_name, chr_seq) in modified_fasta_entries)
        if ref_len > qconfig.MAX_REFERENCE_FILE_LENGTH:
            qconfig.splitted_ref = []  # important for MetaQUAST which runs QUAST multiple times
            _, fasta_ext = os.path.splitext(corrected_fpath)
            split_ref_dirpath = os.path.join(os.path.dirname(corrected_fpath), 'split_ref')
            if os.path.exists(split_ref_dirpath):
                shutil.rmtree(split_ref_dirpath, ignore_errors=True)
            os.makedirs(split_ref_dirpath)
            max_len = min(ref_len/qconfig.max_threads, qconfig.MAX_REFERENCE_LENGTH)
            cur_part_len = 0
            cur_part_num = 1
            cur_part_fpath = os.path.join(split_ref_dirpath, "part_%d" % cur_part_num) + fasta_ext

            for (chr_name, chr_seq) in modified_fasta_entries:
                cur_chr_len = len(chr_seq)
                if cur_chr_len > qconfig.MAX_REFERENCE_LENGTH:
                    logger.warning("Skipping chromosome " + chr_name + " because its length is greater than " +
                            str(qconfig.MAX_REFERENCE_LENGTH) + " (Nucmer's constraint).")
                    continue

                cur_part_len += cur_chr_len
                if cur_part_len > max_len and cur_part_len != cur_chr_len:
                    qconfig.splitted_ref.append(cur_part_fpath)
                    cur_part_len = cur_chr_len
                    cur_part_num += 1
                    cur_part_fpath = os.path.join(split_ref_dirpath, "part_%d" % cur_part_num) + fasta_ext
                fastaparser.write_fasta(cur_part_fpath, [(chr_name, chr_seq)], mode='a')
            if cur_part_len > 0:
                qconfig.splitted_ref.append(cur_part_fpath)
            if len(qconfig.splitted_ref) == 0:
                logger.warning("Skipping reference because all of its chromosomes exceeded Nucmer's constraint.")
                return False
    return True


# Correcting fasta and reporting stats
def get_lengths_from_fasta(contigs_fpath, label):
    lengths = fastaparser.get_chr_lengths_from_fastafile(contigs_fpath).values()

    if not sum(l for l in lengths if l >= qconfig.min_contig):
        logger.warning("Skipping %s because it doesn't contain contigs >= %d bp."
                       % (label, qconfig.min_contig))
        return None

    return lengths


def add_lengths_to_report(lengths, reporting, contigs_fpath):
    if reporting:
        ## filling column "Assembly" with names of assemblies
        report = reporting.get(contigs_fpath)

        ## filling columns "Number of contigs >=110 bp", ">=200 bp", ">=500 bp"
        report.add_field(reporting.Fields.CONTIGS__FOR_THRESHOLDS,
                         [sum(1 for l in lengths if l >= threshold)
                          for threshold in qconfig.contig_thresholds])
        report.add_field(reporting.Fields.TOTALLENS__FOR_THRESHOLDS,
                         [sum(l for l in lengths if l >= threshold)
                          for threshold in qconfig.contig_thresholds])


def correct_contigs(contigs_fpaths, corrected_dirpath, labels, reporting):
    ## removing from contigs' names special characters because:
    ## 1) Some embedded tools can fail on some strings with "...", "+", "-", etc
    ## 2) Nucmer fails on names like "contig 1_bla_bla", "contig 2_bla_bla" (it interprets as a contig's name only the first word of caption and gets ambiguous contigs names)
    n_jobs = min(len(contigs_fpaths), qconfig.max_threads)
    from joblib import Parallel, delayed
    logger.main_info('  Pre-processing...')
    corrected_info = Parallel(n_jobs=n_jobs)(delayed(parallel_correct_contigs)(i, contigs_fpath,
            corrected_dirpath, labels) for i, contigs_fpath in enumerate(contigs_fpaths))
    corrected_contigs_fpaths = []
    old_contigs_fpaths = []
    for contig_idx, (old_fpaths, corr_fpaths, broken_scaffold_fpaths, logs) in enumerate(corrected_info):
        label = labels[contig_idx]
        logger.main_info('\n'.join(logs))
        for old_fpath in old_fpaths:
            old_contigs_fpaths.append(old_fpath)
            qconfig.assembly_labels_by_fpath[old_fpath] = label
        for corr_fpath, lengths in corr_fpaths:
            corrected_contigs_fpaths.append(corr_fpath)
            qconfig.assembly_labels_by_fpath[corr_fpath] = label
            add_lengths_to_report(lengths, reporting, corr_fpath)
        for broken_fpath, lengths in broken_scaffold_fpaths:
            old_contigs_fpaths.append(broken_fpath)
            corrected_contigs_fpaths.append(broken_fpath)
            qconfig.assembly_labels_by_fpath[broken_fpath] = label + '_broken'
            add_lengths_to_report(lengths, reporting, broken_fpath)

    if qconfig.draw_plots or qconfig.html_report:
        from quast_libs import plotter
        if not plotter.dict_color_and_ls:
            plotter.save_colors_and_ls(corrected_contigs_fpaths)

    return corrected_contigs_fpaths, old_contigs_fpaths


def parallel_correct_contigs(file_counter, contigs_fpath, corrected_dirpath, labels):
    contigs_fname = os.path.basename(contigs_fpath)
    fname, fasta_ext = splitext_for_fasta_file(contigs_fname)

    label = labels[file_counter]
    logs = []
    corr_fpaths = []
    old_contigs_fpaths = []
    broken_scaffold_fpaths = []

    corr_fpath = unique_corrected_fpath(os.path.join(corrected_dirpath, label + fasta_ext))
    lengths = get_lengths_from_fasta(contigs_fpath, label)
    if not lengths or not correct_fasta(contigs_fpath, corr_fpath, qconfig.min_contig):
        corr_fpath = None
    if corr_fpath:
        corr_fpaths.append((corr_fpath, lengths))
        old_contigs_fpaths.append(contigs_fpath)
        logs.append('  ' + index_to_str(file_counter, force=(len(labels) > 1)) + '%s ==> %s' % (contigs_fpath, label))

    # if option --scaffolds is specified QUAST adds split version of assemblies to the comparison
    if qconfig.scaffolds and not qconfig.is_combined_ref and corr_fpath:
        broken_scaffold_fpath, logs = broke_scaffolds(file_counter, labels, corr_fpath, corrected_dirpath, logs)
        if broken_scaffold_fpath:
            lengths = get_lengths_from_fasta(broken_scaffold_fpath, label + '_broken')
            if lengths and (qconfig.no_check_meta or correct_fasta(contigs_fpath, corr_fpath, qconfig.min_contig)):
                broken_scaffold_fpaths.append((broken_scaffold_fpath, lengths))
                qconfig.dict_of_broken_scaffolds[broken_scaffold_fpath] = corr_fpath

    return old_contigs_fpaths, corr_fpaths, broken_scaffold_fpaths, logs


def broke_scaffolds(file_counter, labels, contigs_fpath, corrected_dirpath, logs):
    logs.append('  ' + index_to_str(file_counter, force=(len(labels) > 1)) + '  breaking scaffolds into contigs:')
    contigs_fname = os.path.basename(contigs_fpath)
    fname, fasta_ext = splitext_for_fasta_file(contigs_fname)
    label = labels[file_counter]
    corr_fpath = unique_corrected_fpath(os.path.join(corrected_dirpath, label + fasta_ext))
    corr_fpath_wo_ext = os.path.join(corrected_dirpath, name_from_fpath(corr_fpath))
    broken_scaffolds_fpath = corr_fpath_wo_ext + '_broken' + fasta_ext
    broken_scaffolds_fasta = []
    contigs_counter = 0

    scaffold_counter = 0
    for scaffold_counter, (name, seq) in enumerate(fastaparser.read_fasta(contigs_fpath)):
        if contigs_counter % 100 == 0:
            pass
        if contigs_counter > 520:
            pass
        cumul_contig_length = 0
        total_contigs_for_the_scaf = 1
        cur_contig_start = 0
        while (cumul_contig_length < len(seq)) and (seq.find('N', cumul_contig_length) != -1):
            start = seq.find("N", cumul_contig_length)
            end = start + 1
            while (end != len(seq)) and (seq[end] == 'N'):
                end += 1

            cumul_contig_length = end + 1
            if (end - start) >= qconfig.Ns_break_threshold:
                broken_scaffolds_fasta.append(
                    (name.split()[0] + "_" +
                     str(total_contigs_for_the_scaf),
                     seq[cur_contig_start:start]))
                total_contigs_for_the_scaf += 1
                cur_contig_start = end

        broken_scaffolds_fasta.append(
            (name.split()[0] + "_" +
             str(total_contigs_for_the_scaf),
             seq[cur_contig_start:]))

        contigs_counter += total_contigs_for_the_scaf
    if scaffold_counter + 1 != contigs_counter:
        fastaparser.write_fasta(broken_scaffolds_fpath, broken_scaffolds_fasta)
        logs.append("  " + index_to_str(file_counter, force=(len(labels) > 1)) +
                    "    %d scaffolds (%s) were broken into %d contigs (%s)" %
                    (scaffold_counter + 1,
                     label,
                     contigs_counter,
                     label + '_broken'))
        return broken_scaffolds_fpath, logs

    logs.append("  " + index_to_str(file_counter, force=(len(labels) > 1)) +
            "    WARNING: nothing was broken, skipping '%s broken' from further analysis" % label)
    return None, logs


def is_scaffold(seq):
    if qconfig.no_check:
        return False
    cumul_contig_length = 0
    seq_len = len(seq)
    while cumul_contig_length < seq_len and seq.find('N', cumul_contig_length) != -1:
        start = seq.find('N', cumul_contig_length)
        end = start + 1
        while end != seq_len and seq[end] == 'N':
            end += 1

        cumul_contig_length = end + 1
        if end - start >= qconfig.Ns_break_threshold:
            return True
    return False


def correct_reference(ref_fpath, corrected_dirpath):
    ref_fname = os.path.basename(ref_fpath)
    name, fasta_ext = splitext_for_fasta_file(ref_fname)
    corr_fpath = unique_corrected_fpath(
        os.path.join(corrected_dirpath, name + fasta_ext))
    if not qconfig.no_check_meta and not qconfig.is_combined_ref:
        if not correct_fasta(ref_fpath, corr_fpath, qconfig.min_contig, is_reference=True):
            return ''
    else:
        corr_fpath = ref_fpath

    logger.main_info('  %s ==> %s' % (ref_fpath, name_from_fpath(corr_fpath)))
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
    name = rm_extentions_for_fasta_file(os.path.basename(contigs_fpath))
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
        labels = [rm_extentions_for_fasta_file(os.path.basename(fpath)) for fpath in contigs_fpaths]

        for duplicated_label in get_duplicated(labels):
            for i, (label, fpath) in enumerate(zip(labels, contigs_fpaths)):
                if label == duplicated_label:
                    labels[i] = get_label_from_par_dir_and_fname(contigs_fpaths[i])

    # fixing remaining duplicates by adding index
    for duplicated_label in get_duplicated(labels):
        j = 0
        for i, (label, fpath) in enumerate(zip(labels, contigs_fpaths)):
            if label == duplicated_label:
                if j == 0:
                    labels[i] = label
                else:
                    labels[i] = label + ' ' + str(j)
                j += 1

    return labels


def cleanup(corrected_dirpath):
    # removing correcting input contig files
    if not qconfig.debug and not qconfig.is_combined_ref:
        shutil.rmtree(corrected_dirpath)


def assert_file_exists(fpath, message='', logger=logger):
    if not os.path.isfile(fpath):
        logger.error("File not found (%s): %s" % (message, fpath), 2,
                     to_stderr=True)

    return fpath


def index_to_str(i, force=False):
    if qconfig.assemblies_num == 1 and not force:
        return ''
    else:
        return ('%d ' + ('' if (i + 1) >= 10 else ' ')) % (i + 1)


# def uncompress(compressed_fname, uncompressed_fname, logger=logger):
#     fname, ext = os.path.splitext(compressed_fname)
#
#     if ext not in ['.zip', '.bz2', '.gz']:
#         return False
#
#     logger.info('  extracting %s...' % compressed_fname)
#     compressed_file = None
#
#     if ext == '.zip':
#         try:
#             zfile = zipfile.ZipFile(compressed_fname)
#         except Exception, e:
#             logger.error('can\'t open zip file: ' + str(e.message))
#             return False
#
#         names = zfile.namelist()
#         if len(names) == 0:
#             logger.error('zip archive is empty')
#             return False
#
#         if len(names) > 1:
#             logger.warning('zip archive must contain exactly one file. Using %s' % names[0])
#
#         compressed_file = zfile.open(names[0])
#
#     if ext == '.bz2':
#         compressed_file = bz2.BZ2File(compressed_fname)
#
#     if ext == '.gz':
#         compressed_file = gzip.open(compressed_fname)
#
#     with open(uncompressed_fname, 'w') as uncompressed_file:
#         uncompressed_file.write(compressed_file.read())
#
#     logger.info('    extracted!')
#     return True


def remove_reports(output_dirpath):
    for gage_prefix in ["", qconfig.gage_report_prefix]:
        for report_prefix in [qconfig.report_prefix, qconfig.transposed_report_prefix]:
            pattern = os.path.join(output_dirpath, gage_prefix + report_prefix + ".*")
            for f in glob.iglob(pattern):
                os.remove(f)
    plots_fpath = os.path.join(output_dirpath, qconfig.plots_fname)
    if os.path.isfile(plots_fpath):
        os.remove(plots_fpath)
    html_report_aux_dir = os.path.join(output_dirpath, qconfig.html_aux_dir)
    if os.path.isdir(html_report_aux_dir):
        shutil.rmtree(html_report_aux_dir)


def correct_name(name, max_name_len=MAX_CONTIG_NAME):
    name = re.sub(r'[^\w\._\-+|]', '_', name.strip())[:max_name_len]
    return re.sub(r"[\|+=/]", '_', name.strip())[:max_name_len]


def get_uniq_name(name, used_names):
    if name in used_names:
        name += '_' + str(used_names[name])
    return name


def unique_corrected_fpath(fpath):
    dirpath = os.path.dirname(fpath)
    fname = os.path.basename(fpath)

    corr_fname = correct_name(fname)

    if os.path.isfile(os.path.join(dirpath, corr_fname)):
        i = 1
        base_corr_fname = corr_fname
        while os.path.isfile(os.path.join(dirpath, corr_fname)):
            str_i = str(i)
            corr_fname = str(base_corr_fname) + '__' + str_i
            i += 1

    return os.path.join(dirpath, corr_fname)


def rm_extentions_for_fasta_file(fname):
    return correct_name(splitext_for_fasta_file(fname)[0])


def splitext_for_fasta_file(fname):
    # "contigs.fasta", ".gz"
    basename_plus_innerext, outer_ext = os.path.splitext(fname)

    if outer_ext not in ['.zip', '.gz', '.gzip', '.bz2', '.bzip2']:
        basename_plus_innerext, outer_ext = fname, ''  # not a supported archive

    # "contigs", ".fasta"
    basename, fasta_ext = os.path.splitext(basename_plus_innerext)
    if fasta_ext not in ['.fa', '.fasta', '.fas', '.seq', '.fna', '.contig']:
        basename, fasta_ext = basename_plus_innerext, ''  # not a supported extention, or no extention

    return basename, fasta_ext


def check_is_fasta_file(fname, logger=logger):
    if 'blast.res' in fname or 'blast.check' in fname or fname == 'blast.err':
        return False

    basename_plus_innerext, outer_ext = os.path.splitext(fname)
    basename, fasta_ext = os.path.splitext(basename_plus_innerext)
    if fasta_ext == '':
        outer_ext, fasta_ext = fasta_ext, outer_ext
    if outer_ext in ['.fa', '.fasta', '.fas', '.seq', '.fna']:
        return True

    if outer_ext not in ['.zip', '.gz', '.gzip', '.bz2', '.bzip2', '']:
        logger.warning('Skipping %s because it is not a supported archive.' % fname)
        return False

    if fasta_ext not in ['.fa', '.fasta', '.fas', '.seq', '.fna']:
        logger.warning('Skipping %s because it has not a supported extension.' % fname)
        return False

    return True


def name_from_fpath(fpath):
    return os.path.splitext(os.path.basename(fpath))[0]


def label_from_fpath(fpath):
    return qconfig.assembly_labels_by_fpath[fpath]


def label_from_fpath_for_fname(fpath):
    return re.sub('[/= ]', '_', qconfig.assembly_labels_by_fpath[fpath])


def call_subprocess(args, stdin=None, stdout=None, stderr=None,
                    indent='',
                    only_if_debug=True, env=None, logger=logger):
    printed_args = args[:]
    if stdin:
        printed_args += ['<', stdin.name]
    if stdout:
        printed_args += ['>>' if stdout.mode == 'a' else '>', stdout.name]
    if stderr:
        printed_args += ['2>>' if stderr.mode == 'a' else '2>', stderr.name]

    for i, arg in enumerate(printed_args):
        if arg.startswith(os.getcwd()):
            printed_args[i] = relpath(arg)

    logger.print_command_line(printed_args, indent, only_if_debug=only_if_debug)

    return_code = subprocess.call(args, stdin=stdin, stdout=stdout, stderr=stderr, env=env)

    if return_code != 0:
        logger.debug(' ' * len(indent) + 'The tool returned non-zero.' +
                     (' See ' + relpath(stderr.name) + ' for stderr.' if stderr else ''))
        # raise SubprocessException(printed_args, return_code)

    return return_code


def get_chr_len_fpath(ref_fpath):
    chr_len_fpath = ref_fpath + '.fai'
    if not is_non_empty_file(chr_len_fpath):
        chr_lengths = fastaparser.get_chr_lengths_from_fastafile(ref_fpath)
        with open(chr_len_fpath, 'w') as out_f:
            for chr_name, chr_len in chr_lengths.iteritems():
                out_f.write(chr_name + '\t' + str(chr_len) + '\n')
    return chr_len_fpath


# class SubprocessException(Exception):
#     def __init__(self, printed_args, return_code):
#         self.printed_args = printed_args
#         self.return_code = return_code


from posixpath import curdir, sep, pardir, join, abspath, commonprefix


def relpath(path, start=curdir):
    """Return a relative version of a path"""
    if not path:
        raise ValueError("No path specified")
    start_list = abspath(start).split(sep)
    path_list = abspath(path).split(sep)
    # Work out how much of the filepath is shared by start and path.
    i = len(commonprefix([start_list, path_list]))
    rel_list = [pardir] * (len(start_list) - i) + path_list[i:]
    if not rel_list:
        return curdir
    return join(*rel_list)


def get_path_to_program(program):
    """
    returns the path to an executable or None if it can't be found
    """
    def is_exe(fpath):
        return os.path.isfile(fpath) and os.access(fpath, os.X_OK)

    for path in os.environ["PATH"].split(os.pathsep):
        exe_file = os.path.join(path, program)
        if is_exe(exe_file):
            return exe_file
    return None


def is_non_empty_file(fpath):
    return fpath and os.path.exists(fpath) and os.path.getsize(fpath) > 10


def cat_files(in_fnames, out_fname):
    if not isinstance(in_fnames, list):
        in_fnames = [in_fnames]
    with open(out_fname, 'w') as outfile:
        for fname in in_fnames:
            if os.path.exists(fname):
                with open(fname) as infile:
                    for line in infile:
                        outfile.write(line)


def is_float(value):
    try:
        float(value)
        return True
    except ValueError:
        return False
    except TypeError:
        return False


def parseStrToNum(s):
    try:
        return int(s)
    except ValueError:
        return float(s)


def val_to_str(val):
    if val is None:
        return '-'
    else:
        return str(val)


def add_suffix(fname, suffix):
    base, ext = os.path.splitext(fname)
    if ext in ['.gz', '.bz2', '.zip']:
        base, ext2 = os.path.splitext(base)
        ext = ext2 + ext
    return base + (('.' + suffix) if suffix else '') + ext


def all_required_binaries_exist(aligner_dirpath, required_binaries):
    for required_binary in required_binaries:
        if not isfile(join(aligner_dirpath, required_binary)):
            return False
    return True


def check_prev_compilation_failed(name, failed_compilation_flag, just_notice=False, logger=logger):
    if isfile(failed_compilation_flag):
        msg = 'Previous try of ' + name + ' compilation was unsuccessful! ' + \
              'For forced retrying, please remove ' + failed_compilation_flag + ' and restart QUAST.'
        if just_notice:
            logger.notice(msg)
        else:
            logger.warning(msg)
        return True
    return False


def safe_rm(fpath):
    if isfile(fpath):
        try:
            os.remove(fpath)
        except OSError:
            pass


def compile_tool(name, dirpath, requirements, just_notice=False, logger=logger, only_clean=False):
    make_logs_basepath = join(dirpath, 'make')
    failed_compilation_flag = make_logs_basepath + '.failed'
    succeeded_compilation_flag = make_logs_basepath + '.succeeded'

    if only_clean:
        for required_binary in requirements:
            safe_rm(join(dirpath, required_binary))
        safe_rm(failed_compilation_flag)
        safe_rm(succeeded_compilation_flag)
        return True

    if not all_required_binaries_exist(dirpath, requirements):
        if check_prev_compilation_failed(name, failed_compilation_flag, just_notice, logger=logger):
            return False

        # making
        logger.main_info('Compiling ' + name + ' (details are in ' + make_logs_basepath +
                         '.log and make.err)')
        return_code = call_subprocess(['make', '-C', dirpath],
                                      stdout=open(make_logs_basepath + '.log', 'w'),
                                      stderr=open(make_logs_basepath + '.err', 'w'), logger=logger)

        if return_code != 0 or not all_required_binaries_exist(dirpath, requirements):
            msg = "Failed to compile " + name + " (" + dirpath + ")! " \
                  "Try to compile it manually. " + ("You can restart Quast with the --debug flag "
                                                    "to see the compilation command." if not qconfig.debug else "")
            if just_notice:
                logger.notice(msg)
            else:
                logger.warning(msg)
            open(failed_compilation_flag, 'w').close()
            return False
        with open(succeeded_compilation_flag, 'w') as out_f:
            out_f.write(abspath(realpath(dirpath)))
    return True
