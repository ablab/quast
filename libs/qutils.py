############################################################################
# Copyright (c) 2015 Saint Petersburg State University
# Copyright (c) 2011-2015 Saint Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################

from __future__ import with_statement
import glob
import gzip
import shutil
import subprocess
import zipfile
import bz2
import os
import sys
import re
import qconfig

from libs.log import get_logger
logger = get_logger(qconfig.LOGGER_DEFAULT_NAME)

MAX_CONTIG_NAME = 1021  # Nucmer's constraint

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


def correct_name(name):
    name = re.sub(r'[^\w\._\-+|]', '_', name.strip())[:MAX_CONTIG_NAME]
    return re.sub(r"[\|+=/]", '_', name.strip())[:MAX_CONTIG_NAME]


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


def check_is_fasta_file(fname):
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
    return os.path.exists(fpath) and os.path.getsize(fpath) > 10


def cat_files(in_fnames, out_fname):
    if not isinstance(in_fnames, list):
        in_fnames = [in_fnames]
    with open(out_fname, 'w') as outfile:
        for fname in in_fnames:
            if os.path.exists(fname):
                with open(fname) as infile:
                    for line in infile:
                        outfile.write(line)
