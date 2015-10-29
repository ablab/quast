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

#include "blt_types.hh"
#include "known_pos_range2.hh"

#include "boost/icl/interval_map.hpp"


/// accumulates region specific sum(T) give a set of (region,T) associations
///
template <typename T>
struct RegionSum
{
    void
    clear()
    {
        _map.clear();
    }

    void
    add(
        const known_pos_range2 pr,
        const T val)
    {
        _map.add(std::make_pair(boost::icl::interval<pos_t>::right_open(pr.begin_pos(),pr.end_pos()),val));
    }

    /// return peak value for all regions:
    T
    maxVal() const
    {
        T max(0);
        bool isFirst(true);
        for (const auto& val : _map)
        {
            if (isFirst || val.second > max)
            {
                max = val.second;
                isFirst = false;
            }
        }
        return max;
    }

private:
    using map_t = boost::icl::interval_map<pos_t,T>;
    map_t _map;
};
