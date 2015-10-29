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

#include "SVScorePairAltProcessor.hh"
#include "SVScorerShared.hh"
#include "blt_util/seq_util.hh"
#include "blt_util/SimpleAlignment.hh"
#include "common/Exceptions.hh"
#include "htsapi/bam_record_util.hh"
#include "manta/SVCandidateUtil.hh"

#include <cassert>

#include <sstream>


/// standard debug output for this file:
//#define DEBUG_PAIR

/// ridiculous debug output for this file:
//#define DEBUG_MEGAPAIR

//#define DEBUG_SHADOW

#if defined(DEBUG_PAIR) || defined(DEBUG_MEGAPAIR) || defined(DEBUG_SHADOW)
#define ANY_DEBUG_PAIR
#endif

#ifdef ANY_DEBUG_PAIR
#include "blt_util/log.hh"
#endif



ContigParams::
ContigParams(
    const SVCandidateAssemblyData& assemblyData,
    const SVCandidate& sv) :
    extSeq(assemblyData.extendedContigs[sv.assemblyAlignIndex])
{
    // this class is designed for simple alts only:
    assert(sv.bp1.interval.tid == sv.bp2.interval.tid);
    assert(getSVType(sv) == SV_TYPE::INDEL);
    assert(! sv.isImprecise());

    const bool isBp1First(sv.bp1.interval.range.begin_pos()<=sv.bp2.interval.range.begin_pos());

    const SVBreakend& bpA(isBp1First ? sv.bp1 : sv.bp2);
    const SVBreakend& bpB(isBp1First ? sv.bp2 : sv.bp1);

    const pos_t bpAHomLength(static_cast<pos_t>(bpA.interval.range.size())-1);
    const pos_t bpBHomLength(static_cast<pos_t>(bpB.interval.range.size())-1);
    assert(bpAHomLength >= 0);
    assert(bpBHomLength >= 0);

    const bool isSpanning(assemblyData.isSpanning);

    // the begin_pos off by one here is inherited from a more complex previous computation, the code
    // which uses this value has already been debugged and is functioning, so no reason to fix it, but
    // note this range doesn't follow the manta convention (probably by accident)
    segmentSpan.set_range(bpA.interval.range.begin_pos()+1,bpB.interval.range.begin_pos());

    // the beginPos of align is the length of reference padding in the extended contig
    // |ref padding| + |alignment segments|
    // both bp1 and bp2 include the insert and homology,
    // which can avoid false split-read evidence from normal sample when the homology is long

    // all offset range 'begin' values correspond to the zero-indexed base immediately before the breakend on the fwd-strand,
    // and 'end' values correspond to the zero-indexed base immediately before the breakend on the forward strand+homology range
    // In the absence of homology, begin and end should be equal.

    // note that we add align.beginPos here to reflect coordinates in the extended Contig, in the regular contig we wouldn't add this
    pos_t alignBeginPos(0);
    pos_t readStartPos(0);
    if (isSpanning)
    {
        const SVCandidateAssemblyData::JumpAlignmentResultType& alignment(assemblyData.spanningAlignments[sv.assemblyAlignIndex]);
        alignBeginPos = alignment.align1.beginPos;
        readStartPos = apath_read_length(alignment.align1.apath);
    }
    else
    {
        const AlignmentResult<int>& alignment(assemblyData.smallSVAlignments[sv.assemblyAlignIndex]);
        const std::pair<unsigned, unsigned>& alignSegment(assemblyData.smallSVSegments[sv.assemblyAlignIndex][sv.assemblySegmentIndex]);
        ALIGNPATH::path_t apathTillSvStart(&alignment.align.apath[0], &alignment.align.apath[alignSegment.first]);

        alignBeginPos = alignment.align.beginPos;
        readStartPos = apath_read_length(apathTillSvStart);
    }
    bpAOffset.set_begin_pos(alignBeginPos + readStartPos - 1);
    bpAOffset.set_end_pos(bpAOffset.begin_pos() + bpAHomLength);
    bpBOffset.set_begin_pos(bpAOffset.begin_pos() + sv.insertSeq.size());
    bpBOffset.set_end_pos(bpBOffset.begin_pos() + bpBHomLength);

#ifdef DEBUG_SHADOW
    log_os << __FUNCTION__ << ": contigSize: " << extSeq.size()
           << " segmentSpan: " << segmentSpan
           << " bpAOffset: " << bpAOffset
           << " bpBOffset: " << bpBOffset
           << "\n";
#endif
}



void
SVScorePairAltProcessor::
checkInput(
    const SVCandidate& sv)
{
    using namespace illumina::common;

    // this class is designed for simple alts only:
    assert(sv.bp1.interval.tid == sv.bp2.interval.tid);
    assert(getSVType(sv) == SV_TYPE::INDEL);
}



/// test whether a frag reference span provides sufficient support for a breakpoint of this sv:
bool
SVScorePairAltProcessor::
testFragOverlap(
    const int fragBeginRefPos,
    const int fragEndRefPos) const
{
    const pos_t fragOverlap(std::min((1+svParams.centerPosA-fragBeginRefPos), (fragEndRefPos-svParams.centerPosB)));
#ifdef DEBUG_MEGAPAIR
    log_os << __FUNCTION__ << ": frag begin/end/overlap: " << fragBeginRefPos << " " << fragEndRefPos << " " << fragOverlap << "\n";
#endif
    return (fragOverlap >= pairOpt.minFragSupport);
}



bool
SVScorePairAltProcessor::
realignPairedRead(
    const bam_record& bamRead,
    const bool isLeftOfInsert,
    const std::string& floatRead,
    const pos_t anchorPos,
    int& altTemplateSize)
{
    // TODO: basecall qualities??

    // sanity check whether we should even start alignment -- check 'left of insert' consistency
    //
    if (isLeftOfInsert)
    {
        if (anchorPos >= _contig.segmentSpan.begin_pos()) return false;
    }
    else
    {
        const pos_t endPos(anchorPos + floatRead.size());
        if (endPos <= _contig.segmentSpan.end_pos()) return false;
    }

    AlignmentResult<int> readAlignment;

    typedef std::string::const_iterator siter;
    const siter readBegin(floatRead.begin());
    const siter readEnd(floatRead.end());
    siter contigBegin(_contig.extSeq.begin());
    siter contigEnd(_contig.extSeq.end());

    // if the insertion is not fully assembled, align to only part of the contig:
    int contigBeginOffset(0);
    if (sv.isUnknownSizeInsertion)
    {
        // TODO check these results in test case:
        if (isLeftOfInsert)
        {
            contigEnd = contigBegin + _contig.bpAOffset.begin_pos() + sv.unknownSizeInsertionLeftSeq.size();
        }
        else
        {
            contigBeginOffset = static_cast<int>(_contig.bpBOffset.begin_pos()) - sv.unknownSizeInsertionRightSeq.size();
            assert(contigBeginOffset>=0);
            contigBegin = contigBegin + contigBeginOffset;
        }
    }

    _shadowAligner.align(
        readBegin, readEnd,
        contigBegin, contigEnd,
        readAlignment);

    //
    // first determine if the read meets some minimal quality criteria
    //

    // require the complete alignment score to be some percentage of optimal after trimming off any expected softclip
    {
        using namespace ALIGNPATH;

        const path_t readPath(readAlignment.align.apath);

        const unsigned readSize(floatRead.size());
        unsigned clipSize(0);

        if (sv.isUnknownSizeInsertion)
        {
            if (isLeftOfInsert)
            {
                clipSize=apath_soft_clip_trail_size(readPath);
            }
            else
            {
                clipSize=apath_soft_clip_lead_size(readPath);
            }
        }

#ifdef DEBUG_SHADOW
        log_os << __FUNCTION__ << ": alignment: " << readPath << " clipSize: " << clipSize << "\n";
#endif

        assert(clipSize <= readSize);

        const unsigned clippedReadSize(readSize-clipSize);

        static const unsigned minAlignReadLength(40);
        if (clippedReadSize < minAlignReadLength)
        {
            return false;
        }

        int nonClipScore(_shadowAligner.getPathScore(readPath, false));

        static const float minScoreFrac(0.85f);
        const int optimalScore(clippedReadSize*_shadowAligner.getScores().match);

        const float scoreFrac(static_cast<float>(nonClipScore)/static_cast<float>(optimalScore));

#ifdef DEBUG_SHADOW
        log_os << __FUNCTION__ << ": optScore: " << optimalScore
               << " nonClipScore: " << nonClipScore
               << " scoreFrac: " << scoreFrac << "\n";
#endif

        if (scoreFrac < minScoreFrac)
        {
            return false;
        }
    }

    //
    // next determine what the altTemplateSize is if we believe the alignment
    //

    known_pos_range2 fakeRefSpan;
    if (isLeftOfInsert)
    {
        fakeRefSpan.set_begin_pos(anchorPos);

        // offset of read end on the contig, in contig coordinates
        const unsigned shadowRefSpan(apath_ref_length(readAlignment.align.apath));
        const int readContigEndOffset(contigBeginOffset + readAlignment.align.beginPos + shadowRefSpan);

        // translate contig coordinates to fake reference coordinates:
        if (readContigEndOffset < _contig.bpAOffset.begin_pos())
        {
            // definitely does not meet the breakend overlap criteria:
            return false;
        }

        /// set fake end as if the insert allele continued in reference coordinates
        const int readContigEndRefOffset(_contig.segmentSpan.begin_pos() + (readContigEndOffset - _contig.bpAOffset.begin_pos()));
        fakeRefSpan.set_end_pos(readContigEndRefOffset);

#ifdef DEBUG_SHADOW
        log_os << __FUNCTION__ << ": fakeRefSpan: " << fakeRefSpan
               << " shadowRefSpan: " << shadowRefSpan
               << " contigBeginOffset: " << contigBeginOffset
               << " alignBeginPos: " << readAlignment.align.beginPos
               << " readContigEndOffset: " << readContigEndOffset
               << "\n";
#endif
    }
    else
    {
        // approximate mate as having conventional alignment -- we could fix
        // this with some buffering in ShadowReadFinder
        fakeRefSpan.set_end_pos(anchorPos + floatRead.size());

        // set fake begin as if the insert allele continued TO THE LEFT in reference coordinates:

        // offset of read begin on the contig, in contig coordinates:
        const int readContigBeginOffset(contigBeginOffset + readAlignment.align.beginPos);

        // translate contig coordinates to fake reference coordinates:
        if (readContigBeginOffset > _contig.bpBOffset.begin_pos())
        {
            // definitely does not meet the breakend overlap criteria:
            return false;
        }

        const int readContigBeginRefOffset(_contig.segmentSpan.end_pos()-(_contig.bpBOffset.begin_pos()-readContigBeginOffset));

        fakeRefSpan.set_begin_pos(readContigBeginRefOffset);
#ifdef DEBUG_SHADOW
        log_os << __FUNCTION__ << ": fakeRefSpan: " << fakeRefSpan
               << " contigBeginOffset: " << contigBeginOffset
               << " alignBeginPos: " << readAlignment.align.beginPos
               << " readContigBeginOffset: " << readContigBeginOffset
               << "\n";
#endif
    }

    if (fakeRefSpan.begin_pos() > fakeRefSpan.end_pos())
    {
        using namespace illumina::common;

        std::ostringstream oss;
        oss << "ERROR: Failed to parse fragment range from bam record. Frag begin,end: " << fakeRefSpan << " bamRecord: " << bamRead << "\n";
        BOOST_THROW_EXCEPTION(LogicException(oss.str()));
    }

    altTemplateSize=fakeRefSpan.size();

    //
    // finally determine if we cross a breakend boundary
    //
    if (! testFragOverlap(fakeRefSpan.begin_pos(), fakeRefSpan.end_pos())) return false;

    // made it!
#ifdef DEBUG_SHADOW
    log_os << __FUNCTION__ << ": shadow passed pair tests\n";
#endif

    return true;
}



bool
SVScorePairAltProcessor::
alignShadowRead(
    const bam_record& bamRead,
    int& altTemplateSize)
{
    // TODO: basecall qualities??

    // does the shadow occur to the left or right of the insertion?
    const bool isLeftOfInsert(bamRead.is_mate_fwd_strand());
#ifdef DEBUG_SHADOW
    log_os << __FUNCTION__ << ": isLeftOfInsert: " << isLeftOfInsert << "\n";
#endif

    // do we need to revcomp the sequence?
    std::string shadowRead(bamRead.get_bam_read().get_string());
    if (isLeftOfInsert)
    {
        reverseCompStr(shadowRead);
    }

    const pos_t matePos(bamRead.mate_pos() -1);

    return realignPairedRead(bamRead, isLeftOfInsert, shadowRead, matePos, altTemplateSize);
}



void
SVScorePairAltProcessor::
processClearedRecord(
    const bam_record& bamRead)
{
    using namespace illumina::common;

    assert(bamParams.isSet);

    const pos_t refPos(bamRead.pos()-1);
    if (! bamParams.interval.range.is_pos_intersect(refPos)) return;

    // many special rules applied for large insertions:
    const bool isLargeInsert(isLargeInsertSV(sv));

    bool isShadowAlignment(false);
    bool isRepeatChimeraAlignment(false);

    int templateSize(0);
    int altTemplateSize(0);

    if (isLargeInsert)
    {
        // test for shadow
        {
            const bool isShadowRead(_shadow.check(bamRead));

            if (isShadowRead)
            {

                // does the shadow occur to the left or right of the insertion?
                const bool isLeftOfInsert(bamRead.is_mate_fwd_strand());

                // eval left of insert for Bp1 and right of insert for Bp2:
                if (isLeftOfInsert != isBp1)
                {
#ifdef DEBUG_SHADOW
                    log_os << __FUNCTION__ << ": shadow WEREWOLF isLeft: "  << isLeftOfInsert << " " << isBp1 << "\n";
#endif
                    return;
                }

                isShadowAlignment=alignShadowRead(bamRead,altTemplateSize);

                if (! isShadowAlignment) return;
#ifdef DEBUG_SHADOW
                log_os << __FUNCTION__ << ": read passed shadow test altsize/record: "  << altTemplateSize << "/" << bamRead << "\n";
#endif
            }
            else
            {
                // record the mapq value of the shadow mate:
                if (_shadow.isShadowMate())
                {
                    SVFragmentEvidence& fragment(evidence.getSampleEvidence(bamParams.bamIndex)[bamRead.qname()]);
                    SVFragmentEvidenceRead& evRead(fragment.getRead(bamRead.is_first()));
                    setReadEvidence(svParams.minMapQ, svParams.minTier2MapQ, bamRead, isShadowAlignment, evRead);
                }

                // ok, not a shadow read, kick the read out if it fits shadow or shadow-mate criteria:
                if (bamRead.is_unmapped() || (bamRead.is_paired() && bamRead.is_mate_unmapped())) return;
            }
        }

        // test for MAPQ0 pair
        {
            typedef RemoteReadCache::const_iterator riter;
            const RemoteReadCache& remotes(assemblyData.remoteReads);
            riter remoteIter(remotes.find(bamRead.qname()));

            if (remoteIter != remotes.end())
            {
                if (remoteIter->second.readNo != ( (bamRead.read_no() == 1) ? 2 : 1 )) return;

                const bool isLeftOfInsert(bamRead.is_fwd_strand());

                // eval left of insert for Bp1 and right of insert for Bp2:
                if (isLeftOfInsert != isBp1)
                {
#ifdef DEBUG_SHADOW
                    log_os << __FUNCTION__ << ": chimera WEREWOLF isLeft: "  << isLeftOfInsert << " " << isBp1 << "\n";
#endif
                    return;
                }

                const pos_t anchorPos(bamRead.pos()-1);
                const std::string& remoteRead(remoteIter->second.readSeq); /// read is already revcomped as required when stored in cache
                isRepeatChimeraAlignment=realignPairedRead(bamRead, isLeftOfInsert, remoteRead, anchorPos, altTemplateSize);

                if (! isRepeatChimeraAlignment) return;
            }
            else
            {
                /// if we establish it's not a repeat chimera, then filter back down to the usual pair candidates:
                if (! (bamRead.is_unmapped() || bamRead.is_mate_unmapped()))
                {
                    if (! is_innie_pair(bamRead)) return;
                }
            }
        }
    }

    const bool isRealignedTemplate(isLargeInsert && (isShadowAlignment || isRepeatChimeraAlignment));

#ifdef DEBUG_MEGAPAIR
    log_os << __FUNCTION__ << ": read: " << bamRead << "\n";
#endif

    /// check if fragment is too big or too small:
    bool isAnomTemplate(true);
    if (! isRealignedTemplate)
    {
        templateSize=(std::abs(bamRead.template_size()));
        altTemplateSize=(templateSize-svParams.altShift);

        isAnomTemplate=((templateSize < bamParams.minFrag) || (templateSize > bamParams.maxFrag));
    }

#ifdef DEBUG_MEGAPAIR
    log_os << __FUNCTION__ << ": tSize/aSize: " << templateSize << " " << altTemplateSize << "\n";
#endif

    // only filter out anomalous fragments for alt if the ref is also being filtered out:
    //  (if we don't do this there will be a frag prob for ref and a zero for alt, leading to skewed results)
    if (isAnomTemplate)
    {
        if (altTemplateSize < bamParams.minFrag)
        {
#ifdef DEBUG_MEGAPAIR
            log_os << __FUNCTION__ << ": altsize below min\n";
#endif
            return;
        }
        if (altTemplateSize > bamParams.maxFrag)
        {
#ifdef DEBUG_MEGAPAIR
            log_os << __FUNCTION__ << ": altsize above max\n";
#endif
            return;
        }
    }

    // get fragment range and check overlap with breakend:
    if (! isRealignedTemplate)
    {
        // count only from the down stream reads
        const bool isFirstBamRead(isFirstRead(bamRead));

        pos_t fragBeginRefPos(refPos);
        if (! isFirstBamRead)
        {
            fragBeginRefPos=bamRead.mate_pos()-1;
        }

        const pos_t fragEndRefPos(fragBeginRefPos+templateSize);

        if (fragBeginRefPos > fragEndRefPos)
        {
            std::ostringstream oss;
            oss << "ERROR: Failed to parse fragment range from bam record. Frag begin,end: " << fragBeginRefPos << " " << fragEndRefPos << " bamRecord: " << bamRead << "\n";
            BOOST_THROW_EXCEPTION(LogicException(oss.str()));
        }

        if (! testFragOverlap(fragBeginRefPos, fragEndRefPos)) return;
    }

    SVFragmentEvidence& fragment(evidence.getSampleEvidence(bamParams.bamIndex)[bamRead.qname()]);

    SVFragmentEvidenceRead& evRead(fragment.getRead(bamRead.is_first()));

    const unsigned readSize(bamRead.read_size());
    unsigned mapq(bamRead.map_qual());
    if (isShadowAlignment)
    {
        mapq=_shadow.getMateMapq();
    }
    setReadEvidence(svParams.minMapQ, svParams.minTier2MapQ, mapq, readSize, isRealignedTemplate, evRead);

    if (isRepeatChimeraAlignment)
    {
        //enter the mate read:
        SVFragmentEvidenceRead& evMateRead(fragment.getRead(! bamRead.is_first()));
        setReadEvidence(svParams.minMapQ, svParams.minTier2MapQ, mapq, readSize, isRealignedTemplate, evMateRead);
    }

    setAlleleFrag(*bamParams.fragDistroPtr, altTemplateSize, fragment.alt.getBp(isBp1), isLargeInsert);
#ifdef DEBUG_MEGAPAIR
    log_os << __FUNCTION__ << ": altset: " << fragment.alt.getBp(isBp1) << "\n";
#endif

    if (! isRealignedTemplate)
    {
        // when an alt entry is made for a fragment, we try to always create corresponding ref entry
        // in theory this will get picked up by the ref scanner anyway, but the cost of missing this
        // is all sorts of really bad somatic FNs
        setAlleleFrag(*bamParams.fragDistroPtr, templateSize, fragment.ref.getBp(isBp1), isLargeInsert);
    }
}
