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


#include "depth_buffer_util.hh"



void
add_alignment_to_depth_buffer(
    const pos_t& pos,
    const ALIGNPATH::path_t& apath,
    depth_buffer& db)
{
    using namespace ALIGNPATH;

    pos_t ref_head_pos(pos);

    for (const path_segment& ps : apath)
    {
        if ( is_segment_align_match(ps.type) )
        {
            for (unsigned j(0); j<ps.length; ++j) db.inc(ref_head_pos+static_cast<pos_t>(j));
        }

        if ( is_segment_type_ref_length(ps.type) ) ref_head_pos += ps.length;
    }
}

