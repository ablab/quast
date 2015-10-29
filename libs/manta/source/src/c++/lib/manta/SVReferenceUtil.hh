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

#pragma once

#include "blt_util/reference_contig_segment.hh"
#include "htsapi/bam_header_info.hh"
#include "manta/SVCandidate.hh"

#include <string>


/// test whether the two svCandidate breakend regions will overlap
/// after the extra reference padding has been added
bool
isRefRegionOverlap(
    const bam_header_info& header,
    const pos_t extraRefEdgeSize,
    const SVCandidate& sv);


/// test whether the interval intersects a reference contig at all
///
bool
isRefRegionValid(
    const bam_header_info& header,
    const GenomeInterval& interval);


/// given a genome interval, attempt to add an extra buffer
/// to the interval and return the reference sequence corresponding
/// to this interval
///
/// \params[in] extraRefEdgeSize add this value to the ends of each
///             interval prior to chomosome length clipping and reference
///             retrieval
/// \params[out] leadingTrim indicates how much was cut from the
///              front of the requested interval (with edge buffer)
/// \params[out] trailingTrim indicates how much was cut from the
///              end of the requested interval (with edge buffer)
///
void
getIntervalReferenceSegment(
    const std::string& referenceFilename,
    const bam_header_info& header,
    const pos_t extraRefEdgeSize,
    const GenomeInterval& interval,
    reference_contig_segment& intervalRef,
    unsigned& leadingTrim,
    unsigned& trailingTrim);


/// alternate interface to getIntervalReferenceSegment for applications
/// where the returned trim value is not needed:
inline
void
getIntervalReferenceSegment(
    const std::string& referenceFilename,
    const bam_header_info& header,
    const pos_t extraRefEdgeSize,
    const GenomeInterval& interval,
    reference_contig_segment& intervalRef)
{
    unsigned leadingTrim;
    unsigned trailingTrim;
    getIntervalReferenceSegment(referenceFilename, header, extraRefEdgeSize, interval, intervalRef,leadingTrim, trailingTrim);
}


/// extract the reference sequence around each breakend into a reference_contig_segment
/// object
///
/// for each region, we extract the hypothetical breakend region + extraRefEdgeSize bases
/// on each side, but limit the region to [0,chrom_size-1]
///
void
getSVReferenceSegments(
    const std::string& referenceFilename,
    const bam_header_info& header,
    const pos_t extraRefEdgeSize,
    const SVCandidate& sv,
    reference_contig_segment& bp1ref,
    reference_contig_segment& bp2ref,
    unsigned& bp1LeadingTrim,
    unsigned& bp1TrailingTrim,
    unsigned& bp2LeadingTrim,
    unsigned& bp2TrailingTrim);
