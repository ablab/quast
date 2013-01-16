############################################################################
# Copyright (c) 2011-2012 Saint-Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################

from __future__ import with_statement
import gzip
import zipfile
import bz2
import os
import sys
import qconfig
import datetime


def warning(message=''):
    print "====== WARNING! " + str(message) + " ======"


def error(message='', errcode=1):
    print >> sys.stderr, "\n====== ERROR! " + str(message) + " ======\n"
    if errcode:
        sys.exit(errcode)


def assert_file_exists(fpath, message=''):
    if not os.path.isfile(fpath):
        error("File not found (%s): %s" % (message, fpath), 2)
    return fpath


def print_timestamp(message=''):
    now = datetime.datetime.now()
    current_time = now.strftime("%Y-%m-%d %H:%M:%S")
    print "\n" + message + current_time
    return now

def id_to_str(id):
    if qconfig.assemblies_num == 1:
        return ''
    else:
        return ('%d ' + ('' if id >= 10 else ' ')) % (id + 1)


def uncompress(compressed_fname, uncompressed_fname, errout):
    fname, ext = os.path.splitext(compressed_fname)

    if ext not in ['.zip', '.bz2', '.gz']:
        return False

    print >> errout, '  decompressing %s' % compressed_fname, '...',
    compressed_file = None

    if ext == '.zip':
        try:
            zfile = zipfile.ZipFile(compressed_fname)
        except Exception, e:
            print >> errout, '\n    can\'t open zip file:', e.message
            return False

        names = zfile.namelist()
        if len(names) == 0:
            print >> errout, '\n    zip archive is empty'
            return False

        if len(names) > 1:
            print >> errout, '\n    zip archive must contain exactly one file. Using %s' % names[0]

        compressed_file = zfile.open(names[0])

    if ext == '.bz2':
        compressed_file = bz2.BZ2File(compressed_fname)

    if ext == '.gz':
        compressed_file = gzip.open(compressed_fname)

    with open(uncompressed_fname, 'w') as uncompressed_file:
        uncompressed_file.write(compressed_file.read())
        print >> errout, 'uncompressed!'
        return True


# TODO: get rid of Tee (use python logger instead)
class Tee(object):
    def __init__(self, name, mode, console=True):
        self.file = open(name, mode)
        self.stdout = sys.stdout
        self.stderr = sys.stderr
        self.console = console
        sys.stdout = self
        sys.stderr = self

    def free(self):
        sys.stdout = self.stdout
        sys.stderr = self.stderr
        self.file.close()

    def write(self, data):
        self.file.write(data)
        if self.console:
            self.stdout.write(data)
        self.flush()

    def flush(self):
        self.file.flush()
        self.stdout.flush()
