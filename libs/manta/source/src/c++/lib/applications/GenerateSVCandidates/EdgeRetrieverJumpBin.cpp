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

#include "EdgeRetrieverJumpBin.hh"

#include <cassert>

//#define DEBUG_EDGER

#ifdef DEBUG_EDGER
#include <iostream>
#include "blt_util/log.hh"
#endif



EdgeRetrieverJumpBin::
EdgeRetrieverJumpBin(
    const SVLocusSet& set,
    const unsigned graphNodeMaxEdgeCount,
    const unsigned binCount,
    const unsigned binIndex) :
    EdgeRetriever(set,graphNodeMaxEdgeCount),
    _binCount(binCount),
    _binIndex(binIndex),
    _edgeIndex(0),
    _avgBinTotalCount(0),
    _binTotalCount(_binCount,0)
{
    assert(binCount > 0);
    assert(binIndex < binCount);

    const unsigned long totalObservationCount(_set.totalObservationCount());
    _avgBinTotalCount=(1+totalObservationCount/_binCount);

#ifdef DEBUG_EDGER
    log_os << "EDGER: bi,bc,avg: "
           << binIndex << " "
           << binCount << " "
           << _avgBinTotalCount << "\n";
#endif
}



void
EdgeRetrieverJumpBin::
advanceEdge()
{
    typedef SVLocusEdgesType::const_iterator edgeiter_t;

    const bool isFilterNodes(_graphNodeMaxEdgeCount>0);

    // advance to next edge unless this is the first iteration:
    if (0 != _edgeIndex) _edge.nodeIndex2++;

    while (_edge.locusIndex < _set.size())
    {
        const SVLocus& locus(_set.getLocus(_edge.locusIndex));
        while (_edge.nodeIndex1<locus.size())
        {
            const SVLocusNode& node1(locus.getNode(_edge.nodeIndex1));
            const bool isEdgeFilterNode1(isFilterNodes && (node1.size()>_graphNodeMaxEdgeCount));
            const SVLocusEdgeManager node1Manager(node1.getEdgeManager());
            edgeiter_t edgeIter(node1Manager.getMap().lower_bound(_edge.nodeIndex2));
            const edgeiter_t edgeIterEnd(node1Manager.getMap().cend());

            for (; edgeIter != edgeIterEnd; ++edgeIter)
            {
                _edge.nodeIndex2 = edgeIter->first;

                // if both nodes have high edge counts we filter out the edge:
                if (isEdgeFilterNode1)
                {
                    const SVLocusNode& node2(locus.getNode(_edge.nodeIndex2));
                    const bool isEdgeFilterNode2(node2.size()>_graphNodeMaxEdgeCount);
                    if (isEdgeFilterNode2)
                    {
#ifdef DEBUG_EDGER
                        log_os << "EDGER: advance filtering @ index: " << _edgeIndex << "\n";
#endif
                        continue;
                    }
                }


                const unsigned firstTargetBin(_edgeIndex%_binCount);
                unsigned targetBin(firstTargetBin);

                do
                {
                    if (_binTotalCount[targetBin] < _avgBinTotalCount) break;
                    targetBin=((targetBin+1)%_binCount);
                }
                while (targetBin != firstTargetBin);

#ifdef DEBUG_EDGER
                log_os << "EDGER: edgeIndex,ftarget,target,binIndex " << _edgeIndex << " " << firstTargetBin << " " << targetBin << " " << _binIndex << "\n";
#endif

                _edgeIndex++;
                if (targetBin == _binIndex)
                {
                    // get edge count:
                    unsigned edgeCount(edgeIter->second.getCount());
                    {
                        const bool isSelfEdge(edgeIter->first == _edge.nodeIndex1);
                        if (! isSelfEdge) edgeCount += locus.getEdge(edgeIter->first,_edge.nodeIndex1).getCount();
                    }

                    _binTotalCount[targetBin] += edgeCount;
                    return;
                }
            }
            ++_edge.nodeIndex1;
            _edge.nodeIndex2=_edge.nodeIndex1;
        }
        ++_edge.locusIndex;
        _edge.nodeIndex1=0;
        _edge.nodeIndex2=0;
    }
}



bool
EdgeRetrieverJumpBin::
next()
{
#ifdef DEBUG_EDGER
    log_os << "EDGER: start index: " << _edgeIndex << "\n";
#endif

    if (_edge.locusIndex >= _set.size()) return false;

    advanceEdge();
#ifdef DEBUG_EDGER
    log_os << "EDGER: advanced index: " << _edgeIndex << " " << _edge  << "\n";
#endif

    return (_edge.locusIndex < _set.size());
}
