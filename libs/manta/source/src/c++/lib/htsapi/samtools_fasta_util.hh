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
/// \author Bret Barnes
///

#pragma once

#include <map>
#include <string>


/// retrieve a map of chromosome sizes from the fasta index
///
void
get_chrom_sizes(
    const std::string& fai_file,
    std::map<std::string,unsigned>& chrom_sizes);


/// retrieve size of specific chromosome from the fasta index
///
unsigned
get_chrom_length(
    const std::string& fai_file,
    const std::string& chrom_name);


/// get reference sequence from region
void
get_region_seq(
    const std::string& ref_file,
    const std::string& fa_region,
    std::string& ref_seq);

/// get reference sequence from decomposed region
void
get_region_seq(
    const std::string& ref_file,
    const std::string& chrom,
    const int begin_pos,
    const int end_pos,
    std::string& ref_seq);


/// get reference sequence from decomposed region and run standardization result
///
/// \param begin_pos begin position (zero-indexed, closed)
/// \param end_pos end position (zero-indexed, closed)
void
get_standardized_region_seq(
    const std::string& ref_file,
    const std::string& chrom,
    const int begin_pos,
    const int end_pos,
    std::string& ref_seq);
