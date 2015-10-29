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

#include "blt_util/blt_types.hh"
#include "blt_util/RangeMap.hh"

#include <cassert>


/// base object for depth_buffers, do not call this directly
struct depth_buffer_base
{
    void
    clear()
    {
        _data.clear();
    }

protected:
    unsigned
    _val(const pos_t pos) const
    {
        return _data.getConstRefDefault(pos,0);
    }

    /// increment range [pos,pos+range) by one
    void
    _inc(const pos_t pos,
         const unsigned incVal)
    {
        _data.getRef(pos) += incVal;
    }

    void
    _clear(const pos_t pos)
    {
        if (_data.isKeyPresent(pos)) _data.erase(pos);
    }

private:
    typedef RangeMap<pos_t,unsigned> count_t;
    count_t _data;
};


/// simple map of position to depth
///
/// assumes that a narrow list of positions is maintained so that
/// array based lookup optimizations can be used
///
struct depth_buffer : public depth_buffer_base
{
    unsigned
    val(const pos_t pos) const
    {
        return _val(pos);
    }

    void
    inc(const pos_t pos)
    {
        _inc(pos,1);
    }

    void
    clear_pos(const pos_t pos)
    {
        _clear(pos);
    }

    /// return true if buffered depth exceeds depth in [begin,end]
    bool
    is_range_ge_than(const pos_t begin,
                     const pos_t end,
                     const unsigned depth) const
    {
        assert(begin <= end);
        for (pos_t i(begin); i<=end; ++i)
        {
            if (val(i) >= depth) return true;
        }
        return false;
    }
};



/// simple map of position to depth
///
/// assumes that a narrow list of positions is maintained so that
/// array based lookup optimizations can be used
///
/// optionally "compresses" depth buffer so that multiple positions
/// are binned together.
///
struct depth_buffer_compressible : public depth_buffer_base
{
    depth_buffer_compressible(
        const unsigned compressionFactor=1)
        : _csize(compressionFactor),
          _halfcsize(_csize/2)
    {
        assert(_csize>=1);
    }

    unsigned
    val(const pos_t pos) const
    {
        return ((_val(pos/_csize)+_halfcsize)/_csize);
    }

    /// increment range [pos,pos+range) by one
    void
    inc(pos_t pos,
        const unsigned posRange = 1)
    {
        assert(posRange>=1);
        const pos_t endPos(pos+posRange);
        pos_t dataPos(pos/_csize);
        while (true)
        {
            const pos_t blockEndPos(std::min(((dataPos+1)*static_cast<pos_t>(_csize)), endPos));
            _inc(dataPos,(blockEndPos-pos));

            if (blockEndPos==endPos) return;
            pos = blockEndPos;
            dataPos++;
        }
    }

    /// if compressionFactor is gt 1, pos arguments must be ordered to prevent surprising behavior
    void
    clear_pos(const pos_t pos)
    {
        // compression factor only works here by assuming clear_pos is being called in order
        if ((pos % _csize) != (_csize-1)) return;
        const pos_t dataPos(pos/_csize);
        _clear(dataPos);
    }

private:
    const unsigned _csize;
    const unsigned _halfcsize;
};

