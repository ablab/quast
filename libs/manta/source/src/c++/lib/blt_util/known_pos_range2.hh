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

///
/// this is the beginning of a redesign to known_pos_range
/// to be more efficient for the manta case
///

#pragma once

#include "blt_util/blt_types.hh"

#include "boost/serialization/level.hpp"

#include <algorithm>
#include <iosfwd>


/// \brief integer ranges which are right open
///
struct known_pos_range2
{
    known_pos_range2() :
        known_pos_range2(0,0)
    {}

    known_pos_range2(
        const pos_t bp,
        const pos_t ep) :
        _begin_pos(bp),
        _end_pos(ep)
    {}

    void
    set_begin_pos(const pos_t pos)
    {
        _begin_pos=pos;
    }

    void
    set_end_pos(const pos_t pos)
    {
        _end_pos=pos;
    }

    void
    set_range(const pos_t begin,
              const pos_t end)
    {
        set_begin_pos(begin);
        set_end_pos(end);
    }

    /// expand (or contract) range
    void
    expandBy(
        const pos_t expandSize)
    {
        _begin_pos-=expandSize;
        _end_pos+=expandSize;
        if ((expandSize<0) && (_end_pos < _begin_pos))
        {
            _begin_pos = (_begin_pos+_end_pos)/2;
            _end_pos = _begin_pos;
        }
    }

    /// shift range position
    void
    offsetBy(
        const pos_t offsetSize)
    {
        _begin_pos += offsetSize;
        _end_pos += offsetSize;
    }

    pos_t
    begin_pos() const
    {
        return _begin_pos;
    }

    pos_t
    end_pos() const
    {
        return _end_pos;
    }

    pos_t
    center_pos() const
    {
        return _begin_pos + ((std::max(size(),1u)-1)/2);
    }

    bool
    is_pos_intersect(const pos_t pos) const
    {
        return ((pos >= _begin_pos) &&
                (pos < _end_pos));
    }

    bool
    is_range_intersect(const known_pos_range2& pr) const
    {
        return ((pr._end_pos > _begin_pos) &&
                (pr._begin_pos < _end_pos));
    }

    /// does this range completely overlap pr?
    bool
    is_superset_of(const known_pos_range2& pr) const
    {
        return
            ((pr._end_pos <= _end_pos) &&
             (pr._begin_pos >= _begin_pos));
    }

    unsigned
    size() const
    {
        return std::max(0,_end_pos-_begin_pos);
    }

    bool
    operator<(const known_pos_range2& rhs) const
    {
        if (_begin_pos < rhs._begin_pos) return true;
        if (_begin_pos == rhs._begin_pos)
        {
            if (_end_pos < rhs._end_pos) return true;
        }
        return false;
    }

    bool
    operator==(const known_pos_range2& rhs) const
    {
        return ((_begin_pos==rhs._begin_pos) && (_end_pos==rhs._end_pos));
    }

    // expand range to extend of a second range:
    void
    merge_range(const known_pos_range2& kpr)
    {
        if (kpr._begin_pos<_begin_pos) _begin_pos=kpr._begin_pos;
        if (kpr._end_pos>_end_pos) _end_pos=kpr._end_pos;
    }

    void
    clear()
    {
        _begin_pos=0;
        _end_pos=0;
    }


    template<class Archive>
    void serialize(Archive& ar, const unsigned /* version */)
    {
        ar& _begin_pos& _end_pos;
    }

private:
    pos_t _begin_pos;
    pos_t _end_pos;
};



/// return the union of two ranges:
inline
known_pos_range2
merge_range(
    const known_pos_range2& kpr1,
    const known_pos_range2& kpr2)
{
    known_pos_range2 res;
    res.set_begin_pos(std::min(kpr1.begin_pos(),kpr2.begin_pos()));
    res.set_end_pos(std::max(kpr1.end_pos(),kpr2.end_pos()));
    return res;
}


/// generalized intersection test
///
/// this allows a positive or negative window size to be added to the range
/// intersection test, if windowSize is 0, then this is a regular intersection test
///
/// For example, if windowSize is 100 this returns true if the two ranges are within 100
/// of each other
inline
bool
is_intersect_window(
    const known_pos_range2& kpr1,
    const known_pos_range2& kpr2,
    const pos_t windowSize = 0)
{
    return (((kpr1.end_pos()+windowSize) > kpr2.begin_pos()) &&
            ((kpr2.end_pos()+windowSize) > kpr1.begin_pos()));

}

std::ostream& operator<<(std::ostream& os, const known_pos_range2& pr);

BOOST_CLASS_IMPLEMENTATION(known_pos_range2, boost::serialization::object_serializable)

