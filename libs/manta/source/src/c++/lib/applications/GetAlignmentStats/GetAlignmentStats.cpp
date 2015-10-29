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

#include "GetAlignmentStats.hh"

#include "AlignmentStatsOptions.hh"

#include "blt_util/log.hh"
#include "common/OutStream.hh"
#include "manta/ReadGroupStatsUtil.hh"

#include <cstdlib>

#include <iostream>



static
void
runAlignmentStats(const AlignmentStatsOptions& opt)
{
    // calculate fragment size statistics for all read groups in all bams

    // instantiate early to test for filename/permissions problems
    if (opt.alignFileOpt.alignmentFilename.empty())
    {
        log_os << "ERROR: No input files specified.\n";
        exit(EXIT_FAILURE);
    }

    ReadGroupStatsSet rstats;
    for (const std::string& file : opt.alignFileOpt.alignmentFilename)
    {
        extractReadGroupStatsFromBam(file,rstats);
    }

    rstats.save(opt.outputFilename.c_str());
}


void
GetAlignmentStats::
runInternal(int argc, char* argv[]) const
{
    AlignmentStatsOptions opt;

    parseAlignmentStatsOptions(*this,argc,argv,opt);
    runAlignmentStats(opt);
}
