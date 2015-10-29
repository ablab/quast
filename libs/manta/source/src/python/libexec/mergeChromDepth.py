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
Merge chrom depth from multiple samples/BAM files
"""

import os,sys

scriptDir=os.path.abspath(os.path.dirname(__file__))
pythonLibDir=os.path.abspath(os.path.join(scriptDir,"@THIS_RELATIVE_PYTHON_LIBDIR@"))
sys.path.append(pythonLibDir)

from workflowUtil import checkFile



def getOptions() :

    from optparse import OptionParser

    usage = "usage: %prog [options]"
    parser = OptionParser(usage=usage)

    parser.add_option("--in", type="string",dest="inFiles",metavar="FILE", action="append",
                      help="input depth filename, argument may be provided more than once to provide all input")
    parser.add_option("--out", type="string",dest="outFile",metavar="FILE",
                      help="output depth filename (required)")

    (options,args) = parser.parse_args()

    if len(args) != 0 :
        parser.print_help()
        sys.exit(2)

    # validate input:
    if options.inFiles is None :
        parser.print_help()
        sys.exit(2)

    if options.outFile is None :
        parser.print_help()
        sys.exit(2)

    for inFile in options.inFiles :
        checkFile(inFile,"input depth")

    return (options,args)


def main() :

    (options,args) = getOptions()

    chrtot = {}
    for (index,inFile) in enumerate(options.inFiles) :
        chr = {}
        ifp = open(inFile)
        for line in ifp :
            w = line.strip().split('\t')
            assert(w[0] not in chr)
            chr[w[0]] = float(w[1])

        if (index!=0) :
            assert(len(chrtot) == len(chr))

        for k in chr :
            if (index!=0) :
                assert(k in chrtot)
                chrtot[k] += chr[k]
            else :
                chrtot[k] = chr[k]

    ofp = open(options.outFile,"w")
    for k in chrtot :
        ofp.write("%s\t%.3f\n" % (k,chrtot[k]))


main()
