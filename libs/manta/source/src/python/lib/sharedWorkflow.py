#
# Manta - Structural Variant and Indel Caller
# Copyright (c) 2013-2016 Illumina, Inc.
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
Workflow components shared between SV and smal variant packages
"""


import os.path
import sys

# add script path to pull in utils in same directory:
scriptDir=os.path.abspath(os.path.dirname(__file__))
sys.path.append(os.path.abspath(scriptDir))

from workflowUtil import isWindows, preJoin, getRobustChromId



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

def getMvCmd() :
    if isWindows():
        return ["move","/y"]
    else:
        return ["mv"]

def quoteStringList(strList):
    return ["\"%s\"" % (x) for x in strList]



def _runDepthShared(self,taskPrefix, dependencies, bamList, outputPath, depthFunc) :
    """
    estimate chrom depth using the specified depthFunc to compute per-sample depth
    """

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

    if not self.params.isRetainTempFiles :
        rmTmpCmd = getRmdirCmd() + [tmpDir]
        rmTask=self.addTask(preJoin(taskPrefix,"rmTmpDir"),rmTmpCmd,dependencies=mergeTask, isForceLocal=True)

    return nextStepWait


def runDepthFromAlignments(self, bamList, outputPath, taskPrefix="",dependencies=None) :
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
            cmd = [self.params.getChromDepthBin,"--align-file",bamFile,"--output",tmpFiles[-1]]
            for (chromIndex,chromLabel) in chromGroup :
                cmd.extend(["--chrom",chromLabel])
            scatterTasks.add(self.addTask(preJoin(taskPrefix,"estimateChromDepth_"+cid),cmd,dependencies=dirTask))

        catCmd = [self.params.catScript,"--output",outputPath]+tmpFiles
        catTask = self.addTask(preJoin(taskPrefix,"catChromDepth"),catCmd,dependencies=scatterTasks, isForceLocal=True)

        nextStepWait = set()
        nextStepWait.add(catTask)

        return nextStepWait

    return _runDepthShared(self, taskPrefix, dependencies, bamList, outputPath, depthFunc)

