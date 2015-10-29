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

#include "svgraph/EdgeInfoUtil.hh"



bool
testIsolatedEdge(
    const SVLocusSet& cset,
    const EdgeInfo& edge)
{
    if (edge.nodeIndex1 != edge.nodeIndex2) return false;

    const SVLocus& locus(cset.getLocus(edge.locusIndex));
#if 0
    // simple criteria -- make sure there are no other nodes in locus
    return (locus.size() == 1);
#endif

    // search to check to see if any bidirectional edges extend from this node (other than the self-edge):
    const SVLocusNode& node1(locus.getNode(edge.nodeIndex1));
    const SVLocusEdgeManager node1Manager(node1.getEdgeManager());

    typedef SVLocusEdgesType::const_iterator edgeiter_t;
    edgeiter_t edgeIter(node1Manager.getMap().begin());
    const edgeiter_t edgeIterEnd(node1Manager.getMap().end());

    EdgeInfo testEdge(edge);

    unsigned edgeCount(0);
    unsigned biEdgeCount(0);
    for (; edgeIter != edgeIterEnd; ++edgeIter)
    {
        testEdge.nodeIndex2 = (edgeIter->first);
        if (testEdge.nodeIndex1 == testEdge.nodeIndex2) continue;
        edgeCount++;
        if (isBidirectionalEdge(cset, testEdge)) biEdgeCount++;
    }

    const bool isLowBiEdge((biEdgeCount >= 1) && (biEdgeCount <= 2));
    const bool isLowTotalEdge(edgeCount <= 4);
    return (! (isLowBiEdge && isLowTotalEdge));
}
