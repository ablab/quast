// -*- mode: c++; indent-tabs-mode: nil; -*-
//
// Manta - Structural Variant and Indel Caller
// Copyright (c) 2013-2015 Illumina, Inc.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//

///
/// \author Chris Saunders
///

#include "MergeSVLoci.hh"
#include "MSLOptions.hh"

#include "blt_util/log.hh"
#include "common/OutStream.hh"
#include "svgraph/SVLocusSet.hh"



static
void
runMSL(const MSLOptions& opt)
{
    TimeTracker timer;
    timer.resume();
    {
        // early test that we have permission to write to output file
        OutStream outs(opt.outputFilename);
    }

    SVLocusSet mergedSet;

    for (const std::string& graphFile : opt.graphFilename)
    {
        if (opt.isVerbose)
        {
            log_os << "INFO: Merging file: '" << graphFile << "'\n";
        }

        if (mergedSet.empty())
        {
            mergedSet.load(graphFile.c_str());
        }
        else
        {
            SVLocusSet inputSet;
            inputSet.load(graphFile.c_str());
            mergedSet.merge(inputSet);
        }

        if (opt.isVerbose)
        {
            log_os << "INFO: Finished merging file: '" << graphFile << "'\n";
        }
    }

    mergedSet.finalize();
    if (opt.isVerbose)
    {
        log_os << "INFO: Finished cleaning merged graph.\n";
    }
    timer.stop();
    mergedSet.setMergeTime(timer.getTimes());
    mergedSet.save(opt.outputFilename.c_str());
}



void
MergeSVLoci::
runInternal(int argc, char* argv[]) const
{
    MSLOptions opt;

    parseMSLOptions(*this,argc,argv,opt);
    runMSL(opt);
}
