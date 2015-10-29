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

#include "blt_util/known_pos_range2.hh"

#include <iosfwd>


/// single chromosome range
///
/// all internal locations use a chromosome index number
struct GenomeInterval
{
    GenomeInterval(
        const int32_t initTid = 0,
        const pos_t beginPos = 0,
        const pos_t endPos = 0) :
        tid(initTid),
        range(beginPos,endPos)
    {}

    /// does this intersect a second GenomeInterval?
    bool
    isIntersect(const GenomeInterval& gi) const
    {
        if (tid != gi.tid) return false;
        return range.is_range_intersect(gi.range);
    }

    bool
    operator<(const GenomeInterval& rhs) const
    {
        if (tid<rhs.tid) return true;
        if (tid == rhs.tid)
        {
            return (range<rhs.range);
        }
        return false;
    }

    bool
    operator==(const GenomeInterval& rhs) const
    {
        return ((tid==rhs.tid) && (range==rhs.range));
    }

    void
    clear()
    {
        tid = 0;
        range.clear();
    }

    template<class Archive>
    void serialize(Archive& ar, const unsigned /* version */)
    {
        ar& tid& range;
    }

    int32_t tid;
    known_pos_range2 range;
};


std::ostream&
operator<<(std::ostream& os, const GenomeInterval& gi);

BOOST_CLASS_IMPLEMENTATION(GenomeInterval, boost::serialization::object_serializable)
