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
Workflow configuration options shared by multiple
configuration scripts.
"""

import os,sys

scriptDir=os.path.abspath(os.path.dirname(__file__))
scriptName=os.path.basename(__file__)

sys.path.append(scriptDir)

from configureOptions import ConfigureWorkflowOptions
from configureUtil import assertOptionExists, joinFile, OptParseException, validateFixExistingDirArg, validateFixExistingFileArg
from workflowUtil import exeFile, parseGenomeRegion



def cleanLocals(locals_dict) :
    """
    When passed a locals() dictionary, clean out all of the hidden keys and return
    """

    return dict((k,v) for (k,v) in locals_dict.items() if not k.startswith("__") and k != "self")



class MantaWorkflowOptionsBase(ConfigureWorkflowOptions) :

    validAlignerModes = ["bwa","isaac"]

    def addWorkflowGroupOptions(self,group) :
        group.add_option("--referenceFasta",type="string",metavar="FILE",
                         help="samtools-indexed reference fasta file [required]")
        group.add_option("--runDir", type="string",metavar="DIR",
                         help="Run script and run output will be written to this directory [required] (default: %default)")

    def addExtendedGroupOptions(self,group) :
        group.add_option("--scanSizeMb", type="int", metavar="INT",
                         help="Maximum sequence region size (in megabases) scanned by each task during "
                         "SV Locus graph generation. (default: %default)")
        group.add_option("--region", type="string",dest="regionStrList",metavar="REGION", action="append",
                         help="Limit the analysis to a region of the genome for debugging purposes. "
                              "If this argument is provided multiple times all specified regions will "
                              "be analyzed together. All regions must be non-overlapping to get a "
                              "meaningful result. Examples: '--region chr20' (whole chromosome), "
                              "'--region chr2:100-2000 --region chr3:2500-3000' (two regions)'")

        ConfigureWorkflowOptions.addExtendedGroupOptions(self,group)


    def getOptionDefaults(self) :
        """
        Set option defaults.

        Every local variable in this method becomes part of the default hash
        """

        alignerMode = "isaac"

        libexecDir=os.path.abspath(os.path.join(scriptDir,"@THIS_RELATIVE_LIBEXECDIR@"))
        assert os.path.isdir(libexecDir)

        bgzipBin=joinFile(libexecDir,exeFile("bgzip"))
        htsfileBin=joinFile(libexecDir,exeFile("htsfile"))
        tabixBin=joinFile(libexecDir,exeFile("tabix"))

        mantaStatsBin=joinFile(libexecDir,exeFile("GetAlignmentStats"))
        mantaMergeStatsBin=joinFile(libexecDir,exeFile("MergeAlignmentStats"))
        mantaGetChromDepthBin=joinFile(libexecDir,exeFile("GetChromDepth"))
        mantaGraphBin=joinFile(libexecDir,exeFile("EstimateSVLoci"))
        mantaGraphMergeBin=joinFile(libexecDir,exeFile("MergeSVLoci"))
        mantaStatsMergeBin=joinFile(libexecDir,exeFile("MergeEdgeStats"))
        mantaGraphCheckBin=joinFile(libexecDir,exeFile("CheckSVLoci"))
        mantaHyGenBin=joinFile(libexecDir,exeFile("GenerateSVCandidates"))
        mantaGraphStatsBin=joinFile(libexecDir,exeFile("SummarizeSVLoci"))
        mantaStatsSummaryBin=joinFile(libexecDir,exeFile("SummarizeAlignmentStats"))

        mergeChromDepth=joinFile(libexecDir,"mergeChromDepth.py")
        mantaSortVcf=joinFile(libexecDir,"sortVcf.py")
        mantaExtraSmallVcf=joinFile(libexecDir,"extractSmallIndelCandidates.py")
        mantaPloidyFilter=joinFile(libexecDir,"ploidyFilter.py")
        mantaSortEdgeLogs=joinFile(libexecDir,"sortEdgeLogs.py")
        mantaCat=joinFile(libexecDir,"cat.py")

        # default memory request per process-type
        #
        # where different values are provided for SGE and local runs note:
        #  1. for SGE the memory limits must be greater than the highest memory use ever
        #      expected in a production run. The consequence of exceeding this limit is a failed
        #      run.
        #   2. for localhost the memory usage should be at least above the highest mean memory
        #       use ever expected in a production run. The consequence of exceeding the mean is
        #       a slow run due to swapping.
        #
        estimateMemMb=2*1024
        mergeMemMb=4*1024
        hyGenSGEMemMb=4*1024
        hyGenLocalMemMb=2*1024

        scanSizeMb = 12

        return cleanLocals(locals())



    def validateAndSanitizeExistingOptions(self,options) :

        options.runDir=os.path.abspath(options.runDir)

        # check alignerMode:
        if options.alignerMode is not None :
            options.alignerMode = options.alignerMode.lower()
            if options.alignerMode not in self.validAlignerModes :
                raise OptParseException("Invalid aligner mode: '%s'" % options.alignerMode)

        options.referenceFasta=validateFixExistingFileArg(options.referenceFasta,"reference")

        # check for reference fasta index file:
        if options.referenceFasta is not None :
            faiFile=options.referenceFasta + ".fai"
            if not os.path.isfile(faiFile) :
                raise OptParseException("Can't find expected fasta index file: '%s'" % (faiFile))

        if (options.regionStrList is None) or (len(options.regionStrList) == 0) :
            options.genomeRegionList = None
        else :
            options.genomeRegionList = [parseGenomeRegion(r) for r in options.regionStrList]


    def validateOptionExistence(self,options) :

        assertOptionExists(options.runDir,"run directory")

        assertOptionExists(options.alignerMode,"aligner mode")
        assertOptionExists(options.referenceFasta,"reference fasta file")


