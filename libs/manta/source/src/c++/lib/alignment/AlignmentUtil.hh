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

#pragma once

#include "alignment/Alignment.hh"
#include "alignment/GlobalJumpAligner.hh"
#include "alignment/GlobalAligner.hh"


/// return end position of alignment
inline
pos_t
alignEnd(const Alignment& align)
{
    return (align.beginPos + ALIGNPATH::apath_ref_length(align.apath));
}


/// get begin position of alignment, accounting for a possibly reversed reference/alignment:
///
inline
pos_t
getAlignBeginOffset(
    const Alignment& align,
    const unsigned refSize,
    const bool isReversed)
{
    if (isReversed)
    {
        return (refSize - alignEnd(align));
    }
    else
    {
        return align.beginPos;
    }
}


/// get end position of alignment, accounting for a possibly reversed reference/alignment:
///
inline
pos_t
getAlignEndOffset(
    const Alignment& align,
    const unsigned refSize,
    const bool isReversed)
{
    if (isReversed)
    {
        return (refSize - align.beginPos);
    }
    else
    {
        return alignEnd(align);
    }
}



/// check a jump alignment for consistency (only one end aligning)
/// FIXME: not used, need to think what makes an alignment consistent
/// (how about : total number of matches shouldn't exceed sequence length?)
//bool
//isConsistentAlignment(const JumpAlignmentResult<int>& res, const unsigned minAlignContext = 0);


/// extend the contig by padding the flanking regions of the aligned reference regions on each end
void
getExtendedContig(
    const JumpAlignmentResult<int>& align,
    const std::string& querySeq,
    const std::string& ref1Seq,
    const std::string& ref2Seq,
    std::string& extendedContig);


/// extend the somatic contig by padding the flanking regions of the aligned reference regions on each end
void
getExtendedContig(
    const AlignmentResult<int>& alignment,
    const std::string& querySeq,
    const std::string& refSeq,
    std::string& extendedContig);

/// given a jump alignment and query sequence, return the bp1,insert and bp2 query sequences
///
/// the insert sequence is converted to fwd-strand by assuming it is "attached" to bp1
/// note this is targeted for debug-code only
///
void
getFwdStrandQuerySegments(
    const JumpAlignmentResult<int>& align,
    const std::string& querySeq,
    const bool isBp2AlignedFirst,
    const bool isBp1Reversed,
    const bool isBp2Reversed,
    std::string& bp1Seq,
    std::string& bp2Seq,
    std::string& insertSeq);


/// given a jump alignment and query sequence, return the insert sequence
///
/// the insert sequence is converted to fwd-strand by assuming it is "attached" to bp1
///
void
getFwdStrandInsertSegment(
    const JumpAlignmentResult<int>& align,
    const std::string& querySeq,
    const bool isBp1Reversed,
    std::string& insertSeq);


/// TODO: document this if it serves a general purpose, or make private to AssembleSVBreakend
int
estimateBreakPointPos(const Alignment& al, const unsigned refOffset);
