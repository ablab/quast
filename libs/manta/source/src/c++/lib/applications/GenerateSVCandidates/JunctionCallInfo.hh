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
/// \author Chris Saunders and Xiaoyu Chen
///

#pragma once

#include "SVEvidence.hh"
#include "manta/SVScoreInfo.hh"
#include "manta/SVCandidate.hh"


/// manage all per-junction information consumed by an SV calling model
///
/// using this object facilities multi-breakend event scoring, but clearly
/// separating out per-junction input info from junction-independent info
///
struct JunctionCallInfo
{
    JunctionCallInfo() :
        _sv(NULL),
        _evidence(NULL),
        _baseInfo(NULL),
        _spanningPairWeight(0)
    {}

    const SVCandidate&
    getSV() const
    {
        assert(NULL != _sv);
        return *_sv;
    }

    const SVEvidence&
    getEvidence() const
    {
        assert(NULL != _evidence);
        return *_evidence;
    }

    const SVScoreInfo&
    getBaseInfo() const
    {
        assert(NULL != _baseInfo);
        return *_baseInfo;
    }

    float
    getSpanningWeight() const
    {
        return _spanningPairWeight;
    }

    void
    init(
        const SVCandidate& sv,
        const SVEvidence& evidence,
        const SVScoreInfo& baseInfo,
        const float spanningPairWeight)
    {
        _sv=&sv;
        _evidence=&evidence;
        _baseInfo=&baseInfo;
        _spanningPairWeight=spanningPairWeight;
    }

private:
    const SVCandidate* _sv;
    const SVEvidence* _evidence;
    const SVScoreInfo* _baseInfo;
    float _spanningPairWeight;
};
