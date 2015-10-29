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

#include "manta/JunctionIdGenerator.hh"
#include "manta/SVModelScoreInfo.hh"
#include "format/VcfWriterSV.hh"
#include "format/VcfWriterScoredSV.hh"
#include "options/CallOptionsDiploid.hh"


struct VcfWriterDiploidSV : public VcfWriterSV, VcfWriterScoredSV
{
    VcfWriterDiploidSV(
        const CallOptionsDiploid& diploidOpt,
        const bool isMaxDepthFilter,
        const std::string& referenceFilename,
        const bool isRNA,
        const SVLocusSet& set,
        std::ostream& os) :
        VcfWriterSV(referenceFilename,isRNA,set,os),
        _diploidOpt(diploidOpt),
        _isMaxDepthFilter(isMaxDepthFilter),
        _diploidInfoPtr(nullptr),
        _singleJunctionDiploidInfoPtr(nullptr)
    {}

    void
    writeSV(
        const SVCandidateSetData& svData,
        const SVCandidateAssemblyData& adata,
        const SVCandidate& sv,
        const SVId& svId,
        const SVScoreInfo& baseInfo,
        const SVScoreInfoDiploid& diploidInfo,
        const EventInfo& event,
        const SVScoreInfoDiploid& singleJunctionDiploidInfo);

private:

    void
    addHeaderInfo() const override;

    void
    addHeaderFormat() const override;

    void
    addHeaderFilters() const override;

    void
    modifyInfo(
        const EventInfo& event,
        InfoTag_t& infotags) const override;

    void
    modifySample(
        const SVCandidate& sv,
        SampleTag_t& sampletags) const override;

    void
    modifyTranslocInfo(
        const SVCandidate& sv,
        const bool isFirstOfPair,
        InfoTag_t& infotags) const override;

    void
    writeQual() const override;

    void
    writeFilter() const override;

    const SVScoreInfoDiploid&
    getDiploidInfo() const
    {
        assert(nullptr != _diploidInfoPtr);
        return *_diploidInfoPtr;
    }

    const SVScoreInfoDiploid&
    getSingleJunctionDiploidInfo() const
    {
        assert(NULL != _singleJunctionDiploidInfoPtr);
        return *_singleJunctionDiploidInfoPtr;
    }


    const CallOptionsDiploid& _diploidOpt;
    const bool _isMaxDepthFilter;
    const SVScoreInfoDiploid* _diploidInfoPtr;
    const SVScoreInfoDiploid* _singleJunctionDiploidInfoPtr;
};
