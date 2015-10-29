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

#include "common/Exceptions.hh"
#include "manta/ChromDepthFilterUtil.hh"

#include <sstream>



ChromDepthFilterUtil::
ChromDepthFilterUtil(
    const std::string& chromDepthFile,
    const double maxDepthFactor,
    const bam_header_info& header) :
    _isMaxDepthFilter(! chromDepthFile.empty())
{
    using namespace illumina::common;

    // read in chrom depth file if one is specified:
    if (! _isMaxDepthFilter) return;

    cdmap_t chromDepth;
    parse_chrom_depth(chromDepthFile,chromDepth);

    // translate string chrom labels into tid values in lookup vector:
    //
    for (const bam_header_info::chrom_info& cdata : header.chrom_data)
    {
        cdmap_t::const_iterator cdi(chromDepth.find(cdata.label));
        if (cdi == chromDepth.end())
        {
            std::ostringstream oss;
            oss << "ERROR: Can't find chromosome: '" << cdata.label
                << "' in chrom depth file: " << chromDepthFile << "\n";
            BOOST_THROW_EXCEPTION(LogicException(oss.str()));
        }

        _maxDepthFilter.push_back(cdi->second*maxDepthFactor);
        assert(_maxDepthFilter.back()>=0.);
    }
}

