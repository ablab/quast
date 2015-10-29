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

#include "blt_util/RegionSum.hh"
#include "blt_util/time_util.hh"
#include "htsapi/bam_header_info.hh"
#include "manta/SVBreakend.hh"
#include "svgraph/SVLocusSampleCounts.hh"
#include "options/SVLocusSetOptions.hh"
#include "svgraph/SVLocus.hh"

#include <algorithm>
#include <iosfwd>
#include <string>
#include <vector>


#ifdef DEBUG_SVL
#include <iostream>
#include "blt_util/log.hh"
#endif


/// A set of SVLocus objects comprising a full locus graph
///
/// When finalized, the SVLocusSet contains non-overlapping SVLoci
///
struct SVLocusSet : public flyweight_observer<SVLocusNodeMoveMessage>
{
    typedef std::vector<SVLocus> locusset_type;
    typedef locusset_type::const_iterator const_iterator;

    explicit
    SVLocusSet(
        const SVLocusSetOptions& opt = SVLocusSetOptions()) :
        _opt(opt),
        _inodes(*this),
        _source("UNKNOWN"),
        _isFinalized(false),
        _totalCleaned(0),
        _highestSearchCount(0),
        _highestSearchDensity(0),
        _isMaxSearchCount(false),
        _isMaxSearchDensity(false),
        _isIndexed(true)
    {}

    bool
    empty() const
    {
        return _loci.empty();
    }

    unsigned
    size() const
    {
        return _loci.size();
    }

    unsigned
    nonEmptySize() const
    {
        assert(_isIndexed);
        return size()-_emptyLoci.size();
    }

    const_iterator
    begin() const
    {
        return _loci.begin();
    }

    const_iterator
    end() const
    {
        return _loci.end();
    }

    const SVLocus&
    getLocus(const LocusIndexType index) const
    {
#ifdef DEBUG_SVL
        if (index>=_loci.size()) locusHurl(index,"const");
#endif

        assert(index<_loci.size());
        return _loci[index];
    }

    /// merge locus into this:
    ///
    /// locus is destroyed in this process
    ///
    void
    merge(const SVLocus& locus);

    /// merge locus set into this:
    ///
    /// locus set is destroyed in this process
    ///
    void
    merge(const SVLocusSet& set);

    void
    clear()
    {
        _loci.clear();
        clearIndex();
        _isFinalized=false;
        _totalCleaned=0;
        _counts.clear();
        _highestSearchCount=0;
        _highestSearchDensity=0;

        _isMaxSearchCount=false;
        _isMaxSearchDensity=false;

        _isIndexed=true;
    }

    /// indicate that the set is complete
    void
    finalize()
    {
        clean();
        _isFinalized=true;
    }

    /// remove all existing edges with less than minMergeEdgeCount support:
    void
    clean();

    void
    cleanRegion(const GenomeInterval interval);

    unsigned
    totalCleaned() const
    {
        return _totalCleaned;
    }

    // binary serialization
    void
    save(const char* filename) const;

    /// restore from serialization
    ///
    /// \param[in] isSkipIndex if true, don't build the graph index, and only allow a limited set of operations:
    ///
    void
    load(
        const char* filename,
        const bool isSkipIndex = false);

    // debug output
    void
    dump(std::ostream& os) const;

    // debug output
    void
    dumpRegion(
        std::ostream& os,
        const GenomeInterval interval);

    // dump stats on the whole SVLocus set:
    void
    dumpStats(std::ostream& os) const;

    // dump stats on each locus in tsv format:
    void
    dumpLocusStats(std::ostream& os) const;

    const std::string&
    getSource() const
    {
        return _source;
    }

    unsigned
    getMinMergeEdgeCount() const
    {
        return _opt.getMinMergeEdgeCount();
    }

    // total number of reads used as supporting evidence in the graph
    unsigned
    totalObservationCount() const
    {
        unsigned sum(0);
        for (const SVLocus& locus : *this)
        {
            sum += locus.totalObservationCount();
        }
        return sum;
    }

    // total nodes in the graph
    unsigned
    totalNodeCount() const
    {
        unsigned sum(0);
        for (const SVLocus& locus : *this)
        {
            sum += locus.size();
        }
        return sum;
    }

    /// get total number of directed edges in the graph
    unsigned
    totalEdgeCount() const
    {
        unsigned sum(0);
        for (const SVLocus& locus : *this)
        {
            sum += locus.totalEdgeCount();
        }
        return sum;
    }

    /// get total number of self-edges in the graph
    unsigned
    selfEdgeCount() const
    {
        unsigned sum(0);
        for (const SVLocus& locus : *this)
        {
            sum += locus.selfEdgeCount();
        }
        return sum;
    }

    /// fill node edge count histogram up to edgeCount.size()
    void
    getNodeEdgeCountDistro(std::vector<unsigned>& edgeCount) const
    {
        for (const SVLocus& locus : *this)
        {
            locus.getNodeEdgeCountDistro(edgeCount);
        }
    }

    /// fill node observation count histogram up to obsCount.size()
    void
    getNodeObsCountDistro(std::vector<unsigned>& obsCount) const
    {
        for (const SVLocus& locus : *this)
        {
            locus.getNodeObsCountDistro(obsCount);
        }
    }

    /// check that internal data-structures are in
    /// a consistent state, throw on error
    void
    checkState(
        const bool isCheckOverlap = false,
        const bool isCheckLocusConnected = false) const;

    /// updater gets direct access to read counts:
    AllCounts&
    getCounts()
    {
        return _counts;
    }

    const AllCounts&
    getCounts() const
    {
        return _counts;
    }

    void
    setBuildTime(
        const CpuTimes& t)
    {
        _buildTime = t;
    }

    void
    setMergeTime(
        const CpuTimes& t)
    {
        _mergeTime = t;
    }

    typedef std::pair<LocusIndexType,NodeIndexType> NodeAddressType;

    /// get all nodes in this object which intersect with
    /// an external node
    ///
    /// this is effectively const
    void
    getRegionIntersect(
        const GenomeInterval interval,
        std::set<NodeAddressType>& intersectNodes);

private:

    typedef NodeAddressType EdgeMapKeyType;
    typedef NodeIndexType EdgeMapValueType;

    typedef std::multimap<EdgeMapKeyType, EdgeMapValueType> EdgeMapType;

    typedef std::pair<EdgeMapKeyType, EdgeMapValueType> EdgeInfoType;

    struct NodeAddressSorter
    {
        NodeAddressSorter(const SVLocusSet& set) :
            _set(set)
        {}

        bool
        operator()(
            const NodeAddressType& a,
            const NodeAddressType& b) const
        {
            if (getInterval(a)<getInterval(b)) return true;
            if (getInterval(a)==getInterval(b))
            {
                return (a<b);
            }
            return false;
        }

    private:
        const GenomeInterval&
        getInterval(const NodeAddressType& n) const
        {
            return (_set.getLocus(n.first).getNode(n.second).getInterval());
        }

        const SVLocusSet& _set;
    };

    // wrap this set in an object b/c a special copy-ctor is required
    struct LocusSetIndexerType
    {
        typedef std::set<NodeAddressType, NodeAddressSorter> data_t;
        typedef data_t::iterator iterator;
        typedef data_t::const_iterator const_iterator;

        LocusSetIndexerType(const SVLocusSet& set)
            : _data(NodeAddressSorter(set))
        {}

        LocusSetIndexerType(const LocusSetIndexerType& rhs) = delete;

        LocusSetIndexerType& operator=(const LocusSetIndexerType& rhs)
        {
            if (this == &rhs) return *this;
            _data.clear();
            _data.insert(rhs._data.begin(),rhs._data.end());
            return *this;
        }

        data_t&
        data()
        {
            return _data;
        }

        const data_t&
        data() const
        {
            return _data;
        }

    private:
        data_t _data;
    };

    friend
    std::ostream&
    operator<<(std::ostream& os, const NodeAddressType& a);

    SVLocus&
    getLocus(const LocusIndexType index)
    {
#ifdef DEBUG_SVL
        if (index>=_loci.size()) locusHurl(index,"non-const");
#endif

        assert(index<_loci.size());
        return _loci[index];
    }

    void
    locusHurl(const LocusIndexType index, const char* label) const;


    const SVLocusNode&
    getNode(const NodeAddressType n) const
    {
        return getLocus(n.first).getNode(n.second);
    }

    void
    clearLocus(const LocusIndexType index)
    {
#ifdef DEBUG_SVL
        log_os << "SVLocusSet::clearLocus index: " << index << "\n";
#endif
        assert(index<_loci.size());

        _loci[index].clear(this);
        _emptyLoci.insert(index);
        _source="UNKNOWN";
    }

    /// shared node intersection utility
    ///
    /// \param[in] searchNodes the set of nodes to search for intersections in
    /// \param[in] filterLocusIndex ignore intersections from this locus
    ///
    /// \param[in] isTestUsability check whether a node intersection exceeds computablility limits
    ///
    /// \return is usable node (can only be false when isTestUsability is true)
    ///
    bool
    getNodeIntersectCore(
        const LocusIndexType inputLocusIndex,
        const NodeIndexType inputNodeIndex,
        const LocusSetIndexerType& searchNodes,
        const LocusIndexType filterLocusIndex,
        std::set<NodeAddressType>& intersectNodes,
        const bool isTestUsability = false) const;

    /// get all nodes in this object which intersect with the inputNode
    ///
    /// \param[in] isTestUsability check whether a node intersection exceeds computability limits
    ///
    /// \return is usable node (can only be false when isTestUsability is true)
    ///
    bool
    getNodeIntersect(
        const LocusIndexType locusIndex,
        const NodeIndexType nodeIndex,
        std::set<NodeAddressType>& intersectNodes,
        const bool isTestUsability = false) const
    {
        return getNodeIntersectCore(locusIndex, nodeIndex, _inodes, locusIndex, intersectNodes, isTestUsability);
    }

    /// edges returned are in local_addy->remote_node orientation
    void
    getIntersectingEdgeNodes(
        const LocusIndexType inputLocusIndex,
        const NodeIndexType inputRemoteNodeIndex,
        const EdgeMapType& remoteIntersectNodeToLocalNodeMap,
        const LocusSetIndexerType& remoteIntersectNodes,
        std::vector<EdgeInfoType>& edges) const;

    /// find nodes which could be merged with the input node, accounting for edge overlap and noise thresholds
    ///
    /// \param[in] isInputLocusMoved has the input locus been moved into the graph from an initial temporary locus?
    /// \param[out] mergeIntersect nodes which could be merged with input
    ///
    void
    getNodeMergeableIntersect(
        const LocusIndexType inputLocusIndex,
        const NodeIndexType inputNodeIndex,
        const bool isInputLocusMoved,
        std::set<NodeAddressType>& mergeIntersect) const;

    /// assign all intersect clusters to the lowest index number that is not startLocusIndex
    ///
    void
    moveIntersectToLowIndex(
        const std::set<NodeAddressType>& intersectNodes,
        const LocusIndexType startLocusIndex,
        LocusIndexType& locusIndex);

    /// combine all content from 'from' locus into 'to' locus
    ///
    /// this is typically required when a node is merged
    /// which combines two loci
    void
    combineLoci(
        const LocusIndexType fromIndex,
        const LocusIndexType toIndex,
        const bool isClearSource = true);


    /// add locus to this locusSet (intermediate step in merging)
    LocusIndexType
    insertLocus(
        const SVLocus& inputLocus);

    void
    removeNode(const NodeAddressType inputNodePtr)
    {
        assert(_isIndexed);

        LocusSetIndexerType::iterator iter(_inodes.data().find(inputNodePtr));
        if (iter == _inodes.data().end()) return;

        SVLocus& locus(getLocus(inputNodePtr.first));
        locus.eraseNode(inputNodePtr.second, this);
    }

    bool
    isNoiseNode(const NodeAddressType inputAddy) const
    {
        return getLocus(inputAddy.first).isNoiseNode(getMinMergeEdgeCount(),inputAddy.second);
    }

    /// node with a self edge only:
    bool
    isSingletonNode(const NodeAddressType inputAddy) const
    {
        const SVLocusNode& inputNode(getNode(inputAddy));
        return ((inputNode.size() == 1) && inputNode.isEdge(inputAddy.second));
    }

    bool
    isOverlapAllowed() const
    {
        return (! _isFinalized);
    }

    void
    mergeNodePtr(
        NodeAddressType fromPtr,
        NodeAddressType toPtr);

    /// update index when nodes are moved:
    void
    recieve_flyweight_notification(const SVLocusNodeMoveMessage& msg)
    {
        assert(_isIndexed);

        if (msg.first)
        {
            // add
#ifdef DEBUG_SVL
            log_os << "SVLocusSetObserver: Adding node: " << msg.second.first << ":" << msg.second.second << "\n";
#endif
            _inodes.data().insert(msg.second);
            updateMaxRegionSize(getNode(msg.second).getInterval());
        }
        else
        {
            // delete
#ifdef DEBUG_SVL
            log_os << "SVLocusSetObserver: Deleting node: " << msg.second.first << ":" << msg.second.second << "\n";
#endif
            _inodes.data().erase(msg.second);
        }
    }


    void
    updateMaxRegionSize(const GenomeInterval& interval)
    {
        assert(interval.tid>=0);
        const unsigned tid(interval.tid);
        if (tid >= _maxRegionSize.size())
        {
            _maxRegionSize.resize((tid+1),0);
        }
        _maxRegionSize[tid] = std::max(_maxRegionSize[tid], interval.range.size());
    }


    void
    reconstructIndex();

    void
    clearIndex()
    {
        _emptyLoci.clear();
        _inodes.data().clear();
        _maxRegionSize.clear();
    }

    void
    dumpIndex(std::ostream& os) const;

    /// throw an exception if any nodes are overlapping
    ///
    /// if isFilterNoise is true, consider only signal nodes
    void
    checkForOverlapNodes(
        const bool isFilterNoise) const;

    /// look for non-noise nodes intersecting the findSignalAddy node
    ///
    /// Noise nodes are checked for intersection to inputIntersectRemotes,
    /// if found isIntersectRemotes is set to true
    ///
    void
    findSignalNodes(
        const LocusIndexType inputLocusIndex,
        const NodeAddressType findSignalAddy,
        std::set<NodeAddressType>& signalIntersectNodes,
        const std::set<NodeAddressType>& inputIntersectRemotes,
        bool& isIntersectRemotes) const;

    ///////////////////// data

public:
    bam_header_info header;
private:


    struct MergeRegionSumData
    {
        void
        clear()
        {
            localNodeOutbound.clear();
            localNodeInbound.clear();
            remoteNodeOutbound.clear();
            remoteNodeInbound.clear();
        }

        // total counts for this edge:
        using rsum_t = RegionSum<unsigned>;
        rsum_t localNodeOutbound;
        rsum_t localNodeInbound;
        rsum_t remoteNodeOutbound;
        rsum_t remoteNodeInbound;
    };
    SVLocusSetOptions _opt;

    // contains the full set of loci
    locusset_type _loci;
    std::set<unsigned> _emptyLoci;

    // provides an intersection search of overlapping nodes given a bound node size:
    LocusSetIndexerType _inodes;

    // maximum region size per chromosome:
    std::vector<unsigned> _maxRegionSize;

    // simple debug string describing the source of this
    std::string _source;

    // the graph has intermediate states (during build) when overlapping regions are allowed,
    // once complete, overlaps are not present and disallowed:
    bool _isFinalized;

    AllCounts _counts;

    // total number of observations removed on edges with less than minMergeEdgeCount counts
    unsigned _totalCleaned;

    mutable unsigned _highestSearchCount; ///< highest search count observed during graph build
    mutable float _highestSearchDensity; ///< highest node density observed during graph build

    mutable bool _isMaxSearchCount; ///< has input been filtered because we hit the maximum search count
    mutable bool _isMaxSearchDensity; ///< has input been filtered because we hit the maximum node density

    bool _isIndexed;

    CpuTimes _buildTime;
    CpuTimes _mergeTime;

    mutable MergeRegionSumData _mergeRegions;
};


std::ostream&
operator<<(std::ostream& os, const SVLocusSet::NodeAddressType& a);
