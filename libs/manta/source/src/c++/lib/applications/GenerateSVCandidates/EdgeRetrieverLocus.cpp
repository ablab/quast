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

#include "EdgeRetrieverLocus.hh"

#include <cassert>

#include <iostream>

//#define DEBUG_EDGER

#ifdef DEBUG_EDGER
#include "blt_util/log.hh"
#endif



EdgeRetrieverLocus::
EdgeRetrieverLocus(
    const SVLocusSet& set,
    const unsigned graphNodeMaxEdgeCount,
    const LocusEdgeOptions& opt) :
    EdgeRetriever(set, graphNodeMaxEdgeCount),
    _opt(opt),
    _isInit(false)
{
    assert(_opt.locusIndex<set.size());
}



/// this filter enables a special option to filter down to all edges connected to
/// a single node or only the edge connecting two nodes:
static
bool
isEdgeFiltered(
    const LocusEdgeOptions& opt,
    const EdgeInfo& edge)
{
    if (! opt.isNodeIndex1) return false;
    if (opt.isNodeIndex2)
    {
        const bool isMatch(
            (edge.nodeIndex1 == opt.nodeIndex1) &&
            (edge.nodeIndex2 == opt.nodeIndex2));
        const bool isSwapMatch(
            (edge.nodeIndex2 == opt.nodeIndex1) &&
            (edge.nodeIndex1 == opt.nodeIndex2));

        return (! (isMatch || isSwapMatch));
    }
    else
    {
        const bool isMatch
        (edge.nodeIndex1 == opt.nodeIndex1);
        const bool isSwapMatch
        (edge.nodeIndex2 == opt.nodeIndex1);

        return (! (isMatch || isSwapMatch));
    }
}



void
EdgeRetrieverLocus::
advanceEdge()
{
    typedef SVLocusEdgesType::const_iterator edgeiter_t;

    if (_isInit)
    {
        _edge.nodeIndex2++;
    }
    else
    {
        _edge.locusIndex=_opt.locusIndex;
        _edge.nodeIndex1=0;
        _edge.nodeIndex2=0;
        _isInit=true;
    }

    const SVLocus& locus(_set.getLocus(_edge.locusIndex));
    while (_edge.nodeIndex1<locus.size())
    {
        const SVLocusNode& node1(locus.getNode(_edge.nodeIndex1));
        const bool isEdgeFilterNode1((_graphNodeMaxEdgeCount>0) && node1.size()>_graphNodeMaxEdgeCount);
        const SVLocusEdgeManager node1Manager(node1.getEdgeManager());
        edgeiter_t edgeIter(node1Manager.getMap().lower_bound(_edge.nodeIndex2));
        const edgeiter_t edgeIterEnd(node1Manager.getMap().cend());

        for (; edgeIter != edgeIterEnd; ++edgeIter)
        {
            _edge.nodeIndex2 = edgeIter->first;

            // check whether this edge is in the requested set:
            if (isEdgeFiltered(_opt,_edge)) continue;

            // check whether this is a noise edge that we skip:
            if (isEdgeFilterNode1)
            {
                const SVLocusNode& node2(locus.getNode(_edge.nodeIndex2));
                const bool isEdgeFilterNode2(node2.size()>_graphNodeMaxEdgeCount);
                if (isEdgeFilterNode2) continue;
            }
            return;
        }
        _edge.nodeIndex1++;
        _edge.nodeIndex2=_edge.nodeIndex1;
    }

    _edge.locusIndex++;
}



bool
EdgeRetrieverLocus::
next()
{
#ifdef DEBUG_EDGER
    log_os << "EDGERL: start\n";
#endif

    if (_edge.locusIndex > _opt.locusIndex) return false;
    advanceEdge();

    return (_edge.locusIndex == _opt.locusIndex);
}
