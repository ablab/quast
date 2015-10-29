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

#include <iterator>

#include "blt_util/blt_exception.hh"


namespace ALIGNPATH
{


/// convert the input path to use "seq match" state '=' and "seq mismatch" state 'X' instead of "alignment match" state 'M'
/// 'N''s always count as mismatch to the reference
///
template <typename symIter1, typename symIter2>
void
apath_add_seqmatch(
    const symIter1 queryBegin,
    const symIter1 queryEnd,
    const symIter2 refBegin,
    const symIter2 refEnd,
    path_t& apath)
{
    path_t apath2;

    symIter1 queryIndex(queryBegin);
    symIter2 refIndex(refBegin);

    const unsigned as(apath.size());
    for (unsigned segmentIndex(0); segmentIndex<as; ++segmentIndex)
    {
        const path_segment& ps(apath[segmentIndex]);
        if (is_segment_align_match(ps.type))
        {
            for (unsigned segmentPos(0); segmentPos<ps.length; ++segmentPos)
            {
                if (queryIndex >= queryEnd)
                {
                    throw blt_exception("apath_add_seqmatch: past end of query\n");
                }

                if (refIndex >= refEnd)
                {
                    throw blt_exception("apath_add_seqmatch: past end of reference\n");
                }

                bool isSeqMatch((*queryIndex) == (*refIndex));
                if ((*queryIndex == 'N') || (*refIndex == 'N')) isSeqMatch = false;
                apath_append(apath2, ( isSeqMatch ? SEQ_MATCH : SEQ_MISMATCH));

                ++queryIndex;
                ++refIndex;
            }
        }
        else
        {
            apath2.push_back(ps);

            if (is_segment_type_read_length(ps.type)) std::advance(queryIndex,ps.length);
            if (is_segment_type_ref_length(ps.type)) std::advance(refIndex,ps.length);
        }
    }

    apath = apath2;
}

}
