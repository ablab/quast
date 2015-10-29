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

#include "MultiJunctionUtil.hh"

#include "blt_util/log.hh"
#include "manta/SVCandidateUtil.hh"

#include <limits>



/// return true for candidates that should be filtered out, based on
/// information available in a full junction set
///
static
bool
isFilterMultiJunctionCandidate(
    const unsigned minCandidateSpanningCount,
    const SVMultiJunctionCandidate& mjSV)
{
    // one breakend out of the junction set must have the minimum level of evidence:
    bool isCountFilter(true);
    bool isSingleFilter(true);
    for (const SVCandidate& sv : mjSV.junction)
    {
        if (isSpanningSV(sv))
        {
            if (sv.bp1.getSpanningCount() >= minCandidateSpanningCount)
            {
                isCountFilter = false;
            }

            if (! sv.isSingleJunctionFilter)
            {
                isSingleFilter = false;
            }
        }
    }
    return (isCountFilter || isSingleFilter);
}



static
unsigned
getIntervalDist(
    const GenomeInterval& intervalA,
    const GenomeInterval& intervalB)
{
    static const unsigned far(std::numeric_limits<unsigned>::max());

    if (intervalA.tid != intervalB.tid) return far;

    return std::abs(intervalA.range.center_pos() - intervalB.range.center_pos());
}



///
static
bool
isIntervalPairGroupCandidate(
    const GenomeInterval& intervalA,
    const GenomeInterval& intervalB,
    const unsigned minFilterDist)
{
    return (getIntervalDist(intervalA,intervalB) < minFilterDist);
}



/// return:
/// max(dist(A1,B1),dist(A2,B2)) if is11 is true
/// or
/// max(dist(A1,B2),dist(A2,B1)) if is11 is false
///
static
unsigned
getMaxIntervalDistance(
    const SVCandidate& svA,
    const SVCandidate& svB,
    const bool is11)
{
    if (is11)
    {
        const unsigned dist11(getIntervalDist(svA.bp1.interval,svB.bp1.interval));
        const unsigned dist22(getIntervalDist(svA.bp2.interval,svB.bp2.interval));
        return std::max(dist11,dist22);
    }
    else
    {
        const unsigned dist12(getIntervalDist(svA.bp1.interval,svB.bp2.interval));
        const unsigned dist21(getIntervalDist(svA.bp2.interval,svB.bp1.interval));
        return std::max(dist12,dist21);
    }
}



/// return  1 if dist(A1,B1) and dist(A2,B2) are both less than dist(A1,B2) and dist(A2,B1)
/// return -1 if dist(A1,B2) and dist(A2,B1) are both less than dist(A1,B1) and dist(A2,B2)
/// return 0 for all other cases
static
int
getJunctionBpAlignment(
    const SVCandidate& svA,
    const SVCandidate& svB)
{
    const unsigned dist11(getIntervalDist(svA.bp1.interval,svB.bp1.interval));
    const unsigned dist12(getIntervalDist(svA.bp1.interval,svB.bp2.interval));
    const unsigned dist21(getIntervalDist(svA.bp2.interval,svB.bp1.interval));
    const unsigned dist22(getIntervalDist(svA.bp2.interval,svB.bp2.interval));

    if (((dist11 < dist12) && (dist11 < dist21)) && ((dist22 < dist12) && (dist22 < dist21))) return  1;
    if (((dist12 < dist11) && (dist12 < dist22)) && ((dist21 < dist11) && (dist21 < dist22))) return -1;
    return 0;
}



/// are two breakend pairs candidates for a multi-junction analysis?:
///
static
bool
isBreakendPairGroupCandidate(
    const SVBreakend& bpA,
    const SVBreakend& bpB,
    const unsigned groupRange = 1000)
{
    if (! isOppositeOrientation(bpA.state, bpB.state)) return false;

    return isIntervalPairGroupCandidate(bpA.interval, bpB.interval, groupRange);
}



/// test to see if a breakend can participate in a multi-junction analysis:
///
/// right now our only criteria is to exclude small non-inversions, just because
/// such pairs can spontaneously occur at relatively high rates:
static
bool
isSVMJExcluded(
    const SVCandidate& sv)
{
    static const unsigned minInnieSVSize(100000);

    {
        using namespace SV_TYPE;
        const SV_TYPE::index_t svt(getSVType(sv));
        if ((svt != INDEL) && (svt != TANDUP)) return false;
    }

    return (getIntervalDist(sv.bp1.interval, sv.bp2.interval) < minInnieSVSize);
}



namespace MJ_INTERACTION
{
enum index_t
{
    NONE,
    SAME,
    FLIP,
    CONFLICT
};

struct MJState
{
    void
    clear()
    {
        type = NONE;
        partnerId = 0;
        maxPartnerDistance = 0;
    }

    index_t type = NONE;
    unsigned partnerId = 0;
    unsigned maxPartnerDistance = 0;
};
}



static
void
setPartner(
    std::vector<MJ_INTERACTION::MJState>& spanPartners,
    const MJ_INTERACTION::index_t newType,
    const unsigned maxPartnerDistance,
    const unsigned spanIndex1,
    const unsigned spanIndex2)
{
    spanPartners[spanIndex1].type = newType;
    spanPartners[spanIndex1].partnerId = spanIndex2;
    spanPartners[spanIndex1].maxPartnerDistance = maxPartnerDistance;
}



// 1 is the new partner
// 2 is the previously connected partner
static
void
resetPartners(
    std::vector<MJ_INTERACTION::MJState>& spanPartners,
    const MJ_INTERACTION::index_t newType,
    const unsigned maxPartnerDistance,
    const unsigned spanIndex1,
    const unsigned spanIndex2)
{
    using namespace MJ_INTERACTION;

    if (spanPartners[spanIndex2].maxPartnerDistance <= maxPartnerDistance)
    {
        // don't reset -- original pairing was better:
        spanPartners[spanIndex1].type = NONE;
        return;
    }

    // do reset, the new pairing is better:

    // undo previous partner (2):
    const unsigned spanIndexC(spanPartners[spanIndex2].partnerId);
    assert(spanIndexC != spanIndex1);
    spanPartners[spanIndexC].clear();
    spanPartners[spanIndexC].type = CONFLICT;

    // now initialize 1 and "reprogram" 2:
    setPartner(spanPartners,newType,maxPartnerDistance,spanIndex1,spanIndex2);
    setPartner(spanPartners,newType,maxPartnerDistance,spanIndex2,spanIndex1);
}



void
findMultiJunctionCandidates(
    const std::vector<SVCandidate>& svs,
    const unsigned minCandidateSpanningCount,
    unsigned& mjComplexCount,
    unsigned& mjSpanningFilterCount,
    std::vector<SVMultiJunctionCandidate>& mjSVs)
{
    mjSVs.clear();

    std::vector<SVCandidate> complexSVs;
    std::vector<SVCandidate> spanningSVs;

    for (const SVCandidate& candidateSV : svs)
    {
        const bool isComplex(isComplexSV(candidateSV));

        if (isComplex)
        {
            complexSVs.push_back(candidateSV);
        }
        else
        {
            spanningSVs.push_back(candidateSV);
        }
    }

    mjComplexCount = complexSVs.size();

    /// do a brute-force intersection test to see if we can associate candidates:
    ///
    /// intersection rules : breakend region center must be within distance N
    /// intersecting breakend orientation makes it possible for these to be a single event -- ie. pointing away or towards each other
    /// full set of intersections must complete a loop, this is an intentionally conservative requirement to make sure we start into
    ///    this without getting involved in the really difficult stuff
    ///
    /// just for the starting version, the number of SVCandidates which can intersect is limited to 2
    ///

    const unsigned spanCount(spanningSVs.size());
    std::vector<MJ_INTERACTION::MJState> spanPartners(spanCount);
    {
        using namespace MJ_INTERACTION;

        for (unsigned spanIndexA(0); (spanIndexA+1)<spanCount; ++spanIndexA)
        {
            const SVCandidate& spanA(spanningSVs[spanIndexA]);
            if (isSVMJExcluded(spanA)) continue;

            for (unsigned spanIndexB(spanIndexA+1); spanIndexB<spanCount; ++spanIndexB)
            {
                const SVCandidate& spanB(spanningSVs[spanIndexB]);
                if (isSVMJExcluded(spanB)) continue;

                const bool isSameBpGroup(isBreakendPairGroupCandidate(spanA.bp1,spanB.bp1) && isBreakendPairGroupCandidate(spanA.bp2,spanB.bp2));
                const bool isFlipBpGroup(isBreakendPairGroupCandidate(spanA.bp1,spanB.bp2) && isBreakendPairGroupCandidate(spanA.bp2,spanB.bp1));

                bool isGroup(false);
                if (isSameBpGroup || isFlipBpGroup)
                {
                    /// check that this isn't a flipped association as breakpoints get near each other,
                    /// if it is treat the association as independent junctions:
                    if (isSameBpGroup)
                    {
                        isGroup = (getJunctionBpAlignment(spanA, spanB) > 0);
                    }
                    else
                    {
                        isGroup = (getJunctionBpAlignment(spanA, spanB) < 0);
                    }
                }

                if (!isGroup) continue;

                const index_t newType( isSameBpGroup ? SAME : FLIP );
                const unsigned maxPartnerDistance(getMaxIntervalDistance(spanA, spanB, isSameBpGroup));

                if ((spanPartners[spanIndexA].type == NONE) && (spanPartners[spanIndexB].type == NONE))
                {
                    setPartner(spanPartners,newType,maxPartnerDistance,spanIndexA,spanIndexB);
                    setPartner(spanPartners,newType,maxPartnerDistance,spanIndexB,spanIndexA);
                }
                else if (spanPartners[spanIndexA].type == NONE)
                {
                    resetPartners(spanPartners,newType,maxPartnerDistance,spanIndexA,spanIndexB);
                }
                else if (spanPartners[spanIndexB].type == NONE)
                {
                    resetPartners(spanPartners,newType,maxPartnerDistance,spanIndexB,spanIndexA);
                }
                else
                {
                    /// multiple candidates, keep the pair that's closer, and don't tolerate more than one repeat
                    spanPartners[spanIndexA].type = CONFLICT;
                    spanPartners[spanIndexB].type = CONFLICT;
                }
            }
        }
    }

    // spanning SVs are checked for groupings, and filtered
    //
    // spanning SVs should come first in order so that overlap checks can
    // be used to reduce work in the assembler
    for (unsigned spanIndex(0); spanIndex<spanCount; ++spanIndex)
    {
        SVMultiJunctionCandidate mj;
        mj.junction.push_back(spanningSVs[spanIndex]);

        using namespace MJ_INTERACTION;
        if ((spanPartners[spanIndex].type == SAME) ||
            (spanPartners[spanIndex].type == FLIP))
        {
            const unsigned partnerId(spanPartners[spanIndex].partnerId);
            assert(partnerId < spanCount);

            // only include the connected pair once:
            if (partnerId < spanIndex) continue;

            mj.junction.push_back(spanningSVs[partnerId]);
        }

        if (isFilterMultiJunctionCandidate(minCandidateSpanningCount, mj))
        {
            mjSpanningFilterCount++;
            continue;
        }

        mjSVs.push_back(mj);
    }

    // complex SVs are translated directly into single partner candidates:
    for (const SVCandidate& candidateSV : complexSVs)
    {
        SVMultiJunctionCandidate mj;
        mj.junction.push_back(candidateSV);
        mjSVs.push_back(mj);
    }


}
