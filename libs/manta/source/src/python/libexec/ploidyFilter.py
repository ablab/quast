#! /usr/bin/env python
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
filter vcf to remove overlapping diploid calls which can't be resolved to two haplotypes
"""

import sys
import re
from os.path import exists, isfile
from optparse import OptionParser

def getKeyVal(string,key) :
    match=re.search("%s=([^;\t]*);?" % (key) ,string)
    if match is None : return None
    return match.group(1);


VCF_CHROM = 0
VCF_POS = 1
VCF_REF = 3
VCF_ALT = 4
VCF_QUAL = 5
VCF_FILTER = 6
VCF_INFO = 7
VCF_FORMAT = 8
VCF_SAMPLE = 9

class VcfRecord :
    def __init__(self, line) :
        #self.line = line
        w = line.strip().split('\t')
        self.chrom = w[VCF_CHROM]
        self.pos = int(w[VCF_POS])
        self.isPass = (w[VCF_FILTER] == "PASS")

        self.end = self.pos+len(w[VCF_REF])-1
        val = getKeyVal(w[VCF_INFO],"END")
        if val is not None :
            self.end = int(val)

        self.svLen = None
        val = getKeyVal(w[VCF_INFO],"SVLEN")
        if val is not None :
            self.svLen = int(val)

        self.svType = getKeyVal(w[VCF_INFO],"SVTYPE")

        fmt = w[VCF_FORMAT]
        gtIx = fmt.split(':').index("GT")

        self.gtType = []
        for sample in w[VCF_SAMPLE:] :
            gt = sample.split(':')[gtIx]
            t = gt.split('/')
            self.gtType.append(int(t[0]) + int(t[1]))


def getOptions():
    usage = "usage: %prog [options] vcf > filtered_vcf"
    parser = OptionParser(usage=usage)
    (options,args) = parser.parse_args()

    if len(args) != 1 :
        parser.print_help()
        sys.exit(2)

    # validate input:
    if not isfile(args[0]) :
        raise Exception("Can't find input vcf file: " + args[0])

    return (options,args)


def process_block(recordBlock, nextPos, filteredSites):

    # sys.stderr.write("processing a block with %s sites...\n" % len(recordBlock))

    while (len(recordBlock) > 0):
        target = recordBlock[0]
        targetEnd = target.end
        # when a new target's end is larger than
        # the pos of the next site to be read,
        # we need to read in more sites
        if targetEnd > nextPos:
            break

        targetLen = -1
        if target.svLen is not None:
            targetLen = abs(target.svLen)
        targetType = target.svType

        ploidySum = []
        for gtPloidy in target.gtType :
            ploidySum.append(gtPloidy)
        overlapIds = [0]

        for ix in xrange(1, len(recordBlock)):
            record = recordBlock[ix]
            pos = record.pos
            svLen = -1
            if record.svLen is not None:
                svLen = abs(record.svLen)
            svType = record.svType

            # collecting stacked sites
            # with the same type and similar size
            if pos < targetEnd:
                if (
                   # (svType == targetType) and
                    (svLen < 2*targetLen) and
                    (svLen > 0.5*targetLen)):
                    for (sampleIndex, gtPloidy) in enumerate(record.gtType) :
                        ploidySum[sampleIndex] += gtPloidy
                    overlapIds.append(ix)
            else:
                break

        overlapIds.reverse()
        isAnomPloidy = False
        for psum in ploidySum :
            if psum > 2 :
                isAnomPloidy = True
        if isAnomPloidy:
            # sites to be filtered due to ploidity
            for i in overlapIds:
                site = recordBlock.pop(i)
                chrm = site.chrom
                pos = site.pos
                end = site.end

                if not(chrm in filteredSites):
                    filteredSites[chrm] = {}
                filteredSites[chrm][(pos, end)] = True
        else:
            # sites to be kept
            for i in overlapIds:
                recordBlock.pop(i)


def find_stacked_variants(vcfFile):
    filteredSites = {}
    recordBlock = []
    maxEnd = -1
    count = 0

    for line in open(vcfFile):
        if line[0] == "#": continue
        record = VcfRecord(line)

        chrm = record.chrom
        pos = record.pos
        svType = record.svType
        count += 1

        # ignore filtered records
        isPassed = record.isPass
        if not(isPassed):
            continue

        # consider DEL & DUP only
        if (svType != "DEL") and (svType != "DUP"): continue
        end = record.end

        # set up the first target site
        if (len(recordBlock) == 0):
            targetChrm = chrm
            targetEnd = end
        else:
            targetChrm = recordBlock[0].chrom
            targetEnd = recordBlock[0].end

        # keep reading into the block until exceeding the target's end
        if (chrm == targetChrm) and (pos < targetEnd):
            recordBlock.append(record)
            maxEnd = max(maxEnd, end)
        else:
            nextPos = pos
            if (chrm != targetChrm):
                nextPos = maxEnd + 1
                maxEnd = -1

            # process the block until pos < the new target's end
            process_block(recordBlock, nextPos, filteredSites)

            recordBlock.append(record)
            maxEnd = max(maxEnd, end)

    # process the last block
    process_block(recordBlock, maxEnd+1, filteredSites)

    sys.stderr.write("Processed %s sites in the vcf.\n" % count)
    numFiltered = 0
    for c in filteredSites:
        numFiltered += len(filteredSites[c])
    sys.stderr.write("Filtered %s sites due to ploidy.\n" % numFiltered)
    sys.stderr.write("Filtered sites: %s\n" % filteredSites)

    return filteredSites


def check_filtered_sites(site, filteredSites):
    chrm = site.chrom
    pos = site.pos
    end = site.end

    return ((chrm in filteredSites) and ((pos, end) in filteredSites[chrm]))


def filter_variants(vcfFile, filteredSites):

    isHeaderAdded = False
    filterHeadline = "##FILTER=<ID=Ploidy,Description=\"For DEL & DUP variants, the genotypes of overlapping variants (with similar size) are inconsistent with diploid expectation\">\n"

    vcfOut = sys.stdout

    for line in open(vcfFile):
        if line[0] != '#':
            site = VcfRecord(line)
            # only filter on DEL & DUP for now
            if (site.isPass and
                ((site.svType == "DEL") or (site.svType == "DUP"))):

                isFiltered = check_filtered_sites(site, filteredSites)
                if isFiltered:
                    w = line.strip().split('\t')
                    # add the "Ploidy" filter
                    w[VCF_FILTER] = "Ploidy"
                    line = "\t".join(w)+"\n"
        elif not(isHeaderAdded) and (line[:8] == "##FILTER"):
            vcfOut.write(filterHeadline)
            isHeaderAdded = True

        vcfOut.write(line)


if __name__=='__main__':

    # Command-line args
    (options,args) = getOptions()
    vcfFile = args[0]

    filteredSites = find_stacked_variants(vcfFile)
    filter_variants(vcfFile, filteredSites)
