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
///
///

#pragma once

#include "blt_util/align_path.hh"
#include "blt_util/SimpleAlignment.hh"
#include "htsapi/bam_record.hh"


/// is this read part of a pair where both members are mapped?
bool
is_mapped_pair(
    const bam_record& bam_read);


/// is this read part of a pair where both members are mapped to the same chrom?
///
bool
is_mapped_chrom_pair(
    const bam_record& bam_read);

/// is this read part of mapped pair with 'Innie' orientation?
///
/// Note this does not test MAPQ or fragment size, but could
/// be used as the core of a 'proper-pair' predicate
bool
is_innie_pair(
    const bam_record& bam_read);

/// detect cases where paired-end reads overlap in such a way as to suggest a possible unfiltered
/// read into adapter sequence (assuming innie pairs)
bool
is_possible_adapter_pair(
    const bam_record& bamRead);

/// detect cases where paired-end reads overlap (i.e. a fragment shorter than the combined read-length).
///
/// note this is an approximation because it's based on a single bam record, an
/// exact answer would require both records in the pair. In practice, this should
/// be good enough.
bool
is_overlapping_pair(
    const bam_record& bam_read,
    const SimpleAlignment& matchedAlignment);

/// return average basecall qscore for this read
unsigned
get_avg_quality(
    const bam_record& bam_read);

/// select 'first' read in pair such that you
/// consistently get only one read per-pair
/// (assuming the bam file is properly formated)
inline
bool
isFirstRead(
    const bam_record& bamRead)
{
    if (bamRead.pos() < bamRead.mate_pos()) return true;
    if ((bamRead.pos() == bamRead.mate_pos()) && bamRead.is_first()) return true;
    return false;
}

/// get BAM RG tag, return an empty string "" if no RG tag exists:
inline
const char*
getReadGroup(
    const bam_record& bamRead)
{
    static const char defaultRG[] = "";
    static const char rgTag[] = {'R','G'};

    const char* rgStr(bamRead.get_string_tag(rgTag));

    return ((NULL == rgStr) ? defaultRG : rgStr);
}
