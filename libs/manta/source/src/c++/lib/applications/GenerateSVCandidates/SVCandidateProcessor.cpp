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

#include "SVCandidateProcessor.hh"
#include "manta/SVMultiJunctionCandidateUtil.hh"
#include "svgraph/EdgeInfoUtil.hh"

#include "blt_util/log.hh"

#include <iostream>


//#define DEBUG_GSV



SVWriter::
SVWriter(
    const GSCOptions& initOpt,
    const SVLocusScanner& readScanner,
    const SVLocusSet& cset,
    const char* progName,
    const char* progVersion) :
    opt(initOpt),
    isSomatic(! opt.somaticOutputFilename.empty()),
    isTumorOnly(! opt.tumorOutputFilename.empty()),
    svScore(opt, readScanner, cset.header),
    candfs(opt.candidateOutputFilename),
    dipfs(opt.diploidOutputFilename),
    somfs(opt.somaticOutputFilename),
    tumfs(opt.tumorOutputFilename),
    candWriter(opt.referenceFilename, opt.isRNA, cset,candfs.getStream()),
    diploidWriter(opt.diploidOpt, (! opt.chromDepthFilename.empty()),
                  opt.referenceFilename,  opt.isRNA, cset,dipfs.getStream()),
    somWriter(opt.somaticOpt, (! opt.chromDepthFilename.empty()),
              opt.referenceFilename, cset,somfs.getStream()),
    tumorWriter(opt.tumorOpt, (! opt.chromDepthFilename.empty()),
                opt.referenceFilename, cset,tumfs.getStream())
{
    if (0 == opt.edgeOpt.binIndex)
    {
        std::vector<std::string> noSampleNames;
        candWriter.writeHeader(progName, progVersion,noSampleNames);

        const std::vector<std::string>& sampleNames(svScore.sampleNames());
        if (isTumorOnly)
        {
            tumorWriter.writeHeader(progName, progVersion,sampleNames);
        }
        else
        {
            std::vector<std::string> diploidSampleNames(sampleNames.begin(),sampleNames.begin()+svScore.diploidSampleCount());
            diploidWriter.writeHeader(progName, progVersion,diploidSampleNames);
            if (isSomatic) somWriter.writeHeader(progName, progVersion,sampleNames);
        }
    }

    //get_bam_header_sample_name
}



static
bool
isAnyFalse(
    const std::vector<bool>& vb)
{
    for (const bool val : vb)
    {
        if (! val) return true;
    }
    return false;
}



void
SVWriter::
writeSV(
    const EdgeInfo& edge,
    const SVCandidateSetData& svData,
    const std::vector<SVCandidateAssemblyData>& mjAssemblyData,
    const SVMultiJunctionCandidate& mjSV,
    const std::vector<bool>& isInputJunctionFiltered)
{
    const unsigned junctionCount(mjSV.junction.size());
    const unsigned minJunctionCandidateSpanningCount(std::min(2u,opt.minCandidateSpanningCount));

    // track filtration for each junction:
    std::vector<bool> isJunctionFiltered(isInputJunctionFiltered);

    // early SV filtering:
    //
    // 2 junction filter types:
    // 1) tests where the junction can fail independently
    // 2) tests where all junctions have to fail for the candidate to be filtered:

    bool isCandidateSpanFail(true);

    for (unsigned junctionIndex(0); junctionIndex<junctionCount; ++junctionIndex)
    {
        const SVCandidateAssemblyData& assemblyData(mjAssemblyData[junctionIndex]);
        const SVCandidate& sv(mjSV.junction[junctionIndex]);

        const bool isCandidateSpanning(assemblyData.isCandidateSpanning);

#ifdef DEBUG_GSV
        log_os << __FUNCTION__ << ": isSpanningSV junction: " <<  isCandidateSpanning << "\n";
#endif

        // junction dependent tests:
        //   (1) at least one junction in the set must have spanning count of 3 or more
        bool isJunctionSpanFail(false);
        if (isCandidateSpanning)
        {
            if (sv.getPostAssemblySpanningCount() < opt.minCandidateSpanningCount)
            {
                isJunctionSpanFail=true;
            }
        }
        if (! isJunctionSpanFail) isCandidateSpanFail=false;

        // independent tests -- as soon as one of these fails, we can continue:
        //   (1) each spanning junction in the set must have spanning count of
        //      minJunctionCandidateSpanningCount or more
        //   (2) no unassembled non-spanning candidates
        if (isCandidateSpanning)
        {
            if (sv.getPostAssemblySpanningCount() < minJunctionCandidateSpanningCount)
            {
                isJunctionFiltered[junctionIndex] = true;
                continue;
            }
        }
        else
        {
            if (sv.isImprecise())
            {
                // in this case a non-spanning low-res candidate went into assembly but
                // did not produce a successful contig alignment:
#ifdef DEBUG_GSV
                log_os << __FUNCTION__ << ": Rejecting candidate junction: imprecise non-spanning SV\n";
#endif
                isJunctionFiltered[junctionIndex] = true;
                continue;
            }
        }

        // check min size for candidate output:
        if (isSVBelowMinSize(sv,opt.scanOpt.minCandidateVariantSize))
        {
#ifdef DEBUG_GSV
            log_os << __FUNCTION__ << ": Filtering out candidate below min size before candidate output stage\n";
#endif
            isJunctionFiltered[junctionIndex] = true;
            continue;
        }
    }

    // revisit dependent tests:
    //
    if (isCandidateSpanFail)
    {
        for (unsigned junctionIndex(0); junctionIndex<junctionCount; ++junctionIndex)
        {
#ifdef DEBUG_GSV
            log_os << __FUNCTION__ << ": Rejecting candidate junction: minCandidateSpanningCount\n";
#endif
            isJunctionFiltered[junctionIndex] = true;
        }
    }

    // check to see if all junctions are filtered, if so skip the whole candidate:
    //
    if (! isAnyFalse(isJunctionFiltered))
    {
#ifdef DEBUG_GSV
        log_os << __FUNCTION__ << ": Rejecting candidate, all junctions filtered.\n";
#endif
        return;
    }

    std::vector<SVId> junctionSVId(junctionCount);

    // write out candidates for each junction independently:
    for (unsigned junctionIndex(0); junctionIndex<junctionCount; ++junctionIndex)
    {
        if (isJunctionFiltered[junctionIndex]) continue;

        const SVCandidateAssemblyData& assemblyData(mjAssemblyData[junctionIndex]);
        const SVCandidate& sv(mjSV.junction[junctionIndex]);
        SVId& svId(junctionSVId[junctionIndex]);

        _idgen.getId(edge, sv, opt.isRNA, svId);

        candWriter.writeSV(svData, assemblyData, sv, svId);
    }

    if (opt.isSkipScoring) return;

    // check min size for scoring:
    for (unsigned junctionIndex(0); junctionIndex<junctionCount; ++junctionIndex)
    {
        if (isJunctionFiltered[junctionIndex]) continue;

        const SVCandidate& sv(mjSV.junction[junctionIndex]);
        if (isSVBelowMinSize(sv, opt.minScoredVariantSize))
        {
#ifdef DEBUG_GSV
            log_os << __FUNCTION__ << ": Filtering out candidate junction below min size at scoring stage\n";
#endif
            isJunctionFiltered[junctionIndex] = true;
        }
    }

    // check to see if all junctions are filtered before scoring:
    //
    if (! isAnyFalse(isJunctionFiltered)) return;

    bool isMJEvent(false);

    SVModelScoreInfo mjJointModelScoreInfo;
    mjJointModelScoreInfo.setSampleCount(svScore.sampleCount(),svScore.diploidSampleCount());
    svScore.scoreSV(svData, mjAssemblyData, mjSV, isJunctionFiltered, isSomatic, isTumorOnly, mjModelScoreInfo, mjJointModelScoreInfo, isMJEvent);

    const unsigned unfilteredJunctionCount(std::count(isJunctionFiltered.begin(),isJunctionFiltered.end(),true));

    // setup all event-level info that we need to share across all event junctions:
    bool isMJDiploidEvent(isMJEvent);
    EventInfo event;
    event.junctionCount=unfilteredJunctionCount;
    bool isMJEventWriteDiploid(false);
    bool isMJEventWriteSomatic(false);

    if (isMJEvent)
    {
        for (unsigned junctionIndex(0); junctionIndex<junctionCount; ++junctionIndex)
        {
            if (isJunctionFiltered[junctionIndex]) continue;

            if (event.label.empty())
            {
                const SVId& svId(junctionSVId[junctionIndex]);
                event.label = svId.localId;
            }

            const SVModelScoreInfo& modelScoreInfo(mjModelScoreInfo[junctionIndex]);

            // for diploid case only we decide to use multi-junction or single junction based on best score:
            // (for somatic case a lower somatic score could be due to reference evidence in an event member)
            //
            if (mjJointModelScoreInfo.diploid.filters.size() > modelScoreInfo.diploid.filters.size())
            {
                isMJDiploidEvent=false;
            }
            else if (mjJointModelScoreInfo.diploid.altScore < modelScoreInfo.diploid.altScore)
            {
                isMJDiploidEvent=false;
            }
        }

        // for events, we write all junctions, or no junctions, so we need to determine write status over
        // the whole set rather than a single junctions:
        for (unsigned junctionIndex(0); junctionIndex<junctionCount; ++junctionIndex)
        {
            const SVModelScoreInfo& modelScoreInfo(mjModelScoreInfo[junctionIndex]);

            if (isMJDiploidEvent)
            {
                if ((mjJointModelScoreInfo.diploid.altScore >= opt.diploidOpt.minOutputAltScore) ||
                    (modelScoreInfo.diploid.altScore >= opt.diploidOpt.minOutputAltScore))
                {
                    isMJEventWriteDiploid = true;
                }
            }

            if ((mjJointModelScoreInfo.somatic.somaticScore >= opt.somaticOpt.minOutputSomaticScore) ||
                (modelScoreInfo.somatic.somaticScore >= opt.somaticOpt.minOutputSomaticScore))
            {
                isMJEventWriteSomatic = true;
            }

            //TODO: set up criteria for isMJEventWriteTumor
        }
    }

    // final scored output is treated (mostly) independently for each junction:
    //
    for (unsigned junctionIndex(0); junctionIndex<junctionCount; ++junctionIndex)
    {
        if (isJunctionFiltered[junctionIndex]) continue;

        const SVCandidateAssemblyData& assemblyData(mjAssemblyData[junctionIndex]);
        const SVCandidate& sv(mjSV.junction[junctionIndex]);
        const SVModelScoreInfo& modelScoreInfo(mjModelScoreInfo[junctionIndex]);

        const SVId& svId(junctionSVId[junctionIndex]);
        const SVScoreInfo& baseInfo(modelScoreInfo.base);
        static const EventInfo nonEvent;

        if (isTumorOnly)
        {
            //TODO: add logic for MJEvent

            const SVScoreInfoTumor& tumorInfo(modelScoreInfo.tumor);
            tumorWriter.writeSV(svData, assemblyData, sv, svId, baseInfo, tumorInfo, nonEvent);
        }
        else
        {
            {
                const EventInfo& diploidEvent( isMJDiploidEvent ? event : nonEvent );
                const SVModelScoreInfo& scoreInfo(isMJDiploidEvent ? mjJointModelScoreInfo : modelScoreInfo);
                const SVScoreInfoDiploid& diploidInfo(scoreInfo.diploid);

                bool isWriteDiploid(false);
                if (isMJDiploidEvent)
                {
                    isWriteDiploid = isMJEventWriteDiploid;
                }
                else
                {
                    isWriteDiploid = (modelScoreInfo.diploid.altScore >= opt.diploidOpt.minOutputAltScore);
                }

                if (opt.isRNA) isWriteDiploid = true; /// TODO remove after adding RNA scoring

                if (isWriteDiploid)
                {
                    diploidWriter.writeSV(svData, assemblyData, sv, svId, baseInfo, diploidInfo, diploidEvent, modelScoreInfo.diploid);
                }
            }

            if (isSomatic)
            {
                const SVModelScoreInfo& scoreInfo(isMJEvent ? mjJointModelScoreInfo : modelScoreInfo);
                const SVScoreInfoSomatic& somaticInfo(scoreInfo.somatic);

                bool isWriteSomatic(false);

                if (isMJEvent)
                {
                    isWriteSomatic = isMJEventWriteSomatic;
                }
                else
                {
                    isWriteSomatic = (modelScoreInfo.somatic.somaticScore >= opt.somaticOpt.minOutputSomaticScore);
                }

                if (isWriteSomatic)
                {
                    somWriter.writeSV(svData, assemblyData, sv, svId, baseInfo, somaticInfo, event, modelScoreInfo.somatic);
                }
            }
        }
    }
}



SVCandidateProcessor::
SVCandidateProcessor(
    const GSCOptions& opt,
    const SVLocusScanner& readScanner,
    const char* progName,
    const char* progVersion,
    const SVLocusSet& cset,
    EdgeRuntimeTracker& edgeTracker,
    GSCEdgeStatsManager& edgeStatMan) :
    _opt(opt),
    _cset(cset),
    _edgeTracker(edgeTracker),
    _edgeStatMan(edgeStatMan),
    _svRefine(opt, cset.header, cset.getCounts(), _edgeTracker),
    _svWriter(opt, readScanner, cset, progName, progVersion)
{}



void
SVCandidateProcessor::
evaluateCandidates(
    const EdgeInfo& edge,
    const std::vector<SVMultiJunctionCandidate>& mjSVs,
    const SVCandidateSetData& svData)
{
    const bool isIsolatedEdge(testIsolatedEdge(_cset,edge));

    bool isFindLargeInsertions(isIsolatedEdge);
    if (isFindLargeInsertions)
    {
        for (const SVMultiJunctionCandidate& mjCandidateSV : mjSVs)
        {
            for (const SVCandidate& candidateSV : mjCandidateSV.junction)
            {
                if (! isComplexSV(candidateSV)) isFindLargeInsertions=false;
            }
        }
    }

    _svRefine.clearEdgeData();
    for (const auto& cand : mjSVs)
    {
        evaluateCandidate(edge,cand,svData,isFindLargeInsertions);
    }
}



void
SVCandidateProcessor::
evaluateCandidate(
    const EdgeInfo& edge,
    const SVMultiJunctionCandidate& mjCandidateSV,
    const SVCandidateSetData& svData,
    const bool isFindLargeInsertions)
{
    assert(! mjCandidateSV.junction.empty());

    const unsigned junctionCount(mjCandidateSV.junction.size());

    if (_opt.isVerbose)
    {
        log_os << __FUNCTION__ << ": Starting analysis for SV candidate containing " << junctionCount << " junctions. Low-resolution junction candidate ids:";
        for (const SVCandidate& sv : mjCandidateSV.junction)
        {
            log_os << " " << sv.candidateIndex;
        }
        log_os << "\n";
    }
#ifdef DEBUG_GSV
    log_os << __FUNCTION__ << ": CandidateSV: " << mjCandidateSV << "\n";
#endif


    const bool isComplex(isComplexSV(mjCandidateSV));
    _edgeTracker.addCand(isComplex);

    _edgeStatMan.updateJunctionCandidates(edge, junctionCount, isComplex);

    // assemble each junction independently:
    bool isAnySmallAssembler(false);
    std::vector<SVCandidateAssemblyData> mjAssemblyData(junctionCount);

    if (! _opt.isSkipAssembly)
    {
        const TimeScoper assmTime(_edgeTracker.assmTime);
        for (unsigned junctionIndex(0); junctionIndex<junctionCount; ++junctionIndex)
        {
            const SVCandidate& candidateSV(mjCandidateSV.junction[junctionIndex]);
            SVCandidateAssemblyData& assemblyData(mjAssemblyData[junctionIndex]);
            _svRefine.getCandidateAssemblyData(candidateSV, svData, _opt.isRNA, isFindLargeInsertions, assemblyData);

            if (_opt.isVerbose)
            {
                log_os << __FUNCTION__ << ": Candidate assembly complete for junction " << junctionIndex << "/" << junctionCount << ". Assembled candidate count: " << assemblyData.svs.size() << "\n";
            }

            if (! assemblyData.svs.empty())
            {
                const unsigned assemblyCount(assemblyData.svs.size());

                const bool isSpanning(assemblyData.isSpanning);

                _edgeStatMan.updateAssemblyCount(edge, assemblyCount, isSpanning);

                if (isSpanning)
                {
                    // can't be multi-junction and multi-assembly at the same time:
                    assert(! ((junctionCount>1) && (assemblyCount>1)));
                }
                else
                {
                    isAnySmallAssembler=true;
                }

                // fill in assembly tracking data:
                _edgeTracker.addAssm(isComplex);
            }
            else
            {
                _edgeStatMan.updateAssemblyCount(edge, 0, assemblyData.isSpanning, assemblyData.isOverlapSkip);
            }
        }
    }

    SVMultiJunctionCandidate mjAssembledCandidateSV;
    mjAssembledCandidateSV.junction.resize(junctionCount);
    std::vector<bool> isJunctionFiltered(junctionCount,false);

    std::vector<unsigned> junctionTracker(junctionCount,0);
    while (true)
    {
        // Note this loop is an accident -- it was intended to enumerate all assembly
        // combinations for  multiple junctions with multiple assemblies each.
        // It doesn't do that -- but the broken thing it does, in fact, do, is what we
        // want for the isAnySmallAssembler case so it's well enough for now.
        //
        bool isWrite(false);
        for (unsigned junctionIndex(0); junctionIndex<junctionCount; ++junctionIndex)
        {
            const SVCandidateAssemblyData& assemblyData(mjAssemblyData[junctionIndex]);
            unsigned& assemblyIndex(junctionTracker[junctionIndex]);

            if (assemblyData.svs.empty())
            {
                if (assemblyIndex != 0) continue;
#ifdef DEBUG_GSV
                log_os << __FUNCTION__ << ": score and output low-res candidate junction " << junctionIndex << "\n";
#endif
                mjAssembledCandidateSV.junction[junctionIndex] = mjCandidateSV.junction[junctionIndex];
            }
            else
            {
                if (assemblyIndex >= assemblyData.svs.size()) continue;
                const SVCandidate& assembledSV(assemblyData.svs[assemblyIndex]);
#ifdef DEBUG_GSV
                log_os << __FUNCTION__ << ": score and output assembly candidate junction " << junctionIndex << ": " << assembledSV << "\n";
#endif
                mjAssembledCandidateSV.junction[junctionIndex] = assembledSV;
            }
            assemblyIndex++;
            isWrite = true;
        }
        if (! isWrite) break;


        // if any junctions go into the small assembler (for instance b/c the breakends are too close), then
        // treat all junctions as independent:
        //
        const TimeScoper scoreTime(_edgeTracker.scoreTime);
        if ((junctionCount>1) && isAnySmallAssembler)
        {
            for (unsigned junctionIndex(0); junctionIndex<junctionCount; ++junctionIndex)
            {
                std::vector<bool> isJunctionFilteredHack(junctionCount,true);
                isJunctionFilteredHack[junctionIndex] = false;
                _svWriter.writeSV(edge, svData, mjAssemblyData, mjAssembledCandidateSV, isJunctionFilteredHack);
            }
        }
        else
        {
            _svWriter.writeSV(edge, svData, mjAssemblyData, mjAssembledCandidateSV, isJunctionFiltered);
        }
    }
}
