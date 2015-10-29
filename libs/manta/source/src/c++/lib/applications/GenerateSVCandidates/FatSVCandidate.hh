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

#include "manta/SVCandidate.hh"

#include <array>
#include <iosfwd>


template <typename Vec>
void
appendVec(
    Vec& A,
    const Vec& B)
{
    A.insert( A.end(), B.begin(), B.end() );
}



/// an SV candidate with additional details pertaining to input read evidence which is useful for filtration
///
struct FatSVCandidate : public SVCandidate
{
    typedef SVCandidate base_t;

    FatSVCandidate()
        : base_t()
    {}

    explicit
    FatSVCandidate(const SVCandidate& copy)
        : base_t(copy)
    {}

    FatSVCandidate(const FatSVCandidate&) = default;
    FatSVCandidate& operator=(const FatSVCandidate&) = default;


    bool
    merge(
        const FatSVCandidate& rhs,
        const bool isExpandRegion = true)
    {
        if (! base_t::merge(rhs, isExpandRegion)) return false;
        for (unsigned i(0); i<SVEvidenceType::SIZE; ++i)
        {
            appendVec(bp1EvidenceIndex[i],rhs.bp1EvidenceIndex[i]);
            appendVec(bp2EvidenceIndex[i],rhs.bp2EvidenceIndex[i]);
        }
        return true;
    }

#if 0
    bool
    merge(const SVCandidate& rhs)
    {
        if (! base_t::merge(rhs)) return false;

        return true;
    }
#endif

#if 0
    void
    clear()
    {
        base_t::clear();
        for (auto& evi : bp1EvidenceIndex) evi.clear();
        for (auto& evi : bp2EvidenceIndex) evi.clear();
    }
#endif

    typedef std::array<std::vector<double>,SVEvidenceType::SIZE> evidenceIndex_t;

    evidenceIndex_t bp1EvidenceIndex;
    evidenceIndex_t bp2EvidenceIndex;
};


std::ostream&
operator<<(std::ostream& os, const FatSVCandidate& svc);
