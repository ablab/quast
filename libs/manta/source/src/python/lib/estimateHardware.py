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
get cpu and memory capability info from linux and os x hosts
"""


import os



class EstException(Exception):
    pass



def getNodeRealCoreCount() :
    """
    find the number of physical cpu cores

    linux logic adapted from B Sickler

    only works on linux and os x
    """

    import platform
    if platform.system().find("Linux") > -1:

        cpuinfo = '/proc/cpuinfo'
        if not os.path.isfile(cpuinfo):
            raise EstException("Can't read processor information from '%s'" % (cpuinfo))

        physical_core_ids = set()
        cpu_cores = set()

        for line in open(cpuinfo):
            l_array = [item.strip() for item in line.strip().split(': ')]
            if len(l_array) < 2: continue
            cpuid = l_array[0]
            if cpuid == 'cpu cores':
                cpu_cores.add(int(l_array[1]))
            if cpuid == 'physical id':
                physical_core_ids.add(l_array[1])

        if len(physical_core_ids) == 0 :
            raise EstException("No 'physical id' key found in '%s'" % (cpuinfo))
        if len(cpu_cores) == 0 :
            raise EstException("No 'cpu cores' key found in '%s'" % (cpuinfo))

        return ( len(physical_core_ids) * cpu_cores.pop() )

    elif platform.system().find("Darwin") > -1:
        import subprocess
        cmd=['sysctl', '-n', 'hw.physicalcpu']
        proc=subprocess.Popen(cmd,shell=False,stdout=subprocess.PIPE)
        for line in proc.stdout :
            cpuCount=int(line.strip())
            break

        return cpuCount
    else:
        raise EstException("Can't determine total physical cores available on OS: '%s'" (platform.system()))




def getNodeHyperthreadCoreCount():
    """
    return the number of hyperthread (or 'logical') cores on this host

    linux logic taken from R Kelley's function in IsisWorkflow

    only works on linux and os x
    """

    cpuCount = 0

    import platform

    if platform.system().find("Linux") > -1:
        cname="/proc/cpuinfo"
        if not os.path.isfile(cname):
            raise EstException("Can't read processor information from %s" % (cname))

        for line in open(cname):
            if line.startswith("processor"): cpuCount += 1

        if cpuCount == 0: raise EstException("Can't estimate processor core count from %s" % (cname))

    elif platform.system().find("Darwin") > -1:
        import subprocess
        cmd=['sysctl', '-n', 'hw.logicalcpu']
        proc=subprocess.Popen(cmd,shell=False,stdout=subprocess.PIPE)
        for line in proc.stdout :
            cpuCount=int(line.strip())
            break
    else:
        raise EstException("Can't determine total logical cores available on OS: '%s'" (platform.system()))

    return cpuCount



def getNodeMemMb():
    """
    return total memory in Mbytes

    linux logic taken from R Kelley's function in IsisWorkflow

    only works on linux and os x
    """

    memMb = 0

    import platform

    if platform.system().find("Linux") > -1:
        #
        # get this from /proc/meminfo
        #
        mname="/proc/meminfo"
        if not os.path.isfile(mname):
            raise EstException("Can't read memory information from %s" % (mname))

        line = open(mname).readline()
        splat = line.rstrip().split()
        if len(splat) != 3:
            raise EstException("Unexpected format in %s" % (mname))

        try:
            memMb = 1+(int(splat[1])-1)/1024
        except:
            raise EstException("Unexpected format in %s" % (mname))
    elif platform.system().find("Darwin") > -1:
        import subprocess
        cmd=['sysctl', '-n', 'hw.memsize']
        proc=subprocess.Popen(cmd,shell=False,stdout=subprocess.PIPE)
        for line in proc.stdout :
            memMb=int(line.strip())/(1024*1024)
            break
    else:
        raise EstException("Can't determine total memory available on OS: '%s'" (platform.system()))

    return memMb


