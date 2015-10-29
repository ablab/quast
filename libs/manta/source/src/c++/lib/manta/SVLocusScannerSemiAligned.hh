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

#pragma once

#include "blt_util/blt_types.hh"
#include "blt_util/reference_contig_segment.hh"
#include "blt_util/SimpleAlignment.hh"
#include "htsapi/bam_record_util.hh"


/// searches for poorly aligned read ends
///
/// search is based on high-quality mismatches, any soft-clipped sections
/// will be realigned before searching for semi-aligned sections
///
/// \param[in] minQ
/// \param[in] minQFrac this fraction of bases must have qual>=minQ within the clipped region
///
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
    const uint8_t minQ = 20,
    const float minQFrac = 0.75);


/// simplified interface to the full function above:
inline
void
getSVBreakendCandidateSemiAlignedSimple(
    const bam_record& bamRead,
    const SimpleAlignment& bamAlign,
    const reference_contig_segment& refSeq,
    const bool isUseOverlappingPairs,
    unsigned& leadingMismatchLen,
    unsigned& trailingMismatchLen)
{
    pos_t leadingRefPos(0), trailingRefPos(0);
    getSVBreakendCandidateSemiAligned(
        bamRead, bamAlign, refSeq,
        isUseOverlappingPairs,
        leadingMismatchLen, leadingRefPos,
        trailingMismatchLen, trailingRefPos);
}
