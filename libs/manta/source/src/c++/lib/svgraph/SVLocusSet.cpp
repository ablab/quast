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

#include "blt_util/log.hh"
#include "blt_util/SizeDistribution.hh"
#include "common/Exceptions.hh"
#include "svgraph/SVLocusSet.hh"

#include "blt_util/thirdparty_push.h"

#include "boost/archive/binary_iarchive.hpp"
#include "boost/archive/binary_oarchive.hpp"
#include "boost/foreach.hpp"

#include "blt_util/thirdparty_pop.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>



std::ostream&
operator<<(std::ostream& os, const SVLocusSet::NodeAddressType& a)
{
    os << a.first << ":" << a.second;
    return os;
}



void
SVLocusSet::
locusHurl(const LocusIndexType index, const char* label) const
{
    using namespace illumina::common;

    std::ostringstream oss;
    oss << "ERROR: Attempting to access locus: " << index << " in locusSet with size: " << size() << " accessLabel: " << label << "\n";
    BOOST_THROW_EXCEPTION(LogicException(oss.str()));
}



/// is the node set from multiple loci?
static
bool
isMultiLocus(
    const LocusIndexType locusIndex,
    const std::set<SVLocusSet::NodeAddressType>& nodes)
{
    for (const SVLocusSet::NodeAddressType& addy : nodes)
    {
        if (addy.first != locusIndex) return true;
    }
    return false;
}



void
SVLocusSet::
merge(const SVLocus& inputLocus)
{
    //
    // test each node in the input locus for intersection to nodes in this graph and insert/join to existing nodes as appropriate
    //

    using namespace illumina::common;

    assert(! _isFinalized);

    // meaningless input indicates an error in client code:
    assert(! inputLocus.empty());

    if (inputLocus.empty()) return;

#ifdef DEBUG_SVL
    static const std::string logtag("SVLocusSet::merge");
    log_os << logtag << " inputLocus: " << inputLocus;
    checkState(true);
#endif

    inputLocus.checkState(true);

    const LocusIndexType startLocusIndex(insertLocus(inputLocus));
    const SVLocus& startLocus(_loci[startLocusIndex]);
    LocusIndexType headLocusIndex(startLocusIndex);

    // indicates if the input locus has been 'moved' into another locus in the graph:
    bool isInputLocusMoved(false);

    // indicates that the locus will not be inserted into the graph.
    // if true, skip merge and clear out the startLocus
    bool isAbortMerge(false);

    // because we have a non-general interval overlap test, we must order search
    // nodes by begin_pos on each chromosome
    //
    typedef std::map<GenomeInterval,NodeIndexType> nodeMap_t;
    nodeMap_t nodeMap;
    {
        const NodeIndexType nodeCount(startLocus.size());
        for (NodeIndexType nodeIndex(0); nodeIndex<nodeCount; ++nodeIndex)
        {
            nodeMap.insert(std::make_pair(startLocus.getNode(nodeIndex).getInterval(),nodeIndex));
        }
    }

    // reuse this intersectNodes object throughout the merge:
    std::set<NodeAddressType> intersectNodes;

    // test if the graph has grown too complex in these regions. If so, abort the insertion of this locus:
    for (const nodeMap_t::value_type& nodeVal : nodeMap)
    {
        static const bool isTestUsability(true);

        // get a standard intersection of the input node:
        const bool isUsable(getNodeIntersect(startLocusIndex, nodeVal.second, intersectNodes, isTestUsability));

        if (! isUsable)
        {
#ifdef DEBUG_SVL
            log_os << logtag << "Aborting merge\n";
#endif
            isAbortMerge=true;
            break;
        }
    }

    for (const nodeMap_t::value_type& nodeVal : nodeMap)
    {
        if (isAbortMerge) break;

        const NodeIndexType nodeIndex(nodeVal.second);

#ifdef DEBUG_SVL
        log_os << logtag << " inputNode: " << NodeAddressType(std::make_pair(startLocusIndex,nodeIndex)) << " " << startLocus.getNode(nodeIndex);
#endif

        getNodeMergeableIntersect(startLocusIndex, nodeIndex, isInputLocusMoved, intersectNodes);

#ifdef DEBUG_SVL
        log_os << logtag << " intersect_size: " << intersectNodes.size() << "\n";
        for (const NodeAddressType& val : intersectNodes)
        {
            log_os << logtag << " intersect address: " << val << " node: " <<  getNode(val) << "\n";
        }
#endif

        if (isInputLocusMoved)
        {
            if (2>intersectNodes.size()) continue;
        }
        else
        {
            if (intersectNodes.empty()) continue;
        }

        while ( isMultiLocus(headLocusIndex, intersectNodes) )
        {
            // if there are any intersections, copy the loci of all intersecting nodes into
            // a single locus, by convention we use the lowest locusIndex of the intersecting set
            moveIntersectToLowIndex(intersectNodes,startLocusIndex,headLocusIndex);
            if (! isInputLocusMoved) isInputLocusMoved=(headLocusIndex != startLocusIndex);

            getNodeMergeableIntersect(startLocusIndex, nodeIndex, isInputLocusMoved, intersectNodes);
            assert(! intersectNodes.empty());

#ifdef DEBUG_SVL
            log_os << logtag << " multilocus detected, nodes moved and re-intersected. intersect_size: " << intersectNodes.size() << "\n";
#endif
        }

#ifdef DEBUG_SVL
        log_os << logtag << " intersect2_size: " << intersectNodes.size() << "\n";
        for (const NodeAddressType& val : intersectNodes)
        {
            log_os << logtag << " intersect2 address: " << val << " node: " <<  getNode(val) << "\n";
        }
#endif

        // merge overlapping nodes in order from highest nodeid to lowest, so that the
        // merge process does not invalidate nodeids of higher value
        //
        // we first need to find a node corresponding to the input node (but possibly merged to a larger region already):
        //
        NodeAddressType inputSuperAddy;
        {
            bool isInputSuperFound(false);
            const known_pos_range2& inputRange(getLocus(startLocusIndex).getNode(nodeIndex).getInterval().range);

            for (const NodeAddressType& val : intersectNodes)
            {
                assert(val.first == headLocusIndex);

                // one node must be a superset of the input node, find this and store separately:
                if (getNode(val).getInterval().range.is_superset_of(inputRange))
                {
                    inputSuperAddy=val;
                    isInputSuperFound=true;
                    break;
                }
            }
            assert(isInputSuperFound);
        }

        // merge this inputNode with each intersecting Node,
        // and eliminate the intersecting node:
        //
        NodeAddressType mergeTargetAddy(inputSuperAddy);
        BOOST_REVERSE_FOREACH(NodeAddressType nodeAddy, intersectNodes)
        {
            if (nodeAddy == inputSuperAddy) continue;
            if (nodeAddy < mergeTargetAddy) std::swap(nodeAddy,mergeTargetAddy);
#ifdef DEBUG_SVL
            log_os << logtag << " MergeAndRemove: " << nodeAddy << "\n";
#endif
            mergeNodePtr(nodeAddy,mergeTargetAddy);
            removeNode(nodeAddy);
#ifdef DEBUG_SVL
            log_os << logtag << " Finished: " << nodeAddy << "\n";
            checkState();
#endif
        }
    }

    if (isAbortMerge || isInputLocusMoved)
    {
        clearLocus(startLocusIndex);
    }

#ifdef DEBUG_SVL
    checkState(true,true);
#endif
}



void
SVLocusSet::
merge(
    const SVLocusSet& inputSet)
{
    // TODO: check for compatible bam headers between inputSet and this

    assert(getMinMergeEdgeCount() == inputSet.getMinMergeEdgeCount());

    for (const SVLocus& locus : inputSet._loci)
    {
        if (locus.empty()) continue;

        try
        {
            merge(locus);
        }
        catch (...)
        {
            log_os << "ERROR: SVLocusSet merge failed.\n"
                   << "\tSVLocusSet source: " << inputSet.getSource() << "\n"
                   << "\tSVLocus index: " << locus.getIndex() << "\n";
            throw;
        }
    }

    _totalCleaned += inputSet._totalCleaned;
    _counts.merge(inputSet._counts);
    _highestSearchCount = std::max(_highestSearchCount, inputSet._highestSearchCount);
    _isMaxSearchCount = (_isMaxSearchCount || inputSet._isMaxSearchCount);
    _highestSearchDensity = std::max(_highestSearchDensity, inputSet._highestSearchDensity);
    _isMaxSearchDensity = (_isMaxSearchDensity || inputSet._isMaxSearchDensity);
    _buildTime.merge(inputSet._buildTime);
    _mergeTime.merge(inputSet._mergeTime); // this one is more of a formality...
}



bool
SVLocusSet::
getNodeIntersectCore(
    const LocusIndexType inputLocusIndex,
    const NodeIndexType inputNodeIndex,
    const LocusSetIndexerType& searchNodes,
    const LocusIndexType filterLocusIndex,
    std::set<NodeAddressType>& intersectNodes,
    const bool isTestUsability) const
{
    typedef LocusSetIndexerType::const_iterator in_citer;

#ifdef DEBUG_SVL
    static const std::string logtag("SVLocusSet::getNodeIntersectCore");
    log_os << logtag << " inputNode: " << inputLocusIndex << ":" << inputNodeIndex << " " << getNode(std::make_pair(inputLocusIndex,inputNodeIndex));
    checkState();
#endif

    assert(_isIndexed);

    intersectNodes.clear();

    // get all nodes \in searchNodes which intersect with the input node:
    const NodeAddressType inputAddy(std::make_pair(inputLocusIndex,inputNodeIndex));
    const in_citer it(searchNodes.data().lower_bound(inputAddy));
    const GenomeInterval& inputInterval(getNode(inputAddy).getInterval());
    const pos_t maxRegionSize(_maxRegionSize[inputInterval.tid]);

    const in_citer it_begin(searchNodes.data().begin()), it_end(searchNodes.data().end());

    // diagnostics to determine if graph is growing too dense in one region:
    bool isUsable(true);
    unsigned searchCount(0);

    // first look forward and extend to find all nodes which this inputNode intersects:
    for (in_citer it_fwd(it); it_fwd != it_end; ++it_fwd)
    {
        if (isTestUsability)
        {
            searchCount++;
            if (searchCount > _opt.maxSearchCount)
            {
                isUsable = false;
                _isMaxSearchCount=true;
                break;
            }
        }

        if (it_fwd->first == filterLocusIndex) continue;
#ifdef DEBUG_SVL
        log_os << logtag << "\tFWD test: " << (*it_fwd) << " " << getNode(*it_fwd);
#endif
        if (! inputInterval.isIntersect(getNode(*it_fwd).getInterval())) break;
        intersectNodes.insert(*it_fwd);
#ifdef DEBUG_SVL
        log_os << logtag << "\tFWD insert: " << (*it_fwd) << "\n";
#endif
    }

    // now find all intersecting nodes in reverse direction:
    for (in_citer it_rev(it); it_rev != it_begin; )
    {
        --it_rev;

        if (isTestUsability)
        {
            if (! isUsable) break;
            searchCount++;
            if (searchCount > _opt.maxSearchCount)
            {
                isUsable = false;
                _isMaxSearchCount=true;
                break;
            }
        }

        if (it_rev->first == filterLocusIndex) continue;
#ifdef DEBUG_SVL
        log_os << logtag << "\tREV test: " << (*it_rev) << " " << getNode(*it_rev);
#endif
        const GenomeInterval& searchInterval(getNode(*it_rev).getInterval());
        if (! inputInterval.isIntersect(searchInterval))
        {
            if (! isOverlapAllowed()) break;

            if (inputInterval.tid != searchInterval.tid) break;
            if ((searchInterval.range.begin_pos()+maxRegionSize)<inputInterval.range.begin_pos()) break;
            continue;
        }

        intersectNodes.insert(*it_rev);
#ifdef DEBUG_SVL
        log_os << logtag << "\tREV insert: " << (*it_rev) << "\n";
#endif
    }

    if (! isTestUsability) return true;

    _highestSearchCount = std::max(_highestSearchCount, searchCount);

    pos_t searchSize(inputInterval.range.end_pos() - std::max(0, inputInterval.range.begin_pos()-maxRegionSize));

    assert(searchSize>=0);
    if (0 != searchSize)
    {
        static const pos_t minSearchSize(40);
        searchSize = std::max(searchSize, minSearchSize);
        const float searchDensity(static_cast<float>(searchCount)/static_cast<float>(searchSize));
        _highestSearchDensity = std::max(_highestSearchDensity, searchDensity);

        if (searchDensity > _opt.maxSearchDensity)
        {
            isUsable = false;
            _isMaxSearchDensity=true;
        }
    }

    return isUsable;
}



void
SVLocusSet::
getIntersectingEdgeNodes(
    const LocusIndexType inputLocusIndex,
    const NodeIndexType inputRemoteNodeIndex,
    const EdgeMapType& remoteIntersectNodeToLocalNodeMap,
    const LocusSetIndexerType& remoteIntersectNodes,
    std::vector<EdgeInfoType>& edges) const
{
    typedef EdgeMapType::const_iterator rliter_t;
    typedef std::pair<rliter_t,rliter_t> rlmap_range_t;

    edges.clear();

    // find all nodes, from the remoteIntersectNodes set, which intersect this function's input node:
    //
    // for this application, inputLocus is an input set isolated from the rest of the graph, so nodes
    // intersected in the inputLocus are filtered out
    //
    std::set<NodeAddressType> edgeIntersectRemoteTemp;
    getNodeIntersectCore(inputLocusIndex,inputRemoteNodeIndex,remoteIntersectNodes,inputLocusIndex,edgeIntersectRemoteTemp);

    for (const NodeAddressType& remoteIsectAddy : edgeIntersectRemoteTemp)
    {
        // find what local nodes the remote nodes trace back to:
        const rlmap_range_t remoteIsectRange(remoteIntersectNodeToLocalNodeMap.equal_range(remoteIsectAddy));
        assert(remoteIsectRange.first != remoteIntersectNodeToLocalNodeMap.end());
        for (rliter_t riter(remoteIsectRange.first); riter != remoteIsectRange.second; ++riter)
        {
            const NodeAddressType localIntersectAddy(std::make_pair(remoteIsectAddy.first,riter->second));
            edges.push_back(std::make_pair(localIntersectAddy,remoteIsectAddy.second));
        }
    }
}



void
SVLocusSet::
findSignalNodes(
    const LocusIndexType inputLocusIndex,
    const NodeAddressType findSignalAddy,
    std::set<NodeAddressType>& signalIntersectNodes,
    const std::set<NodeAddressType>& inputIntersectRemotes,
    bool& isIntersectRemotes) const
{
#ifdef DEBUG_SVL
    static const std::string logtag("SVLocusSet::findSignalNodes");
    log_os << logtag << " findSignalAddy: " << findSignalAddy << "\n";
#endif
    // get a standard intersection of the input node:
    std::set<NodeAddressType> intersectNodes;
    getNodeIntersectCore(findSignalAddy.first, findSignalAddy.second, _inodes, inputLocusIndex, intersectNodes);
    for (const NodeAddressType& intersectAddy : intersectNodes)
    {
#ifdef DEBUG_SVL
        log_os << logtag << " intersectAddy: " << intersectAddy << "\n";
#endif
        if (isNoiseNode(intersectAddy))
        {
            // check for the rare remote intersect condition:
            if (! isIntersectRemotes)
            {
                if (inputIntersectRemotes.count(intersectAddy))
                {
                    isIntersectRemotes=true;
                }
            }
            continue;
        }

#ifdef DEBUG_SVL
        if (signalIntersectNodes.count(intersectAddy) == 0)
        {
            log_os << logtag << " merge/new: " << findSignalAddy << " " << intersectAddy << "\n";
        }
#endif

        signalIntersectNodes.insert(intersectAddy);
    }
}



void
SVLocusSet::
getNodeMergeableIntersect(
    const LocusIndexType inputLocusIndex,
    const NodeIndexType inputNodeIndex,
    const bool isInputLocusMoved,
    std::set<NodeAddressType>& mergeIntersectNodes) const
{
    //
    // TODO: There's room for significant optimization of these methods. The improvements are not trivial,
    //   but they would allow us to filter fewer nodes from being merged when node intersection counts become large.
    //

    //
    // There are two ways sets of mergeable nodes can occur:
    //
    // (1) There is a set of nodes which overlap with both input node and one
    // of the remote nodes that the input points to (ie they have a shared edge).
    // When totaled together, the edge count of this set + the inputNode edge
    // exceeds minMergeEdgeCount.
    //
    // (2) The input node either contains an edge which is greater than minMergeEdgeCount
    // or will contain such an edge due to (1), in this case the input node can be merged
    // with a locally overlapping node which also contains an edge which is greater than
    // minMergeEdgeCount. Note that in case (2) remote node intersection is not required.
    //

    const NodeAddressType inputAddy(std::make_pair(inputLocusIndex,inputNodeIndex));
    const SVLocusNode& inputNode(getNode(inputAddy));

#ifdef DEBUG_SVL
    static const std::string logtag("SVLocusSet::getNodeMergableIntersect");
    log_os << logtag << " inputNode: " << inputAddy << " " << inputNode;
    checkState();
#endif

    // reuse this intersectNodes as a temporary throughout the methods below
    std::set<NodeAddressType> intersectNodes;

    //
    // build a new index, which contains, for all nodes x which intersect the input, an
    // enumeration of the remote nodes Y connected by edges to node x (remoteIntersectNodes)
    // and a map for each node y \in Y pointing back to node x (remoteIntersectNodeToLocalNodeMap)
    //
    LocusSetIndexerType remoteIntersectNodes(*this);
    EdgeMapType remoteIntersectNodeToLocalNodeMap;

    // nodes which intersect the input and have already been certified as signal:
    std::set<NodeAddressType> signalIntersectNodes;
    {
        // get a standard intersection of the input node:
        getNodeIntersect(inputLocusIndex, inputNodeIndex, intersectNodes);

        //
        // 1. build the new remoteIntersectNodes/remoteIntersectNodeToLocalNodeMap index
        //
        for (const NodeAddressType& intersectAddy : intersectNodes)
        {
            const SVLocusNode& intersectNode(getNode(intersectAddy));

            // get the remotes of each node which intersect with the query node,
            // place these in remoteIntersectNodes
            const SVLocusEdgeManager edgeMap(intersectNode.getEdgeManager());
            for (const SVLocusEdgesType::value_type& intersectEdge : edgeMap.getMap())
            {
                // build remote <-> local indexing structures:
                NodeAddressType remoteAddy(std::make_pair(intersectAddy.first,intersectEdge.first));
                remoteIntersectNodes.data().insert(remoteAddy);
                remoteIntersectNodeToLocalNodeMap.insert(std::make_pair(remoteAddy,intersectAddy.second));
            }
        }

#ifdef DEBUG_SVL
        log_os << logtag << " remoteIntersectNodes.size(): " << remoteIntersectNodes.data().size() << "\n";
        for (const NodeAddressType& addy : remoteIntersectNodes.data())
        {
            log_os << logtag << "\tremoteIntersectNode: " << addy << " " << getNode(addy);
        }
#endif

        //
        // 2. get the signal node set:
        //
        // Note that the signal node search is not transitive b/c we have required all signal nodes
        // in the graph to have merged already.
        //
        for (const NodeAddressType& intersectAddy : intersectNodes)
        {
            if (! isNoiseNode(intersectAddy))
            {
                signalIntersectNodes.insert(intersectAddy);
            }
        }

#ifdef DEBUG_SVL
        log_os << logtag << " signalIntersect.size(): " << signalIntersectNodes.size() << "\n";
        for (const NodeAddressType& addy : signalIntersectNodes)
        {
            log_os << logtag << "\tsignalIntersectNode: " << addy << " " << getNode(addy);
        }
#endif
    }

    //
    // begin building the primary function output, mergeIntersectNodes, by enumerating all edges of the input node
    //
    mergeIntersectNodes.clear();

    // loop through each edge connected to the input node
    const SVLocusEdgeManager edgeMap(inputNode.getEdgeManager());
    for (const SVLocusEdgesType::value_type& inputEdge : edgeMap.getMap())
    {
#ifdef DEBUG_SVL
        log_os << logtag << " processing edge: " << inputAddy << "->" << inputLocusIndex << ":" << inputEdge.first << "\n";
        checkState();
#endif

        //
        // for each edge from the input node, get all intersecting edges
        //
        // 'intersecting edge' means that the nodes connected by the two edges each overlap
        //
        std::vector<EdgeInfoType> inputIntersectEdges;
        getIntersectingEdgeNodes(inputLocusIndex, inputEdge.first, remoteIntersectNodeToLocalNodeMap, remoteIntersectNodes, inputIntersectEdges);

        unsigned intersectCount(inputIntersectEdges.size());
        if (! isInputLocusMoved)
        {
            /// TODO: doc this adjustment, does this normalize the edge count to always include self-intersect?
            intersectCount++;
        }

        // isRegionCheck initiates a more detailed evidence signal threshold check process
        //
        // - The default process checks the total evidence summed over the entire
        // Node intersect set. This neglects to account for the possibility that that evidence
        // density could be low, and yet a high evidence sum could be achieved by transitive over
        // lap of many nodes.
        //
        // - The regioncheck pathway sums up evidence at each genomic region. It more accurately
        // reflects peak evidence but is somewhat slower to compute.
        //
        // Example:
        //
        // Assume each node below has an evidence count of 1.
        //
        // |---node1-----|
        //           |-----node2-----|
        //                        |-----node3---|
        //
        // Default evidence count:
        // 33333333333333333333333333333333333333
        //
        // isRegionCheck evidence count:
        // 11111111112222211111111222211111111111
        //
        //

        // peak RegionCheck count will always equal default count when 2 or fewer nodes exist,
        // so there's no reason to turn it on until we have more nodes
        const bool isRegionCheck(intersectCount>2);

        if (isRegionCheck)
        {
            _mergeRegions.clear();
        }

        // enumerate counts as part of the (non-RegionCheck) process to determine if the intersection set
        // contains sufficient evidence to initiate a merge
        unsigned mergedLocalEdgeCount(0);
        unsigned mergedRemoteEdgeCount(0);

        ///
        /// enumerate node evidence using either the default or RegionCheck process:
        ///
        auto addEdgeEvidenceCount = [&](
                                        const SVLocus& edgeLocus,
                                        const NodeIndexType localNodeIndex,
                                        const NodeIndexType remoteNodeIndex)
        {
            // total edge counts on the remote->local edge:
            const unsigned remoteEdgeCount = edgeLocus.getEdge(remoteNodeIndex,localNodeIndex).getCount();

            // total edge counts on the local->remote edge:
            const unsigned localEdgeCount = edgeLocus.getEdge(localNodeIndex,remoteNodeIndex).getCount();

            if (isRegionCheck)
            {
                const known_pos_range2& localRange(edgeLocus.getNode(localNodeIndex).getInterval().range);
                const known_pos_range2& remoteRange(edgeLocus.getNode(remoteNodeIndex).getInterval().range);

                _mergeRegions.localNodeOutbound.add(localRange,localEdgeCount);
                _mergeRegions.localNodeInbound.add(localRange,remoteEdgeCount);
                _mergeRegions.remoteNodeOutbound.add(remoteRange,remoteEdgeCount);
                _mergeRegions.remoteNodeInbound.add(remoteRange,localEdgeCount);
            }
            else
            {
                mergedLocalEdgeCount += localEdgeCount;
                mergedRemoteEdgeCount += remoteEdgeCount;
            }
        };

        for (const EdgeInfoType& edgeInfo : inputIntersectEdges)
        {
            addEdgeEvidenceCount(getLocus(edgeInfo.first.first),edgeInfo.first.second,edgeInfo.second);
        }

        // if the input hasn't been moved into the primary locus graph yet, then we need to include the inputLocus
        // in order to get an accurate edge intersection count:
        if (! isInputLocusMoved)
        {
            addEdgeEvidenceCount(getLocus(inputAddy.first),inputNodeIndex,inputEdge.first);
        }

        if (isRegionCheck)
        {
            mergedLocalEdgeCount=(std::min(_mergeRegions.localNodeOutbound.maxVal(),_mergeRegions.remoteNodeInbound.maxVal()));
            mergedRemoteEdgeCount=(std::min(_mergeRegions.localNodeInbound.maxVal(),_mergeRegions.remoteNodeOutbound.maxVal()));
        }

#ifdef DEBUG_SVL
        log_os << logtag << " isRegionCheck: " << isRegionCheck << "\n";
        log_os << logtag << " final merge counts"
               << " local: " << mergedLocalEdgeCount
               << " remote: " << mergedRemoteEdgeCount
               << "\n";
        checkState();
#endif

        if ((mergedLocalEdgeCount < getMinMergeEdgeCount()) &&
            (mergedRemoteEdgeCount < getMinMergeEdgeCount())) continue;

        //
        // Add type1 mergeable nodes:
        //
        for (const EdgeInfoType& edgeInfo : inputIntersectEdges)
        {
            mergeIntersectNodes.insert(edgeInfo.first);
        }

        /// for each type1 node, add any new intersections to the signal node set:
        ///
        /// this is not very efficient for now -- each type1 edge added in potentially
        /// expands the current node to intersect new signal nodes
        /// -- this loop looks for those new signal nodes
        ///

        {
            // this is used to search for the (rare) case where the intersection set
            // locals overlap with the intersection set remotes
            std::set<NodeAddressType> inputIntersectRemotes;
            for (const EdgeInfoType& edgeInfo : inputIntersectEdges)
            {
                inputIntersectRemotes.insert(std::make_pair(edgeInfo.first.first,edgeInfo.second));
            }

            bool isIntersectRemotes(false);

            // check both the original node and intersected nodes for intersection to
            // any of the group's remotes, and for new type2 signal intersect:
            findSignalNodes(inputLocusIndex, inputAddy, signalIntersectNodes, inputIntersectRemotes, isIntersectRemotes);
            for (const EdgeInfoType& edgeInfo : inputIntersectEdges)
            {
                findSignalNodes(inputLocusIndex, edgeInfo.first, signalIntersectNodes, inputIntersectRemotes, isIntersectRemotes);
            }

            if (isIntersectRemotes)
            {
                for (const NodeAddressType& intersectAddy : inputIntersectRemotes)
                {
#ifdef DEBUG_SVL
                    log_os << logtag << " adding ownRemote: " << intersectAddy << "\n";
#endif
                    mergeIntersectNodes.insert(intersectAddy);

                    // check to see if this adds even more signal nodes!
                    findSignalNodes(inputLocusIndex, intersectAddy, signalIntersectNodes, inputIntersectRemotes, isIntersectRemotes);
                }
            }
        }
        //
        // Add type2 mergeable nodes:
        //
        for (const NodeAddressType& signalAddy : signalIntersectNodes)
        {
            mergeIntersectNodes.insert(signalAddy);
        }
    }

#ifdef DEBUG_SVL
    log_os << logtag << " END. IntersectNodeSize: " << mergeIntersectNodes.size() << " Nodes:\n";
    for (const NodeAddressType addy : mergeIntersectNodes)
    {
        log_os << logtag << "\tInode: " << addy << "\n";
    }
#endif
}



void
SVLocusSet::
getRegionIntersect(
    const GenomeInterval interval,
    std::set<NodeAddressType>& intersectNodes)
{
    const LocusIndexType startLocusIndex(insertLocus(SVLocus()));
    const NodeIndexType nodeIndex(getLocus(startLocusIndex).addNode(interval, this));

    getNodeIntersect(startLocusIndex, nodeIndex, intersectNodes);

    clearLocus(startLocusIndex);
}



void
SVLocusSet::
moveIntersectToLowIndex(
    const std::set<NodeAddressType>& intersectNodes,
    const LocusIndexType startLocusIndex,
    LocusIndexType& locusIndex)
{
    const unsigned startHeadLocusIndex(locusIndex);

    // assign all intersect clusters to the lowest index number
    const bool isClearSource(startLocusIndex!=startHeadLocusIndex);

    // get lowest index number that is not startLocusIndex:
    bool isFirst(true);
    for (const NodeAddressType& val : intersectNodes)
    {
        if ((!isFirst) && (val.first >= locusIndex)) continue;
        locusIndex = val.first;
        isFirst=false;
    }

    combineLoci(startHeadLocusIndex,locusIndex,isClearSource);
    for (const NodeAddressType& val : intersectNodes)
    {
        combineLoci(val.first,locusIndex);
    }

#ifdef DEBUG_SVL
    static const std::string logtag("SVLocusSet::moveIntersectToLowIndex");
    log_os << logtag << " Reassigned all intersecting nodes to locusIndex: " << locusIndex << " startHeadLocusIndex: " << startHeadLocusIndex << " startLocusIndex:" << startLocusIndex << "\n";
    checkState();
#endif
}



void
SVLocusSet::
combineLoci(
    const LocusIndexType fromIndex,
    const LocusIndexType toIndex,
    const bool isClearSource)
{
    assert(toIndex<_loci.size());

#ifdef DEBUG_SVL
    static const std::string logtag("SVLocusSet::combineLoci");
    log_os << logtag << " from: " << fromIndex << " toIndex: " << toIndex << " isClear:" << isClearSource << "\n";
#endif

    if (fromIndex == toIndex) return;
    if (fromIndex>=_loci.size()) return;

    SVLocus& fromLocus(_loci[fromIndex]);
    if (fromLocus.empty()) return;

    SVLocus& toLocus(_loci[toIndex]);
    toLocus.copyLocus(fromLocus, this);
    if (isClearSource) clearLocus(fromIndex);
}



LocusIndexType
SVLocusSet::
insertLocus(
    const SVLocus& inputLocus)
{
    assert(_isIndexed);

    LocusIndexType locusIndex(0);
    if (_emptyLoci.empty())
    {
        static const unsigned maxIndex(std::numeric_limits<LocusIndexType>::max());
        locusIndex=_loci.size();
        assert(locusIndex<maxIndex);
        _loci.resize(locusIndex+1);
    }
    else
    {
        locusIndex=(*_emptyLoci.begin());
        assert(_loci[locusIndex].empty());
        _emptyLoci.erase(locusIndex);
    }

    SVLocus& locus(_loci[locusIndex]);
    locus.updateIndex(locusIndex);
    locus.copyLocus(inputLocus, this);
    return locusIndex;
}



void
SVLocusSet::
mergeNodePtr(NodeAddressType fromPtr,
             NodeAddressType toPtr)
{
#ifdef DEBUG_SVL
    static const std::string logtag("SVLocusSet::mergeNodePtr");
    log_os << logtag << " from: " << fromPtr << " to: " << toPtr << " fromLocusSize: " << getLocus(fromPtr.first).size() << "\n";
#endif
    assert(_isIndexed);

    LocusSetIndexerType::iterator iter(_inodes.data().find(toPtr));
    assert(iter != _inodes.data().end());
    assert(fromPtr.first == toPtr.first);
    getLocus(fromPtr.first).mergeNode(fromPtr.second, toPtr.second, this);
}



void
SVLocusSet::
clean()
{
    for (SVLocus& locus : _loci)
    {
        if (locus.empty()) continue;
        _totalCleaned += locus.clean(getMinMergeEdgeCount(), this);

        // if true, this locus is newly empty after cleaning:
        if (locus.empty()) _emptyLoci.insert(locus.getIndex());
    }
#ifdef DEBUG_SVL
    checkForOverlapNodes(true);
#endif
}



void
SVLocusSet::
cleanRegion(const GenomeInterval interval)
{
#ifdef DEBUG_SVL
    static const std::string logtag("SVLocusSet::cleanRegion");
    log_os << logtag << " interval: " << interval << "\n";
#endif

    std::set<NodeAddressType> intersectNodes;
    getRegionIntersect(interval, intersectNodes);

    // process nodes in reverse to properly handle instances when a locus has
    // multiple intersect nodes. This way we won't try to iterate into an
    // address which has been shifted by node deletion:
    BOOST_REVERSE_FOREACH(const NodeAddressType& val, intersectNodes)
    {
        SVLocus& locus(getLocus(val.first));
        if (locus.empty()) continue;
        _totalCleaned += locus.cleanNode(getMinMergeEdgeCount(), val.second, this);
        if (locus.empty()) _emptyLoci.insert(locus.getIndex());

#ifdef DEBUG_SVL
        log_os << logtag << " intersect: " << val << " is_empty_after_clean: " << locus.empty() << "\n";
#endif
    }
#ifdef DEBUG_SVL
    checkForOverlapNodes(true);
#endif
}



void
SVLocusSet::
dump(std::ostream& os) const
{
    os << "LOCUSSET_START\n";
    for (const SVLocus& locus : _loci)
    {
        os << locus;
    }
    os << "LOCUSSET_END\n";
}



void
SVLocusSet::
dumpRegion(std::ostream& os,
           const GenomeInterval interval)
{
    std::set<NodeAddressType> intersectNodes;
    getRegionIntersect(interval,intersectNodes);

    LocusSetIndexerType sortedNodes(*this);
    for (const NodeAddressType& val : intersectNodes)
    {
        sortedNodes.data().insert(val);
    }

    for (const NodeAddressType& val : sortedNodes.data())
    {
        os << "SVNode LocusIndex:NodeIndex : " << val << "\n";
        os << getNode(val);
    }
}




void
SVLocusSet::
dumpStats(
    std::ostream& os) const
{
    static const char sep('\t');

    os << "GraphBuildTime" << sep;
    _buildTime.reportHr(os);
    os << "\n";
    os << "GraphMergeTime" << sep;
    _mergeTime.reportHr(os);
    os << "\n";
    os << "disjointSubgraphs" << sep << nonEmptySize() << "\n";
    os << "nodes" << sep << totalNodeCount() << "\n";
    os << "directedEdges" << sep << totalEdgeCount() << "\n";
    os << "selfEdges" << sep << selfEdgeCount() << "\n";
    os << "totalGraphEvidence" << sep << totalObservationCount() << "\n";
    os << "totalCleaned" << sep << _totalCleaned << "\n";
    os << "highestSearchCount" << sep << _highestSearchCount << "\n";
    os << "isMaxSearchCount" << sep << _isMaxSearchCount << "\n";
    os << "highestSearchDensity" << sep << _highestSearchDensity << "\n";
    os << "isMaxSearchDensity" << sep << _isMaxSearchDensity << "\n";

    /// TODO: Add real sample labels:
    {
        std::vector<std::string> labels;
        const unsigned csize(_counts.size());
        for (unsigned i(0); i<csize; ++i)
        {
            std::ostringstream oss;
            oss << "Sample" << i;
            labels.push_back(oss.str());
        }
        _counts.write(os,labels);
    }
    os << "\n";

    // node region size quantiles
    {
        SizeDistribution nodeSize;
        for (const SVLocus& locus : *this)
        {
            for (const SVLocusNode& node : locus)
            {
                const unsigned regionSize(node.getInterval().range.size());
                nodeSize.addObservation(regionSize);
            }
        }

        static const float quantLevel[] = { 0.25f, 0.5f, 0.75f, 0.9f, 0.95f, 0.99f };
        static const unsigned quantLevelCount(sizeof(quantLevel)/sizeof(float));
        os << "NodeRegionSizequantile:\n";
        for (unsigned i(0); i<quantLevelCount; ++i)
        {
            os << quantLevel[i] << sep
               << nodeSize.quantile(quantLevel[i]) << "\n";
        }
    }

    {
        // node edge count distro: 0.1,2,3... X+
        static const unsigned maxEdgeCount(10);
        std::vector<unsigned> edgeCount(maxEdgeCount);
        getNodeEdgeCountDistro(edgeCount);
        os << "NodeEdgeCount:\n";
        for (unsigned i(0); i<maxEdgeCount; ++i)
        {
            os << i;
            if ((i+1) == maxEdgeCount) os << '+';
            os << sep << edgeCount[i] << "\n";
        }
    }

    {
        // node obs distro: 0,1,2,3... X+
        static const unsigned maxObsCount(30);
        std::vector<unsigned> obsCount(maxObsCount);
        getNodeObsCountDistro(obsCount);
        os << "NodeObservationCount:\n";
        for (unsigned i(0); i<maxObsCount; ++i)
        {
            os << i;
            if ((i+1) == maxObsCount) os << '+';
            os << sep << obsCount[i] << "\n";
        }
    }
}


void
SVLocusSet::
dumpLocusStats(std::ostream& os) const
{
    static const char sep('\t');

    os << "locusIndex"
       << sep << "nodeCount"
       << sep << "nodeObsCount"
       << sep << "maxNodeObsCount"
       << sep << "regionSize"
       << sep << "maxRegionSize"
       << sep << "edgeCount"
       << sep << "maxEdgeCount"
       << sep << "edgeObsCount"
       << sep << "maxEdgeObsCount"
       << '\n';

    LocusIndexType locusIndex(0);
    for (const SVLocus& locus : _loci)
    {
        unsigned locusNodeObsCount(0), maxNodeObsCount(0);
        unsigned locusRegionSize(0), maxRegionSize(0);
        unsigned locusEdgeCount(0), maxEdgeCount(0), locusEdgeObsCount(0), maxEdgeObsCount(0);
        for (const SVLocusNode& node : locus)
        {
            // nodes:
            const unsigned nodeObsCount(node.outCount());
            maxNodeObsCount = std::max(maxNodeObsCount,nodeObsCount);
            locusNodeObsCount += nodeObsCount;

            // regions:
            const unsigned regionSize(node.getInterval().range.size());
            maxRegionSize = std::max(maxRegionSize,regionSize);
            locusRegionSize += regionSize;

            // edges:
            maxEdgeCount = std::max(maxEdgeCount,node.size());
            locusEdgeCount += node.size();
            const SVLocusEdgeManager edgeMap(node.getEdgeManager());
            for (const SVLocusEdgesType::value_type& edge : edgeMap.getMap())
            {
                const unsigned edgeObsCount(edge.second.getCount());
                maxEdgeObsCount = std::max(maxEdgeObsCount,edgeObsCount);
                locusEdgeObsCount += edgeObsCount;
            }
        }
        os << locusIndex
           << sep << locus.size()
           << sep << locusNodeObsCount
           << sep << maxNodeObsCount
           << sep << locusRegionSize
           << sep << maxRegionSize
           << sep << locusEdgeCount
           << sep << maxEdgeCount
           << sep << locusEdgeObsCount
           << sep << maxEdgeObsCount
           << "\n";
        locusIndex++;
    }
}



void
SVLocusSet::
save(const char* filename) const
{
    using namespace boost::archive;

    assert(NULL != filename);
    std::ofstream ofs(filename, std::ios::binary);
    binary_oarchive oa(ofs);

    oa << header;
    oa << _opt;
    oa << _isFinalized;
    oa << _totalCleaned;
    oa << _counts;
    oa << _highestSearchCount;
    oa << _highestSearchDensity;
    oa << _isMaxSearchCount;
    oa << _isMaxSearchDensity;
    oa << _buildTime;
    oa << _mergeTime;

    for (const SVLocus& locus : _loci)
    {
        if (locus.empty()) continue;
        oa << locus;
    }
}



void
SVLocusSet::
load(
    const char* filename,
    const bool isSkipIndex)
{
    using namespace boost::archive;

#ifdef DEBUG_SVL
    log_os << "SVLocusSet::load BEGIN\n";
#endif

    clear();

    assert(nullptr != filename);
    std::ifstream ifs(filename, std::ios::binary);
    binary_iarchive ia(ifs);

    _source=filename;

    ia >> header;
    ia >> _opt;
    ia >> _isFinalized;
    ia >> _totalCleaned;
    ia >> _counts;
    ia >> _highestSearchCount;
    ia >> _highestSearchDensity;
    ia >> _isMaxSearchCount;
    ia >> _isMaxSearchDensity;
    ia >> _buildTime;
    ia >> _mergeTime;

    SVLocus locus;
    while (ifs.peek() != EOF)
    {
        locus.clear(this);
        ia >> locus;
        if (locus.empty()) continue;
        const LocusIndexType locusIndex(size());
        _loci.push_back(locus);
        SVLocus& locusCopy(_loci.back());
        locusCopy.updateIndex(locusIndex);
    }

    if (! isSkipIndex)
    {
        reconstructIndex();
        checkState(true,true);
    }
    else
    {
        _isIndexed = false;
    }

#ifdef DEBUG_SVL
    log_os << "SVLocusSet::load END\n";
#endif
}



void
SVLocusSet::
reconstructIndex()
{
#ifdef DEBUG_SVL
    log_os << "reconstructIndex BEGIN\n";
#endif
    clearIndex();

#ifdef DEBUG_SVL
    log_os << "reconstructIndex cleared\n";
#endif

    LocusIndexType locusIndex(0);
    for (SVLocus& locus : _loci)
    {
        const unsigned nodeCount(locus.size());
        for (NodeIndexType nodeIndex(0); nodeIndex<nodeCount; ++nodeIndex)
        {
            const NodeAddressType addy(std::make_pair(locusIndex,nodeIndex));
            _inodes.data().insert(addy);
            updateMaxRegionSize(getNode(addy).getInterval());
        }
        if (locus.empty()) _emptyLoci.insert(locusIndex);
        locusIndex++;
    }

    _isIndexed=true;

#ifdef DEBUG_SVL
    log_os << "reconstructIndex END\n";
#endif
}



void
SVLocusSet::
dumpIndex(std::ostream& os) const
{
    assert(_isIndexed);

    os << "SVLocusSet Index START\n";
    for (const NodeAddressType& in : _inodes.data())
    {
        os << "SVNodeIndex: " << in << "\n";
    }
    os << "SVLocusSet Index END\n";
}



void
SVLocusSet::
checkState(
    const bool isCheckOverlap,
    const bool isCheckLocusConnected) const
{
    using namespace illumina::common;

    assert(_isIndexed);

    unsigned locusIndex(0);
    unsigned checkStateTotalNodeCount(0);
    for (const SVLocus& locus : _loci)
    {
        locus.checkState(isCheckLocusConnected);

        const unsigned nodeCount(locus.size());
        checkStateTotalNodeCount += nodeCount;

        if (nodeCount == 0)
        {
            if (_emptyLoci.count(locusIndex) == 0)
            {
                std::ostringstream oss;
                oss << "ERROR: empty locus is not updated in the empty index. Locus index: " << locusIndex << "\n";
                BOOST_THROW_EXCEPTION(LogicException(oss.str()));
            }
        }

        for (NodeIndexType nodeIndex(0); nodeIndex<nodeCount; ++nodeIndex)
        {
            LocusSetIndexerType::const_iterator citer(_inodes.data().find(std::make_pair(locusIndex,nodeIndex)));
            if (citer == _inodes.data().end())
            {
                std::ostringstream oss;
                oss << "ERROR: locus node is missing from node index\n"
                    << "\tNode index: " << locusIndex << " node: " << getNode(std::make_pair(locusIndex,nodeIndex));
                BOOST_THROW_EXCEPTION(LogicException(oss.str()));
            }
            if ((citer->first != locusIndex) || (citer->second != nodeIndex))
            {
                std::ostringstream oss;
                oss << "ERROR: locus node has conflicting index number in node index\n"
                    << "\tinode index_value: " << citer->first << ":" << citer->second << "\n"
                    << "\tNode index: " << locusIndex << ":" << locusIndex << " node: " << getNode(std::make_pair(locusIndex,nodeIndex));
                BOOST_THROW_EXCEPTION(LogicException(oss.str()));
            }
        }
        locusIndex++;
    }

    if (checkStateTotalNodeCount != _inodes.data().size())
    {
        using namespace illumina::common;
        std::ostringstream oss;
        oss << "ERROR: SVLocusSet conflicting internal node counts. TotalNodeCount: " << checkStateTotalNodeCount << " inodeSize: " << _inodes.data().size() << "n";
        BOOST_THROW_EXCEPTION(LogicException(oss.str()));
    }

    if (! isCheckOverlap) return;

    // if isOverlapAllowed() then we should expect noise nodes to overlap, but we can still check signal nodes:
    const bool isFilterNoise(isOverlapAllowed());
    checkForOverlapNodes(isFilterNoise);
}



#if 0
void
SVLocusSet::
compressSingletonNodes() const
{
    using namespace illumina::common;

    bool isFirst(true);
    GenomeInterval lastInterval;
    NodeAddressType lastAddy;
    for (const NodeAddressType& addy : _inodes.data())
    {
        if (isNoiseNode(addy)) continue;

        if (! isSingletonNode(addy)) continue;

        const GenomeInterval& interval(getNode(addy).getInterval());

        // don't allow zero-length or negative intervals:
        assert(interval.range.begin_pos() < interval.range.end_pos());

        // compress nearby singleton nodes into one:
        if (isFirst)
        {
            isFirst=false;
        }
        else if (interval.tid == lastInterval.tid)
        {
            if (lastInterval.range.end_pos() > interval.range.begin_pos())
            {
                std::ostringstream oss;
                oss << "ERROR: Overlapping nodes in graph\n"
                    << "\tlast_index: " << lastAddy << " interval: " << lastInterval << "\n"
                    << "\tthis_index: " << addy << " interval: " << interval << "\n"
                    << "\tlast_node: " << lastAddy << " "<< getNode(lastAddy) << "\n"
                    << "\tthis_node: " << addy << " "<< getNode(addy) << "\n"
                    << "\n"
                    << header << "\n";
                BOOST_THROW_EXCEPTION(LogicException(oss.str()));
            }
        }
        lastAddy = addy;
        lastInterval = interval;
    }
}
#endif



void
SVLocusSet::
checkForOverlapNodes(
    const bool isFilterNoise) const
{
    using namespace illumina::common;

    bool isFirst(true);
    GenomeInterval lastInterval;
    NodeAddressType lastAddy;
    for (const NodeAddressType& addy : _inodes.data())
    {
        if (isFilterNoise)
        {
            if (isNoiseNode(addy)) continue;
        }

        const GenomeInterval& interval(getNode(addy).getInterval());

        // don't allow zero-length or negative intervals:
        assert(interval.range.begin_pos() < interval.range.end_pos());

        // don't allow overlapping intervals:
        if (isFirst)
        {
            isFirst=false;
        }
        else if (interval.tid == lastInterval.tid)
        {
            if (lastInterval.range.end_pos() > interval.range.begin_pos())
            {
                std::ostringstream oss;
                oss << "ERROR: Overlapping nodes in graph\n"
                    << "\tlast_index: " << lastAddy << " interval: " << lastInterval << "\n"
                    << "\tthis_index: " << addy << " interval: " << interval << "\n"
                    << "\tlast_node: " << lastAddy << " "<< getNode(lastAddy) << "\n"
                    << "\tthis_node: " << addy << " "<< getNode(addy) << "\n"
                    << "\n"
                    << header << "\n";
                BOOST_THROW_EXCEPTION(LogicException(oss.str()));
            }
        }
        lastAddy = addy;
        lastInterval = interval;
    }
}

