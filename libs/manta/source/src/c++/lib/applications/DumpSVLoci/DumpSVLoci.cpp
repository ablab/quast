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

#include "DumpSVLoci.hh"
#include "DSLOptions.hh"

#include "htsapi/bam_header_util.hh"
#include "svgraph/SVLocusSet.hh"

#include "blt_util/thirdparty_push.h"

#include "boost/archive/binary_oarchive.hpp"

#include "blt_util/thirdparty_pop.h"

#include <fstream>
#include <iostream>



static
void
runDSL(const DSLOptions& opt)
{
    SVLocusSet set;
    set.load(opt.graphFilename.c_str());

    const SVLocusSet& cset(set);

    std::ostream& os(std::cout);

    // add this handy map of chromosome id to chromosome label at the start of all output types:
    os << cset.header << "\n";

    if (! opt.region.empty())
    {
        int32_t tid,beginPos,endPos;
        parse_bam_region(set.header, opt.region.c_str(), tid, beginPos, endPos);

        set.dumpRegion(os,GenomeInterval(tid,beginPos,endPos));
    }
    else if (opt.isLocusIndex)
    {
        const SVLocus& locus(cset.getLocus(opt.locusIndex));
        if (opt.locusFilename.empty())
        {
            os << locus;
        }
        else
        {
            std::ofstream ofs(opt.locusFilename.c_str(), std::ios::binary);
            boost::archive::binary_oarchive oa(ofs);
            oa << locus;
        }
    }
    else
    {
        cset.dump(os);
    }
}



void
DumpSVLoci::
runInternal(int argc, char* argv[]) const
{

    DSLOptions opt;

    parseDSLOptions(*this,argc,argv,opt);
    runDSL(opt);
}
