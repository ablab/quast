#!/usr/bin/env python
#
# Manta - Structural Variant and Indel Caller
# Copyright (c) 2013-2015 Illumina, Inc.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#
#

"""
python version of cat to work around portability/quoting and shell line limits
"""

import os, sys


def ensureDir(d):
    """
    make directory if it doesn't already exist, raise exception if
    something else is in the way:
    """
    if os.path.exists(d):
        if not os.path.isdir(d) :
            raise Exception("Can't create directory: %s" % (d))
    else :
        os.makedirs(d)


def getOptions() :

    from optparse import OptionParser

    usage = "usage: %prog -o output [input [input...]]"
    parser = OptionParser(usage=usage,description="Concatenate input files to output")

    parser.add_option("-o","--output", dest="outFile",default=False,
                      help="output filename (required)")

    (options,args) = parser.parse_args()

    if len(args) == 0 :
        parser.print_help()
        sys.exit(2)

    # validate input:
    for arg in args :
        if not os.path.isfile(arg) :
            raise Exception("Can't find input file: " +arg)

    if options.outFile is None :
        parser.print_help()
        sys.exit(2)

    ensureDir(os.path.dirname(os.path.abspath(options.outFile)))

    return (options,args)



def main() :

    (options,args) = getOptions()

    ofp = open(options.outFile,"w")
    for arg in args :
        for line in open(arg) :
            ofp.write(line)


main()
