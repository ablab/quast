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
/// \author Ole Schulz-Trieglaff
/// \author Chris Saunders
///

#include "SVCandidateAssemblyRefiner.hh"

#include "alignment/AlignmentUtil.hh"
#include "blt_util/log.hh"
#include "blt_util/seq_util.hh"
#include "blt_util/align_path.hh"
#include "htsapi/samtools_fasta_util.hh"
#include "common/Exceptions.hh"
#include "manta/SVCandidateUtil.hh"
#include "manta/SVReferenceUtil.hh"

#include <iostream>
#include <unordered_set>

//#define DEBUG_REFINER
//#define DEBUG_CONTIG
//#define DEBUG_KMER

#ifdef DEBUG_REFINER
#include "blt_util/seq_printer.hh"
#endif



/// process assembly/align info into simple reference coordinates that can be reported in the output vcf:
///
/// \param[in] isAlign1 if true, this breakend was aligned first by the jump aligner, and therefore left-aligned (if fwd)
///                     or right-aligned (if rev)
/// \param[in] jumpRange homologous range across the breakend
///
static
void
adjustAssembledBreakend(
    const Alignment& align,
    const bool isAlign1,
    const unsigned jumpRange,
    const reference_contig_segment& ref,
    const bool isReversed,
    SVBreakend& bp)
{
    const pos_t bpBeginOffset(getAlignBeginOffset(align, ref.seq().size(), isReversed));
    const pos_t bpEndOffset(getAlignEndOffset(align, ref.seq().size(), isReversed));

    const bool isBpAtAlignEnd(bp.state == SVBreakendState::RIGHT_OPEN);

    const pos_t bpBreakendOffset(isBpAtAlignEnd ? (bpEndOffset -1) : bpBeginOffset );
    const pos_t bpBreakendPos(ref.get_offset() + bpBreakendOffset);

    const bool isLeftAligned(isAlign1 == isBpAtAlignEnd);

    known_pos_range2& range(bp.interval.range);

    if (isLeftAligned)
    {
        range.set_begin_pos(bpBreakendPos);
        range.set_end_pos(bpBreakendPos + static_cast<pos_t>(jumpRange) + 1);
    }
    else
    {
        range.set_begin_pos(bpBreakendPos - static_cast<pos_t>(jumpRange));
        range.set_end_pos(bpBreakendPos + 1);
    }
}


/// \param[in] maxQCRefSpan what is the longest flanking sequence length considered for the high quality qc requirement?
static
bool
isFilterSpanningAlignment(
    const unsigned maxQCRefSpan,
    const GlobalJumpAligner<int>& aligner,
    const bool isLeadingPath,
    const bool isRNA,
    const ALIGNPATH::path_t& input_apath)
{
    unsigned minAlignReadLength(30); ///< require min length of each contig sub-alignment even after off-reference clipping:
    static const float minScoreFrac(0.75); ///< require min fraction of optimal score in each contig sub-alignment
    if (isRNA)
    {
        minAlignReadLength = 20;
    }

    ALIGNPATH::path_t apath(input_apath);

    // prepare apath by orienting it always going forward from the breakend and limiting the length to
    // the first maxQCRefSpan ref bases covered:
    //
    if (isLeadingPath)
    {
        std::reverse(apath.begin(),apath.end());
    }

    apath_limit_ref_length(maxQCRefSpan,apath);

    const unsigned readSize(apath_read_length(apath));
    const unsigned clipSize(apath_soft_clip_trail_size(apath));

    assert(clipSize <= readSize);

    const unsigned clippedReadSize(readSize-clipSize);

    if (clippedReadSize < minAlignReadLength)
    {
#ifdef DEBUG_REFINER
        log_os << __FUNCTION__ << ": Rejecting highest scoring contig sub-alignment. Sub-alignment read length after clipping is: " << clippedReadSize << " min size is: " << minAlignReadLength << "\n";
#endif
        return true;
    }

    int nonClipScore(aligner.getPathScore(apath, false));
    const int optimalScore(clippedReadSize * aligner.getScores().match);

    assert(optimalScore>0);
    if (nonClipScore < 0) nonClipScore = 0;

    const float scoreFrac(static_cast<float>(nonClipScore)/static_cast<float>(optimalScore));

    if (scoreFrac < minScoreFrac)
    {
#ifdef DEBUG_REFINER
        log_os << __FUNCTION__ << ": Rejecting highest scoring contig sub-alignment. Fraction of optimal alignment score is: " << scoreFrac << " minScoreFrac: " << minScoreFrac << "\n";
#endif
        return true;
    }
    return false;
}


/// identify path indel sequences with an insert or delete segment greater than minSize
///
static
void
getLargeIndelSegments(
    const ALIGNPATH::path_t& apath,
    const unsigned minSize,
    std::vector<std::pair<unsigned,unsigned> >& segments)
{
    using namespace ALIGNPATH;

    bool isInSegment(false);
    bool isCandidate(false);
    unsigned segmentStart(0);

    segments.clear();

    const unsigned as(apath.size());
    for (unsigned i(0); i<as; ++i)
    {
        const path_segment& ps(apath[i]);

        if ((ps.type == DELETE) || (ps.type == INSERT))
        {
            if (ps.length>=minSize) isCandidate=true;
            if (! isInSegment) segmentStart = i;
            isInSegment=true;
        }
        else
        {
            if (isCandidate)
            {
                assert(i>0);
                segments.push_back(std::make_pair(segmentStart,(i-1)));
            }
            isInSegment=false;
            isCandidate=false;
        }
    }

    if (isCandidate)
    {
        assert(as>0);
        segments.push_back(std::make_pair(segmentStart,(as-1)));
    }
}



#ifdef ITERATIVE_ASSEMBLER
static
unsigned
getLargestIndelSize(
    const ALIGNPATH::path_t& apath,
    const std::vector<std::pair<unsigned,unsigned> >& segments)
{
    unsigned largestSize(0);

    typedef std::pair<unsigned,unsigned> segment_t;
    for (const segment_t& seg : segments)
    {
        for (unsigned i(seg.first); i<=seg.second; i++)
        {
            const ALIGNPATH::path_segment& ps(apath[i]);
            if ((ps.type == ALIGNPATH::DELETE) || (ps.type == ALIGNPATH::INSERT))
            {
                if (ps.length > largestSize) largestSize = ps.length;
            }
        }
    }

    return largestSize;
}
#endif


/// identify the single largest insert segment, if one exists above minSize:
///
static
void
getLargestInsertSegment(
    const ALIGNPATH::path_t& apath,
    const unsigned minSize,
    std::vector<std::pair<unsigned,unsigned> >& segments)
{
    using namespace ALIGNPATH;

    bool isInSegment(false);
    bool isCandidate(false);
    unsigned segmentStart(0);

    bool isMaxSegment(false);
    unsigned maxSegmentSize(minSize);
    std::pair<unsigned,unsigned> maxSegment;

    segments.clear();

    const unsigned as(apath.size());
    for (unsigned i(0); i<as; ++i)
    {
        const path_segment& ps(apath[i]);

        if ((ps.type == DELETE) || (ps.type == INSERT))
        {
            if (ps.type == INSERT)
            {
                if (ps.length>=maxSegmentSize)
                {
                    isMaxSegment=true;
                    maxSegmentSize=ps.length;
                    isCandidate=true;
                }
            }
            if (! isInSegment) segmentStart = i;
            isInSegment=true;
        }
        else
        {
            if (isCandidate)
            {
                assert(i>0);
                maxSegment=std::make_pair(segmentStart,(i-1));
            }
            isInSegment=false;
            isCandidate=false;
        }
    }

    if (isCandidate)
    {
        assert(as>0);
        maxSegment=std::make_pair(segmentStart,(as-1));
    }

    if (isMaxSegment)
    {
        segments.push_back(maxSegment);
    }
}



/// add simple cigar string to spanning alignments for the subset of cases (insertions and deletions) where this is possible
///
/// note that we may not always print this out, even though we compute the cigar here -- this is dependent on the output file
/// format and conventions related to variant size, precision, etc.
///
static
void
addCigarToSpanningAlignment(
    SVCandidate& sv)
{
    const SV_TYPE::index_t svType(getSVType(sv));

    if (svType != SV_TYPE::INDEL) return;

    const bool isBp1First(sv.bp1.interval.range.begin_pos()<=sv.bp2.interval.range.begin_pos());

    const SVBreakend& bpA(isBp1First ? sv.bp1 : sv.bp2);
    const SVBreakend& bpB(isBp1First ? sv.bp2 : sv.bp1);

    const pos_t deleteSize((bpB.interval.range.begin_pos() - bpA.interval.range.begin_pos()) -1);
    const pos_t insertSize(sv.insertSeq.size());

    assert(deleteSize >= 0);
    assert(insertSize || deleteSize);

    // follow convention from non-spanning alignments of xIyD in case of complex variant:
    if (insertSize)
    {
        sv.insertAlignment.emplace_back(ALIGNPATH::INSERT,insertSize);
    }
    if (deleteSize)
    {
        sv.insertAlignment.emplace_back(ALIGNPATH::DELETE,deleteSize);
    }
}



/// \param[in] maxQCRefSpan what is the longest flanking sequence length considered for the high quality qc requirement?
static
bool
isSmallSVSegmentFilter(
    const unsigned maxQCRefSpan,
    const AlignerBase<int>& aligner,
    const bool isLeadingPath,
    const bool isComplex,
    ALIGNPATH::path_t& apath)
{
    static const unsigned minAlignRefSpanSimple(30); ///< min reference length for alignment
    static const unsigned minAlignReadLengthSimple(30); ///< min length of alignment after off-reference clipping
    static const unsigned minAlignRefSpanComplex(35); ///< min reference length for alignment
    static const unsigned minAlignReadLengthComplex(35); ///< min length of alignment after off-reference clipping
    static const float minScoreFrac(0.75); ///< min fraction of optimal score in each contig sub-alignment:

    const unsigned minAlignRefSpan(isComplex ? minAlignRefSpanComplex : minAlignRefSpanSimple);
    const unsigned minAlignReadLength(isComplex ? minAlignReadLengthComplex : minAlignReadLengthSimple);

    /// prepare apath by orienting it always going forward from the breakend and limiting the length to
    /// the first maxQCRefSpan ref bases covered:
    ///
    if (isLeadingPath)
    {
        std::reverse(apath.begin(),apath.end());
    }

    apath_limit_ref_length(maxQCRefSpan,apath);

    const unsigned refSize(apath_read_length(apath));
    if (refSize < minAlignRefSpan)
    {
        return true;
    }

    const unsigned pathSize(apath_read_length(apath));
    const unsigned clipSize(apath_soft_clip_trail_size(apath));

    assert(clipSize <= pathSize);

    const unsigned clippedPathSize(pathSize-clipSize);

    if (clippedPathSize < minAlignReadLength)
    {
#ifdef DEBUG_REFINER
//        log_os << "Rejecting highest scoring contig sub-alignment. isFirst?: " << isFirstRead << ". Sub-alignmnet read length after clipping is: " << clippedPathSize << " min size is: " << minAlignReadLength << "\n";
#endif
        return true;
    }

    const int nonClipScore(std::max(0,aligner.getPathScore(apath, false)));
    const int optimalScore(clippedPathSize * aligner.getScores().match);

    const float scoreFrac(static_cast<float>(nonClipScore)/static_cast<float>(optimalScore));
    if (scoreFrac < minScoreFrac)
    {
#ifdef DEBUG_REFINER
//        log_os << "Rejecting highest scoring contig sub-alignment. isFirst?: " << isFirstRead << ". Fraction of optimal alignment score is: " << scoreFrac << " minScoreFrac: " << minScoreFrac << "\n";
#endif
        return true;
    }

    return false;
}


static
int
searchContig(
    const std::string& targetSeq,
    const std::string& querySeq,
    const float mismatchRate)
{
    unsigned numOccur = 0;
    const unsigned querySize = querySeq.size();
    const unsigned targetSize = targetSeq.size();

    if (querySize > targetSize) return numOccur;

    // set the scanning start & end to make sure the candidate window is overlapping the breakpoint
    const unsigned scanStart = 0;
    const unsigned scanEnd = targetSize - querySize;

    for (unsigned i(scanStart); i<= scanEnd; i++)
    {
        unsigned mismatches = 0;
        for (unsigned j(0); j < querySize; j++)
        {
            if ((querySeq[j] != targetSeq[i+j]) || (querySeq[j] == 'N'))
                mismatches ++;
        }

        if (float(mismatches)/float(querySize) <= mismatchRate)
            numOccur++;
    }

    return numOccur;
}


/// test whether this single-node assembly is (1) an interesting variant above the minimum size and
/// (2) passes QC otherwise (appropriate flanking regions, etc)
///
/// \param[in] maxQCRefSpan what is the longest flanking sequence length considered for the high quality qc requirement?
///
/// \return true if these segments are candidates
///
static
bool
isSmallAssemblerSVAlignment(
    const unsigned maxQCRefSpan,
    const AlignerBase<int>& aligner,
    const Alignment& align,
    const std::string& contigSeq,
    const std::string& refSeq,
    const unsigned minCandidateIndelSize,
    std::vector<std::pair<unsigned,unsigned> >& candidateSegments,
    const bool isLargeOnly = false)
{
    using namespace ALIGNPATH;

    const path_t& apath(align.apath);

    // (1) identify all indels above minimum size:
    //
    getLargeIndelSegments(apath, minCandidateIndelSize, candidateSegments);

    // escape if there are no indels above the minimum size
    if (candidateSegments.empty()) return false;

    // complex is anything besides a simple single indel -- more than one indel or an insert/delete combination:
    const bool isComplex((candidateSegments.size() > 1) || (candidateSegments[0].first != candidateSegments[0].second));

    // loop through possible leading segments until a clean one is found:
    //
    while (true)
    {
        // test quality of alignment segments surrounding the variant region:
        const unsigned firstCandIndelSegment(candidateSegments.front().first);
        path_t leadingPath(apath.begin(), apath.begin()+firstCandIndelSegment);

        static const bool isLeadingPath(true);
        if (! isSmallSVSegmentFilter(maxQCRefSpan, aligner, isLeadingPath, isComplex, leadingPath))
        {
            break;
        }

        // escape if this was the last segment
        if (1 == candidateSegments.size()) return false;

        candidateSegments = std::vector<std::pair<unsigned,unsigned> >(candidateSegments.begin()+1,candidateSegments.end());
    }

    // loop through possible trailing segments until a clean one is found:
    //
    while (true)
    {
        // test quality of alignment segments surrounding the variant region:
        const unsigned lastCandIndelSegment(candidateSegments.back().second);
        path_t trailingPath(apath.begin()+lastCandIndelSegment+1, apath.end());

        static const bool isLeadingPath(false);
        if (! isSmallSVSegmentFilter(maxQCRefSpan, aligner, isLeadingPath, isComplex, trailingPath))
        {
            break;
        }

        // escape if this was the last segment
        if (1 == candidateSegments.size()) return false;

        candidateSegments.pop_back();
    }


    {
        // TODO: iterate on all segments
        const path_t apathTillSvStart(apath.begin(), apath.begin() + candidateSegments.front().first);
        const path_t apathTillSvEnd(apath.begin(), apath.begin() + candidateSegments.back().second+1);

        const int leftSize = apath_read_length(apathTillSvStart);
        const int endPos = apath_read_length(apathTillSvEnd);
        const int rightSize = contigSeq.length() - endPos;
        const std::string leftContig = contigSeq.substr(0, leftSize);
        const std::string rightContig = contigSeq.substr(endPos, rightSize);

        const int searchWindow(500);
        const float mismatchRate(0.05f);
        const int refAlignStart = align.beginPos;
        const int refAlignEnd = align.beginPos + apath_ref_length(apath);

        // search leftContig in the downstream of refStart
        const int leftSearchStart = std::max(0, refAlignEnd-searchWindow);
        const std::string refSeq4LeftSearch = refSeq.substr(leftSearchStart, (refAlignEnd-leftSearchStart));
        unsigned occurrences = searchContig(refSeq4LeftSearch, leftContig, mismatchRate);

#ifdef DEBUG_CONTIG
        log_os << __FUNCTION__ << ": refSeq4LeftSearch: \n" << refSeq4LeftSearch << "\n";
        log_os << __FUNCTION__ << ": left contig has size " << leftSize << ":\n" << leftContig << "\n";
        log_os << __FUNCTION__ << ": left contig occurrences " << occurrences << "\n";
#endif
        if (occurrences > 1) return false;

        // search rightContig in the upstream of refEnd
        const int rightSearchSize = std::min(searchWindow, int(refSeq.length()-refAlignStart));
        const std::string refSeq4RightSearch = refSeq.substr(refAlignStart, rightSearchSize);
        occurrences = searchContig(refSeq4RightSearch, rightContig, mismatchRate);

#ifdef DEBUG_CONTIG
        log_os << __FUNCTION__ << ": refSeq4RightSearch: \n" << refSeq4RightSearch << "\n";
        log_os << __FUNCTION__ << ": right contig has size " << rightSize << ":\n" << rightContig << "\n";
        log_os << __FUNCTION__ << ": right contig occurrences " << occurrences << "\n";
#endif
        if (occurrences > 1) return false;
    }

    if (isLargeOnly)
    {
        // only accept large indels in this case
        typedef std::pair<unsigned,unsigned> segment_t;
        std::vector<segment_t> tmpseg(candidateSegments);
        candidateSegments.clear();
        for (const segment_t& segment : tmpseg)
        {
            for (unsigned i(segment.first); i<=segment.second; ++i)
            {
                if (((apath[i].type == INSERT) && (apath[i].length >= 80)) ||
                    ((apath[i].type == DELETE) && (apath[i].length >= 200)))
                {
                    candidateSegments.push_back(segment);
                    break;
                }
            }
        }
    }

    return (! candidateSegments.empty());
}



static const unsigned minSemiLargeInsertionLength(40); // if a large insertion is not complete assembled, it must be assembled at least this far into either side


/// \params[in] trimInsertLength remove extra length from the end of the contig
/// for the purpose of determining if the "unaligned" end is long enough
///
/// \return true if this is a left->right insert candidate
///
static
bool
isLargeInsertSegment(
    const AlignerBase<int>& aligner,
    const ALIGNPATH::path_t& apath,
    unsigned& contigOffset,
    unsigned& refOffset,
    int& score,
    const unsigned trimInsertLength = 0)
{
    using namespace ALIGNPATH;

    static const unsigned minAlignReadLength(40); ///< min length of aligned portion of contig
    static const unsigned minExtendedReadLength(minSemiLargeInsertionLength); ///< min length of unaligned portion of contig

    static const unsigned minAlignRefSpan(40); ///< min reference length for alignment
    static const float minScoreFrac(0.75); ///< min fraction of optimal score in each contig sub-alignment:

    const unsigned pathSize(apath_read_length(apath));

    /// first evaluate in the forward direction
    score=(std::max(0,aligner.getMaxPathScore(apath, contigOffset, refOffset)));

#ifdef DEBUG_REFINER
    log_os << __FUNCTION__ << ": apath " << apath << "\n";
    log_os << __FUNCTION__ << ": score/ref/contig " << score << " " << refOffset << " " << contigOffset << "\n";
#endif

    if (refOffset < minAlignRefSpan) return false;
    if (contigOffset < minAlignReadLength) return false;

    assert(contigOffset <= pathSize);
    if ((pathSize-contigOffset) < (minExtendedReadLength+trimInsertLength)) return false;

    const int optimalScore(contigOffset * aligner.getScores().match);

    const float scoreFrac(static_cast<float>(score)/static_cast<float>(optimalScore));
    if (scoreFrac < minScoreFrac) return false;

    return true;
}



/// \return true if there is a large insert candidate
///
static
bool
isLargeInsertAlignment(
    const GlobalAligner<int> aligner,
    const ALIGNPATH::path_t& apath,
    LargeInsertionInfo& insertInfo)
{
    using namespace ALIGNPATH;

    insertInfo.isLeftCandidate=isLargeInsertSegment(aligner, apath, insertInfo.contigOffset, insertInfo.refOffset, insertInfo.score);

    if (insertInfo.isLeftCandidate)
    {
        return true;
    }

    ALIGNPATH::path_t apath_rev(apath);
    std::reverse(apath_rev.begin(),apath_rev.end());

    insertInfo.isRightCandidate=isLargeInsertSegment(aligner,apath_rev,insertInfo.contigOffset,insertInfo.refOffset, insertInfo.score);

    if (insertInfo.isRightCandidate)
    {
        const unsigned contigSize(apath_read_length(apath));
        const unsigned refSize(apath_ref_length(apath));
        insertInfo.contigOffset=contigSize-insertInfo.contigOffset;
        insertInfo.refOffset=refSize-insertInfo.refOffset;
        return true;
    }

    return false;
}



/// \return true if the alignment represents an acceptable complete insertion:
///
static
bool
isFinishedLargeInsertAlignment(
    const GlobalAligner<int> aligner,
    const ALIGNPATH::path_t& apath,
    const std::pair<unsigned, unsigned>& insertSegment,
    const unsigned middleSize)
{
    using namespace ALIGNPATH;

    const path_t apath_left(apath.begin(), apath.begin()+insertSegment.second+1);;

    LargeInsertionInfo insertInfo;
    insertInfo.isLeftCandidate=isLargeInsertSegment(aligner, apath_left, insertInfo.contigOffset, insertInfo.
                                                    refOffset, insertInfo.score, middleSize);

    path_t apath_rev(apath.begin()+insertSegment.first, apath.end());;
    std::reverse(apath_rev.begin(),apath_rev.end());

    insertInfo.isRightCandidate=isLargeInsertSegment(aligner, apath_rev, insertInfo.contigOffset, insertInfo.
                                                     refOffset, insertInfo.score, middleSize);

    return (insertInfo.isLeftCandidate && insertInfo.isRightCandidate);
}



/// get the range over which an alignment element can vary with equal edit distance
///
/// \param[in] refRange range of the event (ie indel) of interest in reference coordinates
/// \param[in] readRange range of the event (ie indel) of interest in read coordinates
///
/// range coordinates are zero indexed and start at the first affected positions (so are not like vcf coordinates)
/// for instance:
////  the deletion 10M1D10M would have refRange(10,11), readRange(10,10)
////  the insertion 10M1I10M would have refRange(10,10), readRange(10,11)
///
static
known_pos_range2
getVariantRange(
    const std::string& ref,
    const known_pos_range2& refRange,
    const std::string& read,
    const known_pos_range2& readRange)
{
#ifdef DEBUG_VARR
    log_os << __FUNCTION__ << ": refRange " << refRange << "\n";
    log_os << __FUNCTION__ << ": ref:\n";
    printSeq(ref, log_os);
    log_os << "\n";
    log_os << __FUNCTION__ << ": readRange " << readRange << "\n";
    log_os << __FUNCTION__ << ": read:\n";
    printSeq(read, log_os);
    log_os << "\n";
#endif

    // check how far we can slide to the right:
    const pos_t maxRightOffset(std::min(ref.size()-refRange.end_pos(), read.size()-readRange.end_pos()));
    pos_t rightOffset(0);
    for (; rightOffset<maxRightOffset; ++rightOffset)
    {
        const char refSym(ref[refRange.begin_pos()+rightOffset]);
        const char readSym(read[readRange.begin_pos()+rightOffset]);
        if (refSym != readSym) break;
    }

    // check how far we can slide to the left:
    const pos_t minLeftOffset(std::max(-refRange.begin_pos(), -readRange.begin_pos()));
    pos_t leftOffset(0);
    for (; leftOffset>=minLeftOffset; --leftOffset)
    {
        const char refSym(ref[refRange.end_pos()+leftOffset-1]);
        const char readSym(read[readRange.end_pos()+leftOffset-1]);
        if (refSym != readSym) break;
    }

#ifdef DEBUG_VARR
    log_os << __FUNCTION__ << ": left/right offset " << leftOffset << "/" << rightOffset << "\n";
#endif

    return known_pos_range2(leftOffset,rightOffset);
}



/// process smallSV alignment section into a usable sv candidate
static
void
setSmallCandSV(
    const reference_contig_segment& ref,
    const std::string& contig,
    const Alignment& align,
    const std::pair<unsigned,unsigned>& segRange,
    SVCandidate& sv)
{
#ifdef DEBUG_VARR
    log_os << __FUNCTION__ << ": align " << align << "\n";
    log_os << __FUNCTION__ << ": segRange [" << segRange.first << "," << segRange.second << "]\n";
    log_os << __FUNCTION__ << ": inputSV " << sv << "\n";
#endif
    sv.setPrecise();

    // get readRange and refRange, which are translations of segRange into
    // read and reference offsets:
    known_pos_range2 readRange;
    known_pos_range2 refRange;
    {
        using namespace ALIGNPATH;

        pos_t readPos(0);
        pos_t refPos(align.beginPos);

        const path_t& apath(align.apath);
        const unsigned as(apath.size());
        for (unsigned i(0); i<as; ++i)
        {
            const path_segment& ps(apath[i]);
            if (i == segRange.first)
            {
                refRange.set_begin_pos(refPos);
                readRange.set_begin_pos(readPos);
            }

            if (is_segment_type_ref_length(ps.type)) refPos += ps.length;
            if (is_segment_type_read_length(ps.type)) readPos += ps.length;

            if (i == segRange.second)
            {
                refRange.set_end_pos(refPos);
                readRange.set_end_pos(readPos);
            }
        }
    }

    // by how many positions can the alignment position vary with the same alignment score?:
    const known_pos_range2 cipos(getVariantRange(ref.seq(),refRange, contig, readRange));

    // cipos for a precise variant is expected to start from 0 and extend forward zero to many bases
    if (cipos.begin_pos() != 0)
    {
        using namespace illumina::common;

        std::ostringstream oss;
        oss << "ERROR: Attempting to convert alignment to sv candidate."
            << " contigSize: " << contig.size()
            << " alignment: " << align
            << " segments: [" << segRange.first << "," << segRange.second << "]\n"
            << "\treadRange: " << readRange << "\n"
            << "\trefRange: " << refRange << "\n"
            << "\tcipos: " << cipos << "\n";
        BOOST_THROW_EXCEPTION(LogicException(oss.str()));
    }

    sv.bp1.state = SVBreakendState::RIGHT_OPEN;
    const pos_t beginPos(ref.get_offset()+refRange.begin_pos()-1);
    sv.bp1.interval.range.set_range(beginPos,beginPos+cipos.end_pos()+1);

    sv.bp2.state = SVBreakendState::LEFT_OPEN;
    const pos_t endPos(ref.get_offset()+refRange.end_pos());
    sv.bp2.interval.range.set_range(endPos,endPos+cipos.end_pos()+1);
    sv.bp2.interval.tid = sv.bp1.interval.tid;

    sv.insertSeq = contig.substr(readRange.begin_pos(),readRange.size());

    // add CIGAR for all indels:
    sv.insertAlignment = ALIGNPATH::path_t(align.apath.begin()+segRange.first, align.apath.begin()+segRange.second+1);
}



static
known_pos_range2
getInsertTrim(
    const ALIGNPATH::path_t& apath,
    const std::pair<unsigned,unsigned>& segRange)
{
    assert(segRange.first <= segRange.second);

    using namespace ALIGNPATH;

    known_pos_range2 range;

    pos_t readPos(0);

    const unsigned as(apath.size());
    for (unsigned i(0); i<as; ++i)
    {
        const path_segment& ps(apath[i]);
        if (i == segRange.first)
        {
            range.set_begin_pos(readPos);
        }

        if (is_segment_type_read_length(ps.type)) readPos += ps.length;

        if (i == segRange.second)
        {
            range.set_end_pos(readPos);
            return range;
        }
    }

    assert(false && "segRange not found");
    return range;
}



// search for combinations of left and right-side insertion candidates to find a good insertion pair
static
void
processLargeInsertion(
    const SVCandidate& sv,
    const pos_t leadingCut,
    const pos_t trailingCut,
    const GlobalAligner<int>& largeInsertCompleteAligner,
    const std::vector<unsigned>& largeInsertionCandidateIndex,
    const std::set<pos_t>& excludedPos,
    SVCandidateAssemblyData& assemblyData)
{
    if (largeInsertionCandidateIndex.empty()) return;

#ifdef DEBUG_REFINER
    static const std::string logtag("processLargeInsertion: ");
    log_os << logtag << "starting large insertion search\n";
#endif

    bool isLargeInsertionPair(false);
    unsigned largeInsertionLeftIndex(0);
    unsigned largeInsertionRightIndex(0);
    int bestBreakDist(0);
    int bestBreakScore(0);

    // try to pair up a large insertion candidate
    //
    // just do a dumb, all against all evaluation for now, if there's more than one left-right candidate set,
    // resolve according to (1) min ref distance and (2) best combined score
    static const int maxBreakDist(35);

    const unsigned candCount(largeInsertionCandidateIndex.size());
    for (unsigned candCount1(0); (candCount1+1)<candCount; ++candCount1)
    {
        const unsigned candIndex1(largeInsertionCandidateIndex[candCount1]);
        const Alignment& align1(assemblyData.smallSVAlignments[candIndex1].align);
        const LargeInsertionInfo& insert1(assemblyData.largeInsertInfo[candIndex1]);
        for (unsigned candCount2(candCount1+1); candCount2<candCount; ++candCount2)
        {
            const unsigned candIndex2(largeInsertionCandidateIndex[candCount2]);
            const Alignment& align2(assemblyData.smallSVAlignments[candIndex2].align);
            const LargeInsertionInfo& insert2(assemblyData.largeInsertInfo[candIndex2]);
            if (! ((insert1.isLeftCandidate && insert2.isRightCandidate) ||
                   (insert2.isLeftCandidate && insert1.isRightCandidate))) continue;

            const int breakDist(std::abs((long int)(align1.beginPos+insert1.refOffset)-(long int)(align2.beginPos+insert2.refOffset)));

            if (breakDist > maxBreakDist) continue;

            const int breakScore(insert1.score+insert2.score);

            if ( (! isLargeInsertionPair) || (breakDist<bestBreakDist) || (breakScore < bestBreakScore))
            {
                /// set new large insertion candidate:
                isLargeInsertionPair=true;
                largeInsertionLeftIndex=candIndex1;
                largeInsertionRightIndex=candIndex2;
                if (insert1.isRightCandidate)
                {
                    std::swap(largeInsertionLeftIndex,largeInsertionRightIndex);
                }
                bestBreakDist=breakDist;
                bestBreakScore=breakScore;

#ifdef DEBUG_REFINER
                log_os << logtag << "setting new large insertion candidate. Score: " << breakScore << "\n";
#endif
            }
        }
    }

    // no large insertion found:
    if (! isLargeInsertionPair) return;

    // found large insertion, insert this into data structures for downstream scoring/reporting:
    {
        const std::string& align1RefStr(assemblyData.bp1ref.seq());
        const unsigned contigCount(assemblyData.contigs.size());

        static const std::string middle("NNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNN");
        const unsigned middleSize(middle.size());

        assemblyData.contigs.resize(contigCount+1);
        assemblyData.smallSVAlignments.resize(contigCount+1);
        assemblyData.smallSVSegments.resize(contigCount+1);
        assemblyData.extendedContigs.resize(contigCount+1);

        AssembledContig& fakeContig(assemblyData.contigs[contigCount]);
        SVCandidateAssemblyData::SmallAlignmentResultType& fakeAlignment(assemblyData.smallSVAlignments[contigCount]);
        std::vector<std::pair<unsigned,unsigned> >& fakeSegments(assemblyData.smallSVSegments[contigCount]);
        std::string& fakeExtendedContig(assemblyData.extendedContigs[contigCount]);

        const AssembledContig& leftContig(assemblyData.contigs[largeInsertionLeftIndex]);
        const AssembledContig& rightContig(assemblyData.contigs[largeInsertionRightIndex]);

        fakeContig=leftContig;
        fakeContig.seq += (middle + rightContig.seq);

        const AssembledContig& constFakeContig(fakeContig);

        largeInsertCompleteAligner.align(
            constFakeContig.seq.begin(), constFakeContig.seq.end(),
            align1RefStr.begin() + leadingCut, align1RefStr.end() - trailingCut,
            fakeAlignment);

        fakeAlignment.align.beginPos += leadingCut;

#ifdef DEBUG_REFINER
        log_os << logtag << "large insertion fake alignment: " << fakeAlignment << "\n";
#endif

        fakeSegments.clear();
        getLargestInsertSegment(fakeAlignment.align.apath, middleSize, fakeSegments);

        // QC segments
        if ((1 != fakeSegments.size()) || (fakeSegments[0].second < fakeSegments[0].first))
        {
            return;
        }

        // QC the resulting alignment:
        if (! isFinishedLargeInsertAlignment(largeInsertCompleteAligner,fakeAlignment.align.apath, fakeSegments[0], middleSize))
        {
            return;
        }

        // final prep step: check left and right partial insert sequences -- this is a last chance to QC for anomalies and get out:
        //
        const known_pos_range2 insertTrim(getInsertTrim(fakeAlignment.align.apath,fakeSegments[0]));
        {
            static const int minFlankSize(minSemiLargeInsertionLength);
            if ((insertTrim.begin_pos()+minFlankSize) > static_cast<pos_t>(leftContig.seq.size()))
            {
                return;
            }

            const pos_t rightOffset(leftContig.seq.size()+middle.size());
            if ((rightOffset+minFlankSize) > insertTrim.end_pos())
            {
                return;
            }
        }

#ifdef DEBUG_REFINER
        log_os << logtag << "large insertion passed QC\n";
#endif

        getExtendedContig(fakeAlignment, fakeContig.seq, align1RefStr, fakeExtendedContig);

        /// this section mostly imitates the regular SV build below, now that we've constructed our fake contig/alignment
        SVCandidate newSV(sv);
        newSV.assemblyAlignIndex = contigCount;
        newSV.assemblySegmentIndex = 0;
        setSmallCandSV(assemblyData.bp1ref, fakeContig.seq, fakeAlignment.align, fakeSegments[0], newSV);

        /// check if this matches a fully assembled insertion:
        const pos_t startPos(newSV.bp1.interval.range.begin_pos());
        if (excludedPos.count(startPos)) return;

        newSV.isUnknownSizeInsertion = true;

        // final step: get left and right partial insert sequences:
        //
        assert(insertTrim.begin_pos() < static_cast<pos_t>(leftContig.seq.size()));
        newSV.unknownSizeInsertionLeftSeq = leftContig.seq.substr(insertTrim.begin_pos());

        const pos_t rightOffset(leftContig.seq.size()+middle.size());
        assert(rightOffset < insertTrim.end_pos());

        newSV.unknownSizeInsertionRightSeq = rightContig.seq.substr(0,(insertTrim.end_pos()-rightOffset));

        assemblyData.svs.push_back(newSV);
    }
}



SVCandidateAssemblyRefiner::
SVCandidateAssemblyRefiner(
    const GSCOptions& opt,
    const bam_header_info& header,
    const AllCounts& counts,
    EdgeRuntimeTracker& edgeTracker) :
    _opt(opt),
    _header(header),
    _smallSVAssembler(opt.scanOpt, opt.refineOpt.smallSVAssembleOpt, opt.alignFileOpt,
                      opt.statsFilename, opt.chromDepthFilename, header, counts, opt.isRNA, edgeTracker.remoteTime),
    _spanningAssembler(opt.scanOpt, opt.refineOpt.spanningAssembleOpt, opt.alignFileOpt,
                       opt.statsFilename, opt.chromDepthFilename, header, counts, opt.isRNA, edgeTracker.remoteTime),
    _smallSVAligner(opt.refineOpt.smallSVAlignScores),
    _largeSVAligner(opt.refineOpt.largeSVAlignScores,opt.refineOpt.largeGapOpenScore),
    _largeInsertEdgeAligner(opt.refineOpt.largeInsertEdgeAlignScores),
    _largeInsertCompleteAligner(opt.refineOpt.largeInsertCompleteAlignScores),
    _spanningAligner(opt.refineOpt.spanningAlignScores, opt.refineOpt.jumpScore),
    _RNASpanningAligner(
        opt.refineOpt.RNAspanningAlignScores,
        opt.refineOpt.RNAJumpScore,
        opt.refineOpt.RNAIntronOpenScore,
        opt.refineOpt.RNAIntronOffEdgeScore)
{}



void
SVCandidateAssemblyRefiner::
getCandidateAssemblyData(
    const SVCandidate& sv,
    const SVCandidateSetData& /*svData*/,
    const bool isRNA,
    const bool isFindLargeInsertions,
    SVCandidateAssemblyData& assemblyData) const
{
#ifdef DEBUG_REFINER
    log_os << __FUNCTION__ << ": START sv " << sv;
#endif

    assemblyData.clear();

    // separate the problem into different assembly categories:
    //
    if (isSpanningSV(sv))
    {
        // record the spanning status of the original low-resolution candidate:
        assemblyData.isCandidateSpanning=true;

        // this case assumes two suspected breakends with a direction to each, most common large scale SV case:
        getJumpAssembly(sv, isRNA, isFindLargeInsertions, assemblyData);
    }
    else if (isComplexSV(sv))
    {
        // record the spanning status of the original low-resolution candidate:
        assemblyData.isCandidateSpanning=false;

        // this case assumes a single-interval local assembly, this is the most common case for small-scale SVs/indels
        getSmallSVAssembly(sv, isFindLargeInsertions, assemblyData);
    }
    else
    {
        log_os << "Unknown candidate SV: " << sv << "\n";
        assert(false && "Unknown candidate SV type");
    }
}



/// Represents a stretch of reference sequence excluded from alignment by the kmer matcher.
struct exclusion_block
{
    exclusion_block(const unsigned s, const unsigned l, const unsigned sp)
        : start(s), length(l), nSpacer(sp) {}
    unsigned start; // Start of excluded region
    unsigned length; // Nunmber of bp excluded
    unsigned nSpacer; // Number of 'N' added in place of excluded sequence
};



/// Translate a reduced reference position to the original reference coordinates
static
unsigned
translateMaskedPos(
    const std::vector<exclusion_block>& exclBlocks,
    const unsigned maskedPos)
{
    int offset = 0;
    for (const auto& cblock : exclBlocks)
    {
        if (cblock.start > (offset + maskedPos)) break;
        offset += cblock.length - cblock.nSpacer;
    }
    return offset + maskedPos;
}



/// Translate an alignment made against a reduced reference to the original reference coordinates
static
bool
translateMaskedAlignment(
    Alignment& align,
    const std::vector<exclusion_block>& exclBlocks)
{
    using namespace ALIGNPATH;
#ifdef DEBUG_KMER
    log_os << __FUNCTION__ << " original: " << align << "\n";
#endif
    path_t newPath;
    pos_t cpos = align.beginPos;
    for (const path_segment& seg : align.apath)
    {
        if (!is_segment_type_ref_length(seg.type))
        {
            newPath.push_back(seg);
        }
        else
        {
            const unsigned length = translateMaskedPos(exclBlocks, cpos + seg.length) -
                                    translateMaskedPos(exclBlocks, cpos);
            if (is_segment_align_match(seg.type) && (length != seg.length)) return false;
#ifdef DEBUG_KMER
            log_os << __FUNCTION__ << " SEGMENT " << seg.type << " " << seg.length << "\n";
            log_os << "\tlength " << length << "\n";
            log_os << "\tcpos " << cpos << "\n";
#endif
            cpos += seg.length;
            newPath.emplace_back(seg.type, length);
        }
    }
    if (align.apath.size() > 0)
    {
        align.beginPos = translateMaskedPos(exclBlocks, align.beginPos);
        align.apath = newPath;
    }
#ifdef DEBUG_KMER
    log_os << __FUNCTION__ << " final: " << align << "\n";
#endif
    return true;
}


namespace
{

/// Returns a reduced reference sequence where long stretches without kmer matches to the contig are removed
template <typename SymIter>
std::string kmerMaskReference(
    const SymIter refSeqStart,
    const SymIter refSeqEnd,
    const std::string& contig,
    const int nSpacer,
    std::vector<exclusion_block>& exclBlocks)
{
    // Hash all kmers in the contig
    static const int merSize(10);
    std::unordered_set<std::string> contigHash;
    for (unsigned contigMerIndex(0); contigMerIndex<(contig.size() - (merSize - 1)); ++contigMerIndex)
    {
        contigHash.insert(contig.substr(contigMerIndex,merSize));
    }
    // Mask the reference (and keep track of excluded regions for coordinate translation later)
    static const int minExclusion(1000);
    static const int padding(50); // Amount of sequence included around each kmer hit.
    std::string maskedRef;
    const SymIter maxRef = refSeqEnd - (merSize - 1);
    SymIter potExclStart = refSeqStart;
    SymIter inclStart = refSeqStart;
    for (SymIter refIt = refSeqStart; refIt != maxRef; refIt++)
    {
        if (contigHash.count(std::string(refIt, refIt + merSize)) != 0)
        {
            if ((refIt - potExclStart) > (minExclusion + padding))
            {
                unsigned spacer(0);
                if (potExclStart > refSeqStart)
                {
                    maskedRef.append(std::string(inclStart, potExclStart));
                    maskedRef.append(nSpacer, 'N');
                    spacer = nSpacer;
                }
                inclStart = refIt - padding;
                exclBlocks.emplace_back(potExclStart - refSeqStart, inclStart - potExclStart, spacer);
            }
            potExclStart = refIt + padding;
        }
    }
    maskedRef.append(std::string(inclStart, std::min(maxRef, potExclStart)));
#ifdef DEBUG_KMER
    log_os << __FUNCTION__ << "Reduced to " << maskedRef << '\n';
    log_os << __FUNCTION__ << " exclBlocks\n\t";
    for (const auto block : exclBlocks)
        log_os << " " << block.start << ":" << block.length << ":" << block.nSpacer;
    log_os << "\n";
#endif
    if (maskedRef.empty()) maskedRef.append(nSpacer, 'N');
    return maskedRef;
}

}



/// convert jump alignment results into an SVCandidate
///
static
void
generateRefinedSVCandidateFromJumpAlignment(
    const BPOrientation& bporient,
    const SVCandidateAssemblyData& assemblyData,
    const unsigned contigIndex,
    SVCandidate& sv)
{
    const SVCandidateAssemblyData::JumpAlignmentResultType& align(assemblyData.spanningAlignments[contigIndex]);

    // first get each alignment associated with the correct breakend:
    const Alignment* bp1AlignPtr(&align.align1);
    const Alignment* bp2AlignPtr(&align.align2);

    if (bporient.isBp2AlignedFirst) std::swap(bp1AlignPtr, bp2AlignPtr);

    // summarize usable output information in a second SVBreakend
    // object -- this is the 'refined' sv:
    sv.assemblyAlignIndex = contigIndex;
    sv.assemblySegmentIndex = 0;

    sv.setPrecise();

    adjustAssembledBreakend(*bp1AlignPtr, (! bporient.isBp2AlignedFirst), align.jumpRange, assemblyData.bp1ref, bporient.isBp1Reversed, sv.bp1);
    adjustAssembledBreakend(*bp2AlignPtr, (bporient.isBp2AlignedFirst), align.jumpRange, assemblyData.bp2ref, bporient.isBp2Reversed, sv.bp2);
}



/// convert jump alignment results into an SVCandidate and add all
/// extra data required for VCF output
///
static
void
generateRefinedVCFSVCandidateFromJumpAlignment(
    const BPOrientation& bporient,
    const SVCandidateAssemblyData& assemblyData,
    const unsigned contigIndex,
    SVCandidate& sv)
{
    generateRefinedSVCandidateFromJumpAlignment(bporient, assemblyData, contigIndex, sv);

    const AssembledContig& contig(assemblyData.contigs[contigIndex]);
    const SVCandidateAssemblyData::JumpAlignmentResultType& align(assemblyData.spanningAlignments[contigIndex]);

    // fill in insertSeq:
    sv.insertSeq.clear();
    if (align.jumpInsertSize > 0)
    {
        getFwdStrandInsertSegment(align, contig.seq, bporient.isBp1Reversed, sv.insertSeq);
    }

    // add CIGAR for any simple (insert/delete) cases:
    addCigarToSpanningAlignment(sv);
}



void
SVCandidateAssemblyRefiner::
getJumpAssembly(
    const SVCandidate& sv,
    const bool isRNA,
    const bool isFindLargeInsertions,
    SVCandidateAssemblyData& assemblyData) const
{
#ifdef DEBUG_REFINER
    log_os << __FUNCTION__ << ": START\n";
    if (isRNA)
    {
        log_os << __FUNCTION__ << ": RNA\n";
    }
#endif

    // This determines by how much we extend the reference sequence
    // around the breakend region for all discovery and scoring
    // operations. It is possible to discover breakends and small
    // indels in this expanded region.
    //
    const pos_t extraRefEdgeSize(isRNA ? 25000 : 250);

    // This determines by how much we extend the reference sequence
    // around the breakend region for all operations except alignment
    // of the contig back to the reference.
    //
    // The primary motivation for this value is to improve our ability
    // to find reads which support the breakend when quality scoring
    // takes place (a subsequent step outside of this function), but
    // without expanding the regions where a breakend can possibly be
    // found (ie. without the risk of additional false positives)
    //
    // The extra reference sequence is used for:
    // - Extraction of breakend associated reads (reference sequence
    // used to find reads which poorly match the reference in this
    // case)
    // - Contig extension after alignment, this means that the
    // extended region will be used for read support scoring (later
    // during Q-value generation)
    //
    // The extra reference sequence is removed for:
    // - contig alignment, this means that actual breakpoint discovery
    // - will not occur in the extended region
    //
    const pos_t extraRefSplitSize(100);

    const pos_t extraRefSize(extraRefEdgeSize+extraRefSplitSize);

    // if the breakends have a simple insert/delete orientation and
    // the alignment regions overlap, then handle this case as a local
    // assembly problem:
    if (sv.bp1.interval.tid == sv.bp2.interval.tid)
    {
        if (! SVBreakendState::isSameOrientation(sv.bp1.state,sv.bp2.state))
        {
            const SV_TYPE::index_t svType(getSVType(sv));
            if ((svType == SV_TYPE::INDEL) || (svType == SV_TYPE::COMPLEX))
            {
                if ( isRefRegionOverlap( _header, extraRefSize, sv) )
                {
                    // transform SV into a single region format:
                    SVCandidate singleSV = sv;
                    singleSV.bp1.state = SVBreakendState::COMPLEX;
                    singleSV.bp2.state = SVBreakendState::UNKNOWN;
                    singleSV.bp1.interval.range.merge_range(sv.bp2.interval.range);

#ifdef DEBUG_REFINER
                    log_os << __FUNCTION__ << ": Candidate breakends regions are too close, transferring problem to local assembler\n";
#endif

                    getSmallSVAssembly(singleSV, isFindLargeInsertions, assemblyData);
                    return;
                }
            }
        }
    }

    assemblyData.isSpanning = true;
    BPOrientation& bporient(assemblyData.bporient);

    bporient.isBp1First = sv.isForward();
    bporient.isStranded = sv.isStranded();
    if (_opt.isRNA)
    {
        bporient.isBp1First = !sv.isForward(); // RNA-seq reads generate candidates in the opposite direction of the RNA
    }
    //
    // based on sv candidate, we classify the expected relationship
    // between the contig and the sv breakends:
    //
    if (sv.bp1.state != sv.bp2.state)
    {
        // if there's one right-open breakend and one left-open breakend, no matter the bp1/bp2 chromosome and
        // relative bp1/bp2 order etc. we:
        // 1. don't need to do any read/reference reversals
        // 2. always treat the right-open breakend as the first alignment region in order:
        //
        if (sv.bp2.state == SVBreakendState::RIGHT_OPEN)
        {
            bporient.isBp2AlignedFirst = true;
        }
    }
    else
    {
        // If both breakends open in the same direction, then:
        // 1. the reads from one breakend need to be reversed
        // 2. the reference from that same breakend needs to be reversed
        // 3. Treat the un-reversed RIGHT_OPEN or reversed LEFT_OPEN as the first alignment region in order
        //      Note that in the scheme below, we chose which bp to reverse so that no-reordering is required
        //
        if (sv.bp1.state == SVBreakendState::RIGHT_OPEN)
        {
            bporient.isBp2Reversed = true;
        }
        else
        {
            bporient.isBp1Reversed = true;
        }
    }

    // there's always a small chance that our region could fall
    // completely off the edge of the reference b/c of circular
    // chromosomes, this can't be treated as a bug -- it's a
    // legitimate breakend hypothesis that we just aren't setup to
    // handle correctly, so we punt this case:
    if (! isRefRegionValid(_header, sv.bp1.interval)) return;
    if (! isRefRegionValid(_header, sv.bp2.interval)) return;

    // next we extract the reference sequence around both breakends
    //
    // the 'trim' values below refer to the difference between the
    // breakend reference region requested and the region returned
    // after accounting for chromosome edges. The trim values will
    // almost always be zero for large chromosomes.
    //
    unsigned bp1LeadingTrim;
    unsigned bp1TrailingTrim;
    unsigned bp2LeadingTrim;
    unsigned bp2TrailingTrim;
    getSVReferenceSegments(
        _opt.referenceFilename, _header, extraRefSize, sv,
        assemblyData.bp1ref, assemblyData.bp2ref,
        bp1LeadingTrim, bp1TrailingTrim, bp2LeadingTrim, bp2TrailingTrim);

    // the 'cut' values below represent sequence which will be removed
    // from the edges of the reference region for each breakend.
    pos_t align1LeadingCut(std::max(0,extraRefSplitSize - static_cast<pos_t>(bp1LeadingTrim)));
    pos_t align1TrailingCut(std::max(0,extraRefSplitSize - static_cast<pos_t>(bp1TrailingTrim)));
    pos_t align2LeadingCut(std::max(0,extraRefSplitSize - static_cast<pos_t>(bp2LeadingTrim)));
    pos_t align2TrailingCut(std::max(0,extraRefSplitSize - static_cast<pos_t>(bp2TrailingTrim)));

    // assemble contig(s) spanning the breakend:
    _spanningAssembler.assembleSVBreakends(
        sv.bp1, sv.bp2,
        bporient.isBp1Reversed, bporient.isBp2Reversed,
        assemblyData.bp1ref, assemblyData.bp2ref,
        assemblyData.contigs);

    std::string bp1refSeq = assemblyData.bp1ref.seq();
    std::string bp2refSeq = assemblyData.bp2ref.seq();
    if (bporient.isBp1Reversed)
    {
        reverseCompStr(bp1refSeq);
        std::swap(align1LeadingCut, align1TrailingCut);
    }
    if (bporient.isBp2Reversed)
    {
        reverseCompStr(bp2refSeq);
        std::swap(align2LeadingCut, align2TrailingCut);
    }
    const std::string* align1RefStrPtr(&bp1refSeq);
    const std::string* align2RefStrPtr(&bp2refSeq);

    if (bporient.isBp2AlignedFirst)
    {
        std::swap(align1RefStrPtr, align2RefStrPtr);

        std::swap(align1LeadingCut, align2LeadingCut);
        std::swap(align1TrailingCut, align2TrailingCut);
    }

#ifdef DEBUG_REFINER
    log_os << __FUNCTION__ << ": al1RefSize/Seq: " << align1RefStrPtr->size() << '\n';
    printSeq(*align1RefStrPtr,log_os);
    log_os << '\n';
    log_os << __FUNCTION__ << ": al2Refsize/Seq: " << align2RefStrPtr->size() << '\n';
    printSeq(*align2RefStrPtr,log_os);
    log_os << '\n';
#endif

    const unsigned contigCount(assemblyData.contigs.size());

#ifdef DEBUG_REFINER
    log_os << __FUNCTION__ << ": contigCount: " << contigCount << "\n";
    for (unsigned contigIndex(0); contigIndex<contigCount; ++contigIndex)
    {
        const AssembledContig& contig(assemblyData.contigs[contigIndex]);
        log_os << __FUNCTION__ << ": contigIndex: " << contigIndex << " contig: " << contig;
    }
#endif

    // make sure an alignment object exists for every contig, even if
    // it's empty:
    assemblyData.spanningAlignments.resize(contigCount);

    bool isHighScore(false);
    unsigned highScoreIndex(0);

    for (unsigned contigIndex(0); contigIndex<contigCount; ++contigIndex)
    {
        const AssembledContig& contig(assemblyData.contigs[contigIndex]);

#ifdef DEBUG_REFINER
        log_os << __FUNCTION__ << ": start aligning contigIndex: " << contigIndex << "\n";
#endif

        JumpAlignmentResult<int>& alignment(assemblyData.spanningAlignments[contigIndex]);

        if (_opt.isRNA)
        {
#ifdef DEBUG_REFINER
            log_os << __FUNCTION__ << " RNA alignment\n";
#endif
            static const int nSpacer(25);
            std::vector<exclusion_block> exclBlocks1;
            const std::string cutRef1 = kmerMaskReference(align1RefStrPtr->begin() + align1LeadingCut,
                                                          align1RefStrPtr->end() - align1TrailingCut,
                                                          contig.seq, nSpacer, exclBlocks1);
            std::vector<exclusion_block> exclBlocks2;
            const std::string cutRef2 = kmerMaskReference(align2RefStrPtr->begin() + align2LeadingCut,
                                                          align2RefStrPtr->end() - align2TrailingCut,
                                                          contig.seq, nSpacer, exclBlocks2);
#ifdef DEBUG_REFINER
            log_os << __FUNCTION__ << " Kmer-masked references\n";
            log_os << "\t ref Lengths " << align1RefStrPtr->size() << " " << align2RefStrPtr->size() << "\n";
            log_os << "\t cutref Lengths " << cutRef1.size() << " " << cutRef2.size() << "\n";
#endif

            bool bp1RnaStrandFw; // Is the RNA fusion transcript on the forward strand at bp1
            bool bp2RnaStrandFw;
            if (bporient.isBp1First)
            {
                bp1RnaStrandFw = (sv.bp1.state == SVBreakendState::RIGHT_OPEN);
                bp2RnaStrandFw = (sv.bp2.state == SVBreakendState::LEFT_OPEN);
            }
            else
            {
                bp1RnaStrandFw = (sv.bp1.state == SVBreakendState::LEFT_OPEN);
                bp2RnaStrandFw = (sv.bp2.state == SVBreakendState::RIGHT_OPEN);
            }
            bool bp1Fw = (bporient.isBp1Reversed != bp1RnaStrandFw); //Should we look for the splice motif on the fw or rev strand in the bp1 ref seq
            bool bp2Fw = (bporient.isBp2Reversed != bp2RnaStrandFw);
            if (bporient.isBp2AlignedFirst) //bp1 and bp2 sequences have been swapped above
                std::swap(bp1Fw, bp2Fw);


#ifdef DEBUG_REFINER
            log_os << __FUNCTION__ << " isStranded: " << bporient.isStranded << "; bp1Fw: " << bp1Fw << " ; bp2Fw: " << bp2Fw << '\n';
#endif
            _RNASpanningAligner.align(contig.seq.begin(), contig.seq.end(),
                                      cutRef1.begin(), cutRef1.end(), cutRef2.begin(), cutRef2.end(),
                                      bp1Fw, bp2Fw, bporient.isStranded,
                                      alignment);

#ifdef DEBUG_REFINER
            log_os << __FUNCTION__ << " Masked 1: " << alignment.align1 << '\n';
            log_os << __FUNCTION__ << " Masked 2: " << alignment.align2 << '\n';
#endif
            if (!(translateMaskedAlignment(alignment.align1, exclBlocks1) &&
                  translateMaskedAlignment(alignment.align2, exclBlocks2)))
            {
#ifdef DEBUG_REFINER
                log_os << __FUNCTION__ << " Failed to fix kmer-masked alignment\n";
#endif
                alignment.align1.clear();
                alignment.align2.clear();
            }
#ifdef DEBUG_REFINER
            log_os << __FUNCTION__ << " Fixed 1: " << alignment.align1 << '\n';
            log_os << __FUNCTION__ << " Fixed 2: " << alignment.align2 << '\n';
#endif
        }
        else
        {
            _spanningAligner.align(contig.seq.begin(), contig.seq.end(),
                                   align1RefStrPtr->begin() + align1LeadingCut, align1RefStrPtr->end() - align1TrailingCut,
                                   align2RefStrPtr->begin() + align2LeadingCut, align2RefStrPtr->end() - align2TrailingCut,
                                   alignment);
        }

        alignment.align1.beginPos += align1LeadingCut;
        alignment.align2.beginPos += align2LeadingCut;

        std::string extendedContig;
        getExtendedContig(alignment, contig.seq, *align1RefStrPtr, *align2RefStrPtr, extendedContig);
        assemblyData.extendedContigs.push_back(extendedContig);

#ifdef DEBUG_REFINER
        log_os << __FUNCTION__ << ": contigIndex: " << contigIndex << " alignment: " << alignment;

        std::string bp1Seq,bp2Seq,insertSeq;
        getFwdStrandQuerySegments(alignment, contig.seq,
                                  bporient.isBp2AlignedFirst, bporient.isBp1Reversed, bporient.isBp2Reversed,
                                  bp1Seq, bp2Seq, insertSeq);
        log_os << __FUNCTION__ << "\tbp1seq_fwd: " << bp1Seq << "\n";
        log_os << __FUNCTION__ << "\tinsseq_fwd: " << insertSeq << "\n";
        log_os << __FUNCTION__ << "\tbp2seq_fwd: " << bp2Seq << "\n";
#endif

        // QC the alignment to make sure it spans the two breakend locations:
        static const unsigned minAlignRefSpan(20);
        const bool isAlignment1Good(alignment.align1.isAligned() && (apath_ref_length(alignment.align1.apath) >= minAlignRefSpan));
        const bool isAlignment2Good(alignment.align2.isAligned() && (apath_ref_length(alignment.align2.apath) >= minAlignRefSpan));
        const bool isAlignmentGood(isAlignment1Good && isAlignment2Good);
#ifdef DEBUG_REFINER
        log_os << __FUNCTION__ << ": Checking contig aln: " << contigIndex << "\n";
#endif
        if (! isAlignmentGood) continue;
#ifdef DEBUG_REFINER
        log_os << __FUNCTION__ << ": contig okay: " << contigIndex << "\n";
#endif
        if ((! isHighScore) || (alignment.score > assemblyData.spanningAlignments[highScoreIndex].score))
        {
            isHighScore = true;
            highScoreIndex=contigIndex;
        }
    }

    if (! isHighScore) return;
#ifdef DEBUG_REFINER
    log_os << __FUNCTION__ << ": high scoring contig: " << highScoreIndex << "\n";
#endif

    // set any additional QC steps before deciding an alignment is
    // usable:

    {
        // check the min size and fraction of optimal alignment score
        // for each of the two sub-alignments on each side of the bp
        //
        // note this is done for multiple values -- the lower value is
        // motivated by cases where a second breakpoint exists near to
        // the target breakpoint -- the higher value is motivated by
        // cases with some alignment 'messiness' near the breakpoint
        // that stabilizes as we move farther away
        //
        /// TODO change iterative refspan to a single consistent alignment criteria
        /// TODO should this be moved into the candidate selection loop?
        //
        const SVCandidateAssemblyData::JumpAlignmentResultType& hsAlign(assemblyData.spanningAlignments[highScoreIndex]);

        bool isFilterAlign1(true);
        bool isFilterAlign2(true);
        static const unsigned spanSet[] = {75, 100, 200};
        for (const unsigned maxQCRefSpan : spanSet)
        {
            if (! isFilterSpanningAlignment( maxQCRefSpan, _spanningAligner, true, isRNA, hsAlign.align1.apath))
            {
                isFilterAlign1 = false;
            }
            if (! isFilterSpanningAlignment( maxQCRefSpan, _spanningAligner, false, isRNA, hsAlign.align2.apath))
            {
                isFilterAlign2 = false;
            }
        }
        if (isFilterAlign1 || isFilterAlign2) return;
    }


    // ok, passed QC -- mark the high-scoring alignment as usable for
    // hypothesis refinement:
    {
        assemblyData.bestAlignmentIndex = highScoreIndex;
#ifdef DEBUG_REFINER
        log_os << __FUNCTION__ << ": highscoreid: " << highScoreIndex << " alignment: " << assemblyData.spanningAlignments[highScoreIndex];
#endif

        // process the alignment into information that's easily usable
        // in the vcf output (ie. breakends in reference coordinates)

        // summarize usable output information in a second SVBreakend
        // object -- this is the 'refined' sv:
        assemblyData.svs.push_back(sv);
        SVCandidate& newSV(assemblyData.svs.back());

        generateRefinedVCFSVCandidateFromJumpAlignment(bporient, assemblyData, highScoreIndex, newSV);

#ifdef DEBUG_REFINER
        log_os << __FUNCTION__ << ": highscore refined sv: " << newSV;
#endif
    }
}



void
SVCandidateAssemblyRefiner::
getSmallSVAssembly(
    const SVCandidate& sv,
    const bool isFindLargeInsertions,
    SVCandidateAssemblyData& assemblyData) const
{
#ifdef DEBUG_REFINER
    log_os << __FUNCTION__ << ": START\n";
#endif

    assemblyData.isSpanning = false;

    if (assemblyData.isCandidateSpanning)
    {
        _spanToComplexAssmRegions.addInterval(sv.bp1.interval);
    }
    else
    {
        // check if we've already assembled this region?
        if (_spanToComplexAssmRegions.isSubsetOfRegion(sv.bp1.interval))
        {
            assemblyData.isOverlapSkip = true;
            return;
        }
    }

    // how much additional reference sequence should we extract from
    // around each side of the breakend region?
    //
    // see extended description in getJumpAssembly
    static const pos_t extraRefEdgeSize(700);

    // how much reference should we additionally extract for split
    // read alignment, but not for variant-discovery alignment?
    //
    // see extended description in getJumpAssembly
    static const pos_t extraRefSplitSize(100);

    static const pos_t extraRefSize(extraRefEdgeSize+extraRefSplitSize);

#ifdef ITERATIVE_ASSEMBLER
    static const float extraVarSizePerc(0.1f);
    static const float extraSuppReadPerc(0.2f);
#endif

    // min alignment context
    //const unsigned minAlignContext(4);

    // there's always a small chance that our region could fall
    // completely off the edge of the reference.  b/c of circular
    // genomes, this can't be treated as a bug -- it's a legitimate
    // breakend hypothesis that we just aren't setup to handle
    // correctly:
    if (! isRefRegionValid(_header, sv.bp1.interval)) return;

    unsigned leadingTrim;
    unsigned trailingTrim;
    getIntervalReferenceSegment(_opt.referenceFilename, _header, extraRefSize, sv.bp1.interval, assemblyData.bp1ref, leadingTrim, trailingTrim);

    // in most cases, these values should equal extraRefSplitSize,
    // sometimes they're forced to be shorter b/c we didn't retrieve as much reference sequence as targeted:
    const pos_t maxLeadingCut(std::max(0, extraRefSize - static_cast<pos_t>(leadingTrim)));
    const pos_t maxTrailingCut(std::max(0, extraRefSize - static_cast<pos_t>(trailingTrim)));
    const pos_t leadingCut(std::max(0, maxLeadingCut - extraRefEdgeSize));
    const pos_t trailingCut(std::max(0, maxTrailingCut - extraRefEdgeSize));

    const std::string& align1RefStr(assemblyData.bp1ref.seq());

    const bool isSearchRemoteInsertionReads((! _opt.isSkipRemoteReads) && isFindLargeInsertions);

    // assemble contigs in the breakend region
    _smallSVAssembler.assembleSingleSVBreakend(sv.bp1, assemblyData.bp1ref, isSearchRemoteInsertionReads, assemblyData.remoteReads, assemblyData.contigs);

#ifdef DEBUG_REFINER
    log_os << __FUNCTION__ << ": align1RefSize/Seq: " << align1RefStr.size() << '\n';
    printSeq(align1RefStr,log_os);
    log_os << '\n';
#endif

    const unsigned contigCount(assemblyData.contigs.size());

#ifdef DEBUG_REFINER
    log_os << __FUNCTION__ << ": contigCount: " << contigCount << '\n';
    for (unsigned contigIndex(0); contigIndex<contigCount; ++contigIndex)
    {
        const AssembledContig& contig(assemblyData.contigs[contigIndex]);
        log_os << __FUNCTION__ << ": contigIndex: " << contigIndex << " contig: " << contig;
    }
#endif

    // make sure an alignment object exists for every contig, even if
    // it's empty:
    assemblyData.smallSVAlignments.resize(contigCount);
    assemblyData.smallSVSegments.resize(contigCount);
    assemblyData.largeInsertInfo.resize(contigCount);
    assemblyData.extendedContigs.resize(contigCount);

    bool isHighScore(false);
    unsigned highScoreIndex(0);

#ifdef ITERATIVE_ASSEMBLER
    bool isSecHighScore(false);
    unsigned highScoreVarSize(0);
    unsigned secHighScoreIndex(0);
    unsigned secHighScoreVarSize(0);
#endif

    std::vector<unsigned> largeInsertionCandidateIndex;

    for (unsigned contigIndex(0); contigIndex<contigCount; ++contigIndex)
    {
        const AssembledContig& contig(assemblyData.contigs[contigIndex]);

#ifdef DEBUG_REFINER
        log_os << __FUNCTION__ << ": start aligning contigIndex: " << contigIndex << '\n';
#endif


        // try out two different aligners, one optimized to find large
        // events and one optimized to find smaller events, only look
        // for small events if the large event search fails:
        SVCandidateAssemblyData::SmallAlignmentResultType& alignment(assemblyData.smallSVAlignments[contigIndex]);
        std::string& extendedContig(assemblyData.extendedContigs[contigIndex]);
        std::vector<std::pair<unsigned,unsigned> >& candidateSegments(assemblyData.smallSVSegments[contigIndex]);
        candidateSegments.clear();

        // remove candidate from consideration unless we find a
        // sufficiently large indel with good flanking sequence:
        bool isSmallSVCandidate(false);

        // come up with a more intelligent reference span limit:
        pos_t adjustedLeadingCut(leadingCut);
        pos_t adjustedTrailingCut(trailingCut);
        {
            // for all kmers in the contig, find the earliest and
            // latest kmer match in the reference, limit reference
            // to these points (but not less than the original breakend region)
            //
            // pick low k because this is just a simple runtime optimization
            static const int merSize(10);
            std::unordered_set<std::string> contigHash;
            const unsigned contigSize(contig.seq.size());
            for (unsigned contigMerIndex(0); contigMerIndex<(contigSize-(merSize-1)); ++contigMerIndex)
            {
                contigHash.insert(contig.seq.substr(contigMerIndex,merSize));
            }

            const pos_t refSize(align1RefStr.size());
            const pos_t minRefIndex(leadingCut);
            const pos_t maxRefIndex(refSize-(trailingCut+merSize));

            const pos_t maxFwdRefIndex(std::min(maxLeadingCut, maxRefIndex));
            pos_t refIndex=minRefIndex;
            for (refIndex=minRefIndex; refIndex<=maxFwdRefIndex; refIndex++)
            {
                if (contigHash.count(align1RefStr.substr(refIndex,merSize)) != 0) break;
            }
            adjustedLeadingCut=refIndex;

            const pos_t minRevRefIndex(std::max(minRefIndex,refSize-maxTrailingCut));
            for (refIndex=(maxRefIndex); refIndex>=minRevRefIndex; refIndex--)
            {
                if (contigHash.count(align1RefStr.substr(refIndex,merSize)) != 0) break;
            }
            adjustedTrailingCut=(refSize-(refIndex+merSize));
        }

        // start with largeSV aligner
        {
            _largeSVAligner.align(
                contig.seq.begin(), contig.seq.end(),
                align1RefStr.begin() + adjustedLeadingCut, align1RefStr.end() - adjustedTrailingCut,
                alignment);

            alignment.align.beginPos += adjustedLeadingCut;
            getExtendedContig(alignment, contig.seq, align1RefStr, extendedContig);


            // trial two different flanking test sizes, this way we
            // account for multiple neighboring noise scenarios
            //
            static const unsigned spanSet[] = {100,200};
            for (const unsigned maxQCRefSpan : spanSet)
            {
                static const bool isLargeOnly(true);

                std::vector<std::pair<unsigned,unsigned> > segments;
                const bool isCandidate( isSmallAssemblerSVAlignment(
                                            maxQCRefSpan,
                                            _largeSVAligner,
                                            alignment.align,
                                            contig.seq,
                                            align1RefStr,
                                            _opt.scanOpt.minCandidateVariantSize,
                                            segments,
                                            isLargeOnly) );

                if (isCandidate)
                {
                    // in case both ref spans are accepted take the one with the larger segment count:
                    if (segments.size() > candidateSegments.size())
                    {
                        candidateSegments = segments;
                    }
                    isSmallSVCandidate=true;
                }
            }

#ifdef DEBUG_REFINER
            log_os << __FUNCTION__ << ": finished largeAligner. contigIndex: " << contigIndex
                   << " isSmallSVCandidate " << isSmallSVCandidate
                   << " alignment: " << alignment;
#endif
        }


        // didn't find anything? try again focused on smaller events:
        /// TODO: get rid of this step
        if (! isSmallSVCandidate)
        {
            _smallSVAligner.align(
                contig.seq.begin(), contig.seq.end(),
                align1RefStr.begin() + adjustedLeadingCut, align1RefStr.end() - adjustedTrailingCut,
                alignment);

            alignment.align.beginPos += adjustedLeadingCut;
            getExtendedContig(alignment, contig.seq, align1RefStr, extendedContig);

            // trial two different flanking test sizes, this way we
            // account for multiple neighboring noise scenarios
            //
            static const unsigned spanSet[] = {100,200};
            for (const unsigned maxQCRefSpan : spanSet)
            {
                std::vector<std::pair<unsigned,unsigned> > segments;
                const bool isCandidate( isSmallAssemblerSVAlignment(
                                            maxQCRefSpan,
                                            _smallSVAligner,
                                            alignment.align,
                                            contig.seq,
                                            align1RefStr,
                                            _opt.scanOpt.minCandidateVariantSize,
                                            segments) );

                if (isCandidate)
                {
                    // in case both ref spans are accepted take the
                    // one with the larger segment count:
                    if (segments.size() > candidateSegments.size())
                    {
                        candidateSegments = segments;
                    }
                    isSmallSVCandidate=true;
                }
            }

#ifdef DEBUG_REFINER
            log_os << __FUNCTION__ << ": finished smallAligner. contigIndex: " << contigIndex
                   << " isSmallSVCandidate " << isSmallSVCandidate
                   << " alignment: " << alignment;
#endif
        }


        // test each alignment for suitability to be the left or right
        // side of a large insertion:
        //
        // all practical combinations of left and right candidates
        // will be enumerated below to see if there's a good fit:
        //
        if (isFindLargeInsertions)
        {
            LargeInsertionInfo& candidateInsertInfo(assemblyData.largeInsertInfo[contigIndex]);
            candidateInsertInfo.clear();

            LargeInsertionInfo insertInfo;
#ifdef DEBUG_CONTIG
            log_os << __FUNCTION__ << ": contig length: " << contig.seq.size() << "\n"
                   << __FUNCTION__ << ": contig seq: " << contig.seq << "\n"
                   << __FUNCTION__ << ": trim contig start offset: " << contig.conservativeRange.begin_pos() << "\n"
                   << __FUNCTION__ << ": trim contig end offset: " << contig.conservativeRange.end_pos() << "\n";
#endif
            ALIGNPATH::path_t apath_conservative(alignment.align.apath);
            apath_limit_read_length(contig.conservativeRange,apath_conservative);

            bool isCandidate( isLargeInsertAlignment(
                                  _largeInsertEdgeAligner,
                                  apath_conservative,
                                  insertInfo));

            if (isCandidate)
            {
                // if passed, then get corrected insertInfo without
                // using conservativeRange:
                LargeInsertionInfo insertInfo2;
                isCandidate = isLargeInsertAlignment(
                                  _largeInsertEdgeAligner,
                                  alignment.align.apath,
                                  insertInfo2);

                if ((insertInfo.isLeftCandidate != insertInfo2.isLeftCandidate)
                    || (insertInfo.isRightCandidate != insertInfo2.isRightCandidate))
                {
                    isCandidate = false;
                }

                insertInfo.contigOffset = insertInfo2.contigOffset;
                insertInfo.refOffset = insertInfo2.refOffset;
            }

            if (isCandidate)
            {
                candidateInsertInfo=insertInfo;
#ifdef DEBUG_REFINER
                log_os << __FUNCTION__ << ": inserting large insertion candidation: " << candidateInsertInfo << "\n";
#endif
                largeInsertionCandidateIndex.push_back(contigIndex);
            }
        }

        if (isSmallSVCandidate)
        {
            // keep the top two highest scoring QC'd candidate:
            // TODO: we should keep all QC'd candidates for the small event case
            // FIXME : prevents us from finding overlapping events, keep vector of high-scoring contigs?
            if (! isHighScore)
            {
#ifdef DEBUG_REFINER
                log_os << __FUNCTION__ << ": contigIndex: " << contigIndex << " is high score\n";
#endif
                isHighScore = true;
                highScoreIndex=contigIndex;
#ifdef ITERATIVE_ASSEMBLER
                highScoreVarSize = getLargestIndelSize(alignment.align.apath, candidateSegments);
#endif
            }
            else if (alignment.score > assemblyData.smallSVAlignments[highScoreIndex].score)
            {
                highScoreIndex = contigIndex;

#ifdef DEBUG_REFINER
                log_os << __FUNCTION__ << ": contigIndex: " << highScoreIndex << " is high score\n";
#endif

#ifdef ITERATIVE_ASSEMBLER
                isSecHighScore = true;
                secHighScoreIndex = highScoreIndex;
                secHighScoreVarSize = highScoreVarSize;
                highScoreVarSize = getLargestIndelSize(alignment.align.apath, candidateSegments);

#ifdef DEBUG_REFINER
                log_os << __FUNCTION__ << ": contigIndex: " << secHighScoreIndex << " is the second high score\n";
#endif
#endif
            }
#ifdef ITERATIVE_ASSEMBLER
            else if ((! isSecHighScore) || (alignment.score > assemblyData.smallSVAlignments[secHighScoreIndex].score))
            {
                isSecHighScore = true;
                secHighScoreIndex = contigIndex;
                secHighScoreVarSize = getLargestIndelSize(alignment.align.apath, candidateSegments);
#ifdef DEBUG_REFINER
                log_os << __FUNCTION__ << ": contigIndex: " << secHighScoreIndex << " is the second high score\n";
#endif
            }
#endif
        }
    }

#ifdef ITERATIVE_ASSEMBLER
    // select the contig with the larger indel size between the two
    // highest-scoring contigs
    if (isSecHighScore)
    {
        const unsigned highScoreSuppReads = assemblyData.contigs[highScoreIndex].supportReads.size();
        const unsigned secHighScoreSuppReads = assemblyData.contigs[secHighScoreIndex].supportReads.size();
#ifdef DEBUG_REFINER
        log_os << __FUNCTION__ << ": contig #" << highScoreIndex << "has " << highScoreSuppReads
               <<" support reads, with max variant size " << highScoreVarSize;
        log_os << __FUNCTION__ << ": contig #" << secHighScoreIndex << "has " << secHighScoreSuppReads
               <<" support reads, with max variant size " << secHighScoreVarSize;
#endif

        const bool secondIsBest((secHighScoreSuppReads > highScoreSuppReads *(1+extraSuppReadPerc)) ||
                                (secHighScoreVarSize > highScoreVarSize * (1+extraVarSizePerc)));
        if (secondIsBest) highScoreIndex = secHighScoreIndex;
#ifdef DEBUG_REFINER
        log_os << __FUNCTION__ << ": contigIndex: " << highScoreIndex << " is finally selected.\n";
#endif
    }
#endif


    // set any additional QC steps before deciding an alignment is usable:
    // TODO:
    std::set<pos_t> insPos;

    // finished QC, skip small deletions if no candidates have appeared:
    if (isHighScore)
    {
        assemblyData.bestAlignmentIndex = highScoreIndex;
#ifdef DEBUG_REFINER
        log_os << __FUNCTION__ << ": highscoreid: " << highScoreIndex << " alignment: " << assemblyData.smallSVAlignments[highScoreIndex];
#endif

        // process the alignment into information that's easily usable
        // in the vcf output (ie. breakends in reference coordinates)

        const AssembledContig& bestContig(assemblyData.contigs[assemblyData.bestAlignmentIndex]);
        const SVCandidateAssemblyData::SmallAlignmentResultType& bestAlign(assemblyData.smallSVAlignments[assemblyData.bestAlignmentIndex]);

        const SVCandidateAssemblyData::CandidateSegmentSetType& candidateSegments(assemblyData.smallSVSegments[assemblyData.bestAlignmentIndex]);
        unsigned segmentIndex = 0;
        for (const SVCandidateAssemblyData::CandidateSegmentType& segRange : candidateSegments)
        {
            // copy the low-res candidate sv and start customizing:
            assemblyData.svs.push_back(sv);

            SVCandidate& newSV(assemblyData.svs.back());
            newSV.assemblyAlignIndex = assemblyData.bestAlignmentIndex;
            newSV.assemblySegmentIndex = segmentIndex;
            setSmallCandSV(assemblyData.bp1ref, bestContig.seq, bestAlign.align, segRange, newSV);
            segmentIndex++;

            // provide a weak filter to keep fully and partially
            // assembled duplicates of the same event from occurring:
            if (getExtendedSVType(newSV) == EXTENDED_SV_TYPE::INSERT)
            {
                insPos.insert(newSV.bp1.interval.range.begin_pos());
            }

#ifdef DEBUG_REFINER
            log_os << __FUNCTION__ << ": small refined sv: " << newSV;
#endif

#ifdef DEBUG_CONTIG
            const int contigSize = bestContig.seq.length();
            const ALIGNPATH::path_t apathTillSvStart(&bestAlign.align.apath[0], &bestAlign.align.apath[segRange.first]);
            const ALIGNPATH::path_t apathTillSvEnd(&bestAlign.align.apath[0], &bestAlign.align.apath[segRange.second+1]);
            const int leftSize = apath_read_length(apathTillSvStart);
            const int endPos = apath_read_length(apathTillSvEnd);
            const int rightSize = contigSize - apath_read_length(apathTillSvEnd);

            log_os << __FUNCTION__ << ": contig has size " << contigSize << ": " << bestContig.seq << "\n";
            log_os << __FUNCTION__ << ": left part has size " << leftSize << ": " << bestContig.seq.substr(0, leftSize) << "\n";
            log_os << __FUNCTION__ << ": right part has size " << rightSize << ": " << bestContig.seq.substr(endPos, rightSize) << "\n";
#endif
        }
    }

    // search for large deletions with incomplete insertion sequence
    // assembly:
    {
        // In case of no fully-assembled candidate, solve for any
        // strong large insertion candidate
        //
        // This is done by searching through combinations of the left
        // and right insertion side candidates found in the primary
        // contig processing loop
        if (isFindLargeInsertions)
        {
            processLargeInsertion(sv, leadingCut, trailingCut, _largeInsertCompleteAligner, largeInsertionCandidateIndex, insPos, assemblyData);
        }
    }
}
