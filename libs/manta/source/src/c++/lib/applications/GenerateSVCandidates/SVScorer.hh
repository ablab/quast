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

#include "GSCOptions.hh"
#include "JunctionCallInfo.hh"
#include "SplitReadAlignment.hh"
#include "SVEvidence.hh"
#include "SVScorerPairOptions.hh"
#include "SVScorePairProcessor.hh"

#include "assembly/AssembledContig.hh"
#include "blt_util/qscore_snp.hh"
#include "htsapi/bam_streamer.hh"
#include "htsapi/bam_header_info.hh"
#include "manta/ChromDepthFilterUtil.hh"
#include "manta/SVCandidateAssemblyData.hh"
#include "manta/SVCandidateSetData.hh"
#include "manta/SVLocusScanner.hh"
#include "manta/SVModelScoreInfo.hh"
#include "manta/SVMultiJunctionCandidate.hh"
#include "manta/SVScoreInfoSomatic.hh"

#include <vector>
#include <string>


struct CallOptionsSharedDeriv
{
    explicit
    CallOptionsSharedDeriv(
        const CallOptionsShared& opt) :
        refQ(opt.snpPrior),
        altQ(0)
    {}

    const qscore_snp refQ;
    const qscore_snp altQ;
};


struct CallOptionsDiploidDeriv : private boost::noncopyable
{
    explicit
    CallOptionsDiploidDeriv(
        const CallOptionsDiploid& opt)
    {
        using namespace DIPLOID_GT;

        assert(opt.indelPrior < 0.5);

        prior[HET] = opt.indelPrior;
        prior[HOM] = opt.indelPrior/2;
        prior[REF] = 1 - prior[HET] - prior[HOM];

        for (unsigned i(0); i<SIZE; ++i)
        {
            logPrior[i] = std::log(prior[i]);
        }
    }

    std::array<float,DIPLOID_GT::SIZE> prior;
    std::array<float,DIPLOID_GT::SIZE> logPrior;
};


struct CallOptionsSomaticDeriv : private boost::noncopyable
{
    explicit
    CallOptionsSomaticDeriv(
        const CallOptionsSomatic& opt)
    {
        using namespace SOMATIC_GT;

        assert(opt.germlineSVPrior < 0.5);

        prior[SOM] = opt.somaticSVPrior;
        prior[NOISE] = opt.largeNoiseSVPrior;

        prior[HET] = opt.germlineSVPrior;
        prior[HOM] = opt.germlineSVPrior/2;

        // this assumes all states independent, and somatic and noise only occur on germline ref GT background:
        const float nonref(prior[SOM]+prior[NOISE]+prior[HET]+prior[HOM]);
        assert(nonref>=0 && nonref<=1);
        prior[REF] = 1 - nonref;

        for (unsigned i(0); i<SIZE; ++i)
        {
            _logPrior[i] = std::log(prior[i]);
        }

        smallNoisePrior = opt.smallNoiseSVPrior;
        largeNoisePrior = opt.largeNoiseSVPrior;
        logSmallNoisePrior = std::log(opt.smallNoiseSVPrior);
        logLargeNoisePrior = std::log(opt.largeNoiseSVPrior);
    }

    float
    logPrior(
        const unsigned gt,
        const float largeNoiseWeight) const
    {
        assert(largeNoiseWeight >= 0. && largeNoiseWeight <= 1.);

        if (gt != SOMATIC_GT::NOISE) return _logPrior[gt];

        if (largeNoiseWeight <= 0.) return logSmallNoisePrior;
        if (largeNoiseWeight >= 1.) return logLargeNoisePrior;

        return std::log((1-largeNoiseWeight)*smallNoisePrior + largeNoiseWeight*largeNoisePrior);
    }

private:
    std::array<float,SOMATIC_GT::SIZE> prior;
    std::array<float,SOMATIC_GT::SIZE> _logPrior;

    float smallNoisePrior;
    float largeNoisePrior;
    float logSmallNoisePrior;
    float logLargeNoisePrior;
};



struct SVScorer
{
    SVScorer(
        const GSCOptions& opt,
        const SVLocusScanner& readScanner,
        const bam_header_info& header);

    /// gather supporting evidence and generate:
    /// 1) diploid quality score and genotype for SV candidate
    /// 2) somatic quality score
    void
    scoreSV(
        const SVCandidateSetData& svData,
        const std::vector<SVCandidateAssemblyData>& mjAssemblyData,
        const SVMultiJunctionCandidate& mjSV,
        const std::vector<bool>& isJunctionFiltered,
        const bool isSomatic,
        const bool isTumorOnly,
        std::vector<SVModelScoreInfo>& mjModelScoreInfo,
        SVModelScoreInfo& mjJointModelScoreInfo,
        bool& isMJEvent);

    typedef std::shared_ptr<SVScorePairProcessor> pairProcPtr;
    typedef std::shared_ptr<bam_streamer> streamPtr;

    unsigned
    sampleCount() const
    {
        return _sampleCount;
    }

    unsigned
    diploidSampleCount() const
    {
        return _diploidSampleCount;
    }

    const std::vector<std::string>&
    sampleNames() const
    {
        return _sampleNames;
    }

private:

    void
    processExistingAltPairInfo(
        const PairOptions& pairOpt,
        const SVCandidateSetData& svData,
        const SVCandidate& sv,
        SVEvidence& evidence);

    /// estimate pair support for an sv candidate
    /// restricted to simple indel style svs
    void
    getSVAltPairSupport(
        const PairOptions& pairOpt,
        const SVCandidateAssemblyData& assemblyData,
        const SVCandidate& sv,
        SVEvidence& evidence,
        std::vector<pairProcPtr>& pairProcList);

    /// find spanning read support for the reference allele for sv candidate
    void
    getSVRefPairSupport(
        const PairOptions& pairOpt,
        const SVCandidate& sv,
        SVEvidence& evidence,
        std::vector<pairProcPtr>& pairProcList);

    /// find paired read support for ref and alt alleles
    void
    getSVPairSupport(
        const SVCandidateSetData& svData,
        const SVCandidateAssemblyData& assemblyData,
        const SVCandidate& sv,
        SVEvidence& evidence);

    /// find split read support for ref and alt alleles
    void
    getSVSplitReadSupport(
        const SVCandidateAssemblyData& assemblyData,
        const SVCandidate& sv,
        SVScoreInfo& ssInfo,
        SVEvidence& evidence);

    /// determine maximum depth and MQ0 frac in region around breakend of normal sample
    void
    getBreakendMaxMappedDepthAndMQ0(
        const bool isMaxDepth,
        const bool isTumorOnly,
        const double cutoffDepth,
        const SVBreakend& bp,
        unsigned& maxDepth,
        float& MQ0Frac);

    /// apply all scoring models relevant to this event:
    void
    computeAllScoreModels(
        const bool isSomatic,
        const bool isTumorOnly,
        const std::vector<JunctionCallInfo>& junctionData,
        SVModelScoreInfo& modelScoreInfo);

    /// shared information gathering steps of all scoring models
    void
    scoreSV(
        const SVCandidateSetData& svData,
        const SVCandidateAssemblyData& assemblyData,
        const bool isTumorOnly,
        const SVCandidate& sv,
        SVScoreInfo& ssInfo,
        SVEvidence& evidence);

    const std::vector<bool> _isAlignmentTumor;
    const bool _isRNA;
    const CallOptionsShared _callOpt;
    const CallOptionsSharedDeriv _callDopt;
    const CallOptionsDiploid _diploidOpt;
    const CallOptionsDiploidDeriv _diploidDopt;
    const ReadScannerOptions _scanOpt;
    const SVRefinerOptions _refineOpt;
    const CallOptionsSomatic _somaticOpt;
    const CallOptionsSomaticDeriv _somaticDopt;
    const CallOptionsTumor _tumorOpt;
    const ChromDepthFilterUtil _dFilterDiploid;
    const ChromDepthFilterUtil _dFilterSomatic;
    const ChromDepthFilterUtil _dFilterTumor;
    const SVLocusScanner& _readScanner;

    std::vector<streamPtr> _bamStreams;

    unsigned _sampleCount;
    unsigned _diploidSampleCount;
    std::vector<std::string> _sampleNames;
};
