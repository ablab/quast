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

#include "svgraph/GenomeIntervalUtil.hh"



std::vector<unsigned>
intervalCompressor(
    std::vector<GenomeInterval>& intervals)
{
    std::vector<GenomeInterval> intervals2;

    const unsigned count(intervals.size());
    std::vector<bool> isTransfered(count,false);

    std::vector<unsigned> indexMap(count,0);

    for (unsigned headIndex(0); headIndex < count; ++headIndex)
    {
        if (isTransfered[headIndex]) continue;

        const unsigned headIndex2(intervals2.size());
        isTransfered[headIndex] = true;
        indexMap[headIndex] = headIndex2;
        intervals2.push_back(intervals[headIndex]);
        GenomeInterval& headInterval(intervals2.back());

        while (true)
        {
            bool isComplete(true);

            for (unsigned testIndex(headIndex+1); testIndex < count; ++testIndex)
            {
                if (isTransfered[testIndex]) continue;

                if (headInterval.isIntersect(intervals[testIndex]))
                {
                    isTransfered[testIndex] = true;
                    indexMap[testIndex] = headIndex2;
                    headInterval.range.merge_range(intervals[testIndex].range);
                    isComplete=false;
                    break;
                }
            }

            if (isComplete) break;
        }
    }

    intervals = intervals2;
    return indexMap;
}
