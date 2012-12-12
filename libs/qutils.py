############################################################################
# Copyright (c) 2011-2012 Saint-Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################

from __future__ import with_statement

assemblies_number = 0

def id_to_str(id):
    if assemblies_number == 1:
        return ''
    else:
        return ('%d ' + ('' if id >= 10 else ' ')) % (id + 1)

import gzip
import zipfile
import bz2
import os

def uncompress(compressed_fname, uncompressed_fname, errout):
    fname, ext = os.path.splitext(compressed_fname)

    if ext not in ['.zip', '.bz2', '.gz']:
        return False

    print >> errout, 'Decompressing %s' % compressed_fname
    compressed_file = None

    if ext == '.zip':
        try:
            zfile = zipfile.ZipFile(compressed_fname)
        except Exception, e:
            print >> errout, 'Can\'t open zip file:', e.message
            return False

        names = zfile.namelist()
        if len(names) == 0:
            print >> errout, 'Zip archive is empty'
            return False

        if len(names) > 1:
            print >> errout, 'Zip archive must contain exactly one file. Using %s' % names[0]

        compressed_file = zfile.open(names[0])

    if ext == '.bz2':
        compressed_file = bz2.BZ2File(compressed_fname)

    if ext == '.gz':
        compressed_file = gzip.open(compressed_fname)

    with open(uncompressed_fname, 'w') as uncompressed_file:
        uncompressed_file.write(compressed_file.read())
        print >> errout, 'Uncompressed'
        return True
