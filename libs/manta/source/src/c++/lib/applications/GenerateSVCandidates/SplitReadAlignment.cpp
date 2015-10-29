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
/// \author Felix Schlesinger
///

#include "SplitReadAlignment.hh"
#include "blt_util/blt_types.hh"
#include "blt_util/log.hh"
#include "blt_util/seq_printer.hh"
#include "common/Exceptions.hh"
#include "htsapi/SimpleAlignment_bam_util.hh"

#include <cassert>
#include <cmath>

#include <iostream>


//#define DEBUG_SRA



std::ostream&
operator<<(std::ostream& os, const SRAlignmentInfo& info)
{
    os << "leftSize=" << info.leftSize << " homSize=" << info.homSize << " rightSize=" << info.rightSize
       << " leftMismatches=" << info.leftMismatches << " homMismatches=" << info.homMismatches << " rightMismatches=" << info.rightMismatches
       << " alignScore=" << info.alignScore
       << " isEvidence: " << info.isEvidence
       << " isT2Evidence: " << info.isTier2Evidence
       << " evidence: " << info.evidence
       << " alignLnLhood: "  << info.alignLnLhood
       << '\n';
    return os;
}



/// \params[out] return the LnLhood expected from a perfect match to the reference
static
float
getLnLhood(
    const std::string& querySeq,
    const qscore_snp& qualConvert,
    const uint8_t* queryQual,
    const std::string& targetSeq,
    const pos_t targetStartOffset,
    const known_pos_range2& scoreRange,
    const bool isBest,
    const float bestLnLhood)
{
    static const float ln_one_third(std::log(1/3.f));

    const unsigned querySize(querySeq.size());

    assert((targetStartOffset+querySize) <= targetSeq.size());

    float lnLhood(0);
    for (unsigned i(0); i<querySize; i++)
    {
        // put a lower-bound on quality values:
        const int baseQual(std::max(2,static_cast<int>(queryQual[i])));

        if ((targetStartOffset+static_cast<pos_t>(i)) > scoreRange.end_pos()) break;
        if ((targetStartOffset+static_cast<pos_t>(i)) <= scoreRange.begin_pos()) continue;

        const char targetBase(targetSeq[targetStartOffset+i]);

        if ((querySeq[i] != targetBase) || (querySeq[i] == 'N'))
        {
            if ((querySeq[i] == 'N') || (targetBase == 'N'))
            {
                static const float lnRandomBase(-std::log(4.f));
                lnLhood += lnRandomBase;
            }
            else
            {
                lnLhood += qualConvert.qphred_to_ln_error_prob(baseQual) + ln_one_third;
            }
        }
        else
        {
            lnLhood += qualConvert.qphred_to_ln_comp_error_prob(baseQual);
        }

        if (isBest && (lnLhood < bestLnLhood)) break;
    }

    return lnLhood;
}



static
void
calculateAlignScore(
    const std::string& querySeq,
    const std::string& targetSeq,
    const unsigned bestPos,
    SRAlignmentInfo& alignment)
{
    const unsigned querySize = querySeq.size();
    alignment.leftMismatches=0;
    alignment.homMismatches=0;
    alignment.rightMismatches=0;

    assert(bestPos+querySize <= targetSeq.size());

    for (unsigned i(0); i<querySize; i++)
    {
        if ((querySeq[i] != targetSeq[bestPos+i]) || (querySeq[i] == 'N'))
        {
            if (i<=alignment.leftSize)
            {
                alignment.leftMismatches += 1;
            }
            else if (i <= (alignment.leftSize+alignment.homSize))
            {
                alignment.homMismatches += 1;
            }
            else
            {
                alignment.rightMismatches += 1;
            }
        }
    }

    alignment.alignScore = querySize - (alignment.leftMismatches+ alignment.homMismatches+ alignment.rightMismatches);
}



static
bool
isEvidenceCheck(
    const SRAlignmentInfo& alignment,
    const unsigned minFlankSize)
{
    if (alignment.leftSize < minFlankSize) return false;
    if (alignment.rightSize < minFlankSize) return false;

    if ((alignment.leftMismatches/(float)alignment.leftSize) >= 0.25) return false;
    if ((alignment.rightMismatches/(float)alignment.rightSize) >= 0.25) return false;

    const float size(static_cast<float>(alignment.leftSize+alignment.rightSize));
    if ((alignment.alignScore/size) < 0.9) return false;

    return true;
}



static
void
setEvidence(
    SRAlignmentInfo& alignment)
{
    //
    // filters for a read being counted as evidence
    //

    // adding new flank size threshold -- this might have to be changed based on sv size:
    static const unsigned minFlankSize(16);
    static const unsigned minFlankSizeTier2(8);
    alignment.isEvidence = isEvidenceCheck(alignment,minFlankSize);
    alignment.isTier2Evidence = isEvidenceCheck(alignment,minFlankSizeTier2);

    alignment.evidence = 0;
    if (! (alignment.isEvidence || alignment.isTier2Evidence)) return;

    const float size(static_cast<float>(alignment.leftSize+alignment.rightSize));
    alignment.evidence = 2 * std::min(alignment.leftSize, alignment.rightSize) / (size);
}



void
getRefAlignment(
    const bam_record& bamRead,
    const reference_contig_segment& bp1ref,
    const known_pos_range2& bpPos,
    const qscore_snp& qualConvert,
    SRAlignmentInfo& alignment)
{
    using namespace ALIGNPATH;
    const SimpleAlignment align(getAlignment(bamRead));
    const std::string qrySeq(bamRead.get_bam_read().get_string());
    const int refLength(apath_ref_length(align.path));
    std::string bp1Ref;
    bp1ref.get_substring(align.pos, refLength, bp1Ref);
    const uint8_t* qual(bamRead.qual());
#ifdef DEBUG_SRA
    log_os << __FUNCTION__ << bamRead << '\n';
    log_os << "\t" << refLength << " " << qrySeq << '\n';
    log_os << "\t" << bp1Ref.substr(0,10) << '\n';
#endif

    auto queryIndex(qrySeq.begin());
    auto refIndex(bp1Ref.begin());
    for (const path_segment& seg : align.path)
    {
        if (is_segment_align_match(seg.type))
        {
            for (unsigned i=0; i < seg.length; i++)
            {
                int refPos(align.pos + refIndex - bp1Ref.begin());
                bool isSeqMatch(false);
                if ((*queryIndex == 'N') || (*refIndex == 'N'))
                {
                    static const float lnRandomBase(-std::log(4.f));
                    alignment.alignLnLhood += lnRandomBase;
                }
                else
                {
                    const int baseQual(std::max(2, static_cast<int>(qual[i])));
                    if ((*queryIndex) == (*refIndex))
                    {
                        isSeqMatch = true;
                        alignment.alignLnLhood += qualConvert.qphred_to_ln_comp_error_prob(baseQual);
                    }
                    else
                    {
                        static const float ln_one_third(std::log(1 / 3.f));
                        alignment.alignLnLhood += qualConvert.qphred_to_ln_error_prob(baseQual) + ln_one_third;
                    }
                }

                if (refPos <= bpPos.begin_pos())
                {
                    alignment.leftSize++;
                    if (!isSeqMatch) alignment.leftMismatches++;
                }
                if ((refPos > bpPos.begin_pos()) && (refPos < bpPos.end_pos()))
                {
                    alignment.homSize++;
                    if (!isSeqMatch) alignment.homMismatches++;
                }
                if (refPos >= bpPos.end_pos())
                {
                    alignment.rightSize++;
                    if (!isSeqMatch) alignment.rightMismatches++;
                }
                queryIndex++;
                refIndex++;
            }
        }
        else
        {
            if (is_segment_type_read_length(seg.type)) std::advance(queryIndex, seg.length);
            if (is_segment_type_ref_length(seg.type)) std::advance(refIndex, seg.length);
        }
    }
    alignment.alignPos = align.pos - bp1ref.get_offset();
    alignment.alignScore = apath_matched_length(align.path) - alignment.leftMismatches -
                           alignment.homMismatches - alignment.rightMismatches;
    setEvidence(alignment);
}



void
splitReadAligner(
    const unsigned flankScoreSize,
    const std::string& querySeq,
    const qscore_snp& qualConvert,
    const uint8_t* queryQual,
    const std::string& targetSeq,
    const known_pos_range2& targetBpOffsetRange,
    SRAlignmentInfo& alignment)
{
    using namespace illumina::common;

    const unsigned querySize = querySeq.size();
    const unsigned targetSize = targetSeq.size();
    if (querySize >= targetSize)
    {
        std::ostringstream oss;
        oss << "ERROR: Unexpected split read alignment input."
            << " querySize: " << querySize << " targetSize: " << targetSize << '\n'
            << "querySeq:\n";
        printSeq(querySeq,oss);
        oss << '\n'
            << "targetSeq:\n";
        printSeq(targetSeq,oss);
        oss << '\n';
        BOOST_THROW_EXCEPTION(LogicException(oss.str()));
    }

    // set the scanning start & end to make sure the candidate windows overlapping the breakpoint
    const unsigned scanStart(std::max(0, static_cast<pos_t>(targetBpOffsetRange.begin_pos()) - static_cast<pos_t>(querySize) + 2));
    const unsigned scanEnd(std::max(0, std::min((static_cast<pos_t>(targetBpOffsetRange.end_pos())), static_cast<pos_t>(targetSize - querySize))));

    const known_pos_range2 scoreRange(targetBpOffsetRange.begin_pos()-static_cast<pos_t>(flankScoreSize),
                                      targetBpOffsetRange.end_pos()+static_cast<pos_t>(flankScoreSize));

#ifdef DEBUG_SRA
    log_os << __FUNCTION__ << "query size = " << querySize << " target size = " << targetSize << '\n';
    log_os << __FUNCTION__ << "targetBeginPos = " << targetBpOffsetRange.begin_pos() << '\n';
    log_os << __FUNCTION__ << "scan start = " << scanStart << " scan end = " << scanEnd << '\n';
#endif
    if (scanEnd < scanStart)
    {
        std::ostringstream oss;
        oss << "ERROR: scanEnd < scanStart."
            << " scanEnd: " << scanEnd << " scanStart: " << scanStart
            << " querySize: " << querySize << " targetSize: " << targetSize << '\n'
            << "\ttargetRange: " << targetBpOffsetRange << '\n';
        BOOST_THROW_EXCEPTION(LogicException(oss.str()));
    }

    // do one high-speed pass to find the optimal alignment (in terms of lhood), then compute all the goodies later:
    float bestLnLhood(0);
    unsigned bestPos(0);
    {
        bool isBest(false);
        for (unsigned i = scanStart; i<= scanEnd; i++)
        {
            const float lnLhood(getLnLhood(querySeq, qualConvert, queryQual,
                                           targetSeq, i, scoreRange, isBest, bestLnLhood));

#ifdef DEBUG_SRA
            log_os << __FUNCTION__ << "scanning: " << i << " lhood: " << lnLhood << " bestLnLhood " << bestLnLhood << " isBest " << isBest << " bestPos " << bestPos << '\n';
#endif
            if ( (! isBest) || (lnLhood > bestLnLhood))
            {
                bestLnLhood = lnLhood;
                bestPos=i;
                isBest=true;
            }
        }
        assert(isBest);
    }

    assert(static_cast<pos_t>(bestPos) <= (targetBpOffsetRange.end_pos()+1));
    if (static_cast<pos_t>(bestPos) <= (targetBpOffsetRange.begin_pos()+1))
    {
        alignment.leftSize = static_cast<pos_t>(targetBpOffsetRange.begin_pos()+1) - bestPos;
    }
    else
    {
        alignment.leftSize = 0;
    }

    if (alignment.leftSize > querySize)
    {
        std::ostringstream oss;
        oss << "ERROR: Unexpected split read alignment outcome. "
            << " targetRange: " << targetBpOffsetRange << " bestPos: " << bestPos << " bestLnLhood: " << bestLnLhood << " querySize: " << querySize << " targetSize: " << targetSize << '\n'
            << "alignment: " << alignment << "\n"
            << "querySeq:\n";
        printSeq(querySeq,oss);
        oss << '\n'
            << "targetSeq:\n";
        printSeq(targetSeq,oss);
        oss << '\n';
        BOOST_THROW_EXCEPTION(LogicException(oss.str()));
    }
    alignment.homSize = std::min(querySize-alignment.leftSize,(static_cast<pos_t>(targetBpOffsetRange.end_pos()+1) - bestPos) - alignment.leftSize);

    if ((alignment.leftSize + alignment.homSize) < querySize)
    {
        alignment.rightSize = querySize - (alignment.leftSize + alignment.homSize);
    }
    else
    {
        alignment.rightSize = 0;
    }
    alignment.alignLnLhood = bestLnLhood;
    alignment.alignPos = bestPos;

    calculateAlignScore(querySeq, targetSeq, bestPos, alignment);

    // filtering the alignment and set evidence
    setEvidence(alignment);

#ifdef DEBUG_SRA
    log_os << __FUNCTION__ << "bestpos: " << bestPos << " final alignment\n" << alignment << "\n";
#endif
}
