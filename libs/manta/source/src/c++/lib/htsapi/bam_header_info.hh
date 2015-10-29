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

#pragma once

#include "bam_util.hh"

#include "blt_util/thirdparty_push.h"

#include "boost/serialization/string.hpp"
#include "boost/serialization/vector.hpp"
#include "boost/serialization/map.hpp"

#include "blt_util/thirdparty_pop.h"

#include <iosfwd>
#include <string>
#include <vector>
#include <map>


/// minimal c++ bam header info
///
/// this class replicates the minimum information
/// from the bam header required to parse regions
/// (ie. chr1:20-30). It is friendlier to mem management
/// and serialization than using the samtools struct
///
struct bam_header_info
{

    bam_header_info()
    {}

    explicit
    bam_header_info(const bam_hdr_t& header);

    bool
    operator==(const bam_header_info& rhs) const
    {
        const unsigned data_size(chrom_data.size());
        if (chrom_data.size() != rhs.chrom_data.size()) return false;
        for (unsigned i(0); i<data_size; ++i)
        {
            if (chrom_data[i] == rhs.chrom_data[i]) continue;
            return false;
        }
        return true;
    }

    template<class Archive>
    void serialize(Archive& ar, const unsigned /* version */)
    {
        ar& chrom_data;
        ar& chrom_to_index;
    }

    struct chrom_info
    {
        chrom_info(
            const char* init_label = NULL,
            const unsigned init_length = 0) :
            label((NULL==init_label) ? "" : init_label ),
            length(init_length)
        {}

        bool
        operator==(const chrom_info& rhs) const
        {
            return ((label == rhs.label) && (length == rhs.length));
        }

        template<class Archive>
        void serialize(Archive& ar, const unsigned /* version */)
        {
            ar& label& length;
        }

        std::string label;
        unsigned length;
    };

    std::vector<chrom_info> chrom_data;
    std::map<std::string, int32_t> chrom_to_index;
};


std::ostream&
operator<<(std::ostream& os, const bam_header_info& bhi);
