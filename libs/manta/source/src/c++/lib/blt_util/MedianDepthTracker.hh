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

#include <map>


/// online median tracking obj assuming high repeat obs counts
///
/// Note that by design depth=0 is excluded from the median
struct MedianDepthTracker
{
    void
    addObs(const unsigned val)
    {
        auto iter(_cmap.find(val));
        if (iter == _cmap.end())
        {
            _cmap[val] = 1;
        }
        else
        {
            iter->second++;
        }
        _total++;
    }

    double
    getMedian() const
    {
        // +1 makes the 1/2 case work out correctly...
        unsigned ztotal(_total+1);
        const auto ziter(_cmap.find(0));
        if (ziter != _cmap.end())
        {
            ztotal -= ziter->second;
        }

        unsigned sum = 0;
        unsigned lastBefore = 0;
        unsigned firstAfter = 0;
        for (const auto& val : _cmap)
        {
            if (val.first == 0) continue;

            // double instead of half so that we stay away from float math:
            sum += (val.second*2);
            if (sum >= ztotal)
            {
                firstAfter = val.first;
                if ((ztotal + val.second*2) != (sum + 1))
                {
                    lastBefore = firstAfter;
                }
                break;
            }
            lastBefore = val.first;
        }

        assert ((sum+1) >= ztotal);

        if (lastBefore == firstAfter) return lastBefore;
        return (static_cast<double>(lastBefore + firstAfter)/2.);
    }

private:
    unsigned _total = 0;
    std::map<unsigned,unsigned> _cmap;
};
