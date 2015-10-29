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

#include <iostream>



template <typename T>
boost::optional<T>
RegionPayloadTracker<T>::
isIntersectRegionImpl(
    const pos_t beginPos,
    const pos_t endPos) const
{
    boost::optional<T> result;
    if (_regions.empty()) return result;

    // 1. find first region where region.endPos > query.beginPos
    const auto posIter(_regions.upper_bound(known_pos_range2(beginPos,beginPos)));

    // 2. conclusion based on non-overlapping region constraint
    if ((posIter != _regions.end()) && (posIter->first.begin_pos() < endPos))
    {
        result.reset(posIter->second);
    }
    return result;
}



template <typename T>
boost::optional<T>
RegionPayloadTracker<T>::
isSubsetOfRegionImpl(
    const pos_t beginPos,
    const pos_t endPos) const
{
    boost::optional<T> result;
    if (_regions.empty()) return result;

    // 1. find first region where region.endPos > query.beginPos
    const auto posIter(_regions.upper_bound(known_pos_range2(beginPos,beginPos)));

    // 2. conclusion based on non-overlapping region constraint
    if (posIter == _regions.end()) return result;
    if (posIter->first.end_pos() < endPos) return result;

    // 2. conclusion based on non-overlapping region constraint
    if (posIter->first.begin_pos() <= beginPos)
    {
        result.reset(posIter->second);
    }
    return result;
}



template <typename T>
bool
RegionPayloadTracker<T>::
addRegion(
    known_pos_range2 range,
    const T payload)
{
    // check for potential set of intersecting ranges,
    // if found expand range size to represent intersection
    // remove previous content:
    auto startOlap(_regions.upper_bound(known_pos_range2(range.begin_pos()-1,range.begin_pos()-1)));
    while (startOlap != _regions.end())
    {
        // if adjacent, check that payload values match:
        if (startOlap->first.end_pos() == range.begin_pos())
        {
            if (startOlap->second != payload)
            {
                startOlap++;
                continue;
            }
        }
        if (startOlap->first.begin_pos() <= (range.begin_pos()-1))
        {
            // start intersects range:
            range.set_begin_pos(startOlap->first.begin_pos());
        }
        break;
    }

    auto endOlap(_regions.upper_bound(known_pos_range2(range.end_pos(),range.end_pos())));
    if (endOlap != _regions.end())
    {
        // if adjacent, check that payload values match:
        bool isMerge(false);
        if (endOlap->first.begin_pos() == range.end_pos())
        {
            isMerge=(endOlap->second == payload);
        }
        else if (endOlap->first.begin_pos() < range.end_pos())
        {
            isMerge=true;
        }

        if (isMerge)
        {
            // end intersects range:
            range.set_end_pos(endOlap->first.end_pos());
            endOlap++;
        }
    }

    // check for overlap conflicts:
    for (auto iter(startOlap); iter != endOlap; ++iter)
    {
        if (iter->second != payload) return false;
    }

    _regions.erase(startOlap,endOlap);
    _regions.insert(std::make_pair(range,payload));
    return true;
}



template <typename T>
void
RegionPayloadTracker<T>::
removeToPos(
    const pos_t pos)
{
    auto iter(_regions.begin());
    const auto endIter(_regions.end());
    while ( (iter != endIter) && (iter->first.end_pos() <= (pos+1)) )
    {
        ++iter;
    }
    _regions.erase(_regions.begin(),iter);
}



template <typename T>
void
RegionPayloadTracker<T>::
dump(
    std::ostream& os) const
{
    os << "RegionPayloadTracker\n";
    for (const auto& val : _regions)
    {
        os << "region: " << val.first << " value: " << val.second << "\n";
    }
}

