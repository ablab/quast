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

#include "options/SVLocusSetOptionsParser.hh"


boost::program_options::options_description
getOptionsDescription(SVLocusSetOptions& opt)
{
    namespace po = boost::program_options;
    po::options_description desc("sv-locus-graph");
    desc.add_options()
    ("min-edge-observations", po::value(&opt.minMergeEdgeObservations)->default_value(opt.minMergeEdgeObservations),
     "Minimum number of supporting observations required to retain a graph edge")
    ;

    return desc;
}



bool
parseOptions(
    const boost::program_options::variables_map& /*vm*/,
    SVLocusSetOptions& /*opt*/,
    std::string& errorMsg)
{
    errorMsg.clear();
#if 0
    if ((opt.breakendEdgeTrimProb <= 0) || (opt.breakendEdgeTrimProb >= 1.0))
    {
        errorMsg="edge-prob argument is restricted to (0,1)";
    }
#endif
    return (! errorMsg.empty());

}
