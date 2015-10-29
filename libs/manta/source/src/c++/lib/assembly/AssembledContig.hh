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

#include "blt_util/known_pos_range2.hh"

#include <iosfwd>
#include <string>
#include <vector>
#include <set>


/// \brief data pertaining to a de-novo assembly contig
///
/// stores for each contig the sequence and the number of reads
/// containing its seeding k-mer
///
struct AssembledContig
{
    std::string seq; ///< contigsequence

    // reads used for assembly of contig <read_no,mapping position to contig>
    //std::map<std::string,int> contigReads;

    unsigned seedReadCount = 0; ///< no of reads containing the seeding kmer

    std::set<unsigned> supportReads;
    std::set<unsigned> rejectReads;

    known_pos_range2 conservativeRange; ///< subsection of the contig with conservative coverage
};


std::ostream& operator<<(std::ostream& os, const AssembledContig& contig);


typedef std::vector<AssembledContig> Assembly;

