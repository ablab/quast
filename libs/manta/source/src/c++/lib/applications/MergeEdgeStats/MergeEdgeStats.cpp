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

#include "MergeEdgeStats.hh"
#include "MESOptions.hh"
#include "appstats/GSCEdgeStats.hh"
#include "common/OutStream.hh"

#include "blt_util/log.hh"



static
void
runMES(const MESOptions& opt)
{
    {
        // early test that we have permission to write to output file(s)
        OutStream outs(opt.outputFilename);
        if (! opt.reportFilename.empty())
        {
            OutStream reps(opt.reportFilename);
        }
    }

    GSCEdgeStats mergedStats;
    bool isFirst(true);
    for (const std::string& statsFilename : opt.statsFilename)
    {
        if (isFirst)
        {
            mergedStats.load(statsFilename.c_str());
            isFirst=false;
        }
        else
        {
            GSCEdgeStats inputStats;
            inputStats.load(statsFilename.c_str());
            mergedStats.merge(inputStats);
        }

    }

    mergedStats.save(opt.outputFilename.c_str());
    if (! opt.reportFilename.empty())
    {
        mergedStats.report(opt.reportFilename.c_str());
    }
}



void
MergeEdgeStats::
runInternal(int argc, char* argv[]) const
{
    MESOptions opt;

    parseMESOptions(*this,argc,argv,opt);
    runMES(opt);
}
