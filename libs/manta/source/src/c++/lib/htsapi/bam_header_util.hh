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

/// \brief bam record manipulation functions
///

#pragma once

#include "bam_util.hh"
#include "bam_header_info.hh"

#include <string>


/// parse a bam region into chrom/begin/end values
///
void
parse_bam_region(
    const char* region,
    std::string& chrom,
    int32_t& begin_pos,
    int32_t& end_pos);


/// parse a bam region into chrom-index/begin/end values based
/// on chromosome index lookup and end positions in bam header
///
void
parse_bam_region_from_hdr(
    const bam_hdr_t* header,
    const char* region,
    int32_t& tid,
    int32_t& begin_pos,
    int32_t& end_pos);


void
parse_bam_region(
    const bam_header_info& header,
    const char* region,
    int32_t& tid,
    int32_t& begin_pos,
    int32_t& end_pos);


/// return true only if the headers refer to the same
/// reference sequences in the same order.
///
bool
check_header_compatibility(
    const bam_hdr_t* h1,
    const bam_hdr_t* h2);


/// try to determine the sample_name from the BAM header
/// if none found return default string value
std::string
get_bam_header_sample_name(
    const bam_hdr_t* header,
    const char* default_sample_name = "SAMPLE");
