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
/// \author Bret Barnes, Xiaoyu Chen
///

#pragma once

#include "blt_util/SizeDistribution.hh"
#include "common/ReadPairOrient.hh"


/// Read pair insert stats can be computed for each sample or read group, this
/// class represents the statistics for one group:
///
struct ReadGroupStats
{
private:
    friend class boost::serialization::access;
    template<class Archive>
    void serialize(
        Archive& ar,
        const unsigned /*version*/)
    {
        ar& boost::serialization::make_nvp("fragmentSizeDistribution", fragStats);
        ar& boost::serialization::make_nvp("pairOrientation", relOrients);
    }

    ///////////////////////////// data:
public:
    SizeDistribution fragStats;
    ReadPairOrient relOrients;
};

BOOST_CLASS_IMPLEMENTATION(ReadGroupStats, boost::serialization::object_serializable)
