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
///
///

#pragma once

#include "blt_util/align_path.hh"
#include "blt_util/known_pos_range2.hh"

#include <iosfwd>


/// holds minimal alignment information processed
/// from a BAM record or other source
struct SimpleAlignment
{
    bool is_fwd_strand = true;
    int32_t tid = 0;
    pos_t pos = 0;
    ALIGNPATH::path_t path;
};


std::ostream&
operator<<(
    std::ostream& os,
    const SimpleAlignment& sa);


/// convert segment_type to match if the segment exists before or after all match segments currently in the alignment
///
SimpleAlignment
matchifyEdgeSegmentType(
    const SimpleAlignment& al,
    const ALIGNPATH::align_t segmentType,
    const bool isMatchLeadingEdge = true,
    const bool isMatchTrailingEdge = true);


/// transform an alignment such that any soft-clipped edge segments
/// become match.
///
/// segments are joined and start pos is adjusted appropriately
///
inline
SimpleAlignment
matchifyEdgeSoftClip(const SimpleAlignment& al)
{
    return matchifyEdgeSegmentType(al, ALIGNPATH::SOFT_CLIP);
}


/// get the range in reference coordinates if you did run matchifyEdgeSoftClip on an alignment:
known_pos_range2
matchifyEdgeSoftClipRefRange(const SimpleAlignment& al);
