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

#include "svgraph/SVLocus.hh"

#include <iosfwd>


struct EdgeInfo
{
    /// minimal ascii representation:
    void
    write(std::ostream& os) const;

    bool
    isSelfEdge() const
    {
        return (nodeIndex1 == nodeIndex2);
    }

    LocusIndexType locusIndex = 0;
    NodeIndexType nodeIndex1 = 0;
    NodeIndexType nodeIndex2 = 0;
};

std::ostream&
operator<<(std::ostream& os, const EdgeInfo& ei);
