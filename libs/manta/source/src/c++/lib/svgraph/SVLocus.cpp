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

#include "common/Exceptions.hh"
#include "svgraph/SVLocus.hh"

#include "boost/foreach.hpp"

#include <iostream>
#include <stack>

#ifdef DEBUG_SVL
#include "blt_util/log.hh"
#endif



void
SVLocus::
nodeHurl(const NodeIndexType nodePtr) const
{
    using namespace illumina::common;

    std::ostringstream oss;
    oss << "ERROR: Attempting to access node: " << _index << ":" << nodePtr << " in locus with size: " << size() << "\n";
    BOOST_THROW_EXCEPTION(LogicException(oss.str()));
}



void
SVLocus::
mergeNode(
    const NodeIndexType fromIndex,
    const NodeIndexType toIndex,
    flyweight_observer_t* obs)
{
    using namespace illumina::common;

    assert(fromIndex != toIndex);

#ifdef DEBUG_SVL
    static const std::string logtag("SVLocus::mergeNode");
    log_os << logtag << " from: " << fromIndex << " to: " << toIndex << " size: " << size() << "\n";
#endif

    SVLocusNode& fromNode(getNode(fromIndex));
    SVLocusNode& toNode(getNode(toIndex));

#ifdef DEBUG_SVL
    log_os << logtag << " BEFORE fromNode: " << fromNode;
    log_os << logtag << " BEFORE toNode: " << toNode;
#endif

    if (fromNode.getInterval().tid != toNode.getInterval().tid)
    {
        std::ostringstream oss;
        oss << "ERROR: Attempting to merge nodes on different chromosomes\n"
            << "\tNode1: " << fromNode
            << "\tNode2: " << toNode;
        BOOST_THROW_EXCEPTION(LogicException(oss.str()));
    }

    notifyDelete(obs,toIndex);

    toNode.setIntervalRange(merge_range(toNode.getInterval().range,fromNode.getInterval().range));
    const bool isToCount(toNode.isOutCount());
    const bool isFromCount(fromNode.isOutCount());
    if     ((! isToCount) && (isFromCount))
    {
        toNode.setEvidenceRange(fromNode.getEvidenceRange());
    }
    else if ((! isFromCount) && (isToCount))
    {
        // pass (keep toNode value as is)
    }
    else
    {
        toNode.setEvidenceRange(merge_range(toNode.getEvidenceRange(),fromNode.getEvidenceRange()));
    }

    notifyAdd(obs,toIndex);

    // now take all fromNode edges and 'redirect' them to the toNode index
    //
    const SVLocusEdgeManager edgeMap(fromNode.getEdgeManager());
    for (const SVLocusEdgesType::value_type& fromNodeEdgeIter : edgeMap.getMap())
    {
        // alias value_type components (not required, but makes the logic easier to follow):
        const NodeIndexType& fromNodeEdgeIndex(fromNodeEdgeIter.first);
        const SVLocusEdge* fromNodeEdgePtr(&(fromNodeEdgeIter.second));

#ifdef DEBUG_SVL
        // is this edge between the to and from nodes?
        const bool isToFromEdge(fromNodeEdgeIndex == toIndex);

        log_os << logtag << " handle fromEdge: " << _index << ":" << fromNodeEdgeIndex << " isToFromEdge: " << isToFromEdge << "\n";
#endif

        // is this a self edge of the from node?
        const bool isSelfFromEdge(fromNodeEdgeIndex == fromIndex);

        if (isSelfFromEdge)
        {
            // self-edge needs to be handled as a special case:
            toNode.mergeEdge(toIndex,*(fromNodeEdgePtr));
            continue;
        }

        // Check for the special case when there is an edge between from and to, in this case
        // the counts have to be handled so that counts in each region still approximate
        // fragment support. Normally (the chimera case) -- a single fragment will create
        // edges and nodes with weight X. If this is a non-chimera and the nodes collide and
        // merge, we want to prevent the evidence from being doubled to 2X when it should not be.
        //
        // To achieve this, we take the max edge counts from the two nodes being merged instead
        // of the sum. This is an approximate solution, but very simple to add into the
        // graph without blowing up per-node/edge storage.
        //
        const bool isFromToEdge(fromNodeEdgeIndex == toIndex);
        unsigned mergeCount(0);
        if (isFromToEdge)
        {
            auto getNodeEdgeCount = [](const SVLocusNode& node, const NodeIndexType index) -> unsigned
            {
                if (! node.isEdge(index)) return 0u;
                return node.getEdge(index).getCount();
            };

            // determine what the override edge count should be:
            const unsigned fromCount(fromNodeEdgePtr->getCount());
            const unsigned toCount(getNodeEdgeCount(toNode,fromIndex));
            const unsigned maxCount(std::max(fromCount,toCount));
            mergeCount = getNodeEdgeCount(toNode,toIndex) + maxCount;
        }

        // update local edge:
        toNode.mergeEdge(fromNodeEdgeIndex,*(fromNodeEdgePtr));

        if (isFromToEdge)
        {
            toNode.setEdgeCount(toIndex,mergeCount);
            toNode.setEdgeCount(fromIndex,0);
        }

        // update remote inputNodeEdgeIter
        {
            SVLocusNode& remoteNode(getNode(fromNodeEdgeIndex));
            try
            {
                const SVLocusEdge remoteEdge(remoteNode.getEdge(fromIndex));
                remoteNode.mergeEdge(toIndex, remoteEdge);
            }
            catch (illumina::common::ExceptionData& e)
            {
                // decorate an in-flight exception:
                std::ostringstream oss;
                oss << "ERROR: Can't find return edge to node index: " << _index << ":" << fromIndex << " in remote node index: " << _index << ":" << fromNodeEdgeIter.first << "\n"
                    << "\tlocal_node: " << fromNode
                    << "\tremote_node: " << remoteNode;
                e << illumina::common::ExceptionMsg(oss.str());
                throw;
            }
        }
    }

#ifdef DEBUG_SVL
    log_os << logtag << " AFTER toNode: " << toNode;
#endif

    clearNodeEdges(fromIndex);
}


void
SVLocus::
getEdgeException(
    const NodeIndexType fromIndex,
    const NodeIndexType toIndex) const
{
    using namespace illumina::common;

    std::ostringstream oss;
    oss << "ERROR: SVLocus::getEdge() no edge exists\n";
    oss << "\tfrom_node: " << _index << ":" << fromIndex << " " << getNode(fromIndex);
    oss << "\tto_node: " << _index << ":" << toIndex << " " << getNode(toIndex);
    BOOST_THROW_EXCEPTION(LogicException(oss.str()));
}



bool
SVLocus::
isNoiseNode(
    const unsigned minMergeEdgeCount,
    const NodeIndexType nodeIndex) const
{
    const SVLocusNode& node(getNode(nodeIndex));
    const SVLocusEdgeManager edgeMap(node.getEdgeManager());
    for (const SVLocusEdgesType::value_type& edge : edgeMap.getMap())
    {
        if (edge.second.getCount() >= minMergeEdgeCount) return false;
        if (getEdge(edge.first,nodeIndex).getCount() >= minMergeEdgeCount) return false;
    }
    return true;
}



unsigned
SVLocus::
cleanNodeCore(
    const unsigned minMergeEdgeCount,
    const NodeIndexType nodeIndex,
    std::set<NodeIndexType>& emptyNodes)
{
#ifdef DEBUG_SVL
    static const std::string logtag("SVLocus::cleanNodeCore");
    log_os << logtag << " nodeAddy: " << _index << ":" << nodeIndex << "\n";
#endif

    unsigned totalCleaned(0);
    SVLocusNode& queryNode(getNode(nodeIndex));

    std::vector<NodeIndexType> eraseEdges;
    const SVLocusEdgeManager edgeMap(queryNode.getEdgeManager());
    for (const SVLocusEdgesType::value_type& edgeIter : edgeMap.getMap())
    {
        const SVLocusEdge* edgePtr(&(edgeIter.second));
        if (0 != edgePtr->getCount())
        {
            if (edgePtr->getCount() < minMergeEdgeCount)
            {
                // clean criteria met -- go ahead and erase edge count:
                totalCleaned += edgePtr->getCount();
                queryNode.setEdgeCount(edgeIter.first,0);

                // we've just snuck around the const iterator by calling the clearEdge function against this edge,
                // so we have to fix this by hand:
                edgePtr=&(queryNode.getEdge(edgeIter.first));
            }
        }

        if (0 == edgePtr->getCount())
        {
            // if the out edge count is zero, see if the in-edge count is also zero --
            // if so, erase edge
            //
            const SVLocusEdge& fromRemoteEdge(getEdge(edgeIter.first,nodeIndex));
            if (0 == fromRemoteEdge.getCount())
            {
                eraseEdges.push_back(edgeIter.first);

                // also check to see if the remote node will be empty after
                // this edge deletion:
                const SVLocusNode& remoteNode(getNode(edgeIter.first));
                if ((! remoteNode.isOutCount()) &&
                    (1 == remoteNode.size()))
                {
                    emptyNodes.insert(edgeIter.first);
                }
            }
        }
    }

    // delete empty edges:
    for (const NodeIndexType toIndex : eraseEdges)
    {
#ifdef DEBUG_SVL
        log_os << logtag << " deleting edge: " << _index << ":" << nodeIndex << "->" << _index << ":" << toIndex << "\n";
#endif
        eraseEdgePair(nodeIndex,toIndex);
    }

    // if true add the target node to the erase list:
    if ((queryNode.empty()) && (! queryNode.isOutCount()))
    {
        emptyNodes.insert(nodeIndex);
    }

#ifdef DEBUG_SVL
    log_os << logtag << " emptyEdges:\n";
    for (const NodeIndexType toIndex : eraseEdges)
    {
        log_os << logtag << "\tedge: " << _index << ":" << nodeIndex << "->" << _index << ":" << toIndex << "\n";
    }

    log_os << "cleanNodeCore emptyNodes\n";
    for (const NodeIndexType nodeIndex2 : emptyNodes)
    {
        log_os << logtag << "\tnodeAddy: " << _index << ":" << nodeIndex2 << "\n";
    }

    log_os << logtag << " totalCleaned: " << totalCleaned << "\n";
#endif

    return totalCleaned;
}



unsigned
SVLocus::
cleanNode(
    const unsigned minMergeEdgeCount,
    const NodeIndexType nodeIndex,
    flyweight_observer_t* obs)
{
    std::set<NodeIndexType> emptyNodes;
    const unsigned totalCleaned(cleanNodeCore(minMergeEdgeCount,nodeIndex,emptyNodes));
    eraseNodes(emptyNodes, obs);
    return totalCleaned;
}



unsigned
SVLocus::
clean(
    const unsigned minMergeEdgeCount,
    flyweight_observer_t* obs)
{
    std::set<NodeIndexType> emptyNodes;
    unsigned totalCleaned(0);

    const unsigned nodeSize(size());
    for (unsigned nodeIndex(0); nodeIndex<nodeSize; ++nodeIndex)
    {
        totalCleaned += cleanNodeCore(minMergeEdgeCount,nodeIndex,emptyNodes);
    }
    eraseNodes(emptyNodes, obs);
    return totalCleaned;
}




void
SVLocus::
clearNodeEdges(NodeIndexType nodePtr)
{
    using namespace illumina::common;

    static const std::string logtag("SVLocus::clearNodeEdges");

#ifdef DEBUG_SVL
    log_os << logtag << " from nodeIndex: " << nodePtr << "\n";
#endif

    SVLocusNode& node(getNode(nodePtr));
    const SVLocusEdgeManager edgeMap(node.getEdgeManager());
    for (const SVLocusEdgesType::value_type& edgeIter : edgeMap.getMap())
    {

#ifdef DEBUG_SVL
        log_os << logtag << " clearing remote Index: " << edgeIter.first << "\n";
#endif
        // skip self edge (otherwise we invalidate iterators in this foreach loop)
        if (edgeIter.first == nodePtr) continue;

        SVLocusNode& remoteNode(getNode(edgeIter.first));
        try
        {
            remoteNode.eraseEdge(nodePtr);
        }
        catch (illumina::common::ExceptionData& e)
        {
            std::ostringstream oss;
            oss << "ERROR: " << logtag << " no return edge on remote node.\n"
                << "\tlocal_node: " << node
                << "\tremote_node: " << remoteNode;
            e << illumina::common::ExceptionMsg(oss.str());
            throw;
        }
    }

    node.clear();
}



void
SVLocus::
eraseNode(
    const NodeIndexType nodePtr,
    flyweight_observer_t* obs)
{
    using namespace illumina::common;

    if (nodePtr >= _graph.size()) return;

    clearNodeEdges(nodePtr);

    NodeIndexType fromPtr(_graph.size()-1);

#ifdef DEBUG_SVL
    static const std::string logtag("SVLocus::eraseNode");
    log_os << logtag << " " << _index << ":" << nodePtr << " transfer_in: " << _index << ":" << fromPtr << " \n";

    log_os << logtag << " BEFORE: " << getNode(nodePtr) << "\n";
#endif

    if (fromPtr != nodePtr)
    {
#ifdef DEBUG_SVL
        log_os << logtag << " transfer_in: BEFORE: " << getNode(fromPtr) << "\n";
#endif
        // reassign fromNode's remote edges before shifting its address:
        //
        bool isHandleSelfEdge(false);
        SVLocusNode& fromNode(getNode(fromPtr));
        const SVLocusEdgeManager edgeMap(fromNode.getEdgeManager());
        for (const SVLocusEdgesType::value_type& edgeIter : edgeMap.getMap())
        {
            const bool isSelfEdge(edgeIter.first == fromPtr);

            if (isSelfEdge)
            {
                // have to handle this outside the foreach loop so that we
                // don't invalidate our iterators:
                isHandleSelfEdge=true;
                continue;
            }

            SVLocusNode& remoteNode(getNode(edgeIter.first));
            remoteNode.moveEdge(fromPtr,nodePtr);
        }

        if (isHandleSelfEdge)
        {
            fromNode.moveEdge(fromPtr,nodePtr);
        }

        notifyDelete(obs,nodePtr);
        _graph[nodePtr] = _graph[fromPtr];
        notifyAdd(obs,nodePtr);

#ifdef DEBUG_SVL
        log_os << logtag << " transfer_in: AFTER: " << getNode(nodePtr) << "\n";
#endif
    }
    notifyDelete(obs,fromPtr);
    _graph.resize(fromPtr);
}



void
SVLocus::
eraseNodes(
    const std::set<NodeIndexType>& nodes,
    flyweight_observer_t* obs)
{
    if (nodes.empty()) return;

    if (size() == nodes.size())
    {
        // if the whole locus is being erased, this is more efficient:
        clear(obs);
        return;
    }

    // partial deletion must be done in descending order:
    BOOST_REVERSE_FOREACH(const NodeIndexType nodeIndex, nodes)
    {
        eraseNode(nodeIndex, obs);
    }
}



unsigned
SVLocus::
getNodeInCount(
    const LocusIndexType nodeIndex) const
{
    const SVLocusNode& node(getNode(nodeIndex));

    unsigned sum(0);
    const SVLocusEdgeManager edgeMap(node.getEdgeManager());
    for (const SVLocusEdgesType::value_type& edgeIter : edgeMap.getMap())
    {
        sum += getEdge(edgeIter.first,nodeIndex).getCount();
    }
    return sum;
}




void
SVLocus::
dumpNode(
    std::ostream& os,
    const LocusIndexType nodeIndex) const
{
    const SVLocusNode& node(getNode(nodeIndex));
    os << "LocusNode: " << node.getInterval()
       << " n_edges: " << node.size()
       << " out_count: " << node.outCount()
       << " in_count: " << getNodeInCount(nodeIndex)
       << " evidence: " << node.getEvidenceRange()
       << "\n";

    const SVLocusEdgeManager edgeMap(node.getEdgeManager());
    for (const SVLocusEdgesType::value_type& edgeIter : edgeMap.getMap())
    {
        os << "\tEdgeTo: " << edgeIter.first
           << " out_count: " << edgeIter.second.getCount()
           << " in_count: " << getEdge(edgeIter.first,nodeIndex).getCount() << "\n";
    }
}



void
SVLocus::
findConnected(
    const NodeIndexType startIndex,
    std::set<NodeIndexType>& connected) const
{
    connected.clear();

    std::stack<NodeIndexType> nodeStack;
    nodeStack.push(startIndex);

    while (! nodeStack.empty())
    {
        connected.insert(nodeStack.top());
        const SVLocusNode& node(getNode(nodeStack.top()));
        nodeStack.pop();
        const SVLocusEdgeManager edgeMap(node.getEdgeManager());
        for (const SVLocusEdgesType::value_type& edgeIter : edgeMap.getMap())
        {
            if (! connected.count(edgeIter.first)) nodeStack.push(edgeIter.first);
        }
    }
}



void
SVLocus::
mergeSelfOverlap()
{
    const unsigned nodeSize(size());
    for (unsigned nodeIndex(0); nodeIndex<nodeSize; ++nodeIndex)
    {
        for (unsigned nodeIndex2(nodeIndex+1); nodeIndex2<nodeSize; ++nodeIndex2)
        {
            const unsigned revNodeIndex(nodeSize-(nodeIndex+1));
            const unsigned revNodeIndex2(nodeSize-(nodeIndex2+1));
            SVLocusNode& node1(getNode(revNodeIndex));
            SVLocusNode& node2(getNode(revNodeIndex2));

            // test whether 1 and 2 intersect, if they do, merge this into a self-edge node:
            if (! node2.getInterval().isIntersect(node1.getInterval())) continue;

            static flyweight_observer_t* obs(nullptr);
            mergeNode(revNodeIndex, revNodeIndex2, obs);
            eraseNode(revNodeIndex, obs);
            break;
        }
    }
}



void
SVLocus::
checkState(const bool isCheckConnected) const
{
    using namespace illumina::common;

    const unsigned nodeSize(size());
    if (0 == nodeSize) return;

    for (unsigned nodeIndex(0); nodeIndex<nodeSize; ++nodeIndex)
    {
        const SVLocusNode& node(getNode(nodeIndex));

        // check that that every edge has a return path:
        const SVLocusEdgeManager edgeMap(node.getEdgeManager());
        for (const SVLocusEdgesType::value_type& edgeIter : edgeMap.getMap())
        {
            getEdge(edgeIter.first,nodeIndex);
        }
    }

    if (! isCheckConnected) return;

    // check that every locus in the graph is connected:
    std::set<NodeIndexType> connected;
    findConnected(0,connected);

    if (nodeSize != connected.size())
    {
        std::ostringstream oss;
        oss << "ERROR: SVLocus contains unconnected components, LocusIndex: " << _index << "\n";
        BOOST_THROW_EXCEPTION(LogicException(oss.str()));
    }
}



std::ostream&
operator<<(std::ostream& os, const SVLocus& locus)
{
    os << "LOCUS BEGIN INDEX " << locus.getIndex() << "\n";
    const unsigned nodeCount(locus.size());
    for (unsigned nodeIndex(0); nodeIndex<nodeCount; ++nodeIndex)
    {
        os << "NodeIndex: " << nodeIndex << " ";
        locus.dumpNode(os,nodeIndex);
    }
    os << "LOCUS END INDEX " << locus.getIndex() << "\n";
    return os;
}
