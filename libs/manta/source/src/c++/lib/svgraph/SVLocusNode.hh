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

#include "svgraph/GenomeInterval.hh"

#include "blt_util/thirdparty_push.h"

#include "boost/serialization/map.hpp"
#include "boost/serialization/vector.hpp"
#include "boost/serialization/split_member.hpp"

#include "blt_util/thirdparty_pop.h"

#include <iosfwd>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <vector>


//#define DEBUG_SVL


#ifdef DEBUG_SVL
#include "blt_util/log.hh"

#include <iostream>
#endif


struct SVLocusNode;


// no constructor so that this can be used in a union:
struct SVLocusEdge
{
    unsigned
    getCount() const
    {
        return _count;
    }

    bool
    isCountExact() const
    {
        return (getCount() != maxCount());
    }

    template<class Archive>
    void serialize(Archive& ar, const unsigned /* version */)
    {
        ar& _count;
    }

    void
    setCount(const unsigned count)
    {
        clearCount();
        addCount(count);
    }

private:
    typedef unsigned count_t;

    friend struct SVLocusNode;

    // merge edge into this one
    //
    void
    mergeEdge(const SVLocusEdge& edge)
    {
        addCount(edge.getCount());
    }

    void
    addCount(const unsigned increment)
    {
        if ((getCount()+increment)>maxCount())
        {
            _count = maxCount();
        }
        else
        {
            _count += increment;
        }
    }

    void
    clearCount()
    {
        _count = 0;
    }

    static
    unsigned
    maxCount()
    {
        return std::numeric_limits<count_t>::max();
    }

    count_t _count;
};


std::ostream&
operator<<(std::ostream& os, const SVLocusEdge& edge);

BOOST_CLASS_IMPLEMENTATION(SVLocusEdge, boost::serialization::object_serializable)


typedef unsigned NodeIndexType;


/// TODO: get SVLocusNode to switch between real and fake maps transparently using some fancy iterator:
///
#if 0
class customConstEdgeIterator
    : public boost::iterator_adaptor<
      customConstEdgeIterator            // Derived
    , Finite_vertices_iterator      // Base
    , Vertex_handle                 // Value
    , boost::forward_traversal_tag  // Traversal type
    , Vertex_handle>                // Reference
{
private:
    struct enabler {};

public:
    my_vertex_iterator()
        : my_vertex_iterator::iterator_adaptor_(0) {}

    explicit my_vertex_iterator(const Finite_vertices_iterator p)
        : my_vertex_iterator::iterator_adaptor_(p) {}

private:
    friend class boost::iterator_core_access;
    typename my_vertex_iterator::reference
    dereference() const
    {
        return this->base();
    }
};
#endif


typedef std::map<NodeIndexType,SVLocusEdge> SVLocusEdgesType;


/// used for an alternate compact representation of a node with zero or one edges
struct SVLocusEdgeSingle
{
    template<class Archive>
    void serialize(Archive& ar,const unsigned /* version */)
    {
        ar& index& edge& isZero;
    }


    NodeIndexType index;
    SVLocusEdge edge;
    bool isZero;
};


/// The edge manager enables iterator over the two forms of edges stored as a union
/// TODO: hide the union behind an actual iteraror class
struct SVLocusEdgeManager
{
    SVLocusEdgeManager(const SVLocusEdgeSingle& edge) :
        mapPtr(&(staticMap))
    {
        if (! edge.isZero)
        {
            sharedMapPtr.reset(new SVLocusEdgesType);
            sharedMapPtr->insert(std::make_pair(edge.index,edge.edge));
            mapPtr=sharedMapPtr.get();
        }
    }

    SVLocusEdgeManager(const SVLocusEdgesType& edgeMap) :
        mapPtr(&edgeMap)
    {}

    const SVLocusEdgesType&
    getMap() const
    {
        return *mapPtr;
    }

private:
    const SVLocusEdgesType* mapPtr;
    std::shared_ptr<SVLocusEdgesType> sharedMapPtr;

    static const SVLocusEdgesType staticMap;
};


struct SVLocusNode
{
    typedef SVLocusEdgesType::const_iterator const_iterator;

    SVLocusNode() :
        _isSingle(true)
    {
        _edges.single.isZero=true;
    }

    // specialized copy ctor which offsets all address:
    SVLocusNode(
        const SVLocusNode& in,
        const unsigned offset) :
        _interval(in._interval),
        _evidenceRange(in._evidenceRange),
        _isSingle(in._isSingle)
    {
        if (_isSingle)
        {
            _edges.single = in._edges.single;
            if (! _edges.single.isZero)
            {
                _edges.single.index += offset;
            }
        }
        else
        {
            _edges.multiPtr = new SVLocusEdgesType;
            for (const SVLocusEdgesType::value_type& val : in.getMap())
            {
                getMap().insert(std::make_pair(val.first+offset, val.second));
            }
        }
    }

    SVLocusNode(const SVLocusNode& rhs) :
        _interval(rhs._interval),
        _evidenceRange(rhs._evidenceRange),
        _isSingle(rhs._isSingle)
    {
        if (_isSingle)
        {
            _edges.single = rhs._edges.single;
        }
        else
        {
            _edges.multiPtr = new SVLocusEdgesType;
            getMap() = rhs.getMap();
        }
    }

    ~SVLocusNode()
    {
        if (! _isSingle) delete _edges.multiPtr;
    }

    SVLocusNode&
    operator=(const SVLocusNode& rhs)
    {
        if (&rhs==this) return *this;

        clear();

        _interval = rhs._interval;
        _evidenceRange = rhs._evidenceRange;
        _isSingle = rhs._isSingle;

        if (_isSingle)
        {
            _edges.single = rhs._edges.single;
        }
        else
        {
            _edges.multiPtr = new SVLocusEdgesType;
            getMap() = rhs.getMap();
        }
        return *this;
    }


    bool
    empty() const
    {
        return (_isSingle && (_edges.single.isZero));
    }

    unsigned
    size() const
    {
        if (_isSingle)
        {
            return (_edges.single.isZero ? 0u : 1u );
        }
        else
        {
            return getMap().size();
        }
    }

    SVLocusEdgeManager
    getEdgeManager() const
    {
        if (_isSingle)
        {
            return SVLocusEdgeManager(_edges.single);
        }
        else
        {
            return SVLocusEdgeManager(getMap());
        }
    }

    bool
    isOutCount() const
    {
        if (empty()) return false;
        if (_isSingle)
        {
            return (0 != _edges.single.edge.getCount());
        }
        else
        {
            for (const SVLocusEdgesType::value_type& edgeIter : getMap())
            {
                if (edgeIter.second.getCount() > 0) return true;
            }
            return false;
        }
    }

    unsigned
    outCount() const
    {
        if (empty()) return 0;
        if (_isSingle)
        {
            return (_edges.single.edge.getCount());
        }
        else
        {
            unsigned sum(0);
            for (const SVLocusEdgesType::value_type& edgeIter : getMap())
            {
                sum += edgeIter.second.getCount();
            }
            return sum;
        }
    }

    /// return edge from this to node
    const SVLocusEdge&
    getEdge(const NodeIndexType index) const
    {
        if (_isSingle)
        {
            if (! isEdge(index))
            {
                getEdgeException(index, "getEdge");
            }
            return _edges.single.edge;
        }
        else
        {
            const_iterator i(getMap().find(index));
            if (i == getMap().end()) getEdgeException(index, "getEdge");
            return i->second;
        }
    }

    /// return true if edge exists:
    bool
    isEdge(const NodeIndexType index) const
    {
        if (_isSingle)
        {
            return ((! _edges.single.isZero) &&
                    (index == _edges.single.index));
        }
        else
        {
            const_iterator i(getMap().find(index));
            return (i != getMap().end());
        }
    }

    /// add new edge to node, or merge this edge info in if node already has edge:
    ///
    /// this method is responsible for merging edge counts into the node count as well
    void
    mergeEdge(
        const NodeIndexType index,
        const SVLocusEdge& edge)
    {
        if (_isSingle)
        {
            if (_edges.single.isZero)
            {
                _edges.single.isZero = false;
                _edges.single.index = index;
                _edges.single.edge = edge;
                return;
            }
            else if (index == _edges.single.index)
            {
                _edges.single.edge.mergeEdge(edge);
                return;
            }
            else
            {
                convertToMulti();
            }
        }

        assert(! _isSingle);

        SVLocusEdgesType::iterator edgeIter(getMap().find(index));
        if (edgeIter == getMap().end())
        {
            // this node does not already have an edge to "index", add a new edge:
            getMap().insert(std::make_pair(index,edge));
        }
        else
        {
            // this node already has an edge to "index", merge the existing edge with the new one:
            edgeIter->second.mergeEdge(edge);
        }
    }

    /// reduce edge count to zero
    void
    setEdgeCount(
        const NodeIndexType index,
        const unsigned count)
    {
        if (_isSingle)
        {
            if (! isEdge(index))
            {
                getEdgeException(index, "setEdgeCount");
            }
            _edges.single.edge.setCount(count);
        }
        else
        {
            SVLocusEdgesType::iterator i(getMap().find(index));
            if (i == getMap().end()) getEdgeException(index, "setEdgeCount");
            i->second.setCount(count);
        }
    }

    /// eliminate edge
    void
    eraseEdge(const NodeIndexType index)
    {
        if (_isSingle)
        {
            if (! isEdge(index))
            {
                getEdgeException(index, "eraseEdge");
            }
            _edges.single.isZero = true;
        }
        else
        {
            SVLocusEdgesType::iterator i(getMap().find(index));
            if (i == getMap().end()) getEdgeException(index, "eraseEdge");
            getMap().erase(i);
            assert(getMap().size()>=1);
            if (1 == getMap().size()) convertToSingle();
        }
    }

    /// unhook edge from one node id, and stick it to another:
    void
    moveEdge(
        const NodeIndexType fromIndex,
        const NodeIndexType toIndex)
    {
        if (_isSingle)
        {
            assert(isEdge(fromIndex));
            _edges.single.index = toIndex;
        }
        else
        {
            getMap().insert(std::make_pair(toIndex,getEdge(fromIndex)));
            getMap().erase(fromIndex);
        }
    }

    void
    clear()
    {
        if (! _isSingle)
        {
            delete _edges.multiPtr;
            _isSingle=true;
        }
        _edges.single.isZero = true;
    }

    const GenomeInterval&
    getInterval() const
    {
        return _interval;
    }

    void
    setInterval(const GenomeInterval& interval)
    {
        _interval.tid=interval.tid;
        setIntervalRange(interval.range);
    }

    void
    setIntervalRange(const known_pos_range2& range)
    {
        _interval.range=range;
    }

    const known_pos_range2&
    getEvidenceRange() const
    {
        return _evidenceRange;
    }

    void
    setEvidenceRange(const known_pos_range2& range)
    {
        _evidenceRange = range;
    }

    template<class Archive>
    void save(Archive& ar, const unsigned /* version */) const
    {
        ar << _interval << _evidenceRange << _isSingle;
        if (_isSingle)
        {
            ar << _edges.single;
        }
        else
        {
            ar << getMap();
        }
    }

    template<class Archive>
    void load(Archive& ar, const unsigned /* version */)
    {
        clear();

        ar >> _interval >> _evidenceRange >> _isSingle;
        if (_isSingle)
        {
            ar >> _edges.single;
        }
        else
        {
            _edges.multiPtr = new SVLocusEdgesType;
            ar >> getMap();
        }
    }

    BOOST_SERIALIZATION_SPLIT_MEMBER()

private:

    union CompactEdgeType
    {
        SVLocusEdgeSingle single;
        SVLocusEdgesType* multiPtr;
    };


    SVLocusEdgesType&
    getMap()
    {
        assert(! _isSingle);
        return *(_edges.multiPtr);
    }

    const SVLocusEdgesType&
    getMap() const
    {
        assert(! _isSingle);
        return *(_edges.multiPtr);
    }

    // given a node in the multi-edge state with one edge, convert to
    // the single-edge state
    void
    convertToSingle()
    {
        assert(! _isSingle);
        assert(1 == getMap().size());

        const_iterator begin(getMap().begin());

        SVLocusEdgeSingle transfer;
        transfer.isZero=false;
        transfer.index=begin->first;
        transfer.edge=begin->second;

        delete _edges.multiPtr;
        _edges.single = transfer;
        _isSingle=true;
    }


    // given a node in the single-edge state with one edge, convert to
    // the multi-edge state
    void
    convertToMulti()
    {
        assert(_isSingle);
        assert(! _edges.single.isZero);
        const SVLocusEdgeSingle transfer = _edges.single;

        _isSingle = false;
        _edges.multiPtr = new SVLocusEdgesType;
        getMap().insert(std::make_pair(transfer.index, transfer.edge));
    }

    void
    getEdgeException(
        const NodeIndexType toIndex,
        const char* label) const;

    //////////////////  data:
    GenomeInterval _interval;
    known_pos_range2 _evidenceRange;
    CompactEdgeType _edges;
    bool _isSingle;
};


std::ostream&
operator<<(std::ostream& os, const SVLocusNode& node);


BOOST_CLASS_IMPLEMENTATION(SVLocusNode, boost::serialization::object_serializable)
