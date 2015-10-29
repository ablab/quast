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

/// random vcf utilities
///
/// \author Chris Saunders
///

#pragma once

#include <cstring>
#include <iosfwd>
#include <vector>


namespace VCFID
{
enum index_t
{
    CHROM,
    POS,
    ID,
    REF,
    ALT,
    QUAL,
    FILT,
    INFO,
    FORMAT,
    SAMPLE,
    SIZE
};
}



inline
const char*
vcf_col_label()
{
    static const char h[] = "#CHROM\tPOS\tID\tREF\tALT\tQUAL\tFILTER\tINFO";
    return h;
}


std::ostream&
vcf_fileDate(std::ostream& os);


void
write_vcf_filter(
    std::ostream& os,
    const char* id,
    const char* desc);


// look for 'key' in vcf FORMAT field, provide index of key or return
// false
//
inline
bool
get_format_key_index(const char* format,
                     const char* key,
                     unsigned& index)
{
    index=0;
    do
    {
        if (index) format++;
        if (0==strncmp(format,key,strlen(key))) return true;
        index++;
    }
    while (NULL != (format=strchr(format,':')));
    return false;
}



// return pointer to
//
inline
const char*
get_format_string_nocopy(const char* const* word,
                         const char* key)
{
    unsigned keynum(0);
    if (! get_format_key_index(word[VCFID::FORMAT],key,keynum)) return NULL;

    const char* sample(word[VCFID::SAMPLE]);
    for (; keynum; sample++)
    {
        if (! *sample) return NULL;
        if ((*sample)==':') keynum--;
    }
    return sample;
}



// returns -1 for '.' alleles
void
parse_gt(const char* gt,
         std::vector<int>& gti,
         const bool is_allow_bad_end_char=false);
