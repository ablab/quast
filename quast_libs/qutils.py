############################################################################
# Copyright (c) 2015-2022 Saint Petersburg State University
# Copyright (c) 2011-2015 Saint Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################

from __future__ import with_statement
from __future__ import division
import glob
import hashlib
import shutil
import subprocess
import os
import stat
import sys
import re
from collections import defaultdict
from os.path import basename, isfile, isdir, exists, join

try:
    from urllib2 import urlopen
    import urllib
except:
    from urllib.request import urlopen
    import urllib.request as urllib

from quast_libs import fastaparser, qconfig, plotter_data
from quast_libs.log import get_logger
logger = get_logger(qconfig.LOGGER_DEFAULT_NAME)

MAX_CONTIG_NAME = 1021  # Nucmer's constraint
MAX_CONTIG_NAME_GLIMMER = 298   # Glimmer's constraint

external_tools_dirpath = os.path.join(qconfig.QUAST_HOME, 'external_tools')
blast_dirpath = None


def set_up_output_dir(output_dirpath, json_outputpath,
                       make_latest_symlink, save_json):
    existing_quast_dir = False

    if output_dirpath:  # 'output dir was specified with -o option'
        if isdir(output_dirpath) and (isfile(join(output_dirpath, qconfig.LOGGER_DEFAULT_NAME + '.log')) or
                                      isfile(join(output_dirpath, qconfig.LOGGER_META_NAME + '.log'))):
            existing_quast_dir = True
    else:  # output dir was not specified, creating our own one
        output_dirpath = os.path.join(os.path.abspath(
            qconfig.default_results_root_dirname), qconfig.output_dirname)

        # in case of starting two instances of QUAST in the same second
        if isdir(output_dirpath):
            if make_latest_symlink:
                i = 2
                base_dirpath = output_dirpath
                while isdir(output_dirpath):
                    output_dirpath = str(base_dirpath) + '__' + str(i)
                    i += 1

    if not isdir(output_dirpath):
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
            if not isdir(json_outputpath):
                os.makedirs(json_outputpath)
        else:
            json_outputpath = os.path.join(output_dirpath, qconfig.default_json_dirname)
            if not isdir(json_outputpath):
                os.makedirs(json_outputpath)

    return output_dirpath, json_outputpath, existing_quast_dir


def correct_seq(seq, original_fpath):
    corr_seq = seq.upper()

    # correcting alternatives (gage can't work with alternatives)
    # dic = {'M': 'A', 'K': 'G', 'R': 'A', 'Y': 'C', 'W': 'A', 'S': 'C', 'V': 'A', 'B': 'C', 'H': 'A', 'D': 'A'}
    dic = {'M': 'N', 'K': 'N', 'R': 'N', 'Y': 'N', 'W': 'N', 'S': 'N', 'V': 'N', 'B': 'N', 'H': 'N', 'D': 'N'}
    pat = "(%s)" % "|".join(map(re.escape, dic.keys()))
    corr_seq = re.sub(pat, lambda m: dic[m.group()], corr_seq)

    # make sure that only A, C, G, T or N are in the sequence
    if re.compile(r'[^ACGTN]').search(corr_seq):
        logger.error('Skipping ' + original_fpath + ' because it contains non-ACGTN characters.', indent='    ')
        return None
    return corr_seq


def correct_fasta(original_fpath, min_contig, corrected_fpath=None, is_reference=False):
    modified_fasta_entries = []
    used_seq_names = defaultdict(int)
    for first_line, seq in fastaparser.read_fasta(original_fpath):
        if not first_line:
            logger.error('Skipping ' + original_fpath + ' because >sequence_name field is empty '
                                                        'for the entry starting with "%s".' % seq[:20], indent='    ')
            return False
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
                if re.compile(r'[^ACGTN]').search(seq):
                    logger.error('File ' + original_fpath + ' contains non-ACGTN characters. '
                                 'Please re-run QUAST without --no-check.', indent='    ', exit_with_code=1)
                    return False
                corr_seq = seq
            modified_fasta_entries.append((uniq_name, corr_seq))

    if not modified_fasta_entries:
        logger.warning('Skipping ' + original_fpath + ' because file is empty.', indent='    ')
        return False
    if corrected_fpath:
        fastaparser.write_fasta(corrected_fpath, modified_fasta_entries)
    return True


# Correcting fasta and reporting stats
def get_lengths_from_fasta(contigs_fpath, label):
    lengths = fastaparser.get_chr_lengths_from_fastafile(contigs_fpath).values()

    if not sum(l for l in lengths if l >= qconfig.min_contig):
        logger.warning("Skipping %s because it doesn't contain contigs >= %d bp."
                       % (label, qconfig.min_contig))
        return None

    return list(lengths)


def add_lengths_to_report(lengths, reporting, contigs_fpath):
    if reporting:
        ## filling column "Assembly" with names of assemblies
        report = reporting.get(contigs_fpath)

        ## filling columns "Number of contigs >=110 bp", ">=200 bp", ">=500 bp"
        is_broken = False
        if qconfig.split_scaffolds:
            if contigs_fpath in qconfig.dict_of_broken_scaffolds or \
                            plotter_data.get_color_and_ls(contigs_fpath)[1] == plotter_data.secondary_line_style:
                is_broken = True
        min_threshold = 0 if not is_broken else qconfig.min_contig
        report.add_field(reporting.Fields.CONTIGS__FOR_THRESHOLDS,
                         [sum(1 for l in lengths if l >= threshold) if threshold >= min_threshold else None
                          for threshold in qconfig.contig_thresholds])
        report.add_field(reporting.Fields.TOTALLENS__FOR_THRESHOLDS,
                         [sum(l for l in lengths if l >= threshold) if threshold >= min_threshold else None
                          for threshold in qconfig.contig_thresholds])


def correct_contigs(contigs_fpaths, corrected_dirpath, labels, reporting):
    ## removing from contigs' names special characters because:
    ## 1) Some embedded tools can fail on some strings with "...", "+", "-", etc
    ## 2) Nucmer fails on names like "contig 1_bla_bla", "contig 2_bla_bla" (it interprets as a contig's name only the first word of caption and gets ambiguous contigs names)

    if qconfig.max_threads is None:
        qconfig.max_threads = 1

    n_jobs = min(len(contigs_fpaths), qconfig.max_threads)
    logger.main_info('  Pre-processing...')
    parallel_run_args = [(i, contigs_fpath, corrected_dirpath, labels) for i, contigs_fpath in enumerate(contigs_fpaths)]
    old_fpaths, corr_fpaths, broken_scaffold_fpaths, logs, is_fatal_error = run_parallel(parallel_correct_contigs, parallel_run_args, n_jobs)
    corrected_contigs_fpaths = []
    old_contigs_fpaths = []
    if any(is_fatal_error):
       exit(4)
    for contig_idx in range(len(old_fpaths)):
        label = labels[contig_idx]
        logger.main_info('\n'.join(logs[contig_idx]))
        for old_fpath in old_fpaths[contig_idx]:
            old_contigs_fpaths.append(old_fpath)
            qconfig.assembly_labels_by_fpath[old_fpath] = label
        for corr_fpath, lengths in corr_fpaths[contig_idx]:
            corrected_contigs_fpaths.append(corr_fpath)
            qconfig.assembly_labels_by_fpath[corr_fpath] = label
            add_lengths_to_report(lengths, reporting, corr_fpath)
        for broken_fpath, lengths in broken_scaffold_fpaths[contig_idx]:
            old_contigs_fpaths.append(broken_fpath)
            corrected_contigs_fpaths.append(broken_fpath)
            qconfig.assembly_labels_by_fpath[broken_fpath] = label + '_broken'
            add_lengths_to_report(lengths, reporting, broken_fpath)

    if qconfig.draw_plots or qconfig.html_report:
        if not plotter_data.dict_color_and_ls:
            plotter_data.save_colors_and_ls(corrected_contigs_fpaths)

    return corrected_contigs_fpaths, old_contigs_fpaths


def convert_to_unicode(value):
    if is_python2():
        return unicode(value)
    else:
        return str(value)


def slugify(value):
    """
    Prepare string to use in file names: normalizes string,
    removes non-alpha characters, and converts spaces to hyphens.
    """
    import unicodedata
    value = unicodedata.normalize('NFKD', convert_to_unicode(value)).encode('ascii', 'ignore').decode('utf-8')
    value = convert_to_unicode(re.sub('[^\w\s-]', '-', value).strip())
    value = convert_to_unicode(re.sub('[-\s]+', '-', value))
    return str(value)


def parallel_correct_contigs(file_counter, contigs_fpath, corrected_dirpath, labels):
    contigs_fname = os.path.basename(contigs_fpath)
    fname, fasta_ext = splitext_for_fasta_file(contigs_fname)

    label = labels[file_counter]
    logs = []
    corr_fpaths = []
    old_contigs_fpaths = []
    broken_scaffold_fpaths = []
    is_fatal_error = False

    corr_fpath = unique_corrected_fpath(os.path.join(corrected_dirpath, slugify(label) + fasta_ext))
    lengths = get_lengths_from_fasta(contigs_fpath, label)
    if not lengths:
        corr_fpath = None
    elif qconfig.no_check_meta:
        if not correct_fasta(contigs_fpath, qconfig.min_contig):
            corr_fpath = None
            is_fatal_error = True
        else:
            corr_fpath = contigs_fpath
    elif not correct_fasta(contigs_fpath, qconfig.min_contig, corr_fpath):
        corr_fpath = None
        is_fatal_error = True
    if corr_fpath:
        corr_fpaths.append((corr_fpath, lengths))
        old_contigs_fpaths.append(contigs_fpath)
        logs.append('  ' + index_to_str(file_counter, force=(len(labels) > 1)) + '%s ==> %s' % (contigs_fpath, label))

    # if option --scaffolds is specified QUAST adds split version of assemblies to the comparison
    if qconfig.split_scaffolds and not qconfig.is_combined_ref and corr_fpath:
        broken_scaffold_fpath, logs = broke_scaffolds(file_counter, labels, corr_fpath, corrected_dirpath, logs)
        if broken_scaffold_fpath:
            lengths = get_lengths_from_fasta(broken_scaffold_fpath, label + '_broken')
            if lengths and (qconfig.no_check_meta or correct_fasta(contigs_fpath, qconfig.min_contig, corr_fpath)):
                broken_scaffold_fpaths.append((broken_scaffold_fpath, lengths))
                qconfig.dict_of_broken_scaffolds[broken_scaffold_fpath] = corr_fpath

    return old_contigs_fpaths, corr_fpaths, broken_scaffold_fpaths, logs, is_fatal_error


def broke_scaffolds(file_counter, labels, contigs_fpath, corrected_dirpath, logs):
    logs.append('  ' + index_to_str(file_counter, force=(len(labels) > 1)) + '  breaking scaffolds into contigs:')
    contigs_fname = os.path.basename(contigs_fpath)
    fname, fasta_ext = splitext_for_fasta_file(contigs_fname)
    label = labels[file_counter]
    corr_fpath = unique_corrected_fpath(os.path.join(corrected_dirpath, slugify(label) + fasta_ext))
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
        total_contigs_for_the_scaf = split_by_ns(seq, name, broken_scaffolds_fasta, qconfig.Ns_break_threshold, qconfig.min_contig)
        contigs_counter += total_contigs_for_the_scaf
    if contigs_counter > scaffold_counter + 1:
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


def split_by_ns(seq, name, splitted_fasta, Ns_break_threshold=1, min_contig=1, total_contigs=0):
    cur_contig_start = 0
    cumul_contig_length = 0
    while (cumul_contig_length < len(seq)) and (seq.find('N', cumul_contig_length) != -1):
        start = seq.find("N", cumul_contig_length)
        end = start + 1
        while (end != len(seq)) and (seq[end] == 'N'):
            end += 1

        cumul_contig_length = end + 1
        if end - start >= Ns_break_threshold:
            splitted_fasta.append(
                (name.split()[0] + "_" +
                 str(total_contigs + 1),
                 seq[cur_contig_start: start]))
            total_contigs += 1
            cur_contig_start = end
    if len(seq) - cur_contig_start >= min_contig:
        splitted_fasta.append(
            (name.split()[0] + "_" +
             str(total_contigs + 1),
             seq[cur_contig_start: ]))
        total_contigs += 1
    return total_contigs


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
        if not correct_fasta(ref_fpath, qconfig.min_contig, corr_fpath, is_reference=True):
            logger.error('Reference file ' + ref_fpath +
                         ' is empty or contains incorrect sequences (header-only or with non-ACGTN characters)!',
                         exit_with_code=1)
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
    lowercase_labels = [label.lower() for label in labels]
    dup_labels = [label for label in labels if lowercase_labels.count(label.lower()) > 1]
    return dup_labels


def get_labels_from_par_dirs(contigs_fpaths):
    labels = []
    for fpath in contigs_fpaths:
        labels.append(get_label_from_par_dir(fpath))

    for duplicated_label in get_duplicated(labels):
        for i, (label, fpath) in enumerate(zip(labels, contigs_fpaths)):
            if label == duplicated_label:
                labels[i] = get_label_from_par_dir_and_fname(fpath)

    return labels


def process_labels(contigs_fpaths, labels=None, all_labels_from_dirs=False):
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

        duplicated_labels = set(get_duplicated(labels))
        for i, (label, fpath) in enumerate(zip(labels, contigs_fpaths)):
            if label in duplicated_labels:
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
    plots_fpath = os.path.join(output_dirpath, qconfig.plots_fname)
    if os.path.isfile(plots_fpath):
        os.remove(plots_fpath)
    html_fpath = os.path.join(output_dirpath, qconfig.report_prefix + '.html')
    if os.path.isfile(html_fpath):
        os.remove(html_fpath)
    html_report_aux_dir = os.path.join(output_dirpath, qconfig.html_aux_dir)
    if isdir(html_report_aux_dir):
        shutil.rmtree(html_report_aux_dir)


def correct_name(name, max_name_len=MAX_CONTIG_NAME):
    # only word characters, '.', '_', and '-' are allowed. Anything else, e.g., '|' or '=', is substituted with underscore
    return re.sub(r'[^\w\._\-]', '_', name.strip())[:max_name_len]


def correct_asm_label(name, max_name_len=MAX_CONTIG_NAME):
    return name.strip()[:max_name_len]


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
    return correct_asm_label(splitext_for_fasta_file(fname)[0])


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
    return slugify(qconfig.assembly_labels_by_fpath[fpath])


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


def get_free_memory():
    total_mem, free_mem = 2, 2
    if qconfig.platform_name == 'linux_64':
        with open('/proc/meminfo', 'r') as mem:
            for line in mem:
                line = line.split()
                if str(line[0]) == 'MemTotal:':
                    total_mem = int(line[1]) // 1024 // 1024
                if str(line[0]) == 'MemFree:':
                    free_mem = int(line[1]) // 1024 // 1024
    return min(total_mem // 4, free_mem // 2)


def get_chr_len_fpath(ref_fpath, correct_chr_names=None):
    chr_len_fpath = ref_fpath + '.fai'
    raw_chr_names = dict((raw_name, correct_name) for correct_name, raw_name in correct_chr_names.items()) \
        if correct_chr_names else None
    if not is_non_empty_file(chr_len_fpath):
        chr_lengths = fastaparser.get_chr_lengths_from_fastafile(ref_fpath)
        with open(chr_len_fpath, 'w') as out_f:
            for chr_name, chr_len in chr_lengths.items():
                chr_name = raw_chr_names[chr_name] if correct_chr_names else chr_name
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


def get_path_to_program(program, dirpath=None, min_version=None, recommend_version=None, max_allowed_version=None):
    """
    returns the path to an executable or None if it can't be found
    """
    INCOMPATIBLE_VERSION = 0
    CORRECT_VERSION = 1
    NOT_RECOMMENDED_VERSION = 2

    def is_exe(fpath):
        if os.path.isfile(fpath) and os.access(fpath, os.X_OK):
            version_check = check_version(fpath, min_version, recommend_version, max_allowed_version)
            if version_check != INCOMPATIBLE_VERSION:
                if version_check == NOT_RECOMMENDED_VERSION:
                    logger.warning('Version of installed %s differs from its version in the QUAST package (%s). '
                                   'Please make sure that you use an actual version of software.' % (program, recommend_version))
                return True
        return False

    def check_version(fpath, min_version, recommend_version=None, max_allowed_version=None):
        if not min_version:
            return CORRECT_VERSION
        p = subprocess.Popen([fpath, '--version'], stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        stdout, stderr = p.communicate()
        version_pattern = re.compile('(?P<major_version>\d+)\.(?P<minor_version>\d+)')
        v = version_pattern.search(str(stdout))
        if not v.group('major_version') or not v.group('minor_version'):
            return INCOMPATIBLE_VERSION
        version, minor_version = map(int, min_version.split('.')[:2])
        if int(v.group('major_version')) == version and int(v.group('minor_version')) >= minor_version:
            if max_allowed_version is not None:
                max_version, max_minor_version = map(int, max_allowed_version.split('.')[:2])
                if int(v.group('major_version')) > max_version or int(v.group('minor_version')) > max_minor_version:
                    return INCOMPATIBLE_VERSION
            if recommend_version is not None:
                rec_version, rec_minor_version = map(int, recommend_version.split('.')[:2])
                if int(v.group('major_version')) != rec_version or int(v.group('minor_version')) != rec_minor_version:
                    return NOT_RECOMMENDED_VERSION
            return CORRECT_VERSION
        else:
            return INCOMPATIBLE_VERSION

    if dirpath:
        exe_file = os.path.join(dirpath, program)
        if is_exe(exe_file):
            return exe_file

    for path in os.environ["PATH"].split(os.pathsep):
        exe_file = os.path.join(path, program)
        if is_exe(exe_file):
            return exe_file
    return None


def check_java_version(min_version):
    def __get_java_major_version(ver):  # from both 1.X.Y and X.Y formats
        if ver.startswith('1.'):
            return float(ver[2:])
        import math
        return math.floor(float(ver))

    try:
        p = subprocess.Popen(['java', '-version'], stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        stdout, stderr = p.communicate()
        version_pattern = re.compile('version "(\d+\.\d+)')
        if len(version_pattern.findall(str(stdout))) < 1:
            return False
        version = __get_java_major_version(version_pattern.findall(str(stdout))[0])
        return version >= min_version
    except ValueError:
        return False


def is_non_empty_file(fpath, min_size=1):
    # optional parameter is the minimum file size (in bytes) for considering it as a non empty file
    return fpath and os.path.exists(fpath) and os.path.getsize(fpath) > min_size


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


def parse_str_to_num(s):
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
              'For forced retrying, please remove ' + failed_compilation_flag + ' and restart QUAST. '
        if just_notice:
            logger.notice(msg)
        else:
            logger.warning(msg)
        return True
    return False


def safe_rm(path):
    if isfile(path):
        try:
            os.remove(path)
        except OSError:
            pass
    if isdir(path):
        try:
            shutil.rmtree(path, ignore_errors=True)
        except OSError:
            pass


def safe_create(fpath, logger, is_required=False):
    try:
        open(fpath, 'w').close()
    except IOError:
        msg = fpath + ' cannot be created. Did you forget sudo?'
        if is_required:
            logger.error(msg)
        else:
            logger.notice(msg)


def is_python2():
    return sys.version_info[0] < 3


def fix_configure_timestamps(dirpath):
    try:
        os.utime(join(dirpath, 'aclocal.m4'), None)
        os.utime(join(dirpath, 'Makefile.in'), None)
        os.utime(join(dirpath, 'config.h.in'), None)
        os.utime(join(dirpath, 'configure'), None)
    except:
        pass


def compile_tool(name, dirpath, requirements, just_notice=False, logger=logger, only_clean=False, flag_suffix=None,
                 make_cmd=None, configure_args=None):
    make_logs_basepath = join(dirpath, 'make')
    failed_compilation_flag = make_logs_basepath + (str(flag_suffix) if flag_suffix else '') + '.failed'

    if only_clean:
        for required_binary in requirements:
            safe_rm(join(dirpath, required_binary))
        safe_rm(failed_compilation_flag)
        return True

    if not all_required_binaries_exist(dirpath, requirements):
        if check_prev_compilation_failed(name, failed_compilation_flag, just_notice, logger=logger):
            return False

        # making
        logger.main_info('Compiling ' + name + ' (details are in ' + make_logs_basepath +
                         '.log and make.err)')
        open(make_logs_basepath + '.log', 'w').close()
        open(make_logs_basepath + '.err', 'w').close()
        if configure_args:
            prev_dir = os.getcwd()
            os.chdir(dirpath)
            call_subprocess(['./configure'] + configure_args, stdout=open(make_logs_basepath + '.log', 'a'),
                            stderr=open(make_logs_basepath + '.err', 'a'))
            os.chdir(prev_dir)
        try:
            return_code = call_subprocess((['make', make_cmd] if make_cmd else ['make']) + ['-C', dirpath],
                                      stdout=open(make_logs_basepath + '.log', 'a'),
                                      stderr=open(make_logs_basepath + '.err', 'a'), logger=logger)
        except IOError:
            msg = 'Permission denied accessing ' + dirpath + '. Did you forget sudo?'
            if just_notice:
                logger.notice(msg)
            else:
                logger.warning(msg)
            return False

        if return_code != 0 or not all_required_binaries_exist(dirpath, requirements):
            write_failed_compilation_flag(name, dirpath, failed_compilation_flag, just_notice=just_notice, logger=logger)
            return False
    return True


def check_dirpath(path, message="", exit_code=3):
    if not is_ascii_string(path):
        logger.error('QUAST does not support non-ASCII characters in path.\n' + message, to_stderr=True, exit_with_code=exit_code)
    if ' ' in path:
        logger.error('QUAST does not support spaces in paths.\n' + message, to_stderr=True, exit_with_code=exit_code)
    return True


def is_dir_writable(dirpath):
    if isdir(dirpath) and not check_write_permission(dirpath):
        return False
    if not isdir(dirpath) and not check_write_permission(os.path.dirname(dirpath)):
        return False
    return True


def write_failed_compilation_flag(tool, tool_dirpath, failed_compilation_flag, just_notice=False, logger=logger):
    msg = "Failed to compile " + tool + " (" + tool_dirpath + ")! Try to compile it manually. " + (
          "You can restart Quast with the --debug flag to see the compilation command." if not qconfig.debug else "")
    if just_notice:
        logger.notice(msg)
    else:
        logger.warning(msg)
    safe_create(failed_compilation_flag, logger)


def check_write_permission(path):
    return os.access(path, os.W_OK)


def get_dir_for_download(dirname, tool, required_files, logger, only_clean=False):
    tool_dirpath = join(qconfig.LIBS_LOCATION, dirname)
    quast_home_dirpath = join(os.path.expanduser('~'), '.quast')
    tool_home_dirpath = join(quast_home_dirpath, dirname)
    if all(os.path.exists(join(tool_dirpath, fpath)) for fpath in required_files):
        return tool_dirpath
    if all(os.path.exists(join(tool_home_dirpath, fpath)) for fpath in required_files):
        return tool_home_dirpath
    if not is_dir_writable(tool_dirpath):
        if not only_clean:
            logger.notice('Permission denied accessing ' + tool_dirpath + '. ' + tool + ' will be downloaded to home directory ' + quast_home_dirpath)
        if not is_dir_writable(quast_home_dirpath):
            if not only_clean:
                logger.warning('Permission denied accessing home directory ' + quast_home_dirpath + '. ' + tool + ' cannot be downloaded.')
            return None
        tool_dirpath = tool_home_dirpath

    if not isdir(tool_dirpath):
        os.makedirs(tool_dirpath)
    return tool_dirpath


def check_reads_fpaths(logger):
    reads_libraries = [qconfig.forward_reads, qconfig.reverse_reads, qconfig.interlaced_reads, qconfig.unpaired_reads,
                       qconfig.mp_forward_reads, qconfig.mp_reverse_reads, qconfig.mp_interlaced_reads,
                       qconfig.pacbio_reads, qconfig.nanopore_reads]
    qconfig.reads_fpaths = [fpath for lib in reads_libraries for fpath in lib if fpath]
    if not qconfig.reads_fpaths:
        return None
    qconfig.paired_reads = [(read1, read2) for read1, read2 in zip(qconfig.forward_reads, qconfig.reverse_reads)]
    qconfig.paired_reads.extend(qconfig.interlaced_reads)
    qconfig.mate_pairs = [(read1, read2) for read1, read2 in zip(qconfig.mp_forward_reads, qconfig.mp_reverse_reads)]
    qconfig.mate_pairs.extend(qconfig.mp_interlaced_reads)


def get_blast_fpath(fname):
    blast_path = get_path_to_program(fname, blast_dirpath)
    return blast_path


def show_progress(a, b, c):
    if a > 0 and int(c/(b*100)) > 0 and a % int(c/(b*100)) == 0:
        print("% 3.1f%% of %d bytes" % (min(100, int(float(a * b) / c * 100)), c)),
        sys.stdout.flush()


def download_blast_binaries(logger=logger, filenames=None, only_clean=False):
    global blast_dirpath

    all_binaries = ['makeblastdb', 'blastn', 'tblastn']
    filenames = filenames or all_binaries
    required_files = [cmd for cmd in filenames if not get_blast_fpath(cmd)]
    if not required_files and not only_clean:
        return True

    blast_dirpath = get_dir_for_download('blast', 'BLAST', filenames, logger, only_clean=only_clean)
    if not blast_dirpath:
        return False

    required_files = [cmd for cmd in filenames if not get_blast_fpath(cmd)]
    if only_clean:
        if os.path.isdir(blast_dirpath):
            shutil.rmtree(blast_dirpath, ignore_errors=True)
        return True

    for i, cmd in enumerate(required_files):
        blast_fpath = download_blast_binary(cmd, logger=logger)
        logger.info()
        if not blast_fpath:
            return False
        os.chmod(blast_fpath, os.stat(blast_fpath).st_mode | stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH)
    return True


def download_file(url, fpath, tool_name, move_file=True):
    if not os.path.exists(fpath):
        logger.info('Downloading %s (file: %s)...' % (tool_name, os.path.basename(fpath)))
        try:
            urllib.URLopener().retrieve(url, fpath + '.download', show_progress)
        except Exception:
            _, exc_value, _ = sys.exc_info()
            logger.error(
                'Failed downloading %s (url: %s), QUAST functionality will be limited! '
                'Exception caught: %s\n'
                'You can try to download the file manually, place it in %s and restart QUAST'
                % (tool_name, url, exc_value, fpath))
            return None
        if move_file:
            shutil.move(fpath + '.download', fpath)
            logger.info('%s successfully downloaded!' % tool_name)
    return fpath


def download_blast_binary(blast_filename, logger=logger):
    if not os.path.isdir(blast_dirpath):
        os.makedirs(blast_dirpath)

    return download_external_tool(blast_filename, blast_dirpath, 'blast', platform_specific=True, is_executable=True)


def download_external_tool(fname, dirpath, tool, platform_specific=False, is_executable=False):
    downloaded_fpath = join(dirpath, fname)
    if os.path.exists(downloaded_fpath):
        return downloaded_fpath

    external_dirpath = join(external_tools_dirpath, tool)
    if platform_specific:
        external_dirpath = join(external_dirpath, qconfig.platform_name)
    external_fpath = os.path.join(external_dirpath, fname)
    url = join(qconfig.GIT_ROOT_URL + relpath(external_dirpath, qconfig.QUAST_HOME), fname)
    if not exists(dirpath):
        os.makedirs(dirpath)
    if isfile(external_fpath):
        logger.info('Copying ' + fname + ' from ' + external_fpath)
        shutil.copy(external_fpath, downloaded_fpath)
    else:
        downloaded_fpath = download_file(url, downloaded_fpath, tool)
    if is_executable:
        if downloaded_fpath:
            os.chmod(downloaded_fpath, os.stat(downloaded_fpath).st_mode | stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH)
        else:
            logger.error('Please try to install the tool manually, add it to your PATH environment variable, and restart QUAST.')
    return downloaded_fpath


def run_parallel(_fn, fn_args, n_jobs=None, filter_results=False):
    if qconfig.memory_efficient:
        results_tuples = [_fn(*args) for args in fn_args]
    else:
        n_jobs = n_jobs or qconfig.max_threads
        parallel_args = {'n_jobs': n_jobs}
        try:
            import joblib
            from joblib import Parallel, delayed
            try:
                # starting from joblib 0.10 the default backend has changed to 'loky' which causes QUAST crashes,
                # since it uses default values in qconfig module. So, we explicitly require 'multiprocessing' here.
                # Note that Parallel doesn't have 'require' argument in joblib 0.9 and earlier.
                new_style_parallel_args = {'backend': 'multiprocessing'}
                Parallel(**new_style_parallel_args)
                parallel_args.update(new_style_parallel_args)
            except TypeError:
                pass
        except ImportError:
            if is_python2():
                from joblib2 import Parallel, delayed
            else:
                from joblib3 import Parallel, delayed
        results_tuples = Parallel(**parallel_args)(delayed(_fn)(*args) for args in fn_args)
    results = []
    if results_tuples:
        if isinstance(results_tuples[0], list) or isinstance(results_tuples[0], tuple):
            results_cnt = len(results_tuples[0])
            if filter_results:
                results = [[result_list[i] for result_list in results_tuples if result_list[i]] for i in range(results_cnt)]
            else:
                results = [[result_list[i] for result_list in results_tuples] for i in range(results_cnt)]
        else:
            results = [result for result in results_tuples if result or not filter_results]
    return results


# based on http://stackoverflow.com/questions/196345/how-to-check-if-a-string-in-python-is-in-ascii
def is_ascii_string(line):
    try:
        line.encode('ascii')
    except UnicodeDecodeError:  # python2
        return False
    except UnicodeEncodeError:  # python3
        return False
    else:
        return True


def md5(fpath):
    hash_md5 = hashlib.md5()
    with open(fpath, 'rb') as f:
        while True:
            buf = f.read(8192)
            if not buf:
                break
            hash_md5.update(buf)
    return hash_md5.hexdigest()


def verify_md5(fpath, md5_fpath=None):
    if md5_fpath is None:
        md5_fpath = fpath + '.md5'
    if is_non_empty_file(fpath) and is_non_empty_file(md5_fpath):
        # logger.warning()
        with open(md5_fpath) as f:
            expected_md5 = f.readline().strip().split()[0]
        calculated_md5 = str(md5(fpath))
        if expected_md5 != calculated_md5:
            logger.warning('Failure of md5 check for %s! Expected md5 is %s, calculated md5 is %s.' %
                           (fpath, expected_md5, calculated_md5))
            return False
        return True
    logger.warning('Failed to check md5 for %s! Either this file or its md5 file (%s) is missing or empty.' %
                   (fpath, md5_fpath))
    return False


def percentile(values, percent):
    import math
    percentile_idx = int(math.ceil((len(values) * percent) / 100)) - 1
    return values[max(0, percentile_idx)]


def calc_median(values):
    if len(values) % 2 == 1:  # odd number of values
        median = values[(len(values) - 1) // 2]
    else:  # even number of values - take the avg of central
        median = (values[len(values) // 2] + values[len(values) // 2 - 1]) // 2
    return median

