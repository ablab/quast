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

#include "GetChromDepth.hh"
#include "ChromDepthOptions.hh"

#include "blt_util/log.hh"
#include "common/OutStream.hh"
#include "manta/ReadChromDepthUtil.hh"

#include <cstdlib>

#include <iomanip>
#include <iostream>



static
void
getChromDepth(const ChromDepthOptions& opt)
{
    // check that we have write permission on the output file early:
    {
        OutStream outs(opt.outputFilename);
    }

    std::vector<double> chromDepth;
    for (const std::string& chrom : opt.chromNames)
    {
        chromDepth.push_back(readChromDepthFromAlignment(opt.alignmentFilename, chrom));
    }

    OutStream outs(opt.outputFilename);
    std::ostream& os(outs.getStream());

    const unsigned chromCount(opt.chromNames.size());
    for (unsigned chromIndex(0); chromIndex<chromCount; ++chromIndex)
    {
        os << opt.chromNames[chromIndex] << "\t" << std::fixed << std::setprecision(2) << chromDepth[chromIndex] << "\n";
    }
}


void
GetChromDepth::
runInternal(int argc, char* argv[]) const
{
    ChromDepthOptions opt;

    parseChromDepthOptions(*this,argc,argv,opt);
    getChromDepth(opt);
}
