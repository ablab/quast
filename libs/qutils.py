############################################################################
# Copyright (c) 2011-2013 Saint-Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################

from __future__ import with_statement
import glob
import gzip
import logging
log = logging.getLogger('quast')
import shutil
import zipfile
import bz2
import os
import sys
import re
import qconfig
import datetime


def notice(message='', indent=''):
    log.info(indent + "NOTICE: " + str(message))


def warning(message='', indent=''):
    log.info(indent + "WARNING! " + str(message))


def error(message='', exit_with_code=1, console_output=False, indent=''):
    msg = indent + 'ERROR! ' + str(message)

    with open(qconfig.error_log_fpath, 'w') as error_f:
        error_f.write(message)

    # decorated_msg = '=' * len(msg) + '\n' + msg + '\n' + '=' * len(msg)
    decorated_msg = '\n' + msg + '\n'

    if console_output:
        print >> sys.stderr, decorated_msg
    else:
        log.info(decorated_msg)
    if exit_with_code:
        exit(exit_with_code)


def assert_file_exists(fpath, message=''):
    if not os.path.isfile(fpath):
        error("File not found (%s): %s" % (message, fpath), 2, console_output=True)
    return fpath


def print_timestamp(message=''):
    now = datetime.datetime.now()
    current_time = now.strftime("%Y-%m-%d %H:%M:%S")
    log.info("\n" + message + current_time)
    return now


def print_version(to_stderr=False):
    version_filename = os.path.join(qconfig.LIBS_LOCATION, '..', 'VERSION')
    version = "unknown"
    build = "unknown"
    if os.path.isfile(version_filename):
        version_file = open(version_filename)
        version = version_file.readline()
        if version:
            version = version.strip()
        else:
            version = "unknown"
        build = version_file.readline()
        if build:
            build = build.split()[1].strip()
        else:
            build = "unknown"

    if to_stderr:
        print >> sys.stderr, "Version", str(version) + (", build " + str(build) if build != "unknown" else "")
    else:
        log.info("Version " + str(version) + (", build " + str(build) if build != "unknown" else ""))


def print_system_info():
    log.info("System information:")
    try:
        import platform
        log.info("  OS: " + platform.platform())
        log.info("  Python version: " + str(sys.version_info[0]) + "." + str(sys.version_info[1]) + '.'\
                  + str(sys.version_info[2]))
        import multiprocessing
        log.info("  CPUs number: " + str(multiprocessing.cpu_count()))
    except:
        log.info("  Problem occurred when getting system information")


def id_to_str(id):
    if qconfig.assemblies_num == 1:
        return ''
    else:
        return ('%d ' + ('' if id >= 10 else ' ')) % (id + 1)


def uncompress(compressed_fname, uncompressed_fname):
    fname, ext = os.path.splitext(compressed_fname)

    if ext not in ['.zip', '.bz2', '.gz']:
        return False

    log.info('  extracting %s...' % compressed_fname)
    compressed_file = None

    if ext == '.zip':
        try:
            zfile = zipfile.ZipFile(compressed_fname)
        except Exception, e:
            error('can\'t open zip file: ' + str(e.message))
            return False

        names = zfile.namelist()
        if len(names) == 0:
            error('zip archive is empty')
            return False

        if len(names) > 1:
            warning('zip archive must contain exactly one file. Using %s' % names[0])

        compressed_file = zfile.open(names[0])

    if ext == '.bz2':
        compressed_file = bz2.BZ2File(compressed_fname)

    if ext == '.gz':
        compressed_file = gzip.open(compressed_fname)

    with open(uncompressed_fname, 'w') as uncompressed_file:
        uncompressed_file.write(compressed_file.read())

    log.info('    extracted!')
    return True


def remove_reports(output_dirpath):
    for gage_prefix in ["", qconfig.gage_report_prefix]:
        for report_prefix in [qconfig.report_prefix, qconfig.transposed_report_prefix]:
            pattern = os.path.join(output_dirpath, gage_prefix + report_prefix + ".*")
            for f in glob.iglob(pattern):
                os.remove(f)
    plots_filename = os.path.join(output_dirpath, qconfig.plots_filename)
    if os.path.isfile(plots_filename):
        os.remove(plots_filename)
    html_report_aux_dir = os.path.join(output_dirpath, qconfig.html_aux_dir)
    if os.path.isdir(html_report_aux_dir):
        shutil.rmtree(html_report_aux_dir)


def correct_name(name):
    return re.sub(r'[^\w\._\-+|]', '_', name.strip())
