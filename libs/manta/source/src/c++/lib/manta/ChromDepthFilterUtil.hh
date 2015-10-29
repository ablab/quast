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

#pragma once

#include "blt_util/chrom_depth_map.hh"
#include "htsapi/bam_header_info.hh"

#include <cassert>

#include <vector>


/// hold information about chrom depth cutoffs
///
/// preprocess the chrom depth file so that the filter value can be
/// efficiently looked up by bam tid
///
struct ChromDepthFilterUtil
{
    ChromDepthFilterUtil(
        const std::string& chromDepthFile,
        const double maxDepthFactor,
        const bam_header_info& header);

    bool
    isMaxDepthFilter() const
    {
        return _isMaxDepthFilter;
    }

    double
    maxDepth(const int32_t tid) const
    {
        assert((tid >= 0) && (tid < static_cast<int32_t>(_maxDepthFilter.size())));
        return _maxDepthFilter[tid];
    }

private:
    bool _isMaxDepthFilter;
    std::vector<double> _maxDepthFilter;
};
