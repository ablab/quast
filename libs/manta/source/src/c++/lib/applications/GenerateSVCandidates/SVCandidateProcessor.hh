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

#include "EdgeRuntimeTracker.hh"
#include "GSCEdgeStatsManager.hh"
#include "GSCOptions.hh"
#include "SVCandidateAssemblyRefiner.hh"
#include "SVScorer.hh"

#include "common/OutStream.hh"
#include "manta/JunctionIdGenerator.hh"
#include "manta/SVCandidateAssemblyData.hh"
#include "manta/SVMultiJunctionCandidate.hh"
#include "format/VcfWriterCandidateSV.hh"
#include "format/VcfWriterDiploidSV.hh"
#include "format/VcfWriterSomaticSV.hh"
#include "format/VcfWriterTumorSV.hh"


#include <memory>

//#define DEBUG_GSV



struct SVWriter
{
    SVWriter(
        const GSCOptions& initOpt,
        const SVLocusScanner& readScanner,
        const SVLocusSet& cset,
        const char* progName,
        const char* progVersion);

    void
    writeSV(
        const EdgeInfo& edge,
        const SVCandidateSetData& svData,
        const std::vector<SVCandidateAssemblyData>& assemblyData,
        const SVMultiJunctionCandidate& mjSV,
        const std::vector<bool>& isInputJunctionFiltered);

    ///////////////////////// data:
    const GSCOptions& opt;
    const bool isSomatic;
    const bool isTumorOnly;

    SVScorer svScore;

    std::vector<SVModelScoreInfo> mjModelScoreInfo;

    OutStream candfs;
    OutStream dipfs;
    OutStream somfs;
    OutStream tumfs;

    VcfWriterCandidateSV candWriter;
    VcfWriterDiploidSV diploidWriter;
    VcfWriterSomaticSV somWriter;
    VcfWriterTumorSV tumorWriter;

    JunctionIdGenerator _idgen;
};


struct SVCandidateProcessor
{
    SVCandidateProcessor(
        const GSCOptions& opt,
        const SVLocusScanner& readScanner,
        const char* progName,
        const char* progVersion,
        const SVLocusSet& cset,
        EdgeRuntimeTracker& edgeTracker,
        GSCEdgeStatsManager& _edgeStatMan);

    void
    evaluateCandidates(
        const EdgeInfo& edge,
        const std::vector<SVMultiJunctionCandidate>& mjSVs,
        const SVCandidateSetData& svData);

private:

    void
    evaluateCandidate(
        const EdgeInfo& edge,
        const SVMultiJunctionCandidate& mjCandidateSV,
        const SVCandidateSetData& svData,
        const bool isFindLargeInsertions);

    const GSCOptions& _opt;
    const SVLocusSet& _cset;
    EdgeRuntimeTracker& _edgeTracker;
    GSCEdgeStatsManager& _edgeStatMan;
    SVCandidateAssemblyRefiner _svRefine;
    SVWriter _svWriter;
};
