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

#include "EdgeRetrieverBin.hh"

#include <cassert>


//#define DEBUG_EDGER

#ifdef DEBUG_EDGER
#include <iostream>
#include "blt_util/log.hh"
#endif



static
unsigned long
getBoundaryCount(
    const double binCount,
    const double binIndex,
    const double totalCount)
{
    return static_cast<unsigned>(std::floor((totalCount*binIndex)/binCount));
}



EdgeRetrieverBin::
EdgeRetrieverBin(
    const SVLocusSet& set,
    const unsigned graphNodeMaxEdgeCount,
    const unsigned binCount,
    const unsigned binIndex) :
    EdgeRetriever(set,graphNodeMaxEdgeCount),
    _headCount(0)
{
    assert(binCount > 0);
    assert(binIndex < binCount);

    const unsigned long totalObservationCount(_set.totalObservationCount());
    _beginCount=(getBoundaryCount(binCount,binIndex,totalObservationCount));
    _endCount=(getBoundaryCount(binCount,binIndex+1,totalObservationCount));

#ifdef DEBUG_EDGER
    log_os << "EDGER: bi,bc,begin,end: "
           << binIndex << " "
           << binCount << " "
           << _beginCount << " "
           << _endCount << "\n";
#endif
}



void
EdgeRetrieverBin::
jumpToFirstEdge()
{
    typedef SVLocusEdgesType::const_iterator edgeiter_t;

    const bool isFilterNodes(_graphNodeMaxEdgeCount>0);
    const unsigned setSize(_set.size());

    // first catch headCount up to the begin edge if required:
    while (true)
    {
        assert(_edge.locusIndex < setSize);

        const SVLocus& locus(_set.getLocus(_edge.locusIndex));
        const unsigned locusObservationCount(locus.totalObservationCount());

        if ((_headCount+locusObservationCount) > _beginCount)
        {
            const unsigned locusSize(locus.size());
            while (_edge.nodeIndex1 < locusSize)
            {
                const SVLocusNode& node1(locus.getNode(_edge.nodeIndex1));
                const bool isEdgeFilterNode1(isFilterNodes && (node1.size()>_graphNodeMaxEdgeCount));

                const SVLocusEdgeManager node1Manager(node1.getEdgeManager());
                edgeiter_t edgeIter(node1Manager.getMap().lower_bound(_edge.nodeIndex1));
                const edgeiter_t edgeiterEnd(node1Manager.getMap().cend());

                for (; edgeIter != edgeiterEnd; ++edgeIter)
                {
                    unsigned edgeCount(edgeIter->second.getCount());
                    const bool isSelfEdge(edgeIter->first == _edge.nodeIndex1);
                    if (! isSelfEdge) edgeCount += locus.getEdge(edgeIter->first,_edge.nodeIndex1).getCount();
                    _headCount += edgeCount;
                    if (_headCount > _beginCount)
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
                                log_os << "EDGER: jump filtering @ hc: " << _headCount << "\n";
#endif
                                continue;
                            }
                        }
                        return;
                    }
                }

                _edge.nodeIndex1++;
            }
            assert(_headCount >= _beginCount);
        }
        _headCount += locusObservationCount;
        _edge.locusIndex++;
    }

    assert(false && "jumpToFirstEdge: invalid state");
}



void
EdgeRetrieverBin::
advanceEdge()
{
    typedef SVLocusEdgesType::const_iterator edgeiter_t;

    const bool isFilterNodes(_graphNodeMaxEdgeCount>0);
    const unsigned setSize(_set.size());

    if (0 != _headCount) _edge.nodeIndex2++;

    bool isLastFiltered(false);

    while (true)
    {
        if (isLastFiltered && (_edge.locusIndex == setSize))
        {
            _headCount = (_endCount + 1);
            return;
        }
        assert(_edge.locusIndex < setSize);

        const SVLocus& locus(_set.getLocus(_edge.locusIndex));
        const unsigned locusSize(locus.size());

        while (_edge.nodeIndex1 < locusSize)
        {
            const SVLocusNode& node1(locus.getNode(_edge.nodeIndex1));
            const bool isEdgeFilterNode1(isFilterNodes && (node1.size()>_graphNodeMaxEdgeCount));
            const SVLocusEdgeManager node1Manager(node1.getEdgeManager());
            edgeiter_t edgeIter(node1Manager.getMap().lower_bound(_edge.nodeIndex2));
            const edgeiter_t edgeIterEnd(node1Manager.getMap().cend());

            for (; edgeIter != edgeIterEnd; ++edgeIter)
            {
                unsigned edgeCount(edgeIter->second.getCount());
                const bool isSelfEdge(edgeIter->first == _edge.nodeIndex1);
                if (! isSelfEdge) edgeCount += locus.getEdge(edgeIter->first,_edge.nodeIndex1).getCount();
                _headCount += edgeCount;
                _edge.nodeIndex2 = edgeIter->first;

                // if both nodes have high edge counts we filter out the edge:
                if (isEdgeFilterNode1)
                {
                    const SVLocusNode& node2(locus.getNode(_edge.nodeIndex2));
                    const bool isEdgeFilterNode2(node2.size()>_graphNodeMaxEdgeCount);
                    if (isEdgeFilterNode2)
                    {
#ifdef DEBUG_EDGER
                        log_os << "EDGER: advance filtering @ hc: " << _headCount << "\n";
#endif
                        isLastFiltered=true;
                        continue;
                    }
                }

                return;
            }
            ++_edge.nodeIndex1;
            _edge.nodeIndex2=_edge.nodeIndex1;
        }
        ++_edge.locusIndex;
        _edge.nodeIndex1=0;
        _edge.nodeIndex2=0;
    }

    assert(false && "advanceEdge: invalid state");
}



bool
EdgeRetrieverBin::
next()
{
#ifdef DEBUG_EDGER
    log_os << "EDGER: start next hc: " << _headCount << "\n";
#endif

    if (_headCount >= _endCount) return false;

    // first catch headCount up to the begin edge if required:
    if (_headCount < _beginCount)
    {
        jumpToFirstEdge();
#ifdef DEBUG_EDGER
        log_os << "EDGER: jumped hc: " << _headCount << " " << _edge  << "\n";
#endif
    }
    else
    {
        advanceEdge();
#ifdef DEBUG_EDGER
        log_os << "EDGER: advanced hc: " << _headCount << " " << _edge  << "\n";
#endif
    }

    assert(_headCount >= _beginCount);
    return (_headCount <= _endCount);
}
