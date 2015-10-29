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
Manta SV discovery workflow
"""


import os.path
import shutil
import sys

# add script path to pull in utils in same directory:
scriptDir=os.path.abspath(os.path.dirname(__file__))
sys.path.append(os.path.abspath(scriptDir))

# add pyflow path:
# TODO: get a more robust link to the pyflow dir at config time:
pyflowDir=os.path.join(scriptDir,"pyflow")
sys.path.append(os.path.abspath(pyflowDir))

from configBuildTimeInfo import workflowVersion
from pyflow import WorkflowRunner
from workflowUtil import checkFile, ensureDir, isWindows, preJoin, which, \
                         getNextGenomeSegment, getFastaChromOrderSize, getRobustChromId, cleanPyEnv

from configureUtil import getIniSections,dumpIniSections



__version__ = workflowVersion


def isString(x):
    return isinstance(x, basestring)


def isIterable(x):
    return (getattr(x, '__iter__', False) != False)


def lister(x):
    """
    Convert input into a list, whether it's already iterable or
    not. Make an exception for individual strings to be returned
    as a list of one string, instead of being chopped into letters
    Also, convert None type to empty list:
    """
    # special handling in case a single string is given:
    if x is None : return []
    if (isString(x) or (not isIterable(x))) : return [x]
    return list(x)



def setzer(x) :
    """
    convert user input into a set, handling the pathological case
    that you have been handed a single string, and you don't want
    a set of letters:
    """
    return set(lister(x))


def getMkdirCmd() :
    if isWindows() :
        return ["mkdir"]
    else:
        return ["mkdir","-p"]

def getRmdirCmd() :
    if isWindows():
        return ["rd","/s","/q"]
    else:
        return ["rm","-rf"]

def getRmCmd() :
    if isWindows():
        return ["del","/f"]
    else:
        return ["rm","-f"]

def quoteStringList(strList):
    return ["\"%s\"" % (x) for x in strList]



def runStats(self,taskPrefix="",dependencies=None) :

    statsPath=self.paths.getStatsPath()
    statsFilename=os.path.basename(statsPath)

    tmpStatsDir=statsPath+".tmpdir"

    makeTmpStatsDirCmd = getMkdirCmd() + [tmpStatsDir]
    dirTask=self.addTask(preJoin(taskPrefix,"makeTmpDir"), makeTmpStatsDirCmd, dependencies=dependencies, isForceLocal=True)

    tmpStatsFiles = []
    statsTasks = set()

    for (bamIndex,bamPath) in enumerate(self.params.normalBamList + self.params.tumorBamList) :
        indexStr = str(bamIndex).zfill(3)
        tmpStatsFiles.append(os.path.join(tmpStatsDir,statsFilename+"."+ indexStr +".xml"))

        cmd = [ self.params.mantaStatsBin ]
        cmd.extend(["--output-file",tmpStatsFiles[-1]])
        cmd.extend(["--align-file",bamPath])

        statsTasks.add(self.addTask(preJoin(taskPrefix,"generateStats_"+indexStr),cmd,dependencies=dirTask))

    cmd = [ self.params.mantaMergeStatsBin ]
    cmd.extend(["--output-file",statsPath])
    for tmpStatsFile in tmpStatsFiles :
        cmd.extend(["--align-stats-file",tmpStatsFile])

    mergeTask = self.addTask(preJoin(taskPrefix,"mergeStats"),cmd,dependencies=statsTasks,isForceLocal=True)

    nextStepWait = set()
    nextStepWait.add(mergeTask)

    rmStatsTmpCmd = getRmdirCmd() + [tmpStatsDir]
    rmTask=self.addTask(preJoin(taskPrefix,"rmTmpDir"),rmStatsTmpCmd,dependencies=mergeTask, isForceLocal=True)

    # summarize stats in format that's easier for human review
    cmd = [self.params.mantaStatsSummaryBin]
    cmd.extend(["--align-stats ", statsPath])
    cmd.extend(["--output-file", self.paths.getStatsSummaryPath()])
    self.addTask(preJoin(taskPrefix,"summarizeStats"),cmd,dependencies=mergeTask)

    return nextStepWait



def _runDepthShared(self,taskPrefix,dependencies, depthFunc) :
    """
    estimate chrom depth using the specified depthFunc to compute per-sample dpeth
    """

    bamList=[]
    if len(self.params.normalBamList) :
        bamList = self.params.normalBamList
    elif len(self.params.tumorBamList) :
        bamList = self.params.tumorBamList
    else :
        return set()

    outputPath=self.paths.getChromDepth()
    outputFilename=os.path.basename(outputPath)

    tmpDir=outputPath+".tmpdir"
    makeTmpDirCmd = getMkdirCmd() + [tmpDir]
    dirTask=self.addTask(preJoin(taskPrefix,"makeTmpDir"), makeTmpDirCmd, dependencies=dependencies, isForceLocal=True)

    tmpFiles = []
    scatterTasks = set()

    for (bamIndex, bamFile) in enumerate(bamList) :
        indexStr = str(bamIndex).zfill(3)
        tmpFiles.append(os.path.join(tmpDir,outputFilename+"."+ indexStr +".txt"))
        scatterTasks |= setzer(depthFunc(self,taskPrefix+"_sample"+indexStr,dirTask,bamFile,tmpFiles[-1]))

    cmd = [ self.params.mergeChromDepth ]
    cmd.extend(["--out",outputPath])
    for tmpFile in tmpFiles :
        cmd.extend(["--in",tmpFile])

    mergeTask = self.addTask(preJoin(taskPrefix,"mergeChromDepth"),cmd,dependencies=scatterTasks,isForceLocal=True)

    nextStepWait = set()
    nextStepWait.add(mergeTask)

    rmTmpCmd = getRmdirCmd() + [tmpDir]
    rmTask=self.addTask(preJoin(taskPrefix,"rmTmpDir"),rmTmpCmd,dependencies=mergeTask, isForceLocal=True)

    return nextStepWait


def runDepthFromAlignments(self,taskPrefix="",dependencies=None) :
    """
    estimate chrom depth directly from BAM/CRAM file
    """

    def depthFunc(self,taskPrefix,dependencies,bamFile,outFile) :
        outputPath=outFile
        outputFilename=os.path.basename(outputPath)

        tmpDir=os.path.join(outputPath+".tmpdir")
        makeTmpDirCmd = getMkdirCmd() + [tmpDir]
        dirTask=self.addTask(preJoin(taskPrefix,"makeTmpDir"), makeTmpDirCmd, dependencies=dependencies, isForceLocal=True)

        tmpFiles = []
        scatterTasks = set()

        def getChromosomeGroups(params) :
            """
            Iterate chromosomes/contigs and 'clump' small contigs together
            """
            minSize=200000
            group = []
            headSize = 0

            chromCount = len(params.chromSizes)
            assert(len(params.chromOrder) == chromCount)
            for chromIndex in range(chromCount) :
                chromLabel = params.chromOrder[chromIndex]
                chromSize = params.chromSizes[chromLabel]
                if headSize+chromSize <= minSize :
                    group.append((chromIndex,chromLabel))
                    headSize += chromSize
                else :
                    if len(group) != 0 : yield(group)
                    group = [(chromIndex,chromLabel)]
                    headSize = chromSize
            if len(group) != 0 : yield(group)

        for chromGroup in getChromosomeGroups(self.params) :
            assert(len(chromGroup) > 0)
            cid = getRobustChromId(chromGroup[0][0], chromGroup[0][1])
            if len(chromGroup) > 1 :
                cid += "_to_"+getRobustChromId(chromGroup[-1][0], chromGroup[-1][1])
            tmpFiles.append(os.path.join(tmpDir,outputFilename+"_"+cid))
            cmd = [self.params.mantaGetChromDepthBin,"--align-file",bamFile,"--output",tmpFiles[-1]]
            for (chromIndex,chromLabel) in chromGroup :
                cmd.extend(["--chrom",chromLabel])
            scatterTasks.add(self.addTask(preJoin(taskPrefix,"estimateChromDepth_"+cid),cmd,dependencies=dirTask))

        catCmd = [self.params.mantaCat,"--output",outputPath]+tmpFiles
        catTask = self.addTask(preJoin(taskPrefix,"catChromDepth"),catCmd,dependencies=scatterTasks, isForceLocal=True)

        nextStepWait = set()
        nextStepWait.add(catTask)

        return nextStepWait

    return _runDepthShared(self,taskPrefix,dependencies,depthFunc)



def runLocusGraph(self,taskPrefix="",dependencies=None):
    """
    Create the full SV locus graph
    """

    statsPath=self.paths.getStatsPath()
    graphPath=self.paths.getGraphPath()
    graphStatsPath=self.paths.getGraphStatsPath()

    graphFilename=os.path.basename(graphPath)
    tmpGraphDir=os.path.join(self.params.workDir,graphFilename+".tmpdir")

    makeTmpDirCmd = getMkdirCmd() + [tmpGraphDir]
    dirTask=self.addTask(preJoin(taskPrefix,"makeTmpDir"), makeTmpDirCmd, dependencies=dependencies, isForceLocal=True)

    tmpGraphFiles = []
    graphTasks = set()

    def getGenomeSegmentGroups(params) :
        """
        Iterate segment groups and 'clump' small contigs together
        """
        minSegmentGroupSize=200000
        group = []
        headSize = 0
        for gseg in getNextGenomeSegment(self.params) :
            if headSize+gseg.size() <= minSegmentGroupSize :
                group.append(gseg)
                headSize += gseg.size()
            else :
                if len(group) != 0 : yield(group)
                group = [gseg]
                headSize = gseg.size()
        if len(group) != 0 : yield(group)

    for gsegGroup in getGenomeSegmentGroups(self.params) :
        assert(len(gsegGroup) != 0)
        gid=gsegGroup[0].id
        if len(gsegGroup) > 1 :
            gid += "_to_"+gsegGroup[-1].id
        tmpGraphFiles.append(os.path.join(tmpGraphDir,graphFilename+"."+gid+".bin"))
        graphCmd = [ self.params.mantaGraphBin ]
        graphCmd.extend(["--output-file", tmpGraphFiles[-1]])
        graphCmd.extend(["--align-stats",statsPath])
        for gseg in gsegGroup :
            graphCmd.extend(["--region",gseg.bamRegion])
        graphCmd.extend(["--min-candidate-sv-size", self.params.minCandidateVariantSize])
        graphCmd.extend(["--min-edge-observations", self.params.minEdgeObservations])
        graphCmd.extend(["--ref",self.params.referenceFasta])
        for bamPath in self.params.normalBamList :
            graphCmd.extend(["--align-file",bamPath])
        for bamPath in self.params.tumorBamList :
            graphCmd.extend(["--tumor-align-file",bamPath])

        if self.params.isHighDepthFilter :
            graphCmd.extend(["--chrom-depth", self.paths.getChromDepth()])

        if self.params.isIgnoreAnomProperPair :
            graphCmd.append("--ignore-anom-proper-pair")
        if self.params.isRNA :
            graphCmd.append("--rna")

        graphTaskLabel=preJoin(taskPrefix,"makeLocusGraph_"+gid)
        graphTasks.add(self.addTask(graphTaskLabel,graphCmd,dependencies=dirTask,memMb=self.params.estimateMemMb))

    if len(tmpGraphFiles) == 0 :
        raise Exception("No SV Locus graphs to create. Possible target region parse error.")

    mergeCmd = [ self.params.mantaGraphMergeBin ]
    mergeCmd.extend(["--output-file", graphPath])
    for gfile in tmpGraphFiles :
        mergeCmd.extend(["--graph-file", gfile])

    mergeTask = self.addTask(preJoin(taskPrefix,"mergeLocusGraph"),mergeCmd,dependencies=graphTasks,memMb=self.params.mergeMemMb)

    # Run a separate process to rigorously check that the final graph is valid, the sv candidate generators will check as well, but
    # this makes the check much more clear:

    checkCmd = [ self.params.mantaGraphCheckBin ]
    checkCmd.extend(["--graph-file", graphPath])
    checkTask = self.addTask(preJoin(taskPrefix,"checkLocusGraph"),checkCmd,dependencies=mergeTask,memMb=self.params.mergeMemMb)

    rmGraphTmpCmd = getRmdirCmd() + [tmpGraphDir]
    rmTask=self.addTask(preJoin(taskPrefix,"rmTmpDir"),rmGraphTmpCmd,dependencies=mergeTask)

    graphStatsCmd  = [self.params.mantaGraphStatsBin,"--global"]
    graphStatsCmd.extend(["--graph-file",graphPath])
    graphStatsCmd.extend(["--output-file",graphStatsPath])

    graphStatsTask = self.addTask(preJoin(taskPrefix,"locusGraphStats"),graphStatsCmd,dependencies=mergeTask,memMb=self.params.mergeMemMb)

    nextStepWait = set()
    nextStepWait.add(checkTask)
    return nextStepWait



def runHyGen(self, taskPrefix="", dependencies=None) :
    """
    Run hypothesis generation on each SV locus
    """

    import copy

    statsPath=self.paths.getStatsPath()
    graphPath=self.paths.getGraphPath()
    hygenDir=self.paths.getHyGenDir()

    makeHyGenDirCmd = getMkdirCmd() + [hygenDir]
    dirTask = self.addTask(preJoin(taskPrefix,"makeHyGenDir"), makeHyGenDirCmd, dependencies=dependencies, isForceLocal=True)

    isSomatic = (len(self.params.normalBamList) and len(self.params.tumorBamList))
    isTumorOnly = ((not isSomatic) and len(self.params.tumorBamList))

    hyGenMemMb = self.params.hyGenLocalMemMb
    if self.getRunMode() == "sge" :
        hyGenMemMb = self.params.hyGenSGEMemMb

    hygenTasks=set()
    candidateVcfPaths = []
    diploidVcfPaths = []
    somaticVcfPaths = []
    tumorVcfPaths = []

    edgeRuntimeLogPaths = []
    edgeStatsLogPaths = []

    for binId in range(self.params.nonlocalWorkBins) :
        binStr = str(binId).zfill(4)
        candidateVcfPaths.append(self.paths.getHyGenCandidatePath(binStr))
        if isTumorOnly :
            tumorVcfPaths.append(self.paths.getHyGenTumorPath(binStr))
	else:
	    diploidVcfPaths.append(self.paths.getHyGenDiploidPath(binStr))
	    if isSomatic :
                somaticVcfPaths.append(self.paths.getHyGenSomaticPath(binStr))

        hygenCmd = [ self.params.mantaHyGenBin ]
        hygenCmd.extend(["--align-stats",statsPath])
        hygenCmd.extend(["--graph-file",graphPath])
        hygenCmd.extend(["--bin-index", str(binId)])
        hygenCmd.extend(["--bin-count", str(self.params.nonlocalWorkBins)])
        hygenCmd.extend(["--min-candidate-sv-size", self.params.minCandidateVariantSize])
        hygenCmd.extend(["--min-candidate-spanning-count", self.params.minCandidateSpanningCount])
        hygenCmd.extend(["--min-scored-sv-size", self.params.minScoredVariantSize])
        hygenCmd.extend(["--ref",self.params.referenceFasta])
        hygenCmd.extend(["--candidate-output-file", candidateVcfPaths[-1]])

	# tumor-only mode
        if isTumorOnly :
            hygenCmd.extend(["--tumor-output-file", tumorVcfPaths[-1]])
	else:
            hygenCmd.extend(["--diploid-output-file", diploidVcfPaths[-1]])
            hygenCmd.extend(["--min-qual-score", self.params.minDiploidVariantScore])
            hygenCmd.extend(["--min-pass-qual-score", self.params.minPassDiploidVariantScore])
            hygenCmd.extend(["--min-pass-gt-score", self.params.minPassDiploidGTScore])
	    # tumor/normal mode
	    if isSomatic :
       	        hygenCmd.extend(["--somatic-output-file", somaticVcfPaths[-1]])
                hygenCmd.extend(["--min-somatic-score", self.params.minSomaticScore])
                hygenCmd.extend(["--min-pass-somatic-score", self.params.minPassSomaticScore])

                # temporary fix for FFPE:
                hygenCmd.append("--skip-remote-reads")

        if self.params.isHighDepthFilter :
            hygenCmd.extend(["--chrom-depth", self.paths.getChromDepth()])

        edgeRuntimeLogPaths.append(self.paths.getHyGenEdgeRuntimeLogPath(binStr))
        hygenCmd.extend(["--edge-runtime-log", edgeRuntimeLogPaths[-1]])

        edgeStatsLogPaths.append(self.paths.getHyGenEdgeStatsPath(binStr))
        hygenCmd.extend(["--edge-stats-log", edgeStatsLogPaths[-1]])

        for bamPath in self.params.normalBamList :
            hygenCmd.extend(["--align-file", bamPath])
        for bamPath in self.params.tumorBamList :
            hygenCmd.extend(["--tumor-align-file", bamPath])

        if self.params.isIgnoreAnomProperPair :
            hygenCmd.append("--ignore-anom-proper-pair")
        if self.params.isRNA :
            hygenCmd.append("--rna")
        if self.params.isUnstrandedRNA :
            hygenCmd.append("--unstranded")

        hygenTaskLabel=preJoin(taskPrefix,"generateCandidateSV_"+binStr)
        hygenTasks.add(self.addTask(hygenTaskLabel,hygenCmd,dependencies=dirTask, memMb=hyGenMemMb))

    nextStepWait = copy.deepcopy(hygenTasks)

    def getVcfSortCmd(vcfPaths, outPath, isDiploid) :
        cmd  = "\"%s\" -E \"%s\" -u " % (sys.executable,self.params.mantaSortVcf)
        cmd += " ".join(quoteStringList(vcfPaths))

        # apply the ploidy filter to diploid variants
        if isDiploid:
            tempVcf = self.paths.getTempDiploidPath()
            cmd += " > \"%s\"" % (tempVcf)
            cmd += " && \"%s\" -E \"%s\" \"%s\"" % (sys.executable, self.params.mantaPloidyFilter, tempVcf)

        cmd += " | \"%s\" -c > \"%s\"" % (self.params.bgzipBin, outPath)

        if isDiploid:
            cmd += " && " + " ".join(getRmCmd()) + " \"%s\"" % (self.paths.getTempDiploidPath())
        return cmd

    def getVcfTabixCmd(vcfPath) :
        return [self.params.tabixBin,"-f","-p","vcf", vcfPath]


    def sortVcfs(pathList, outPath, label, isDiploid=False) :
        if len(pathList) == 0 : return set()
        sortCmd = getVcfSortCmd(pathList, outPath, isDiploid)
        sortTask=self.addTask(preJoin(taskPrefix,"sort_"+label),sortCmd,dependencies=hygenTasks)
        nextStepWait.add(self.addTask(preJoin(taskPrefix,"tabix_"+label),getVcfTabixCmd(outPath),dependencies=sortTask,isForceLocal=True))
        return sortTask

    candSortTask = sortVcfs(candidateVcfPaths,
                            self.paths.getSortedCandidatePath(),
                            "sortCandidateSV")
    sortVcfs(diploidVcfPaths,
             self.paths.getSortedDiploidPath(),
             "sortDiploidSV",
             isDiploid=True)
    sortVcfs(somaticVcfPaths,
             self.paths.getSortedSomaticPath(),
             "sortSomaticSV")
    sortVcfs(tumorVcfPaths,
             self.paths.getSortedTumorPath(),
             "sortTumorSV")

    def getExtractSmallCmd(maxSize, inPath, outPath) :
        cmd  = "\"%s\" -dc \"%s\"" % (self.params.bgzipBin, inPath)
        cmd += " | \"%s\" -E \"%s\" --maxSize %i" % (sys.executable, self.params.mantaExtraSmallVcf, maxSize)
        cmd += " | \"%s\" -c > \"%s\"" % (self.params.bgzipBin, outPath)
        return cmd

    def extractSmall(inPath, outPath) :
        maxSize = int(self.params.minScoredVariantSize) - 1
        if maxSize < 1 : return
        smallCmd = getExtractSmallCmd(maxSize, inPath, outPath)
        smallLabel=self.addTask(preJoin(taskPrefix,"extractSmallIndels"), smallCmd, dependencies=candSortTask, isForceLocal=True)
        nextStepWait.add(self.addTask(smallLabel+"_tabix", getVcfTabixCmd(outPath), dependencies=smallLabel, isForceLocal=True))

    extractSmall(self.paths.getSortedCandidatePath(), self.paths.getSortedCandidateSmallIndelsPath())

    # sort edge logs:
    def getEdgeLogSortCmd(logPaths, outPath) :
        cmd  = [sys.executable,"-E",self.params.mantaSortEdgeLogs,"-o",outPath]
        cmd.extend(logPaths)
        return cmd

    edgeSortLabel=preJoin(taskPrefix,"sortEdgeRuntimeLogs")
    edgeSortCmd=getEdgeLogSortCmd(edgeRuntimeLogPaths,self.paths.getSortedEdgeRuntimeLogPath())
    self.addTask(edgeSortLabel, edgeSortCmd, dependencies=hygenTasks, isForceLocal=True)

    # merge edge stats:
    edgeStatsMergeLabel=preJoin(taskPrefix,"mergeEdgeStats")
    edgeStatsMergeCmd=[self.params.mantaStatsMergeBin]
    for statsFile in edgeStatsLogPaths :
        edgeStatsMergeCmd.extend(["--stats-file",statsFile])
    edgeStatsMergeCmd.extend(["--output-file",self.paths.getFinalEdgeStatsPath()])
    edgeStatsMergeCmd.extend(["--report-file",self.paths.getFinalEdgeStatsReportPath()])
    self.addTask(edgeStatsMergeLabel, edgeStatsMergeCmd, dependencies=hygenTasks, isForceLocal=True)

    return nextStepWait



class PathInfo:
    """
    object to centralize shared workflow path names
    """

    def __init__(self, params) :
        self.params = params

    def getStatsPath(self) :
        return os.path.join(self.params.workDir,"alignmentStats.xml")

    def getStatsSummaryPath(self) :
        return os.path.join(self.params.statsDir,"alignmentStatsSummary.txt")

    def getChromDepth(self) :
        return os.path.join(self.params.workDir,"chromDepth.txt")

    def getGraphPath(self) :
        return os.path.join(self.params.workDir,"svLocusGraph.bin")

    def getHyGenDir(self) :
        return os.path.join(self.params.workDir,"svHyGen")

    def getHyGenCandidatePath(self, binStr) :
        return os.path.join(self.getHyGenDir(),"candidateSV.%s.vcf" % (binStr))

    def getSortedCandidatePath(self) :
        return os.path.join(self.params.variantsDir,"candidateSV.vcf.gz")

    def getSortedCandidateSmallIndelsPath(self) :
        return os.path.join(self.params.variantsDir,"candidateSmallIndels.vcf.gz")

    def getHyGenDiploidPath(self, binStr) :
        return os.path.join(self.getHyGenDir(),"diploidSV.%s.vcf" % (binStr))

    def getTempDiploidPath(self) :
        return os.path.join(self.getHyGenDir(),"diploidSV.vcf.temp")

    def getSortedDiploidPath(self) :
        return os.path.join(self.params.variantsDir,"diploidSV.vcf.gz")

    def getHyGenSomaticPath(self, binStr) :
        return os.path.join(self.getHyGenDir(),"somaticSV.%s.vcf" % (binStr))

    def getHyGenTumorPath(self, binStr) :
        return os.path.join(self.getHyGenDir(),"tumorSV.%s.vcf" % (binStr))

    def getSortedSomaticPath(self) :
        return os.path.join(self.params.variantsDir,"somaticSV.vcf.gz")

    def getSortedTumorPath(self) :
        return os.path.join(self.params.variantsDir,"tumorSV.vcf.gz")

    def getHyGenEdgeRuntimeLogPath(self, binStr) :
        return os.path.join(self.getHyGenDir(),"edgeRuntimeLog.%s.txt" % (binStr))

    def getHyGenEdgeStatsPath(self, binStr) :
        return os.path.join(self.getHyGenDir(),"edgeStats.%s.xml" % (binStr))

    def getSortedEdgeRuntimeLogPath(self) :
        return os.path.join(self.params.workDir,"edgeRuntimeLog.txt")

    def getFinalEdgeStatsPath(self) :
        return os.path.join(self.params.statsDir,"svCandidateGenerationStats.xml")

    def getFinalEdgeStatsReportPath(self) :
        return os.path.join(self.params.statsDir,"svCandidateGenerationStats.tsv")

    def getGraphStatsPath(self) :
        return os.path.join(self.params.statsDir,"svLocusGraphStats.tsv")



class MantaWorkflow(WorkflowRunner) :
    """
    Manta SV discovery workflow
    """

    def __init__(self,params,iniSections) :

        cleanPyEnv()

        self.params=params
        self.iniSections=iniSections

        # format bam lists:
        if self.params.normalBamList is None : self.params.normalBamList = []
        if self.params.tumorBamList is None : self.params.tumorBamList = []

        # make sure run directory is setup:
        self.params.runDir=os.path.abspath(self.params.runDir)
        ensureDir(self.params.runDir)

        # everything that's not intended to be a final result should dump directories/files in workDir
        self.params.workDir=os.path.join(self.params.runDir,"workspace")
        ensureDir(self.params.workDir)

        # all finalized pretty results get transfered to resultsDir
        self.params.resultsDir=os.path.join(self.params.runDir,"results")
        ensureDir(self.params.resultsDir)
        self.params.statsDir=os.path.join(self.params.resultsDir,"stats")
        ensureDir(self.params.statsDir)
        self.params.variantsDir=os.path.join(self.params.resultsDir,"variants")
        ensureDir(self.params.variantsDir)
#         self.params.reportsDir=os.path.join(self.params.resultsDir,"reports")
#         ensureDir(self.params.reportsDir)

        indexRefFasta=self.params.referenceFasta+".fai"

        if self.params.referenceFasta is None:
            raise Exception("No reference fasta defined.")
        else:
            checkFile(self.params.referenceFasta,"reference fasta")
            checkFile(indexRefFasta,"reference fasta index")

        # read fasta index
        (self.params.chromOrder,self.params.chromSizes) = getFastaChromOrderSize(indexRefFasta)

        self.paths = PathInfo(self.params)

        self.params.isHighDepthFilter = (not (self.params.isExome or self.params.isRNA))
        self.params.isIgnoreAnomProperPair = (self.params.isRNA)



    def getSuccessMessage(self) :
        "Message to be included in email for successful runs"

        msg  = "Manta workflow successfully completed.\n\n"
        msg += "\tworkflow version: %s\n" % (__version__)
        return msg



    def workflow(self) :
        self.flowLog("Initiating Manta workflow version: %s" % (__version__))

        graphTaskDependencies = set()

        if not self.params.useExistingAlignStats :
            statsTasks = runStats(self,taskPrefix="getAlignmentStats")
            graphTaskDependencies |= statsTasks

        if not ((not self.params.isHighDepthFilter) or self.params.useExistingChromDepths) :
            depthTasks = runDepthFromAlignments(self,taskPrefix="getChromDepth")
            graphTaskDependencies |= depthTasks

        graphTasks = runLocusGraph(self,dependencies=graphTaskDependencies)

        hygenTasks = runHyGen(self,dependencies=graphTasks)
