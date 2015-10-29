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

#pragma once

#include <vector>


/// very simple matrix implementation, row major
template <typename T>
struct basic_matrix
{
    typedef typename std::vector<T> data_t;
    typedef typename data_t::iterator iterator;
    typedef typename data_t::const_iterator const_iterator;

    basic_matrix(
        const unsigned rowCount = 0,
        const unsigned colCount = 0) :
        _colCount(colCount),
        _data(rowCount* colCount)
    {}

    void
    resize(
        const unsigned rowCount,
        const unsigned colCount)
    {
        _colCount=colCount;
        _data.resize(rowCount*colCount);
    }

    T&
    val(const unsigned row,
        const unsigned col)
    {
        return _data[(row*_colCount+col)];
    }

    const T&
    val(const unsigned row,
        const unsigned col) const
    {
        return _data[(row*_colCount+col)];
    }

    bool
    empty()
    {
        return _data.empty();
    }

    size_t
    size()
    {
        return _data.size();
    }

    iterator
    begin()
    {
        return _data.begin();
    }

    const_iterator
    begin() const
    {
        return _data.begin();
    }

    iterator
    end()
    {
        return _data.end();
    }

    const_iterator
    end() const
    {
        return _data.end();
    }

private:
    unsigned _colCount;
    std::vector<T> _data;
};


