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

/// \author Chris Saunders
///

#include "bam_header_info.hh"

#include <iostream>



bam_header_info::
bam_header_info(const bam_hdr_t& header)
{
    for (int i(0); i<header.n_targets; ++i)
    {
        chrom_data.emplace_back(header.target_name[i],header.target_len[i]);
        chrom_to_index[header.target_name[i]] = (int32_t)i;
    }
}


std::ostream&
operator<<(std::ostream& os, const bam_header_info& bhi)
{
    unsigned index(0);

    os << "chomosome_id_map:\n";
    for (const bam_header_info::chrom_info& info : bhi.chrom_data)
    {
        os << "index: " << index << " label: " << info.label << " length: " << info.length << '\n';
        index++;
    }
    return os;
}
