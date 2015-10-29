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
///

#include "alignment/AlignmentUtil.hh"
#include "blt_util/seq_util.hh"

#include <cassert>

//#define DEBUG_RS



#ifdef DEBUG_RS
#include "blt_util/log.hh"
#include <iostream>
#endif



/// tests if prefix of sequence is aligned
static
bool
hasAlignedPrefix(const Alignment& al)
{
    if (al.apath.empty()) return false;
    return (is_segment_align_match(al.apath[0].type));
}


/// tests if suffix of sequence is aligned
static
bool
hasAlignedSuffix(const Alignment& al)
{
    if (al.apath.empty()) return false;
    return (is_segment_align_match(al.apath.back().type));
}



// check a jump alignment for consistency (only one end aligning)
// FIXME: not used, need to think what makes an alignment consistent
// (how about : total number of matches shouldn't exceed sequence length?)
//bool
//isConsistentAlignment(const JumpAlignmentResult<int>& res, const unsigned /*minAlignContext = 0*/)
//{
//    // not consistent if both unaligned
//    if (! (res.align1.isAligned() && res.align2.isAligned()) ) return false;
//
//    return true;
//}



void
getFwdStrandQuerySegments(
    const JumpAlignmentResult<int>& align,
    const std::string& querySeq,
    const bool isBp2AlignedFirst,
    const bool isBp1Reversed,
    const bool isBp2Reversed,
    std::string& bp1Seq,
    std::string& bp2Seq,
    std::string& insertSeq)
{
    const unsigned align1Size(apath_read_length(align.align1.apath));
    const unsigned insertSize(align.jumpInsertSize);
    const unsigned insertOffset(align1Size + insertSize);
    const unsigned align2Size(apath_read_length(align.align2.apath));
    const unsigned align2Offset(insertOffset + align2Size);

    assert(querySeq.size() == align2Offset);

    bp1Seq = querySeq.substr(0,align1Size);
    insertSeq = querySeq.substr(align1Size,insertSize);
    bp2Seq = querySeq.substr(insertOffset,align2Size);

    if (isBp2AlignedFirst)
    {
        std::swap(bp1Seq,bp2Seq);
    }

    if (isBp1Reversed)
    {
        reverseCompStr(bp1Seq);
        reverseCompStr(insertSeq);
    }

    if (isBp2Reversed)
    {
        reverseCompStr(bp2Seq);
    }
}



void
getExtendedContig(
    const JumpAlignmentResult<int>& align,
    const std::string& querySeq,
    const std::string& ref1Seq,
    const std::string& ref2Seq,
    std::string& extendedContig)
{
    // extend aligned ref1Seq to the beginning
    const std::string align1RefSeqExt = ref1Seq.substr(0, align.align1.beginPos);
    // extend aligned ref2Seq to the end
    const unsigned align2RefEnd = align.align2.beginPos + apath_ref_length(align.align2.apath);
    const std::string align2RefSeqExt = ref2Seq.substr(align2RefEnd, (ref2Seq.size()-align2RefEnd));

    extendedContig = align1RefSeqExt + querySeq + align2RefSeqExt;
}



/// extend the assembly contig to cover the whole target reference region after alignment
void
getExtendedContig(
    const AlignmentResult<int>& alignment,
    const std::string& querySeq,
    const std::string& refSeq,
    std::string& extendedContig)
{
    // extend aligned refSeq to the beginning
    const std::string alignRefSeqExt1 = refSeq.substr(0, alignment.align.beginPos);
    // extend aligned refSeq to the end
    const unsigned alignRefEnd = alignment.align.beginPos + apath_ref_length(alignment.align.apath);
    const std::string alignRefSeqExt2 = refSeq.substr(alignRefEnd, (refSeq.size()-alignRefEnd));

    extendedContig = alignRefSeqExt1 + querySeq + alignRefSeqExt2;
}



void
getFwdStrandInsertSegment(
    const JumpAlignmentResult<int>& align,
    const std::string& querySeq,
    const bool isBp1Reversed,
    std::string& insertSeq)
{
    const unsigned align1Size(apath_read_length(align.align1.apath));
    const unsigned insertSize(align.jumpInsertSize);

    insertSeq = querySeq.substr(align1Size,insertSize);

    if (isBp1Reversed) reverseCompStr(insertSeq);
}



int
estimateBreakPointPos(const Alignment& al, const unsigned refOffset)
{
    // -1 means no breakpoint found
    int breakPointPosEstimate(-1);

    const bool isPrefix = hasAlignedPrefix(al);
    const bool isSuffix = hasAlignedSuffix(al);

    if (! (isPrefix || isSuffix) )
    {
        return breakPointPosEstimate;
    }

    if (isPrefix)
    {
        breakPointPosEstimate = refOffset + al.beginPos /*+ isPrefix*/;
    }
    else if (isSuffix)
    {
        breakPointPosEstimate = refOffset + alignEnd(al) /*- isSuffix*/;
    }

    assert(breakPointPosEstimate>0);

    return breakPointPosEstimate;
}
