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


/// provide an iterator over edges in a set of SV locus graphs
///
/// designed to allow parallelization of the graph processing by
/// dividing iteration into a set of bins with similar total edge
/// observation counts
///
struct EdgeRetrieverBin final : public EdgeRetriever
{
    /// \param[in] graphNodeMaxEdgeCount filtration parameter for skipping edges
    ///            from highly connected nodes (set to zero to disable)
    /// \param[in] binCount total number of parallel bins, must be 1 or greater
    /// \param[in] binIndex parallel bin id, must be less than binCount
    EdgeRetrieverBin(
        const SVLocusSet& set,
        const unsigned graphNodeMaxEdgeCount,
        const unsigned binCount,
        const unsigned binIndex);

    bool
    next() override;

private:
    void
    jumpToFirstEdge();

    void
    advanceEdge();

    /// _beginCount and _endCount provide the observation range for the bin we're retrieving.
    /// these values should be contant following the ctor
    unsigned long _beginCount;
    unsigned long _endCount;

    /// _headCount is a tracking index of cumulative observation count as we step through
    /// the graph
    unsigned long _headCount;
};
