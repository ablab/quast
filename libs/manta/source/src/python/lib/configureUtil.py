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
util -- simple utilities shared by workflow configurations
"""


import os.path

from optparse import OptionParser
from checkChromSet import checkChromSet



class OptParseException(Exception):
    pass



def argToBool(x) :
    """
    convert argument of unknown type to a bool:
    """
    class FalseStrings :
        val = ("", "0", "false", "f", "no", "n", "off")

    if isinstance(x, basestring) :
        return (x.lower() not in FalseStrings.val)
    return bool(x)



def safeSetBool(obj,dataname) :
    """
    translate ojb.dataname to a bool, or set to false if it doesn't exist
    """
    if hasattr(obj, dataname) :
        setattr(obj, dataname, argToBool(getattr(obj, dataname)))
    else :
        setattr(obj, dataname, False)



def pickleConfigSections(pickleConfigFile, configSections) :
    """
    write configSections object, expected to be a hash or hashes, into a pickle file
    """
    import pickle

    pickle.dump(configSections, open(pickleConfigFile, "w"))



def getConfigSections(pickleConfigFile) :
    """
    deserialize the config file and return a hash of hashes
    """

    import pickle

    if not os.path.isfile(pickleConfigFile) : return {}

    return pickle.load(open(pickleConfigFile))



def getPrimarySectionOptions(configSections,primarySection) :

    class WorkflowOptions(object) :
        pass

    options=WorkflowOptions()
    if primarySection not in configSections : return options
    for (k,v) in configSections[primarySection].items() :
        setattr(options,k,v)

    return options



def getConfigWithPrimaryOptions(pickleConfigFile,primarySection) :
    """
    Deserialize the config pickle file and return (1) a class representing the
    options of a section specified as primary (2) a hash of hashes representing
    all sections
    """
    configSections=getConfigSections(pickleConfigFile)
    options=getPrimarySectionOptions(configSections,primarySection)

    return (options,configSections)



def dumpIniSections(iniFile,iniSections) :
    """
    convert iniSections object, expected to be a hash or hashes, into an iniFile
    """
    from ConfigParser import SafeConfigParser

    config = SafeConfigParser()
    config.optionxform=str

    def clean_value(v) :
        if v is None : return ""
        else :         return str(v)

    for section in iniSections.keys():
        config.add_section(section)
        for k,v in iniSections[section].items() :
            config.set(section,k,clean_value(v))

    configfp=open(iniFile,"w")
    config.write(configfp)
    configfp.close()



def getIniSections(iniFile) :
    """
    parse the ini iniFile and return a hash of hashes
    """
    from ConfigParser import SafeConfigParser

    if not os.path.isfile(iniFile) : return {}

    config = SafeConfigParser()
    config.optionxform=str
    config.read(iniFile)

    iniSections = {}
    for section in config.sections() :
        iniSections[section] = {}
        for (k,v) in config.items(section) :
            if v == "" : v = None
            iniSections[section][k] = v

    return iniSections



class EpilogOptionParser(OptionParser) :
    """
    This extension to OptionParser fakes the epilog feature introduced
    in versions of OptionParser after python 2.4
    """
    def __init__(self, *args, **kwargs):
        self.myepilog = None
        try:
            self.myepilog = kwargs.pop('epilog')
        except KeyError:
            pass
        OptionParser.__init__(self,*args, **kwargs)

    def print_help(self,*args,**kwargs) :
        import sys,textwrap
        OptionParser.print_help(self,*args, **kwargs)
        if self.myepilog is not None :
            sys.stdout.write("\n%s\n\n" % (textwrap.fill(self.myepilog)))



def _validateFixArgHelper(x,label,checkfunc) :
    if x is not None:
        x=os.path.abspath(x)
        if not checkfunc(x) :
            raise OptParseException("Can't find %s: '%s'" % (label,x))
    return x

def validateFixExistingDirArg(argDir,label) :
    """
    convert directory arg to absolute path and check that it exists
    """
    return _validateFixArgHelper(argDir,label,os.path.isdir)

def validateFixExistingFileArg(argFile,label) :
    """
    convert file arg to absolute path and check that it exists
    """
    return _validateFixArgHelper(argFile,label,os.path.isfile)


def checkOptionalTabixIndexedFile(iname,desc) :
    if iname is None : return
    tabixFile = iname + ".tbi"
    if os.path.isfile(tabixFile) : return
    raise OptParseException("Can't find expected %s index file: '%s'" % (desc,tabixFile))


def checkForBamIndex(bamFile):
    """
    make sure bam file has an index
    """
    for ext in (".bai", ".csi", ".crai") :
        indexFile=bamFile + ext
        if os.path.isfile(indexFile) : return
    raise OptParseException("Can't find any expected BAM/CRAM index files for: '%s'" % (bamFile))


def groomBamList(bamList, sampleLabel):
    """
    check that bam/cram files exist and have an index, convert to abs path if they check out
    """
    if bamList is None : return
    for (index,bamFile) in enumerate(bamList) :
        bamList[index]=validateFixExistingFileArg(bamFile,"%s BAM/CRAM file" % (sampleLabel))
        checkForBamIndex(bamList[index])


class BamSetChecker(object):
    """
    check properties of the input bams as an aggregate set

    for instance, same chrom order, no repeated files, etc...
    """

    def  __init__(self) :
        self.bamList=[]
        self.bamLabels=[]

    def appendBams(self,inputBamList,inputLabel) :

        if inputBamList is None : return
        for inputBamFile in inputBamList :
            self.bamList.append(inputBamFile)
            self.bamLabels.append(inputLabel)

    def check(self, htsfileBin, referenceFasta) :

        checkChromSet(htsfileBin,
                      referenceFasta,
                      self.bamList,
                      self.bamLabels,
                      isReferenceLocked=True)

        # check for repeated bam entries:
        #
        bamSet=set()
        for bamFile in self.bamList :
            if bamFile in bamSet :
                raise OptParseException("Repeated input BAM/CRAM file: %s" % (bamFile))
            bamSet.add(bamFile)


def checkListArgRepeats(listName,itemLabel) :
    """
    screen a list argument for repeated entries
    """
    if listName is None : return
    if len(set(listName)) != len(listName) :
        raise OptParseException("Repeated %s entries" % (itemLabel))



def assertOptionExists(arg,label) :
    if arg is None:
        raise OptParseException("No %s specified" % (label))



def joinFile(*arg) :
    filePath = os.path.join(*arg)
    assert os.path.isfile(filePath)
    return filePath
