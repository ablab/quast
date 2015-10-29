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

#include "blt_util/align_path.hh"
#include "blt_util/blt_types.hh"


#include <cassert>


namespace ALIGNPATH
{

inline
void
increment_path(const path_t& path,
               unsigned& path_index,
               unsigned& read_offset,
               pos_t& ref_offset)
{
    const path_segment& ps(path[path_index]);

    if       (is_segment_align_match(ps.type))
    {
        read_offset += ps.length;
        ref_offset += ps.length;
    }
    else if ((ps.type == DELETE) || (ps.type == SKIP))
    {
        ref_offset += ps.length;
    }
    else if ((ps.type == INSERT) || (ps.type == SOFT_CLIP))
    {
        read_offset += ps.length;
    }
    else if ((ps.type == HARD_CLIP) || (ps.type == PAD))
    {
        // do nothing
    }
    else
    {
        assert(false && "Unexpected alignment type"); // can't handle other CIGAR types yet
    }

    path_index++;
}


// Initialize to the segment count, insert and delete size of a
// swap in the path. assumes path_index points to the begining of
// a swap:
//
struct swap_info
{
    swap_info(const path_t& path,
              const unsigned path_index)
        : n_seg(path_index)
        , insert_length(0)
        , delete_length(0)
    {
        const unsigned aps(path.size());
        for (; (n_seg<aps) && is_segment_type_indel(path[n_seg].type); ++n_seg)
        {
            const path_segment& ps(path[n_seg]);
            if     (ps.type==INSERT)
            {
                insert_length += ps.length;
            }
            else if (ps.type==DELETE)
            {
                delete_length += ps.length;
            }
            else
            {
                assert(false && "Unexpected alignment type");
            }
        }
        n_seg -= path_index;
    }

    unsigned n_seg;
    unsigned insert_length;
    unsigned delete_length;
};
}
