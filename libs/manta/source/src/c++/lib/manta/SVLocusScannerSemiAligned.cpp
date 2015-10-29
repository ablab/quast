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
/// \author Ole Schulz-Trieglaff
/// \author Bret Barnes
/// \author Felix Schlesinger
///

#include "alignment/ReadScorer.hh"

#include "common/Exceptions.hh"
#include "manta/SVLocusScannerSemiAligned.hh"

#include "boost/foreach.hpp"


//#define DEBUG_SEMI_ALIGNED

#ifdef DEBUG_SEMI_ALIGNED
#include "blt_util/log.hh"
#include <iostream>
#endif



/// In the general case we want 'N''s to be counted as mismatches, but for
/// the semi-aligned metric, we don't want N's to nominate read segments as
/// SV-associated because of a string of Ns, so we treat these as matches
///
static
bool
isSemiAlignedBaseMatch(
    const char a,
    const char b)
{
    if ((a=='N') || (b=='N')) return true;
    return (a==b);
}



/// report the length from 0 to immediately before the indicated number of
/// contiguous matches
///
static
void
leadingEdgeMismatchLength(
    const SimpleAlignment& bamAlign,
    const bam_seq& querySeq,
    const reference_contig_segment& refSeq,
    const unsigned contiguousMatchCount,
    unsigned& leadingLength,
    pos_t& leadingRefPos)
{
    using namespace ALIGNPATH;

    assert(contiguousMatchCount != 0);

    pos_t readIndex(0);
    pos_t refIndex(bamAlign.pos);

    leadingLength=0;
    leadingRefPos=refIndex;

    unsigned matchLength(0);
    for (const path_segment& ps : bamAlign.path)
    {
        if (is_segment_align_match(ps.type))
        {
            for (unsigned segPos(0); segPos<ps.length; ++segPos)
            {
                if (isSemiAlignedBaseMatch(querySeq.get_char(readIndex+segPos), refSeq.get_base(refIndex+segPos)))
                {
                    matchLength++;

                    if (matchLength>=contiguousMatchCount)
                    {
                        leadingLength=(readIndex+segPos)-(matchLength-1);
                        leadingRefPos=(refIndex+segPos)-(matchLength-1);
                        return;
                    }
                }
                else
                {
                    matchLength=0;
                }
            }
        }
        else
        {
            matchLength=0;
        }

        if (is_segment_type_read_length(ps.type)) readIndex += ps.length;
        if (is_segment_type_ref_length(ps.type)) refIndex += ps.length;
    }

    leadingLength=readIndex;
    leadingRefPos=refIndex;
}



static
void
trailingEdgeMismatchLength(
    const SimpleAlignment& bamAlign,
    const bam_seq& querySeq,
    const reference_contig_segment& refSeq,
    const unsigned contiguousMatchCount,
    unsigned& trailingLength,
    pos_t& trailingRefPos)
{
    using namespace ALIGNPATH;

    assert(contiguousMatchCount != 0);

    const pos_t readSize(querySeq.size());

    pos_t readIndex(readSize-1);
    pos_t refIndex(bamAlign.pos + apath_ref_length(bamAlign.path)-1);

    unsigned matchLength(0);
    BOOST_REVERSE_FOREACH(const path_segment& ps, bamAlign.path)
    {
        if (is_segment_align_match(ps.type))
        {
            for (unsigned segPos(0); segPos<ps.length; ++segPos)
            {
                if (isSemiAlignedBaseMatch(querySeq.get_char(readIndex-segPos), refSeq.get_base(refIndex-segPos)))
                {
                    matchLength++;

                    if (matchLength>=contiguousMatchCount)
                    {
                        trailingLength=(readSize-(readIndex-segPos))-matchLength;
                        trailingRefPos=(refIndex-segPos)+matchLength;
                        return;
                    }
                }
                else
                {
                    matchLength=0;
                }
            }
        }
        else
        {
            matchLength=0;
        }

        if (is_segment_type_read_length(ps.type)) readIndex -= ps.length;
        if (is_segment_type_ref_length(ps.type)) refIndex -= ps.length;
    }

    trailingLength=readSize-(readIndex+1);
    trailingRefPos=refIndex+1;
}



/// report the length from 0 to immediately before the indicated number of
/// contiguous matches moving inwards from each end of the read
///
/// \param[out] leadingLength semi-aligned length in read coordinates from the start of the read
/// \param[out] leadingRefPos reference position of read start after removing semi-aligned portion from consideration
///
/// ...similar for trailing output params...
///
static
void
edgeMismatchLength(
    const SimpleAlignment& bamAlign,
    const bam_seq& querySeq,
    const reference_contig_segment& refSeq,
    const unsigned contiguousMatchCount,
    unsigned& leadingLength,
    pos_t& leadingRefPos,
    unsigned& trailingLength,
    pos_t& trailingRefPos)
{
    leadingEdgeMismatchLength(bamAlign,querySeq,refSeq,contiguousMatchCount,leadingLength,leadingRefPos);
    trailingEdgeMismatchLength(bamAlign,querySeq,refSeq,contiguousMatchCount,trailingLength,trailingRefPos);

    const unsigned readSize(querySeq.size());
    assert(leadingLength<=readSize);
    assert(trailingLength<=readSize);
}



void
getSVBreakendCandidateSemiAligned(
    const bam_record& bamRead,
    const SimpleAlignment& bamAlign,
    const reference_contig_segment& refSeq,
    const bool isUseOverlappingPairs,
    unsigned& leadingMismatchLen,
    pos_t& leadingRefPos,
    unsigned& trailingMismatchLen,
    pos_t& trailingRefPos,
    const uint8_t minQ,
    const float minQFrac)
{
    static const unsigned contiguousMatchCount(5);

    leadingMismatchLen = 0;
    leadingRefPos = 0;
    trailingMismatchLen = 0;
    trailingRefPos = 0;

    if (is_possible_adapter_pair(bamRead)) return;

    // create a new alignment with all soft-clip sections forced to match:
    const SimpleAlignment matchedAlignment(matchifyEdgeSoftClip(bamAlign));

    if (! isUseOverlappingPairs)
    {
        if (is_overlapping_pair(bamRead, matchedAlignment)) return;
    }

    using namespace ALIGNPATH;
    const bam_seq querySeq(bamRead.get_bam_read());

    const uint8_t* qual(bamRead.qual());
    const unsigned readSize(bamRead.read_size());

    unsigned leadingMismatchLenTmp(0);
    unsigned trailingMismatchLenTmp(0);
    edgeMismatchLength(matchedAlignment, querySeq, refSeq, contiguousMatchCount,
                       leadingMismatchLenTmp, leadingRefPos,
                       trailingMismatchLenTmp, trailingRefPos);

    if ((leadingMismatchLenTmp + trailingMismatchLenTmp) >= readSize) return;

    if (0 != leadingMismatchLenTmp)
    {
        if (bamRead.is_fwd_strand() || (!is_overlapping_pair(bamRead, matchedAlignment)))
        {
            unsigned minQCount(0);
            for (unsigned pos(0); pos<leadingMismatchLenTmp; ++pos)
            {
                if (qual[pos] >= minQ) ++minQCount;
            }
            if ((static_cast<float>(minQCount)/(leadingMismatchLenTmp)) >= minQFrac)
            {
                leadingMismatchLen = leadingMismatchLenTmp;
            }
        }
#ifdef DEBUG_SEMI_ALIGNED
        else
            log_os << " Overlapping_pair leading" << " read qname=" << bamRead.qname() << std::endl;
#endif
    }

    if (0 != trailingMismatchLenTmp)
    {
        if ((!bamRead.is_fwd_strand()) || (!is_overlapping_pair(bamRead, matchedAlignment)))
        {
            unsigned minQCount(0);
            for (unsigned pos(0); pos<trailingMismatchLenTmp; ++pos)
            {
                if (qual[readSize-pos-1] >= minQ) ++minQCount;
            }
            if ((static_cast<float>(minQCount)/(trailingMismatchLenTmp)) >= minQFrac)
            {
                trailingMismatchLen = trailingMismatchLenTmp;
            }
        }
#ifdef DEBUG_SEMI_ALIGNED
        else
            log_os << "Overlapping_pair trailing" << " read qname=" << bamRead.qname() << std::endl;
#endif
    }
}


#if 0
// TODO: pass iterator instead of ref substring
bool
isSemiAligned(
    const bam_record& bamRead,
    const std::string& refSeq,
    const double minSemiAlignedScore)
{
    // read cannot be semi-aligned in unmapped
    if (bamRead.is_unmapped()) return false;

    ALIGNPATH::path_t apath;
    bam_cigar_to_apath(bamRead.raw_cigar(),bamRead.n_cigar(),apath);

    // soft-clipped reads, not looked at here
    /*if (refSeq.size() != qrySeq.size())
    {
        std::cerr << "Skip because of bad ref length." << std::endl;
        return false;
    }*/

    const std::string qrySeq(bamRead.get_bam_read().get_string());
    //std::cerr << "qrySeq = " << qrySeq << std::endl;
    //std::cerr << "refSeq = " << refSeq << std::endl;
    apath_add_seqmatch(qrySeq.begin(), qrySeq.end(),
                       refSeq.begin(), refSeq.end(),
                       apath);
    //std::cerr << "apath = " << apath << std::endl;

    const double semiAlignedScore(ReadScorer::getSemiAlignedMetric(bamRead.read_size(),apath,bamRead.qual()));
    //std::cerr << " semi-aligned score=" << semiAlignedScore << "\n";
#ifdef DEBUG_SEMI_ALIGNED
    static const std::string logtag("isSemiAligned");
    log_os << logtag << " semi-aligned score=" << semiAlignedScore << " read qname=" << bamRead.qname() << " apath=" << apath <<  std::endl;
#endif
    //std::cerr << " semi-aligned score=" << semiAlignedScore << " read qname=" << bamRead.qname() << " apath=" << apath <<  std::endl;
    //if (semiAlignedScore>minSemiAlignedScore) {
    //std::cerr << "SEMI-ALIGNED" << std::endl;
    //} else {
    //std::cerr << "NOT SEMI-ALIGNED" << std::endl;
    //}
    return (semiAlignedScore>minSemiAlignedScore);
}
#endif
