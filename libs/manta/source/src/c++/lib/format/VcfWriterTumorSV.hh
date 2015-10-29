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
/// \author Xiaoyu Chen
///

#pragma once

#include "format/VcfWriterSV.hh"
#include "format/VcfWriterScoredSV.hh"
#include "options/CallOptionsTumor.hh"


struct VcfWriterTumorSV : public VcfWriterSV, VcfWriterScoredSV
{
    static const bool isRNA = false;

    VcfWriterTumorSV(
        const CallOptionsTumor& tumorOpt,
        const bool isMaxDepthFilter,
        const std::string& referenceFilename,
        const SVLocusSet& set,
        std::ostream& os) :
        VcfWriterSV(referenceFilename, isRNA, set, os),
        _tumorOpt(tumorOpt),
        _isMaxDepthFilter(isMaxDepthFilter),
        _tumorInfoPtr(nullptr)
    {}

    void
    writeSV(
        const SVCandidateSetData& svData,
        const SVCandidateAssemblyData& adata,
        const SVCandidate& sv,
        const SVId& svId,
        const SVScoreInfo& baseInfo,
        const SVScoreInfoTumor& tumorInfo,
        const EventInfo& event);

private:

    void
    addHeaderInfo() const override;

    void
    addHeaderFormat() const override;

    void
    addHeaderFilters() const override;

    void
    writeFilter() const override;

    void
    modifySample(
        const SVCandidate& sv,
        SampleTag_t& sampletags) const override;

    void
    modifyTranslocInfo(
        const SVCandidate& sv,
        const bool isFirstOfPair,
        InfoTag_t& infotags) const override;



    const SVScoreInfoTumor&
    getTumorInfo() const
    {
        assert(NULL != _tumorInfoPtr);
        return *_tumorInfoPtr;
    }

    const CallOptionsTumor& _tumorOpt;
    const bool _isMaxDepthFilter;
    const SVScoreInfoTumor* _tumorInfoPtr;
};

