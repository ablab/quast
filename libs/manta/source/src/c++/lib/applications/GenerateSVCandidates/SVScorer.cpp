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

#include "SVScorer.hh"

#include "SVScorePairAltProcessor.hh"

#include "blt_util/LinearScaler.hh"
#include "blt_util/math_util.hh"
#include "blt_util/prob_util.hh"
#include "blt_util/qscore.hh"
#include "common/Exceptions.hh"
#include "htsapi/align_path_bam_util.hh"
#include "htsapi/bam_header_util.hh"
#include "htsapi/bam_streamer.hh"
#include "manta/ReadGroupStatsSet.hh"
#include "manta/SVCandidateUtil.hh"

#include <algorithm>
#include <iostream>
#include <string>


//#define DEBUG_SCORE
//#define DEBUG_SOMATIC_SCORE

#if defined(DEBUG_SCORE) || defined(DEBUG_SOMATIC_SCORE)
#define ANY_DEBUG_SCORE
#endif

#ifdef ANY_DEBUG_SCORE
#include "blt_util/log.hh"
#endif




SVScorer::
SVScorer(
    const GSCOptions& opt,
    const SVLocusScanner& readScanner,
    const bam_header_info& header) :
    _isAlignmentTumor(opt.alignFileOpt.isAlignmentTumor),
    _isRNA(opt.isRNA),
    _callOpt(opt.callOpt),
    _callDopt(_callOpt),
    _diploidOpt(opt.diploidOpt),
    _diploidDopt(_diploidOpt),
    _scanOpt(opt.scanOpt),
    _refineOpt(opt.refineOpt),
    _somaticOpt(opt.somaticOpt),
    _somaticDopt(_somaticOpt),
    _tumorOpt(opt.tumorOpt),
    _dFilterDiploid(opt.chromDepthFilename, _diploidOpt.maxDepthFactor, header),
    _dFilterSomatic(opt.chromDepthFilename, _somaticOpt.maxDepthFactor, header),
    _dFilterTumor(opt.chromDepthFilename, _tumorOpt.maxDepthFactor, header),
    _readScanner(readScanner)
{
    // setup regionless bam_streams:
    // setup all data for main analysis loop:
    for (const std::string& afile : opt.alignFileOpt.alignmentFilename)
    {
        // avoid creating shared_ptr temporaries:
        streamPtr tmp(new bam_streamer(afile.c_str()));
        _bamStreams.push_back(tmp);
    }

    _sampleCount=0;
    _diploidSampleCount=0;
    for (const bool isTumor : opt.alignFileOpt.isAlignmentTumor)
    {
        _sampleCount ++;
        if (! isTumor) _diploidSampleCount++;
    }

    // initialize sampleNames from all bam headers (assuming 1 sample per bam for now)
    const unsigned bamCount(_bamStreams.size());
    for (unsigned bamIndex(0); bamIndex<bamCount; ++bamIndex)
    {
        const bam_hdr_t* indexHeader(_bamStreams[bamIndex]->get_header());
        std::ostringstream oss;
        oss << "SAMPLE" << bamIndex;
        const char* defaultName(oss.str().c_str());
        std::string name(get_bam_header_sample_name(indexHeader,defaultName));
        // remove spaces from sample name for everyone's sanity:
        std::replace(name.begin(), name.end(), ' ', '_');
        _sampleNames.push_back(name);
    }
}



/// add bam alignment to simple short-range vector depth estimate
///
/// \param[in] beginPos this is the begin position of the range covered by the depth array
///
static
void
addReadToDepthEst(
    const bam_record& bamRead,
    const pos_t beginPos,
    std::vector<unsigned>& depth)
{
    using namespace ALIGNPATH;

    const pos_t endPos(beginPos+depth.size());

    // get cigar:
    path_t apath;
    bam_cigar_to_apath(bamRead.raw_cigar(), bamRead.n_cigar(), apath);

    pos_t refPos(bamRead.pos()-1);
    for (const path_segment& ps : apath)
    {
        if (refPos>=endPos) return;

        if (is_segment_align_match(ps.type))
        {
            for (pos_t pos(refPos); pos < (refPos+static_cast<pos_t>(ps.length)); ++pos)
            {
                if (pos>=beginPos)
                {
                    if (pos>=endPos) return;
                    depth[pos-beginPos]++;
                }
            }
        }
        if (is_segment_type_ref_length(ps.type)) refPos += ps.length;
    }
}



void
SVScorer::
getBreakendMaxMappedDepthAndMQ0(
    const bool isTumorOnly,
    const bool isMaxDepth,
    const double cutoffDepth,
    const SVBreakend& bp,
    unsigned& maxDepth,
    float& MQ0Frac)
{
    /// define a new interval -/+ 50 bases around the center pos
    /// of the breakpoint
    static const pos_t regionSize(50);

    maxDepth=0;
    MQ0Frac=0;

    unsigned totalReads(0);
    unsigned totalMQ0Reads(0);

    const pos_t centerPos(bp.interval.range.center_pos());
    const known_pos_range2 searchRange(std::max((centerPos-regionSize),0), (centerPos+regionSize));

    if (searchRange.size() == 0) return;

    std::vector<unsigned> depth(searchRange.size(),0);

    bool isCutoff(false);
    bool isBamFound(false);

    const unsigned bamCount(_bamStreams.size());
    for (unsigned bamIndex(0); bamIndex < bamCount; ++bamIndex)
    {
        if ((!isTumorOnly) && (_isAlignmentTumor[bamIndex])) continue;
        isBamFound=true;

        bam_streamer& bamStream(*_bamStreams[bamIndex]);

        // set bam stream to new search interval:
        bamStream.set_new_region(bp.interval.tid, searchRange.begin_pos(), searchRange.end_pos());

        while (bamStream.next())
        {
            const bam_record& bamRead(*(bamStream.get_record_ptr()));

            // turn filtration down to mapped only to match depth estimate method:
            if (bamRead.is_unmapped()) continue;

            const pos_t refPos(bamRead.pos()-1);
            if (refPos >= searchRange.end_pos()) break;

            addReadToDepthEst(bamRead,searchRange.begin_pos(),depth);

            totalReads++;
            if (0 == bamRead.map_qual()) totalMQ0Reads++;

            if (isMaxDepth)
            {
                const pos_t depthOffset(refPos-searchRange.begin_pos());
                if (depthOffset>=0)
                {
                    if (depth[depthOffset] > cutoffDepth)
                    {
                        isCutoff=true;
                        break;
                    }
                }
            }
        }

        if (isCutoff) break;
    }

    assert(isBamFound);

    maxDepth = *(std::max_element(depth.begin(),depth.end()));
    if (totalReads>=10)
    {
        MQ0Frac = static_cast<float>(totalMQ0Reads)/static_cast<float>(totalReads);
    }
}



static
void
lnToProb(
    float& lower,
    float& higher)
{
    lower = std::exp(lower-higher);
    higher = 1/(lower+1);
    lower  = lower/(lower+1);
}



/// return false if no split read support
static
bool
getSampleSplitReadLnLhood(
    const SVFragmentEvidence& fragev,
    const bool isRead1,
    float& refLnLhood,
    float& altLnLhood,
    const bool isForcedSupport = false)
{
    refLnLhood = 1.;
    altLnLhood = 1.;

    const std::pair<bool,bool> isBpSupport(fragev.isAnySplitReadSupport(isRead1));
    if (! isForcedSupport)
    {
        if (! (isBpSupport.first || isBpSupport.second)) return false;
    }

    bool isUseBp1Score(isBpSupport.first);

    if (isForcedSupport || (isBpSupport.first == isBpSupport.second))
    {
        isUseBp1Score = (fragev.alt.bp1.getRead(isRead1).splitLnLhood >= fragev.alt.bp2.getRead(isRead1).splitLnLhood);
    }

    altLnLhood =
        ( isUseBp1Score ?
          fragev.alt.bp1.getRead(isRead1).splitLnLhood :
          fragev.alt.bp2.getRead(isRead1).splitLnLhood);

    if (isBpSupport.first && isBpSupport.second)
    {
        isUseBp1Score = (fragev.ref.bp1.getRead(isRead1).splitLnLhood >= fragev.ref.bp2.getRead(isRead1).splitLnLhood);
    }

    refLnLhood =
        ( isUseBp1Score ?
          fragev.ref.bp1.getRead(isRead1).splitLnLhood :
          fragev.ref.bp2.getRead(isRead1).splitLnLhood);

    return true;
}



static
void
addConservativeSplitReadSupport(
    const SVFragmentEvidence& fragev,
    const bool isRead1,
    SVSampleInfo& sampleBaseInfo)
{
    static const float splitSupportProb(0.999f);

    // only consider reads where at least one allele and one breakend is confident
    //
    // ...note this is done in the absence of having a noise state in the model
    //
    float refLnLhood;
    float altLnLhood;
    if (! getSampleSplitReadLnLhood(fragev, isRead1, refLnLhood, altLnLhood)) return;

    // convert to normalized prob:
    if (altLnLhood > refLnLhood)
    {
        lnToProb(refLnLhood, altLnLhood);
        if (altLnLhood > splitSupportProb) sampleBaseInfo.alt.confidentSplitReadCount++;
    }
    else
    {
        lnToProb(altLnLhood, refLnLhood);
        if (refLnLhood > splitSupportProb)
        {
            sampleBaseInfo.ref.confidentSplitReadCount++;
            if (fragev.ref.bp1.getRead(isRead1).isSplitSupport) sampleBaseInfo.ref.confidentSplitReadAndPairCountRefBp1++;
            if (fragev.ref.bp2.getRead(isRead1).isSplitSupport) sampleBaseInfo.ref.confidentSplitReadAndPairCountRefBp2++;
        }
    }
}



static
float
getSpanningPairAlleleLhood(
    const SVFragmentEvidenceAllele& allele)
{
    float fragProb(0);
    if (allele.bp1.isFragmentSupport)
    {
        fragProb = allele.bp1.fragLengthProb;
    }

    if (allele.bp2.isFragmentSupport)
    {
        fragProb = std::max(fragProb, allele.bp2.fragLengthProb);
    }

    return fragProb;
}

static
void
addSpanningPairSupport(
    const SVFragmentEvidence& fragev,
    SVSampleInfo& sampleBaseInfo)
{
    if (fragev.alt.bp1.isFragmentSupport || fragev.alt.bp2.isFragmentSupport)
    {
        sampleBaseInfo.alt.spanningPairCount++;
    }
    if (fragev.ref.bp1.isFragmentSupport || fragev.ref.bp2.isFragmentSupport)
    {
        sampleBaseInfo.ref.spanningPairCount++;
    }
}

static
void
addConservativeSpanningPairSupport(
    const SVFragmentEvidence& fragev,
    SVSampleInfo& sampleBaseInfo)
{
    static const float pairSupportProb(0.9f);

    if (! fragev.isAnySpanningPairSupport()) return;

    float altLhood(getSpanningPairAlleleLhood(fragev.alt));
    float refLhood(getSpanningPairAlleleLhood(fragev.ref));

    assert(altLhood >= 0);
    assert(refLhood >= 0);
    if ((altLhood <= 0) && (refLhood <= 0))
    {
        using namespace illumina::common;

        std::ostringstream oss;
        oss << "ERROR: Spanning likelihood is zero for all alleles. Fragment: " << fragev << "\n";
        BOOST_THROW_EXCEPTION(LogicException(oss.str()));
    }

    static const bool isTier2(false);
    const bool isFullyMapped(fragev.read1.isObservedAnchor(isTier2) && fragev.read2.isObservedAnchor(isTier2));

    // convert to normalized prob:
    const float sum(altLhood+refLhood);
    if (altLhood > refLhood)
    {
        if ((altLhood/sum) > pairSupportProb)
        {
#ifdef DEBUG_SCORE
            log_os << __FUNCTION__ << ": semi-mapped alt pair support\n";
#endif
            sampleBaseInfo.alt.confidentSemiMappedSpanningPairCount++;
            if (isFullyMapped)
            {
#ifdef DEBUG_SCORE
                log_os << __FUNCTION__ << ": fully-mapped alt pair support\n";
#endif
                sampleBaseInfo.alt.confidentSpanningPairCount++;
            }
        }
    }
    else
    {
        if ((refLhood/sum) > pairSupportProb)
        {
#ifdef DEBUG_SCORE
            log_os << __FUNCTION__ << ": semi-mapped ref pair support\n";
#endif
            sampleBaseInfo.ref.confidentSemiMappedSpanningPairCount++;
            if (isFullyMapped)
            {
#ifdef DEBUG_SCORE
                log_os << __FUNCTION__ << ": fully-mapped ref pair support\n";
#endif
                sampleBaseInfo.ref.confidentSpanningPairCount++;
                if (fragev.ref.bp1.isFragmentSupport) sampleBaseInfo.ref.confidentSplitReadAndPairCountRefBp1++;
                if (fragev.ref.bp2.isFragmentSupport) sampleBaseInfo.ref.confidentSplitReadAndPairCountRefBp2++;
            }
        }
    }
}



static
void
getSampleCounts(
    const SVEvidence::evidenceTrack_t& sampleEvidence,
    SVSampleInfo& sampleBaseInfo)
{
    for (const SVEvidence::evidenceTrack_t::value_type& val : sampleEvidence)
    {
        const SVFragmentEvidence& fragev(val.second);
#ifdef DEBUG_SCORE
        log_os << __FUNCTION__ << ": Counting read: " << val.first << "\n";
#endif
        // evaluate read1 and read2 from this fragment
        //
        addConservativeSplitReadSupport(fragev,true,sampleBaseInfo);
        addConservativeSplitReadSupport(fragev,false,sampleBaseInfo);
        addSpanningPairSupport(fragev, sampleBaseInfo);
        addConservativeSpanningPairSupport(fragev, sampleBaseInfo);
    }
}



/// get conservative count of reads which support only one allele, ie. P ( allele | read ) is high
///
static
void
getSVSupportSummary(
    const SVEvidence& evidence,
    SVScoreInfo& baseInfo)
{
    const unsigned sampleCount(baseInfo.samples.size());
    assert(sampleCount == evidence.samples.size());

    for (unsigned sampleIndex(0); sampleIndex<sampleCount; ++sampleIndex)
    {
        getSampleCounts(evidence.getSampleEvidence(sampleIndex), baseInfo.samples[sampleIndex]);
    }
}



static
void
resolvePairSplitConflictsSample(
    const bool isFindAltPairConflict,
    SVEvidence::evidenceTrack_t& sampleEvidence)
{
    for (SVEvidence::evidenceTrack_t::value_type& val : sampleEvidence)
    {
#ifdef DEBUG_SCORE
        log_os << __FUNCTION__ << ": conflict check for " << val.first << "\n";
#endif
        SVFragmentEvidence& fragev(val.second);

        /// filtration scheme only works if there's pair and split support for the same fragment:
        if (! fragev.isAnySpanningPairSupport()) continue;
        //    if (! (fragev.isAnySplitReadSupport(true) || fragev.isAnySplitReadSupport(false))) continue;

        // if there's a difference in fragment support for one "frag-favored" allele, then
        // there must also be either neutral split support or split support in favor of the same allele

        const float refPairLhood(getSpanningPairAlleleLhood(fragev.ref));
        const float altPairLhood(getSpanningPairAlleleLhood(fragev.alt));

        static const bool isForcedSupport(true);
        float refSplitLnLhoodRead1;
        float altSplitLnLhoodRead1;
        const bool isRead1Split(getSampleSplitReadLnLhood(fragev, true, refSplitLnLhoodRead1, altSplitLnLhoodRead1, isForcedSupport));

        float refSplitLnLhoodRead2;
        float altSplitLnLhoodRead2;
        const bool isRead2Split(getSampleSplitReadLnLhood(fragev, false, refSplitLnLhoodRead2, altSplitLnLhoodRead2, isForcedSupport));

#ifdef DEBUG_SCORE
        log_os << __FUNCTION__ << ": fragev " << fragev << "\n";
        log_os << __FUNCTION__ << ": r1/r2 " << isRead1Split << " " << isRead2Split << "\n";
#endif
        const bool isRefPair(refPairLhood > altPairLhood);
        const bool isAltPair(altPairLhood > refPairLhood);

        if (isAltPair)
        {
            if (! isFindAltPairConflict) continue;
        }

        if (isRead1Split)
        {
            if (altSplitLnLhoodRead1 > refSplitLnLhoodRead1)
            {
                if (isRefPair)
                {
#ifdef DEBUG_SCORE
                    log_os << __FUNCTION__ << ": clearing alt1/ref\n";
#endif
                    fragev.clearPairSupport();
                }
            }
            if (refSplitLnLhoodRead1 > altSplitLnLhoodRead1)
            {
                if (isAltPair)
                {
#ifdef DEBUG_SCORE
                    log_os << __FUNCTION__ << ": clearing ref1/alt\n";
#endif
                    fragev.clearPairSupport();
                }
            }
        }

        if (isRead2Split)
        {
            if (altSplitLnLhoodRead2 > refSplitLnLhoodRead2)
            {
                if (isRefPair)
                {
#ifdef DEBUG_SCORE
                    log_os << __FUNCTION__ << ": clearing alt2/ref\n";
#endif
                    fragev.clearPairSupport();
                }
            }
            if (refSplitLnLhoodRead2 > altSplitLnLhoodRead2)
            {
                if (isAltPair)
                {
#ifdef DEBUG_SCORE
                    log_os << __FUNCTION__ << ": clearing ref2/alt\n";
#endif
                    fragev.clearPairSupport();
                }
            }
        }
    }
}



/// check for cases where pair support was added in error, the fragment does span the breakpoint, but the
/// alignment past the breakpoint is poor, and is better in the alt allele.
///
/// note this might be done more naturally during the pair computation, but all the info we need is added
/// during the split routine, so it's at least checked here as well:
static
void
resolvePairSplitConflicts(
    const SVCandidate& sv,
    SVEvidence& evidence)
{
    if (sv.isImprecise()) return;

    static const pos_t maxAltPairConflictSearch(1000);
    const bool isFindAltPairConflict(sv.centerSize() <= maxAltPairConflictSearch);

    const unsigned sampleCount(evidence.size());

    for (unsigned sampleIndex(0); sampleIndex<sampleCount; ++sampleIndex)
    {
        resolvePairSplitConflictsSample(isFindAltPairConflict, evidence.getSampleEvidence(sampleIndex));
    }
}



/// shared information gathering steps of all scoring models
void
SVScorer::
scoreSV(
    const SVCandidateSetData& svData,
    const SVCandidateAssemblyData& assemblyData,
    const bool isTumorOnly,
    const SVCandidate& sv,
    SVScoreInfo& baseInfo,
    SVEvidence& evidence)
{
    // at what factor above the maxDepth FILTER criteria do we stop enumerating scoring components?
    static const unsigned cutoffDepthFactor(2);

    const bool isMaxDepth(isTumorOnly ?
                          _dFilterTumor.isMaxDepthFilter() :
                          (_dFilterDiploid.isMaxDepthFilter() && _dFilterSomatic.isMaxDepthFilter()));

    double bp1CutoffDepth(0);
    double bp2CutoffDepth(0);
    if (isMaxDepth)
    {
        const double bp1MaxMaxDepth(std::max(_dFilterDiploid.maxDepth(sv.bp1.interval.tid), _dFilterSomatic.maxDepth(sv.bp1.interval.tid)));
        const double bp2MaxMaxDepth(std::max(_dFilterDiploid.maxDepth(sv.bp2.interval.tid), _dFilterSomatic.maxDepth(sv.bp2.interval.tid)));

        bp1CutoffDepth = cutoffDepthFactor*bp1MaxMaxDepth;
        bp2CutoffDepth = cutoffDepthFactor*bp2MaxMaxDepth;
    }

    // get breakend center_pos depth estimate:
    getBreakendMaxMappedDepthAndMQ0(isTumorOnly, isMaxDepth, bp1CutoffDepth, sv.bp1, baseInfo.bp1MaxDepth, baseInfo.bp1MQ0Frac);
    const bool isBp1OverDepth(baseInfo.bp1MaxDepth > bp1CutoffDepth);
    if (! (isMaxDepth && isBp1OverDepth))
    {
        getBreakendMaxMappedDepthAndMQ0(isTumorOnly, isMaxDepth, bp2CutoffDepth, sv.bp2, baseInfo.bp2MaxDepth, baseInfo.bp2MQ0Frac);
    }
    const bool isBp2OverDepth(baseInfo.bp2MaxDepth > bp2CutoffDepth);

    const bool isOverDepth(isBp1OverDepth || isBp2OverDepth);
    const bool isSkipEvidenceSearch(isMaxDepth && isOverDepth);

    if (! isSkipEvidenceSearch)
    {
        // count the paired-read fragments supporting the ref and alt alleles in each sample:
        //
        getSVPairSupport(svData, assemblyData, sv, evidence);

        // count the split reads supporting the ref and alt alleles in each sample
        //
        getSVSplitReadSupport(assemblyData, sv, baseInfo, evidence);

        // fix erroneous pair support based on split evidence:
        resolvePairSplitConflicts(sv, evidence);
    }

    // compute allele likelihoods, and any other summary metric shared between all models:
    //
    getSVSupportSummary(evidence, baseInfo);
}



/// record a set of convenient companion values for any probability
///
struct ProbSet
{
    explicit
    ProbSet(const double initProb) :
        prob(initProb),
        comp(1-prob),
        lnProb(std::log(prob)),
        lnComp(std::log(comp))
    {}

    double prob;
    double comp;
    double lnProb;
    double lnComp;
};



static
void
incrementSpanningPairAlleleLnLhood(
    const ProbSet& selfChimeraProb,
    const ProbSet& otherChimeraProb,
    const SVFragmentEvidenceAllele& allele,
    const double power,
    double& bpLnLhood)
{
    const float fragProb(getSpanningPairAlleleLhood(allele));
    bpLnLhood += std::log(selfChimeraProb.comp*fragProb + otherChimeraProb.prob)*power;
}



static
double
incrementAlleleSplitReadLhood(
    const ProbSet& selfMapProb,
    const ProbSet& otherMapProb,
    const SVFragmentEvidenceAllele& allele,
    const double /*readLnPrior*/,
    const std::pair<bool,bool>& isSupported,
    const bool isRead1,
    bool& isReadEvaluated)
{
    if (! (allele.bp1.getRead(isRead1).isSplitEvaluated &&
           allele.bp2.getRead(isRead1).isSplitEvaluated))
    {
        isReadEvaluated = false;
    }

    const double alignBp1LnLhood(allele.bp1.getRead(isRead1).splitLnLhood);
    const double alignBp2LnLhood(allele.bp2.getRead(isRead1).splitLnLhood);

    bool isUseBp1Lhood(isSupported.first);
    if (isSupported.first && isSupported.second)
    {
        isUseBp1Lhood=(alignBp1LnLhood >= alignBp2LnLhood);
    }

    const double alignLnLhood( isUseBp1Lhood ? alignBp1LnLhood : alignBp2LnLhood );

    const double fragLnLhood = log_sum((selfMapProb.lnComp+alignLnLhood), (otherMapProb.lnProb)); //+readLnPrior));

#ifdef DEBUG_SCORE
    static const std::string logtag("incrementAlleleSplitReadLhood: ");
    log_os << logtag //<< "readPrior: " << readLnPrior
           << " isRead1?: " << isRead1 << "\n";
    log_os << logtag << "isEval " << isReadEvaluated << "\n";
    log_os << logtag << "alignBp1LnLhood " << alignBp1LnLhood << "\n";
    log_os << logtag << "alignBp2LnLhood " << alignBp2LnLhood << "\n";
    log_os << logtag << "selfMap " << selfMapProb.lnProb << "\n";
    log_os << logtag << "otherMap " << otherMapProb.lnProb << "\n";
    log_os << logtag << "increment " << fragLnLhood << "\n";
#endif

    return fragLnLhood;
}



static
void
incrementSplitReadLhood(
    const std::string& /*fragLabel*/,
    const SVFragmentEvidence& fragev,
    const ProbSet& refMapProb,
    const ProbSet& altMapProb,
    const bool isPermissive,
    const bool isRead1,
    double& refSplitLnLhood,
    double& altSplitLnLhood,
    bool& isReadEvaluated)
{
    static const double baseLnPrior(std::log(0.25));

#ifdef DEBUG_SCORE
    log_os << __FUNCTION__ << ": pre-support\n";
#endif

    std::pair<bool,bool> isSupported;
    if (isPermissive)
    {
        isSupported=fragev.isAnyTier2SplitReadSupport(isRead1);
    }
    else
    {
        isSupported=fragev.isAnySplitReadSupport(isRead1);
    }

    if (! (isSupported.first || isSupported.second))
    {
        isReadEvaluated = false;
        return;
    }

#ifdef DEBUG_SCORE
    log_os << __FUNCTION__ << ": post-support\n";
#endif

    const unsigned readSize(fragev.getRead(isRead1).size);
    const double readLnPrior(baseLnPrior*readSize);

#ifdef DEBUG_SCORE
    log_os << __FUNCTION__ << ": starting ref\n";
#endif
    const double refSplit = incrementAlleleSplitReadLhood(refMapProb, altMapProb, fragev.ref, readLnPrior, isSupported, isRead1, isReadEvaluated);
#ifdef DEBUG_SCORE
    log_os << __FUNCTION__ << ": starting alt\n";
#endif
    const double altSplit = incrementAlleleSplitReadLhood(altMapProb, refMapProb, fragev.alt, readLnPrior, isSupported, isRead1, isReadEvaluated);

    // filter out split read evidence with a poor alignment to both alleles:
    /// TODO: fraction of these noise reads could be informative for filtration
    // if ((refSplit < (altMapProb.lnProb+1)) && (altSplit < (refMapProb.lnProb+1))) return;

    refSplitLnLhood += refSplit;
    altSplitLnLhood += altSplit;
}



struct AlleleLnLhood
{
    double fragPair = 0.;
    double read1Split = 0.;
    double read2Split = 0.;
};



static
double
getFragLnLhood(
    const AlleleLnLhood& al,
    const bool isRead1Evaluated,
    const bool isRead2Evaluated)
{
#ifdef DEBUG_SCORE
    log_os << "getFragLnLhood: frag/read1/read2 " << al.fragPair << " " << al.read1Split << " " << al.read2Split << "\n";
    log_os << "getFragLnLhood: isread1/isread2 " << isRead1Evaluated << " " << isRead2Evaluated << "\n";
#endif

    double ret(al.fragPair);

    // limit split read evidence to only one read, b/c it's only possible for one section
    // of the molecule to independently cross the breakend:
    if (isRead1Evaluated)
    {
        if (isRead2Evaluated)
        {
            ret += std::max(al.read1Split, al.read2Split);
        }
        else
        {
            ret += al.read1Split;
        }
    }
    else if (isRead2Evaluated)
    {
        ret += al.read2Split;
    }

    return ret;
}



/// when an sv is treated as 'small', we skip all paired-read evidence and rely on split reads only:
///
/// with further model improvements we can add pairs back into the small variant calls:
///
/// this function returns 1 for a variant which is "fully large" and 0 for a variant which is "fully small",
/// with intermediate values for sizes in between
///
static
float
getSpanningPairWeight(
    const SVCandidate& sv)
{
    const auto svType(getExtendedSVType(sv));
    if (! ((svType == EXTENDED_SV_TYPE::INSERT) || (svType == EXTENDED_SV_TYPE::DELETE))) return 1.f;

    if ((svType == EXTENDED_SV_TYPE::INSERT) &&
        SVScorePairAltProcessor::isLargeInsertSV(sv))
    {
        static const int minInsertSmallSize(100);
        static const int maxInsertSmallSize(150);
        static const LinearScaler<int> insertSizeRamp(minInsertSmallSize, maxInsertSmallSize);

        return insertSizeRamp.getScale(sv.insertSeq.size());
    }
    else
    {
        /// TODO set these numbers from insert size:
        static const int minSmallSize(300);
        static const int maxSmallSize(500);
        static const LinearScaler<int> svSizeRamp(minSmallSize, maxSmallSize);

        return svSizeRamp.getScale(sv.centerSize());
    }
}



static
float
largeNoiseSVPriorWeight(
    const SVCandidate& sv)
{
    static const int smallSize(5000);
    static const int largeSize(10000);
    static const LinearScaler<int> svSizeRamp(smallSize, largeSize);

    if (sv.bp1.interval.tid != sv.bp2.interval.tid) return 1.f;

    return svSizeRamp.getScale(sv.centerSize());
}



/// return true if any evidence exists for fragment:
///
/// \param semiMappedPower multiply out semi-mapped reads (in log space) by this value
///
static
bool
getRefAltFromFrag(
    const float spanningPairWeight,
    const double semiMappedPower,
    const ProbSet& refChimeraProb,
    const ProbSet& altChimeraProb,
    const ProbSet& refSplitMapProb,
    const ProbSet& altSplitMapProb,
    const bool isPermissive,
    const std::string& fragLabel,
    const SVFragmentEvidence& fragev,
    AlleleLnLhood& refLnLhoodSet,
    AlleleLnLhood& altLnLhoodSet,
    bool& isRead1Evaluated,
    bool& isRead2Evaluated)
{
#ifdef DEBUG_SCORE
    log_os << __FUNCTION__ << ": qname: " << fragLabel << " fragev: " << fragev << "\n";
#endif

    /// TODO: add read pairs with one shadow read to the alt read pool

    bool isFragEvaluated(false);

    // high-quality spanning support relies on read1 and read2 mapping well:
    bool isPairUsable;
    if (isPermissive)
    {
        isPairUsable = (fragev.read1.isObservedAnchor(isPermissive) || fragev.read2.isObservedAnchor(isPermissive));
    }
    else
    {
        isPairUsable = ((fragev.read1.isScanned && fragev.read2.isScanned) &&
                        (fragev.read1.isAnchored(isPermissive) || fragev.read2.isAnchored(isPermissive)));
    }

    if (isPairUsable)
    {
        /// only add to the likelihood if the fragment supports at least one allele:
        if ( fragev.isAnySpanningPairSupport() )
        {
            // reduce the impact of spanning reads to zero as svs become small, this is because of complex signal/noise
            // which the scoring models haven't (yet) been designed to handle.
            const bool isSemiMapped(! (fragev.read1.isAnchored(isPermissive) && fragev.read2.isAnchored(isPermissive)));
            double spanPower(spanningPairWeight);

            if (isSemiMapped)
            {
                // only count semi-mapped reads for the alt allele
                if (getSpanningPairAlleleLhood(fragev.alt) > getSpanningPairAlleleLhood(fragev.ref))
                {
                    spanPower *= semiMappedPower;
                }
                else
                {
                    spanPower = 0.;
                }
            }

            incrementSpanningPairAlleleLnLhood(refChimeraProb, altChimeraProb, fragev.ref, spanPower, refLnLhoodSet.fragPair);
            incrementSpanningPairAlleleLnLhood(altChimeraProb, refChimeraProb, fragev.alt, spanPower, altLnLhoodSet.fragPair);
            isFragEvaluated=true;
        }
    }

    /// split support is less dependent on mapping quality of the individual read, because
    /// we're potentially relying on shadow reads recovered from the unmapped state
    isRead1Evaluated = true;
    isRead2Evaluated = true;
#ifdef DEBUG_SCORE
    log_os << __FUNCTION__ << ": starting read1 split\n";
#endif
    incrementSplitReadLhood(fragLabel, fragev, refSplitMapProb, altSplitMapProb, isPermissive, true,  refLnLhoodSet.read1Split, altLnLhoodSet.read1Split, isRead1Evaluated);
#ifdef DEBUG_SCORE
    log_os << __FUNCTION__ << ": starting read2 split\n";
#endif
    incrementSplitReadLhood(fragLabel, fragev, refSplitMapProb, altSplitMapProb, isPermissive, false, refLnLhoodSet.read2Split, altLnLhoodSet.read2Split, isRead2Evaluated);

#ifdef DEBUG_SCORE
    log_os << __FUNCTION__ << ": iseval frag/read1/read2: " << isFragEvaluated << " " << isRead1Evaluated << " " << isRead1Evaluated << "\n";
#endif
    return (isFragEvaluated || isRead1Evaluated || isRead2Evaluated);
}



/// score diploid germline specific components:
static
void
addDiploidLoglhood(
    const float spanningPairWeight,
    const SVEvidence::evidenceTrack_t& sampleEvidence,
    std::array<double,DIPLOID_GT::SIZE>& loglhood)
{
    for (const SVEvidence::evidenceTrack_t::value_type& val : sampleEvidence)
    {
        const std::string& fragLabel(val.first);
        const SVFragmentEvidence& fragev(val.second);

        AlleleLnLhood refLnLhoodSet, altLnLhoodSet;
        bool isRead1Evaluated(true);
        bool isRead2Evaluated(true);

        /// TODO: set this from graph data:
        ///
        /// put some more thought into this -- is this P (spurious | any old read) or P( spurious | chimera ) ??
        /// it seems like it should be the latter in the usages that really matter.
        ///
        static const ProbSet chimeraProb(1e-3);

        /// use a constant mapping prob for now just to get the zero-th order concept into the model
        /// that "reads are mismapped at a non-trivial rate"
        /// TODO: experiment with per-read mapq values
        ///
        static const ProbSet refSplitMapProb(1e-6);
        static const ProbSet altSplitMapProb(1e-5);

        /// don't use semi-mapped reads for germline calling:
        static const double semiMappedPower(0.);

        static const bool isPermissive(false);

        if (! getRefAltFromFrag(spanningPairWeight, semiMappedPower, chimeraProb, chimeraProb,
                                refSplitMapProb, altSplitMapProb, isPermissive, fragLabel, fragev,
                                refLnLhoodSet, altLnLhoodSet, isRead1Evaluated, isRead2Evaluated))
        {
            // continue if this fragment was not evaluated for pair or split support for either allele:
            continue;
        }

        for (unsigned gt(0); gt<DIPLOID_GT::SIZE; ++gt)
        {
            using namespace DIPLOID_GT;

#ifdef DEBUG_SCORE
            log_os << __FUNCTION__ << ": starting gt: " << gt << " " << label(gt) << "\n";
#endif

            const index_t gtid(static_cast<const index_t>(gt));
            const double refLnFragLhood(getFragLnLhood(refLnLhoodSet, isRead1Evaluated, isRead2Evaluated));
#ifdef DEBUG_SCORE
            log_os << __FUNCTION__ << ": refLnFragLhood: " << refLnFragLhood << "\n";
#endif
            const double altLnFragLhood(getFragLnLhood(altLnLhoodSet, isRead1Evaluated, isRead2Evaluated));
#ifdef DEBUG_SCORE
            log_os << __FUNCTION__ << ": altLnFragLhood: " << altLnFragLhood << "\n";
#endif
            const double refLnLhood(refLnFragLhood + altLnCompFraction(gtid));
            const double altLnLhood(altLnFragLhood + altLnFraction(gtid));
            loglhood[gt] += log_sum(refLnLhood, altLnLhood);

#ifdef DEBUG_SCORE
            log_os << __FUNCTION__ << ": gt/fragref/ref/fragalt/alt/loglhood: "
                   << label(gt)
                   << " " << refLnFragLhood
                   << " " << refLnLhood
                   << " " << altLnFragLhood
                   << " " << altLnLhood
                   << " " << loglhood[gt]
                   << "\n";
#endif
        }
    }
}



/// score diploid germline specific components:
static
void
scoreDiploidSV(
    const CallOptionsDiploid& diploidOpt,
    const SVLocusScanner& readScanner,
    const CallOptionsDiploidDeriv& diploidDopt,
    const ChromDepthFilterUtil& dFilter,
    const std::vector<JunctionCallInfo>& junctionData,
    SVScoreInfoDiploid& diploidInfo)
{
    //
    // compute qualities
    //
    static const int maxQ(999);

    assert(! junctionData.empty());

    double jointRefProb(1.);

    const unsigned diploidSampleCount(diploidInfo.samples.size());
    for (unsigned diploidSampleIndex(0); diploidSampleIndex<diploidSampleCount; ++diploidSampleIndex)
    {
        SVScoreInfoDiploidSample& diploidSampleInfo(diploidInfo.samples[diploidSampleIndex]);

        std::array<double,DIPLOID_GT::SIZE> loglhood;
        std::fill(loglhood.begin(),loglhood.end(),0);
        for (const JunctionCallInfo& junction : junctionData)
        {
            const SVEvidence::evidenceTrack_t& etrack(junction.getEvidence().samples[diploidSampleIndex]);
            addDiploidLoglhood(junction.getSpanningWeight(), etrack, loglhood);
        }
        std::array<double,DIPLOID_GT::SIZE> pprob;
        for (unsigned gt(0); gt<DIPLOID_GT::SIZE; ++gt)
        {
            pprob[gt] = loglhood[gt] + diploidDopt.logPrior[gt];
        }

        unsigned maxGt(0);
        normalize_ln_distro(pprob.begin(), pprob.end(), maxGt);

#ifdef DEBUG_SCORE
        for (unsigned gt(0); gt<DIPLOID_GT::SIZE; ++gt)
        {
            log_os << __FUNCTION__ << ": gt/lhood/prior/pprob: "
                   << DIPLOID_GT::label(gt)
                   << " " << loglhood[gt]
                   << " " << diploidDopt.prior[gt]
                   << " " << pprob[gt]
                   << "\n";
        }
#endif

        diploidSampleInfo.gt=static_cast<DIPLOID_GT::index_t>(maxGt);
        diploidSampleInfo.gtScore=std::min(maxQ,error_prob_to_qphred(prob_comp(pprob.begin(),pprob.end(), diploidSampleInfo.gt)));

        // set phredLoghood:
        {
            unsigned maxIndex(0);
            for (unsigned gt(1); gt<DIPLOID_GT::SIZE; ++gt)
            {
                if (loglhood[gt] > loglhood[maxIndex]) maxIndex = gt;
            }
            for (unsigned gt(0); gt<DIPLOID_GT::SIZE; ++gt)
            {
                diploidSampleInfo.phredLoghood[gt] = std::min(maxQ,ln_error_prob_to_qphred(loglhood[gt]-loglhood[maxIndex]));
            }
        }

        jointRefProb *= pprob[DIPLOID_GT::REF];
    }
    diploidInfo.altScore=std::min(maxQ,error_prob_to_qphred(jointRefProb));


    //
    // apply filters
    //
    {
        if (diploidInfo.altScore < diploidOpt.minPassAltScore)
        {
            diploidInfo.filters.insert(diploidOpt.minAltFilterLabel);
        }

        // add sample specific filters
        bool isAllMinGTFiltered(true);
        for (unsigned sampleIndex(0); sampleIndex<diploidSampleCount; ++sampleIndex)
        {
            SVScoreInfoDiploidSample& diploidSampleInfo(diploidInfo.samples[sampleIndex]);
            if (diploidSampleInfo.gtScore < diploidOpt.minPassGTScore)
            {
                diploidSampleInfo.filters.insert(diploidOpt.minGTFilterLabel);
            }
            else
            {
                isAllMinGTFiltered=false;
            }
        }

        // apply sample-specific filter to whole record when all samples are impacted
        if ((diploidSampleCount>0) && isAllMinGTFiltered)
        {
            diploidInfo.filters.insert(diploidOpt.minGTFilterLabel);
        }

        const unsigned junctionCount(junctionData.size());

        // apply high depth filter:
        if (dFilter.isMaxDepthFilter())
        {
            unsigned filteredJunctionCount(0);
            for (const JunctionCallInfo& junction : junctionData)
            {
                const SVScoreInfo& baseInfo(junction.getBaseInfo());
                const SVCandidate& sv(junction.getSV());

                // apply maxdepth filter if either of the breakpoints exceeds the maximum depth:
                if ((baseInfo.bp1MaxDepth > dFilter.maxDepth(sv.bp1.interval.tid)) ||
                    (baseInfo.bp2MaxDepth > dFilter.maxDepth(sv.bp2.interval.tid)))
                {
                    filteredJunctionCount++;
                }
            }

            if ((filteredJunctionCount*2) > junctionCount)
            {
                diploidInfo.filters.insert(diploidOpt.maxDepthFilterLabel);
            }
        }

        // apply MQ0 filter
        {
            unsigned filteredJunctionCount(0);
            for (const JunctionCallInfo& junction : junctionData)
            {
                const SVScoreInfo& baseInfo(junction.getBaseInfo());
                const SVCandidate& sv(junction.getSV());

                const bool isMQ0FilterSize(isSVBelowMinSize(sv,1000));
                if (isMQ0FilterSize)
                {
                    // apply MQ0 filter for one junction if either breakend meets the filter criteria:
                    if ((baseInfo.bp1MQ0Frac > diploidOpt.maxMQ0Frac) ||
                        (baseInfo.bp2MQ0Frac > diploidOpt.maxMQ0Frac))
                    {
                        filteredJunctionCount++;
                    }
                }
            }

            if ((filteredJunctionCount*2) > junctionCount)
            {
                diploidInfo.filters.insert(diploidOpt.maxMQ0FracLabel);
            }
        }

        // apply zero pair filter
        {
            // this size represents the outer edge of variant size above which we expect pair
            // discovery to suffer no dropouts due to normal pair distro sizes
            static const double insertSizeFactor(1);
            const unsigned maxClosePairSize(readScanner.getExtremeFifthRange().max * insertSizeFactor);

            unsigned filteredJunctionCount(0);
            for (const JunctionCallInfo& junction : junctionData)
            {
                const SVScoreInfo& baseInfo(junction.getBaseInfo());
                const SVCandidate& sv(junction.getSV());

                // only apply the zero pair filter to variants that should definitely have located supporting pairs:
                const EXTENDED_SV_TYPE::index_t svType(getExtendedSVType(sv));
                const bool isZeroPairFilterSize((svType != EXTENDED_SV_TYPE::INSERT) && (! isSVBelowMinSize(sv, maxClosePairSize)));

                if (isZeroPairFilterSize)
                {
                    unsigned totalDiploidSpanningPairCount(0);
                    for (unsigned diploidSampleIndex(0); diploidSampleIndex<diploidSampleCount; ++diploidSampleIndex)
                    {
                        totalDiploidSpanningPairCount += baseInfo.samples[diploidSampleIndex].alt.confidentSpanningPairCount;
                    }

                    if (totalDiploidSpanningPairCount == 0)
                    {
                        filteredJunctionCount++;
                    }
                }
            }

            if ((filteredJunctionCount*2) > junctionCount)
            {
                diploidInfo.filters.insert(diploidOpt.noPairSupportLabel);
            }
        }
    }
}


/// score diploid tumor specific components (under tumor-only mode):
static
void
scoreTumorSV(
    const CallOptionsTumor& tumorOpt,
    const ChromDepthFilterUtil& dFilter,
    const std::vector<JunctionCallInfo>& junctionData,
    SVScoreInfoTumor& tumorInfo)
{
    //
    // compute qualities
    //
    assert(! junctionData.empty());

    //TODO: scoring tumor-only variants

    //
    // apply filters
    //
    {
        //TODO: add score filter

        const unsigned junctionCount(junctionData.size());

        // apply high depth filter:
        if (dFilter.isMaxDepthFilter())
        {
            unsigned filteredJunctionCount(0);
            for (const JunctionCallInfo& junction : junctionData)
            {
                const SVScoreInfo& baseInfo(junction.getBaseInfo());
                const SVCandidate& sv(junction.getSV());

                // apply maxdepth filter if either of the breakpoints exceeds the maximum depth:
                if ((baseInfo.bp1MaxDepth > dFilter.maxDepth(sv.bp1.interval.tid)) ||
                    (baseInfo.bp2MaxDepth > dFilter.maxDepth(sv.bp2.interval.tid)))
                {
                    filteredJunctionCount++;
                }
            }

            if ((filteredJunctionCount*2) > junctionCount)
            {
                tumorInfo.filters.insert(tumorOpt.maxDepthFilterLabel);
            }
        }

        // apply MQ0 filter
        {
            unsigned filteredJunctionCount(0);
            for (const JunctionCallInfo& junction : junctionData)
            {
                const SVScoreInfo& baseInfo(junction.getBaseInfo());
                const SVCandidate& sv(junction.getSV());


                const bool isMQ0FilterSize(isSVBelowMinSize(sv,1000));
                if (isMQ0FilterSize)
                {
                    // apply MQ0 filter for one junction if either breakend meets the filter criteria:
                    if ((baseInfo.bp1MQ0Frac > tumorOpt.maxMQ0Frac) ||
                        (baseInfo.bp2MQ0Frac > tumorOpt.maxMQ0Frac))
                    {
                        filteredJunctionCount++;
                    }
                }
            }

            if ((filteredJunctionCount*2) > junctionCount)
            {
                tumorInfo.filters.insert(tumorOpt.maxMQ0FracLabel);
            }
        }
    }
}


//todo This is mostly a placeholder. Add real RNA scoring model
static
void
scoreRNASV(
    const CallOptionsDiploid& diploidOpt,
    SVScoreInfo& baseInfo,
    SVScoreInfoDiploid& diploidInfo)
{
#ifdef DEBUG_SCORE
    //log_os << __FUNCTION__ << "Scoring RNA candidate " << sv << "\n";
#endif

    /// TODO TMP add real sampleIndex
    const unsigned sampleIndex(0);

    diploidInfo.samples[sampleIndex].gtScore=0;
    diploidInfo.altScore=42;
    if (baseInfo.samples[sampleIndex].alt.splitReadCount == 0)
    {
        diploidInfo.filters.insert(diploidOpt.rnaFilterLabel);
#ifdef DEBUG_SCORE
        log_os << __FUNCTION__ << "Failed. No spanning pair " << "\n";
#endif
    }
    if (baseInfo.samples[sampleIndex].alt.confidentSpanningPairCount == 0)
    {
        diploidInfo.filters.insert(diploidOpt.rnaFilterLabel);
#ifdef DEBUG_SCORE
        log_os << __FUNCTION__ << "Failed. No split read " << "\n";
#endif
    }
}

static
unsigned
getSpanningPairCount(
    const SVSampleAlleleInfo& allele,
    const float spanningPairWeight,
    const bool isPermissive)
{
    if (isPermissive) return spanningPairWeight*allele.confidentSemiMappedSpanningPairCount;
    else              return spanningPairWeight*allele.confidentSpanningPairCount;
}



static
unsigned
getSupportCount(
    const SVSampleAlleleInfo& allele,
    const float spanningPairWeight,
    const bool isPermissive)
{
    return allele.confidentSplitReadCount + getSpanningPairCount(allele, spanningPairWeight, isPermissive);
}



#if 0
static
double
estimateSomaticMutationFreq(
    const SVScoreInfo& baseInfo,
    const float spanningPairWeight,
    const bool /*isPermissive*/)
{
    static const bool isPermissive(false);
    const unsigned altCounts = getSupportCount(baseInfo.tumor.alt, spanningPairWeight, isPermissive);
    const unsigned refCounts = getSupportCount(baseInfo.tumor.ref, spanningPairWeight, isPermissive);
    if ((altCounts + refCounts) == 0) return 0;
    return static_cast<double>(altCounts) / static_cast<double>(altCounts + refCounts);
}
#endif



static
double
estimateSomaticMutationFreq(
    const unsigned tumorSampleIndex,
    const std::vector<JunctionCallInfo>& junctionData,
    const bool /*isPermissive*/)
{
    static const bool isPermissive(false);

    unsigned altCounts(0);
    unsigned refCounts(0);
    for (const JunctionCallInfo& junction : junctionData)
    {
        const SVScoreInfo& baseInfo(junction.getBaseInfo());
        const float& spanningPairWeight(junction.getSpanningWeight());
        const SVSampleInfo& tumorSampleInfo(baseInfo.samples[tumorSampleIndex]);
        altCounts += getSupportCount(tumorSampleInfo.alt, spanningPairWeight, isPermissive);
        refCounts += getSupportCount(tumorSampleInfo.ref, spanningPairWeight, isPermissive);
    }
    if ((altCounts + refCounts) == 0) return 0;
    return static_cast<double>(altCounts) / static_cast<double>(altCounts + refCounts);
}



#if 0
static
double
estimateNoiseMutationFreq(
    const SVScoreInfo& baseInfo,
    const float spanningPairWeight,
    const bool /*isPermissive*/)
{
    static const bool isPermissive(false);
    const unsigned normalAltCounts = getSupportCount(baseInfo.normal.alt, spanningPairWeight, isPermissive);
    const unsigned normalRefCounts = getSupportCount(baseInfo.normal.ref, spanningPairWeight, isPermissive);
    const unsigned tumorAltCounts = getSupportCount(baseInfo.tumor.alt, spanningPairWeight, isPermissive);
    const unsigned tumorRefCounts = getSupportCount(baseInfo.tumor.ref, spanningPairWeight, isPermissive);

    const unsigned altCounts(normalAltCounts + tumorAltCounts);
    const unsigned refCounts(normalRefCounts + tumorRefCounts);

    if ((altCounts + refCounts) == 0) return 0;
    return static_cast<double>(altCounts) / static_cast<double>(altCounts + refCounts);
}
#endif



static
double
estimateNoiseMutationFreq(
    const unsigned normalSampleIndex,
    const unsigned tumorSampleIndex,
    const std::vector<JunctionCallInfo>& junctionData,
    const bool /*isPermissive*/)
{
    static const bool isPermissive(false);
    unsigned altCounts(0);
    unsigned refCounts(0);
    for (const JunctionCallInfo& junction : junctionData)
    {
        const SVScoreInfo& baseInfo(junction.getBaseInfo());
        const float& spanningPairWeight(junction.getSpanningWeight());

        const SVSampleInfo& normalSampleInfo(baseInfo.samples[normalSampleIndex]);
        const unsigned normalAltCounts(getSupportCount(normalSampleInfo.alt, spanningPairWeight, isPermissive));
        const unsigned normalRefCounts(getSupportCount(normalSampleInfo.ref, spanningPairWeight, isPermissive));

        const SVSampleInfo& tumorSampleInfo(baseInfo.samples[tumorSampleIndex]);
        const unsigned tumorAltCounts(getSupportCount(tumorSampleInfo.alt, spanningPairWeight, isPermissive));
        const unsigned tumorRefCounts(getSupportCount(tumorSampleInfo.ref, spanningPairWeight, isPermissive));

        altCounts += (normalAltCounts + tumorAltCounts);
        refCounts += (normalRefCounts + tumorRefCounts);
    }
    if ((altCounts + refCounts) == 0) return 0;
    return static_cast<double>(altCounts) / static_cast<double>(altCounts + refCounts);
}



static
void
computeSomaticSampleLoghood(
    const float spanningPairWeight,
    const SVEvidence::evidenceTrack_t& evidenceTrack,
    const double somaticMutationFreq,
    const double noiseMutationFreq,
    const bool isPermissive,
    const bool isTumor,
    const ProbSet& refChimeraProb,
    const ProbSet& altChimeraProb,
    const ProbSet& refSplitMapProb,
    const ProbSet& altSplitMapProb,
    std::array<double,SOMATIC_GT::SIZE>& loglhood)
{
    // semi-mapped alt reads make a partial contribution in tier1, and a full contribution in tier2:
    const double semiMappedPower( (isPermissive && (! isTumor)) ? 1. : 0. );

    for (const SVEvidence::evidenceTrack_t::value_type& val : evidenceTrack)
    {
        const std::string& fragLabel(val.first);
        const SVFragmentEvidence& fragev(val.second);

        AlleleLnLhood refLnLhoodSet, altLnLhoodSet;
        bool isRead1Evaluated(true);
        bool isRead2Evaluated(true);

        if (! getRefAltFromFrag(spanningPairWeight, semiMappedPower, refChimeraProb, altChimeraProb,
                                refSplitMapProb, altSplitMapProb, isPermissive, fragLabel, fragev,
                                refLnLhoodSet, altLnLhoodSet, isRead1Evaluated, isRead2Evaluated))
        {
            // continue if this fragment was not evaluated for pair or split support for either allele:
            continue;
        }

        for (unsigned gt(0); gt<SOMATIC_GT::SIZE; ++gt)
        {
            using namespace SOMATIC_GT;

#ifdef DEBUG_SCORE
            log_os << __FUNCTION__ << ": starting gt: " << gt << " " << label(gt) << "\n";
#endif

            const index_t gtid(static_cast<const index_t>(gt));

            const double refLnFragLhood(getFragLnLhood(refLnLhoodSet, isRead1Evaluated, isRead2Evaluated));
            const double altLnFragLhood(getFragLnLhood(altLnLhoodSet, isRead1Evaluated, isRead2Evaluated));

            // update likelihood with Pr[allele | G]
            const double refLnLhood = refLnFragLhood + altLnCompFraction(gtid, somaticMutationFreq, noiseMutationFreq);
            const double altLnLhood = altLnFragLhood + altLnFraction(gtid, somaticMutationFreq, noiseMutationFreq);

#ifdef DEBUG_SCORE
            log_os << __FUNCTION__ << ": refLnFragLhood: " << refLnFragLhood << "\n";
            log_os << __FUNCTION__ << ": altLnFragLhood: " << altLnFragLhood << "\n";
            log_os << __FUNCTION__ << ": refLnLhood: " << refLnLhood << "\n";
            log_os << __FUNCTION__ << ": altLnLhood: " << altLnLhood << "\n";
            log_os << __FUNCTION__ << ": loghood delta: " << log_sum(refLnLhood, altLnLhood) << "\n";
#endif

            loglhood[gt] += log_sum(refLnLhood, altLnLhood);
        }
    }
}



/// score somatic specific components:
static
void
scoreSomaticSV(
    const unsigned sampleCount,
    const unsigned diploidSampleCount,
    const CallOptionsSomatic& somaticOpt,
    const CallOptionsSomaticDeriv& somaticDopt,
    const ChromDepthFilterUtil& dFilter,
    const std::vector<JunctionCallInfo>& junctionData,
    SVScoreInfoSomatic& somaticInfo)
{
    //
    // compute somatic score
    //
    assert(! junctionData.empty());
    const bool isMJEvent(junctionData.size() > 1);

    // somatic score is computed at a high stringency date tier (1) and low stringency tier (2), the min value is
    // kept as the final reported quality:
    static const unsigned tierCount(2);
    int tierScore[tierCount] = { 0 , 0 };

    // hard code 1 tumor - 1 normal for now, should be able to support multiple tumors in future:
    assert(sampleCount==2);
    assert(diploidSampleCount==1);
    const unsigned normalSampleIndex(0);
    const unsigned tumorSampleIndex(1);

    /// for multi-junction events, we use the prior noise weight associated with the largest event:
    float largeNoiseWeight(0.f);
    for (const JunctionCallInfo& junction : junctionData)
    {
        const SVCandidate& sv(junction.getSV());
        const float weight(largeNoiseSVPriorWeight(sv));
        if (weight > largeNoiseWeight) largeNoiseWeight = weight;
    }

    for (unsigned tierIndex(0); tierIndex<tierCount; ++tierIndex)
    {
        const bool isPermissive(tierIndex != 0);

        std::array<double,SOMATIC_GT::SIZE> normalSomaticLhood;
        std::array<double,SOMATIC_GT::SIZE> tumorSomaticLhood;
        std::fill(normalSomaticLhood.begin(),normalSomaticLhood.end(),0);
        std::fill(tumorSomaticLhood.begin(),tumorSomaticLhood.end(),0);

        // estimate the somatic mutation rate using alternate allele freq from the tumor sample
        const double somaticMutationFreq = estimateSomaticMutationFreq(tumorSampleIndex,junctionData, isPermissive);

        // estimate the noise mutation rate using alternate allele freq from the tumor and normal samples
        const double noiseMutationFreq = estimateNoiseMutationFreq(normalSampleIndex,tumorSampleIndex,junctionData, isPermissive);

#ifdef DEBUG_SOMATIC_SCORE
        log_os << __FUNCTION__ << ": somaticMutationFrequency: " << somaticMutationFreq << "\n";
        log_os << __FUNCTION__ << ": noiseMutationFrequency: " << noiseMutationFreq << "\n";
        log_os << __FUNCTION__ << ": largeNoiseWeight: " << largeNoiseWeight << "\n";
#endif

        /// TODO: find a better way to set this number from training data:
        static const ProbSet chimeraProbDefaultSingleJunction(1e-4);
        static const ProbSet chimeraProbDefaultMultiJunction(2e-5);
        const ProbSet& chimeraProbDefault( isMJEvent ? chimeraProbDefaultMultiJunction : chimeraProbDefaultSingleJunction );

        static const ProbSet chimeraProbPermissive(5e-6);
        const ProbSet& chimeraProb( isPermissive ? chimeraProbPermissive : chimeraProbDefault );

        /// use a constant mapping prob for now just to get the zero-th order concept into the model
        /// that "reads are mismapped at a non-trivial rate"
        /// TODO: experiment with per-read mapq values
        ///
        static const ProbSet refSplitMapProb(1e-6);

        static const ProbSet altSplitMapProbDefault(1e-4);
        static const ProbSet altSplitMapProbPermissive(1e-6);
        const ProbSet& altSplitMapProb( isPermissive ? altSplitMapProbPermissive : altSplitMapProbDefault );

        for (const JunctionCallInfo& junction : junctionData)
        {
            const SVEvidence& evidence(junction.getEvidence());
            const float& spanningPairWeight(junction.getSpanningWeight());

            // compute likelihood for the fragments from the tumor sample
            computeSomaticSampleLoghood(spanningPairWeight, evidence.samples[tumorSampleIndex], somaticMutationFreq, noiseMutationFreq,
                                        isPermissive, true,
                                        chimeraProbDefault, chimeraProbDefault,
                                        refSplitMapProb, altSplitMapProbDefault, tumorSomaticLhood);

            // compute likelihood for the fragments from the normal sample
            computeSomaticSampleLoghood(spanningPairWeight, evidence.samples[normalSampleIndex], 0, noiseMutationFreq,
                                        isPermissive, false,
                                        chimeraProbDefault, chimeraProb,
                                        refSplitMapProb, altSplitMapProb, normalSomaticLhood);
        }

        std::array<double,SOMATIC_GT::SIZE> somaticPprob;
        for (unsigned gt(0); gt<SOMATIC_GT::SIZE; ++gt)
        {
            somaticPprob[gt] = tumorSomaticLhood[gt] + normalSomaticLhood[gt] + somaticDopt.logPrior(gt,largeNoiseWeight);
        }

        {
            unsigned maxGt(0);
            normalize_ln_distro(somaticPprob.begin(), somaticPprob.end(), maxGt);
        }

        // independently estimate diploid genotype:
        std::array<double,DIPLOID_GT::SIZE> normalLhood;
        std::fill(normalLhood.begin(),normalLhood.end(),0);
        for (const JunctionCallInfo& junction : junctionData)
        {
            addDiploidLoglhood(junction.getSpanningWeight(), junction.getEvidence().samples[normalSampleIndex], normalLhood);
        }

        std::array<double,DIPLOID_GT::SIZE> normalPprob;
        for (unsigned gt(0); gt<DIPLOID_GT::SIZE; ++gt)
        {
            normalPprob[gt] = normalLhood[gt]; // uniform prior for now....
        }

        {
            unsigned maxGt(0);
            normalize_ln_distro(normalPprob.begin(), normalPprob.end(), maxGt);
        }

#ifdef DEBUG_SOMATIC_SCORE
        for (unsigned gt(0); gt<SOMATIC_GT::SIZE; ++gt)
        {
            log_os << __FUNCTION__ << ": somatic gt/tumor_lhood/normal_lhood/prior/pprob: "
                   << SOMATIC_GT::label(gt)
                   << " " << tumorSomaticLhood[gt]
                   << " " << normalSomaticLhood[gt]
                   << " " << somaticDopt.logPrior(gt,largeNoiseWeight)
                   << " " << somaticPprob[gt]
                   << "\n";
        }

        for (unsigned gt(0); gt<DIPLOID_GT::SIZE; ++gt)
        {
            log_os << __FUNCTION__ << ": diploid gt/lhood/pprob: "
                   << DIPLOID_GT::label(gt)
                   << " " << normalLhood[gt]
                   << " " << normalPprob[gt]
                   << "\n";
        }
#endif

        const double nonsomaticProb(prob_comp(somaticPprob.begin(), somaticPprob.end(), SOMATIC_GT::SOM));
        const double nonrefProb(prob_comp(normalPprob.begin(), normalPprob.end(), DIPLOID_GT::REF));

        // not (somatic AND normal ref):
        // (1-(1-a)(1-b)) -> a+b-(ab)
        const double nonsomatic_ref_prob(nonsomaticProb+nonrefProb-(nonsomaticProb*nonrefProb));

        tierScore[tierIndex]=error_prob_to_qphred(nonsomatic_ref_prob);

#ifdef DEBUG_SOMATIC_SCORE
        log_os << __FUNCTION__ << ": tier: " << tierIndex << " somatic score: " << tierScore[tierIndex] << "\n";
#endif

        // don't bother with tier2 if tier1 is too low:
        if (tierScore[tierIndex] <= 0) break;
    }

    somaticInfo.somaticScore=std::min(tierScore[0],tierScore[1]);

    somaticInfo.somaticScoreTier = 0;
    if (tierScore[1] > tierScore[0])
    {
        somaticInfo.somaticScoreTier = 1;
    }


    //
    // apply filters
    //
    {
        const unsigned junctionCount(junctionData.size());

        // apply high depth filter:
        if (dFilter.isMaxDepthFilter())
        {
            unsigned filteredJunctionCount(0);
            for (const JunctionCallInfo& junction : junctionData)
            {
                const SVScoreInfo& baseInfo(junction.getBaseInfo());
                const SVCandidate& sv(junction.getSV());

                // apply maxdepth filter if either of the breakpoints exceeds the maximum depth:
                if ((baseInfo.bp1MaxDepth > dFilter.maxDepth(sv.bp1.interval.tid)) ||
                    (baseInfo.bp2MaxDepth > dFilter.maxDepth(sv.bp2.interval.tid)))
                {
                    filteredJunctionCount++;
                }
            }

            // apply MQ0 filter for an entire event if a majority of junctions meet the junction filter criteria:
            if ((filteredJunctionCount*2) > junctionCount)
            {
                somaticInfo.filters.insert(somaticOpt.maxDepthFilterLabel);
            }
        }

        if (somaticInfo.somaticScore < somaticOpt.minPassSomaticScore)
        {
            somaticInfo.filters.insert(somaticOpt.minSomaticScoreLabel);
        }

        // apply MQ0 filter
        {
            unsigned filteredJunctionCount(0);
            for (const JunctionCallInfo& junction : junctionData)
            {
                const SVScoreInfo& baseInfo(junction.getBaseInfo());
                const SVCandidate& sv(junction.getSV());

                const bool isMQ0FilterSize(isSVBelowMinSize(sv,1000));
                if (isMQ0FilterSize)
                {
                    // apply MQ0 filter for one junction if either breakend meets the filter criteria:
                    if ((baseInfo.bp1MQ0Frac > somaticOpt.maxMQ0Frac) ||
                        (baseInfo.bp2MQ0Frac > somaticOpt.maxMQ0Frac))
                    {
                        filteredJunctionCount++;
                    }
                }
            }

            // apply MQ0 filter for an entire event if a majority of junctions meet the junction filter criteria:
            if ((filteredJunctionCount*2) > junctionCount)
            {
                somaticInfo.filters.insert(somaticOpt.maxMQ0FracLabel);
            }
        }
    }
}



void
SVScorer::
computeAllScoreModels(
    const bool isSomatic,
    const bool isTumorOnly,
    const std::vector<JunctionCallInfo>& junctionData,
    SVModelScoreInfo& modelScoreInfo)
{
    if (isTumorOnly)
    {
        scoreTumorSV(_tumorOpt, _dFilterDiploid, junctionData, modelScoreInfo.tumor);
    }
    else
    {
        scoreDiploidSV(_diploidOpt, _readScanner, _diploidDopt, _dFilterDiploid, junctionData, modelScoreInfo.diploid);

        // score components specific to somatic model:
        if (isSomatic)
        {
            scoreSomaticSV(_sampleCount,_diploidSampleCount,_somaticOpt, _somaticDopt, _dFilterSomatic, junctionData, modelScoreInfo.somatic);
        }
    }

    if (_isRNA)
    {
        scoreRNASV(_diploidOpt, modelScoreInfo.base, modelScoreInfo.diploid);
    }
}



void
SVScorer::
scoreSV(
    const SVCandidateSetData& svData,
    const std::vector<SVCandidateAssemblyData>& mjAssemblyData,
    const SVMultiJunctionCandidate& mjSV,
    const std::vector<bool>& isJunctionFiltered,
    const bool isSomatic,
    const bool isTumorOnly,
    std::vector<SVModelScoreInfo>& mjModelScoreInfo,
    SVModelScoreInfo& mjJointModelScoreInfo,
    bool& isMJEvent)
{
    // scoring is roughly divided into two parts -- treating individual dna-junctions
    // independently (the simpler call mechanism used the great majority of the time) and
    // joint junction analysis for larger scale events
    //
    const unsigned junctionCount(mjSV.junction.size());
    mjModelScoreInfo.resize(junctionCount);
    std::vector<SVEvidence> junctionEvidence(junctionCount);
    std::vector<float> junctionSpanningPairWeight(junctionCount);

    for (unsigned jIndex(0); jIndex<junctionCount; ++jIndex)
    {
        mjModelScoreInfo[jIndex].setSampleCount(sampleCount(),diploidSampleCount());
        junctionEvidence[jIndex].samples.resize(sampleCount());
    }

    mjJointModelScoreInfo.clear();

    unsigned unfilteredJunctionCount(0);

    std::vector<JunctionCallInfo> junctionData;

    for (unsigned junctionIndex(0); junctionIndex<junctionCount; ++junctionIndex)
    {
        if (isJunctionFiltered[junctionIndex]) continue;

#ifdef ANY_DEBUG_SCORE
        log_os << __FUNCTION__ << ": Scoring single junction " << junctionIndex << "/" << junctionCount << "\n";
#endif

        unfilteredJunctionCount++;

        const SVCandidateAssemblyData& assemblyData(mjAssemblyData[junctionIndex]);
        const SVCandidate& sv(mjSV.junction[junctionIndex]);
        SVModelScoreInfo& modelScoreInfo(mjModelScoreInfo[junctionIndex]);

        modelScoreInfo.clear();

        // accumulate model-neutral evidence for each candidate (or its corresponding reference allele)
        SVEvidence& evidence(junctionEvidence[junctionIndex]);
        scoreSV(svData, assemblyData, isTumorOnly, sv, modelScoreInfo.base, evidence);

        // score components specific to diploid-germline model:
        float& spanningPairWeight(junctionSpanningPairWeight[junctionIndex]);;
        spanningPairWeight=(getSpanningPairWeight(sv));

        junctionData.resize(1);
        junctionData[0].init(sv, evidence, modelScoreInfo.base, spanningPairWeight);

        computeAllScoreModels(isSomatic, isTumorOnly, junctionData, modelScoreInfo);
    }

    //
    // handle multi-junction case:
    //
    if (unfilteredJunctionCount == 1)
    {
        isMJEvent=false;
    }
    else if (unfilteredJunctionCount == 2)
    {
#ifdef ANY_DEBUG_SCORE
        log_os << __FUNCTION__ << ": Scoring multi-junction " << junctionCount << "\n";
#endif
        isMJEvent=true;

        junctionData.resize(unfilteredJunctionCount);
        for (unsigned junctionIndex(0); junctionIndex<junctionCount; ++junctionIndex)
        {
            junctionData[junctionIndex].init(
                mjSV.junction[junctionIndex],
                junctionEvidence[junctionIndex],
                mjModelScoreInfo[junctionIndex].base,
                junctionSpanningPairWeight[junctionIndex]);
        }

        computeAllScoreModels(isSomatic, isTumorOnly, junctionData, mjJointModelScoreInfo);
    }
    else
    {
        using namespace illumina::common;
        std::ostringstream oss;
        oss << "ERROR: unexpected junction count: " << unfilteredJunctionCount << ".\n";
        BOOST_THROW_EXCEPTION(LogicException(oss.str()));
    }
}

