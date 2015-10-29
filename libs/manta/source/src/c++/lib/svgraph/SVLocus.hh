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

#include "blt_util/flyweight_observer.hh"
#include "svgraph/SVLocusNode.hh"


typedef unsigned LocusIndexType;

/// move message is composed of a bool, indicating if the node is being added (true) or deleted (false) from the index,
/// and the id of the node itself.
///
typedef std::pair<bool, std::pair<LocusIndexType,NodeIndexType> > SVLocusNodeMoveMessage;



struct SVLocusSet;



/// \brief a set of regions containing dependent SV evidence
///
/// An SV locus is a region set hypothetically containing the breakends of 1 to many
/// SVs.
///
/// The locus is composed of a set of non-overlapping contiguous genomic regions and
/// the links (edges) between them. Each link has an associated evidence count.
///
/// This class internally manages the node shared pointers in a synced data structure,
/// there's probably a better way to do this with transform_iterator, but I've always
/// regretted using that.
///
struct SVLocus : public flyweight_notifier<SVLocusNodeMoveMessage>
{
    typedef std::vector<SVLocusNode> graph_type;

    typedef graph_type::iterator iterator;
    typedef graph_type::const_iterator const_iterator;

    friend struct SVLocusSet;

    bool
    empty() const
    {
        return _graph.empty();
    }

    unsigned
    size() const
    {
        return _graph.size();
    }

    const_iterator
    begin() const
    {
        return _graph.begin();
    }

    const_iterator
    end() const
    {
        return _graph.end();
    }

    LocusIndexType
    getIndex() const
    {
        return _index;
    }

    const SVLocusNode&
    getNode(const NodeIndexType nodePtr) const
    {
#ifdef DEBUG_SVL
        if (nodePtr>=_graph.size()) nodeHurl(nodePtr);
#endif
        assert(nodePtr<_graph.size());
        return _graph[nodePtr];
    }

    NodeIndexType
    addNode(
        const GenomeInterval interval,
        flyweight_observer_t* obs = NULL)
    {
        assert(interval.tid >= 0);

        NodeIndexType nodePtr(newGraphNode());
        SVLocusNode& node(getNode(nodePtr));
        node.setInterval(interval);
        // default _evidenceRange to the breakend interval unless a better estimate is provided
        node.setEvidenceRange(interval.range);
        notifyAdd(obs, nodePtr);
        return nodePtr;
    }

    // an edge count is only added on on from->to
    //
    void
    linkNodes(
        const NodeIndexType fromIndex,
        const NodeIndexType toIndex,
        const unsigned fromCount = 1,
        const unsigned toCount = 0)
    {
        SVLocusNode& fromNode(getNode(fromIndex));
        SVLocusNode& toNode(getNode(toIndex));
        assert(! fromNode.isEdge(toIndex));
        assert(! toNode.isEdge(fromIndex));

        SVLocusEdge fromEdge;
        fromEdge.setCount(fromCount);
        SVLocusEdge toEdge;
        toEdge.setCount(toCount);
        fromNode.mergeEdge(toIndex,fromEdge);
        toNode.mergeEdge(fromIndex,toEdge);
    }

    void
    setNodeEvidence(
        const NodeIndexType nodeIndex,
        const known_pos_range2& evidenceRange)
    {
        getNode(nodeIndex).setEvidenceRange(evidenceRange);
    }

    /// find all node indices connected to startIndex
    ///
    /// non-recursive version
    void
    findConnected(
        const NodeIndexType startIndex,
        std::set<NodeIndexType>& connected) const;

    /// the total observations in all nodes of the locus
    unsigned
    totalObservationCount() const
    {
        unsigned sum(0);
        for (const SVLocusNode& node : *this)
        {
            sum += node.outCount();
        }
        return sum;
    }

    // the total number of edges between all nodes of the locus
    unsigned
    totalEdgeCount() const
    {
        unsigned sum(0);
        for (const SVLocusNode& node : *this)
        {
            sum += node.size();
        }
        return sum;
    }

    // the total number of self edges in the locus
    unsigned
    selfEdgeCount() const
    {
        unsigned sum(0);
        const unsigned nodeSize(size());
        for (unsigned nodeIndex(0); nodeIndex<nodeSize; ++nodeIndex)
        {
            if (getNode(nodeIndex).isEdge(nodeIndex)) sum++;
        }
        return sum;
    }

    /// fill edge count histogram up to edgeCount.size()
    void
    getNodeEdgeCountDistro(std::vector<unsigned>& edgeCount) const
    {
        if (edgeCount.empty()) return;
        const unsigned maxEdge(edgeCount.size()-1);
        for (const SVLocusNode& node : *this)
        {
            edgeCount[std::min(node.size(),maxEdge)]++;
        }
    }

    /// fill obs count histogram up to obsCount.size()
    void
    getNodeObsCountDistro(std::vector<unsigned>& obsCount) const
    {
        if (obsCount.empty()) return;
        const unsigned maxObs(obsCount.size()-1);
        for (const SVLocusNode& node : *this)
        {
            obsCount[std::min(node.outCount(),maxObs)]++;
        }
    }

    /// return from->to edge
    const SVLocusEdge&
    getEdge(
        const NodeIndexType fromIndex,
        const NodeIndexType toIndex) const
    {
        const SVLocusNode& fromNode(getNode(fromIndex));
        try
        {
            return fromNode.getEdge(toIndex);
        }
        catch (...)
        {
            // throw a richer exception message than node can produce on its own:
            getEdgeException(fromIndex,toIndex);
        }

        // handle compiler warning for return val, this code should never run:
        static SVLocusEdge bogusWarning;
        return bogusWarning;
    }

    void
    clear(
        flyweight_observer_t* obs)
    {
        for (NodeIndexType i(0); i<size(); ++i)
        {
            notifyDelete(obs, i);
        }
        _graph.clear();
    }

    // find any self-overlapping nodes within the locus and merge
    void
    mergeSelfOverlap();

    /// debug func to check that internal data-structures are in
    /// a consistent state
    void
    checkState(const bool isCheckConnected = false) const;

    // total the evidence count of all in-edges to this node
    unsigned
    getNodeInCount(const LocusIndexType nodeIndex) const;

    void
    clearNodeEdges(const NodeIndexType nodePtr);

    // a fancier version of the SVLocusNode dumper which can
    // report in-edge information
    void
    dumpNode(
        std::ostream& os,
        const LocusIndexType nodeIndex) const;

    template<class Archive>
    void save(Archive& ar, const unsigned /* version */) const
    {
        ar << _graph;
    }

    template<class Archive>
    void load(Archive& ar, const unsigned /* version */)
    {
        clear(nullptr);
        ar >> _graph;
    }

    BOOST_SERIALIZATION_SPLIT_MEMBER()

private:

    SVLocusNode&
    getNode(const NodeIndexType nodePtr)
    {
#ifdef DEBUG_SVL
        if (nodePtr>=_graph.size()) nodeHurl(nodePtr);
#endif
        assert(nodePtr<_graph.size());
        return _graph[nodePtr];
    }

    void
    nodeHurl(const NodeIndexType nodePtr) const;

    void
    updateIndex(const LocusIndexType& index)
    {
        _index=index;
    }

    // return true if node contains no out or in edges greater than
    // minMergeEdgeCount
    bool
    isNoiseNode(
        const unsigned minMergeEdgeCount,
        const NodeIndexType nodeIndex) const;

    /// remove all unmerged noise in-edges of node and
    /// provide list of nodes which should be deleted
    ///
    /// return amount of evidence cleaned
    unsigned
    cleanNodeCore(
        const unsigned minMergeEdgeCount,
        const NodeIndexType nodeIndex,
        std::set<NodeIndexType>& emptyNodes);

    /// remove all unmerged noise in-edges of node and possibly
    /// delete empty nodes
    ///
    /// return amount of evidence cleaned
    unsigned
    cleanNode(
        const unsigned minMergeEdgeCount,
        const NodeIndexType nodeIndex,
        flyweight_observer_t* obs);

    /// remove all unmerged noise edges and nodes
    ///
    /// return amount of evidence cleaned
    unsigned
    clean(
        const unsigned minMergeEdgeCount,
        flyweight_observer_t* obs);

    void
    getEdgeException(
        const NodeIndexType fromIndex,
        const NodeIndexType toIndex) const;

    /// erase edges in both directions:
    void
    eraseEdgePair(
        const NodeIndexType index1,
        const NodeIndexType index2)
    {
        eraseEdge(index1,index2);
        if (index1 == index2) return;
        eraseEdge(index2,index1);
    }

    /// erase edge in one direction
    void
    eraseEdge(
        const NodeIndexType fromIndex,
        const NodeIndexType toIndex)
    {
        getNode(fromIndex).eraseEdge(toIndex);
    }

    /// copy fromLocus into this locus (this should be an intermediate part of a locus merge)
    void
    copyLocus(
        const SVLocus& fromLocus,
        flyweight_observer_t* obs)
    {
        assert(&fromLocus != this);

        // simple method -- copy everything in with an offset in all index numbers:
        const unsigned offset(_graph.size());
        for (const SVLocusNode& fromNode : fromLocus)
        {
            const NodeIndexType nodeIndex(newGraphNode());
            getNode(nodeIndex) = SVLocusNode(fromNode, offset);
            notifyAdd(obs, nodeIndex);
        }
    }

    /// join from node into to node
    ///
    /// from node is effectively destroyed,
    //// because all incoming edges will be updated
    ///
    void
    mergeNode(
        const NodeIndexType fromIndex,
        const NodeIndexType toIndex,
        flyweight_observer_t* obs);

    // remove node
    //
    void
    eraseNode(
        const NodeIndexType nodePtr,
        flyweight_observer_t* obs);

    // remove a list of node ids
    void
    eraseNodes(
        const std::set<NodeIndexType>& nodes,
        flyweight_observer_t* obs);

    NodeIndexType
    newGraphNode()
    {
        static const unsigned maxIndex(std::numeric_limits<NodeIndexType>::max());
        unsigned index(_graph.size());
        assert(index<maxIndex);
        _graph.resize(index+1);
        return static_cast<NodeIndexType>(index);
    }

    void
    notifyAdd(
        flyweight_observer_t* obs,
        const NodeIndexType nodePtr)
    {
        if (NULL == obs) return;
#ifdef DEBUG_SVL
        log_os << "SVLocusNotifier: Add node: " << _index << ":" << nodePtr << "\n";
#endif
        notify_flyweight_observer(obs, std::make_pair(true,std::make_pair(_index, nodePtr)));
    }

    void
    notifyDelete(
        flyweight_observer_t* obs,
        const NodeIndexType nodePtr)
    {
        if (NULL == obs) return;
#ifdef DEBUG_SVL
        log_os << "SVLocusNotifier: Delete node: " << _index << ":" << nodePtr << "\n";
#endif
        notify_flyweight_observer(obs, std::make_pair(false,std::make_pair(_index, nodePtr)));
    }

    graph_type _graph;
    LocusIndexType _index = 0;
};


std::ostream&
operator<<(std::ostream& os, const SVLocus& locus);

BOOST_CLASS_IMPLEMENTATION(SVLocus, boost::serialization::object_serializable)
