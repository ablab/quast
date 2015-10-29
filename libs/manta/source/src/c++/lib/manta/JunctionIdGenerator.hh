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
#include "manta/SVCandidateUtil.hh"
#include "boost/format.hpp"

#include <string>


/// A pair of ids for both ends of a single SV junction
///
/// the mateid will only be defined for tranlocations, and empty otherwise
///
struct SVId
{
    const char*
    getLabel() const
    {
        return EXTENDED_SV_TYPE::label(svType);
    }

    EXTENDED_SV_TYPE::index_t svType = EXTENDED_SV_TYPE::UNKNOWN;
    std::string localId;
    std::string mateId;
};


/// create IDs for each variant that are guaranteed to be unique for a single
/// manta run
///
struct JunctionIdGenerator
{
    JunctionIdGenerator() :
        _SVIdFormatter("Manta%s:%i:%i:%i:%i:%i:%i")
    {}

    void
    getId(
        const EdgeInfo& edge,
        const SVCandidate& sv,
        const bool isRNA,
        SVId& svId);

private:
    boost::format _SVIdFormatter;
};
