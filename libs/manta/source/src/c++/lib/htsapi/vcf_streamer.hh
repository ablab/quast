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

#include "hts_streamer.hh"
#include "vcf_record.hh"


struct vcf_streamer : public hts_streamer
{
    // optionally provide a BAM header to validate vcf chromosome names against
    //
    vcf_streamer(
        const char* filename,
        const char* region,
        const bam_hdr_t* bh = nullptr);

    ~vcf_streamer();

    // advance to next vcf record
    //
    // is_indel_only - if set, skip all records except indels
    //
    bool next(const bool is_indel_only=false);

    const vcf_record*
    get_record_ptr() const
    {
        if (_is_record_set) return &_vcfrec;
        else                return nullptr;
    }

    void report_state(std::ostream& os) const;

private:
    bcf_hdr_t* _hdr;
    vcf_record _vcfrec;
};
