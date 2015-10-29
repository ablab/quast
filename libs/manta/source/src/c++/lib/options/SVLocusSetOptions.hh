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


/// parameters specific to SVLocusSet:
///
struct SVLocusSetOptions
{
    explicit
    SVLocusSetOptions(
        const unsigned initObservationWeight = 1) :
        observationWeight(initObservationWeight),
        minMergeEdgeObservations(3),
        maxSearchCount(500),
        maxSearchDensity(0.5)
    {}

    unsigned
    getMinMergeEdgeCount() const
    {
        return (observationWeight*minMergeEdgeObservations);
    }

    template<class Archive>
    void serialize(Archive& ar, const unsigned /* version */)
    {
        ar& observationWeight;
        ar& minMergeEdgeObservations;
        ar& maxSearchCount;
        ar& maxSearchDensity;
    }

    unsigned observationWeight; ///< used to translate graph edges counts to observations
    unsigned minMergeEdgeObservations; ///< to reduce noise in the graph, we only merge once shared edges reach this number of observations
    unsigned maxSearchCount; ///< the search for intersecting regions in the graph stops once this number is reached
    float maxSearchDensity; ///< the search for intersecting regions in the graph stops once this many regions/base are found
};

