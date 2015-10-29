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


/// WARNING -- initial testing suggests this class still has a possible edge repetition/dropout bug
///            this still has potential but only if you have time to go in and hunt down the bug


/// provide an iterator over edges in a set of SV locus graphs
///
/// designed to allow parallelization of the graph processing by
/// dividing iteration into a set of bins with similar total edge
/// observation counts
///
/// the contents of the bins are designed to be distributed evenly over the sequence of edges
///
struct EdgeRetrieverJumpBin : public EdgeRetriever
{
    /// \param[in] graphNodeMaxEdgeCount filtration parameter for skipping edges from highly connected nodes (set to zero to disable)
    /// \param[in] binCount total number of parallel bins, must be 1 or greater
    /// \param[in] binIndex parallel bin id, must be less than binCount
    EdgeRetrieverJumpBin(
        const SVLocusSet& set,
        const unsigned graphNodeMaxEdgeCount,
        const unsigned binCount,
        const unsigned binIndex);

    bool
    next();

private:
    void
    advanceEdge();

    typedef unsigned long count_t;

    unsigned _binCount;
    unsigned _binIndex;
    count_t _edgeIndex;

    // additional 'load balancing' structures:
    count_t _avgBinTotalCount;

    std::vector<count_t> _binTotalCount;
};
