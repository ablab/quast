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

#include <cassert>
#include <cstdint>
#include <iosfwd>
#include <vector>


// maintains the average of a set in a sliding window
//
struct window_average
{
    window_average(const int32_t init_full_size)
        : _total(0)
        , _size(0)
        , _null_size(0)
        , _head(0)
        , _full_size(init_full_size)
        , _buf(init_full_size)
        , _is_buf(init_full_size)
    {
        assert(init_full_size>0);
    }

    void
    reset()
    {
        _total=0;
        _size=0;
        _null_size=0;
        _head=0;
    }

    void
    insert(const int32_t x)
    {
        pre_insert();
        _total += x;
        _buf[_head] = x;
        _is_buf[_head] = true;
        _head=(_head+1)%_full_size;
    }

    // inserts an N/A value:
    void
    insert_null()
    {
        pre_insert();
        _null_size++;
        _buf[_head] = 0;
        _is_buf[_head] = false;
        _head=(_head+1)%_full_size;
    }

    uint32_t
    full_size() const
    {
        return _full_size;
    }

    // actual number of data points currently in the window, this can
    // be less than data size due to initialization or N/A values
    //
    uint32_t
    size() const
    {
        return (_size-_null_size);
    }

    float
    avg() const
    {
        return (static_cast<float>(_total)/static_cast<float>(size()));
    }

private:
    void
    pre_insert()
    {
        if (_size==_full_size)
        {
            if (_is_buf[_head])
            {
                _total -= _buf[_head];
            }
            else
            {
                _null_size--;
            }
        }
        else
        {
            _size++;
        }
    }

    int64_t _total;
    uint32_t _size;
    uint32_t _null_size;
    uint32_t _head;
    uint32_t _full_size;
    std::vector<int32_t> _buf;
    std::vector<bool> _is_buf;
};
