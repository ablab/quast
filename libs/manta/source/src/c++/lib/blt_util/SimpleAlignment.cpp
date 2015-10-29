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

#include "SimpleAlignment.hh"

#include <cassert>

#include <iostream>



std::ostream&
operator<<(
    std::ostream& os,
    const SimpleAlignment& sa)
{
    os << "alignment: " << sa.tid << ':' << sa.pos << (sa.is_fwd_strand ? '+' : '-') << ' ' << sa.path;
    return os;
}



/// convert segment_type to match if the segment exists before or after all match segments currently in the alignment
///
SimpleAlignment
matchifyEdgeSegmentType(
    const SimpleAlignment& al,
    const ALIGNPATH::align_t segmentType,
    const bool isMatchLeadingEdge,
    const bool isMatchTrailingEdge)
{
    using namespace ALIGNPATH;

    assert(is_segment_type_read_length(segmentType));

    SimpleAlignment al2;
    al2.is_fwd_strand=al.is_fwd_strand;
    al2.tid=al.tid;
    al2.pos=al.pos;

    const std::pair<unsigned,unsigned> ends(get_match_edge_segments(al.path));
    const unsigned as(al.path.size());
    for (unsigned i(0); i<as; ++i)
    {
        const path_segment& ps(al.path[i]);
        const bool isLeadingEdgeSegment(i<ends.first);
        const bool isTrailingEdgeSegment(i>ends.second);
        const bool isTargetType(ps.type==segmentType);
        const bool isCandidateEdge((isMatchLeadingEdge && isLeadingEdgeSegment) ||
                                   (isMatchTrailingEdge && isTrailingEdgeSegment));
        const bool isEdgeTarget(isCandidateEdge && isTargetType);
        if (isEdgeTarget && isLeadingEdgeSegment) al2.pos-=ps.length;
        if (isEdgeTarget || (ps.type==MATCH))
        {
            if ((! al2.path.empty()) && (al2.path.back().type == MATCH))
            {
                al2.path.back().length += ps.length;
            }
            else
            {
                al2.path.push_back(ps);
                al2.path.back().type = MATCH;
            }
        }
        else
        {
            al2.path.push_back(ps);
        }
    }

    return al2;
}


/// get the range in reference coordinates if you did run matchifyEdgeSoftClip on an alignment:
known_pos_range2
matchifyEdgeSoftClipRefRange(const SimpleAlignment& al)
{
    using namespace ALIGNPATH;

    pos_t beginPos(al.pos);
    pos_t endPos(beginPos);

    const std::pair<unsigned,unsigned> ends(get_match_edge_segments(al.path));
    const unsigned as(al.path.size());
    for (unsigned i(0); i<as; ++i)
    {
        const path_segment& ps(al.path[i]);
        const bool isLeadingEdgeSegment(i<ends.first);
        const bool isTrailingEdgeSegment(i>ends.second);
        const bool isEdgeTarget(isLeadingEdgeSegment || isTrailingEdgeSegment);

        if (isEdgeTarget)
        {
            if (is_segment_type_read_length(ps.type))
            {
                if (isLeadingEdgeSegment)
                {
                    beginPos -= ps.length;
                }
                else
                {
                    endPos += ps.length;
                }
            }

        }
        else
        {
            if (is_segment_type_ref_length(ps.type))
            {
                endPos += ps.length;
            }
        }
    }

    return known_pos_range2(beginPos, endPos);
}
