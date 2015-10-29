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

#include "format/VcfWriterSV.hh"


struct VcfWriterCandidateSV : public VcfWriterSV
{
    VcfWriterCandidateSV(
        const std::string& referenceFilename,
        const bool isRNA,
        const SVLocusSet& set,
        std::ostream& os) :
        VcfWriterSV(referenceFilename, isRNA, set, os)
    {}

    void
    addHeaderInfo() const override;

    void
    modifyTranslocInfo(
        const SVCandidate& sv,
        const bool isFirstOfPair,
        InfoTag_t& infoTags) const override;

    void
    modifyInvdelInfo(
        const SVCandidate& sv,
        const bool isBp1First,
        InfoTag_t& infoTags) const override;

    void
    writeSV(
        const SVCandidateSetData& svData,
        const SVCandidateAssemblyData& adata,
        const SVCandidate& sv,
        const SVId& svId);
};

