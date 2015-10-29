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

#include "svgraph/EdgeInfo.hh"
#include "svgraph/SVLocusSet.hh"


/// provide an iterator over edges in a set of SV locus graphs
///
struct EdgeRetriever
{
    EdgeRetriever(
        const SVLocusSet& set,
        const unsigned graphNodeMaxEdgeCount) :
        _set(set),
        _graphNodeMaxEdgeCount(graphNodeMaxEdgeCount)
    {}

    virtual
    ~EdgeRetriever()
    {}

    virtual
    bool
    next() = 0;

    const EdgeInfo&
    getEdge() const
    {
        return _edge;
    }

protected:
    const SVLocusSet& _set;
    const unsigned _graphNodeMaxEdgeCount;
    EdgeInfo _edge;
};
