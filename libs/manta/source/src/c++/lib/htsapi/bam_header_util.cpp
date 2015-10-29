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

#include "htsapi/bam_header_util.hh"
#include "blt_util/blt_exception.hh"
#include "blt_util/parse_util.hh"
#include "blt_util/string_util.hh"

#include <cassert>

#include <algorithm>
#include <limits>
#include <sstream>



void
parse_bam_region(
    const char* region,
    std::string& chrom,
    int32_t& begin_pos,
    int32_t& end_pos)
{
    // make first split:
    const char* afterChrom;
    {
        static const char region_sep1(':');
        afterChrom = strrchr(region,region_sep1);
        if (nullptr != afterChrom)
        {
            chrom=std::string(region,afterChrom);
            assert((*afterChrom) != '\0');
            afterChrom++;
        }
    }


    bool isWholeChrom(nullptr == afterChrom);

    if (! isWholeChrom)
    {
        // make second split
        static const char region_sep2('-');
        std::vector<std::string> words2;
        split_string(afterChrom,region_sep2,words2);

        if (words2.empty() || (words2.size() > 2))
        {
            std::ostringstream oss;
            oss << "ERROR: can't parse begin and end positions from bam_region '" << region << "'\n";
            throw blt_exception(oss.str().c_str());
        }

        if (words2.size() == 2)
        {
            begin_pos = (illumina::blt_util::parse_int_str(words2[0]))-1;
            end_pos = (illumina::blt_util::parse_int_str(words2[1]));
        }
        else
        {
            // this exception allows for chrom names with colons (HLA...) but no positions included
            isWholeChrom=true;
        }
    }

    if (isWholeChrom)
    {
        chrom=region;
        begin_pos = 0;
        end_pos = std::numeric_limits<int32_t>::max();

    }

    if (chrom.empty())
    {
        std::ostringstream oss;
        oss << "ERROR: can't parse contig name from bam_region '" << region << "'\n";
        throw blt_exception(oss.str().c_str());
    }

    if ((begin_pos<0) || (end_pos<0) || (end_pos<=begin_pos))
    {
        std::ostringstream oss;
        oss << "ERROR: nonsensical begin (" << begin_pos << ") and end (" << end_pos << ") positions parsed from bam_region '" << region << "'\n";
        throw blt_exception(oss.str().c_str());
    }
}



void
parse_bam_region_from_hdr(
    const bam_hdr_t* header,
    const char* region,
    int32_t& tid,
    int32_t& begin_pos,
    int32_t& end_pos)
{
    assert(nullptr != header);
    assert(nullptr != region);

    std::string chrom;
    parse_bam_region(region,chrom,begin_pos,end_pos);

    tid = bam_name2id(const_cast<bam_hdr_t*>(header),chrom.c_str());

    if (tid < 0)
    {
        std::ostringstream oss;
        oss << "ERROR: contig '" << chrom << "' from bam_region '" << region << "' not found in BAM/CRAM header\n";
        throw blt_exception(oss.str().c_str());
    }

    end_pos = std::min(end_pos,static_cast<int32_t>(header->target_len[tid]));
}



void
parse_bam_region(
    const bam_header_info& header,
    const char* region,
    int32_t& tid,
    int32_t& begin_pos,
    int32_t& end_pos)
{
    assert(nullptr != region);

    std::string chrom;
    parse_bam_region(region,chrom,begin_pos,end_pos);

    const auto citer(header.chrom_to_index.find(chrom));

    if (citer == header.chrom_to_index.end())
    {
        std::ostringstream oss;
        oss << "ERROR: contig '" << chrom << "' from bam_region '" << region << "' not found in BAM/CRAM header\n";
        throw blt_exception(oss.str().c_str());
    }

    tid = citer->second;
    end_pos = std::min(end_pos,static_cast<int32_t>(header.chrom_data[tid].length));
}



bool
check_header_compatibility(
    const bam_hdr_t* h1,
    const bam_hdr_t* h2)
{
    if (h1->n_targets != h2->n_targets)
    {
        return false;
    }

    for (int32_t i(0); i<h1->n_targets; ++i)
    {
        if (h1->target_len[i] != h2->target_len[i]) return false;
        if (0 != strcmp(h1->target_name[i],h2->target_name[i])) return false;
    }
    return true;
}



std::string
get_bam_header_sample_name(
    const bam_hdr_t* header,
    const char* default_sample_name)
{
    assert(header != nullptr);

    std::vector<std::string> lines;
    std::vector<std::string> words;
    split_string(header->text,'\n',lines);
    for (const auto& line : lines)
    {
        split_string(line,'\t',words);
        if ((! words.empty()) && (words.front() == "@RG"))
        {
            for (const auto& word : words)
            {
                static const std::string prefix("SM:");
                const auto res = std::mismatch(prefix.begin(), prefix.end(), word.begin());

                if (res.first == prefix.end())
                {
                    return std::string(res.second, word.end());
                }
            }
        }
    }
    return default_sample_name;
}
