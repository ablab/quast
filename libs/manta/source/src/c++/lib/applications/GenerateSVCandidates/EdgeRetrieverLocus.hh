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

#include "EdgeRetriever.hh"
#include "EdgeOptions.hh"


/// provide an iterator over edges in a set of SV locus graphs
///
/// designed to allow parallelization of the graph processing by
/// dividing iteration into a set of bins with similar total edge
/// observation counts
///
struct EdgeRetrieverLocus final : public EdgeRetriever
{
    /// \param locusIndex iterate over all edges of a specific locus
    EdgeRetrieverLocus(
        const SVLocusSet& set,
        const unsigned graphNodeMaxEdgeCount,
        const LocusEdgeOptions& opt);

    bool
    next() override;

private:
    void
    advanceEdge();

    LocusEdgeOptions _opt;
    bool _isInit;
};
