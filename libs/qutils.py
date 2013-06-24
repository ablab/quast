############################################################################
# Copyright (c) 2011-2013 Saint-Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################

from __future__ import with_statement
import glob
import gzip
import shutil
import zipfile
import bz2
import os
import sys
import re
import qconfig

from libs.log import get_logger
logger = get_logger(qconfig.LOGGER_DEFAULT_NAME)


def assert_file_exists(fpath, message='', logger=logger):
    if not os.path.isfile(fpath):
        logger.error("File not found (%s): %s" % (message, fpath), 2,
                     to_stderr=True)

    return fpath


def id_to_str(id):
    if qconfig.assemblies_num == 1:
        return ''
    else:
        return ('%d ' + ('' if id >= 10 else ' ')) % (id + 1)


def uncompress(compressed_fname, uncompressed_fname, logger=logger):
    fname, ext = os.path.splitext(compressed_fname)

    if ext not in ['.zip', '.bz2', '.gz']:
        return False

    logger.info('  extracting %s...' % compressed_fname)
    compressed_file = None

    if ext == '.zip':
        try:
            zfile = zipfile.ZipFile(compressed_fname)
        except Exception, e:
            logger.error('can\'t open zip file: ' + str(e.message))
            return False

        names = zfile.namelist()
        if len(names) == 0:
            logger.error('zip archive is empty')
            return False

        if len(names) > 1:
            logger.warning('zip archive must contain exactly one file. Using %s' % names[0])

        compressed_file = zfile.open(names[0])

    if ext == '.bz2':
        compressed_file = bz2.BZ2File(compressed_fname)

    if ext == '.gz':
        compressed_file = gzip.open(compressed_fname)

    with open(uncompressed_fname, 'w') as uncompressed_file:
        uncompressed_file.write(compressed_file.read())

    logger.info('    extracted!')
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
