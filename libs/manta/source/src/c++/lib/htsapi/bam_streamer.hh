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

#include "htsapi/bam_record.hh"
#include "htsapi/sam_util.hh"

#include "boost/utility.hpp"

#include <string>


/// Stream bam records from CRAM/BAM/SAM files. For CRAM/BAM
/// files you can run an indexed stream from a specific genome region.
///
//
// Example use:
// while (stream.next()) {
//     const bam_record& read(*(stream.get_record_ptr()));
//     if(read.is_unmapped) foo++;
// }
//
struct bam_streamer : public boost::noncopyable
{
    /// \param filename CRAM/BAM/SAM input file
    /// \param region if filename is indexed CRAM or BAM, you can
    ///        restrict the stream to a specific region
    explicit
    bam_streamer(const char* filename,
                 const char* region = nullptr);

    ~bam_streamer();

    /// \brief set new or first region for file:
    ///
    /// \param region if ctor filename is indexed CRAM or BAM, you can
    ///        restrict the stream to a specific region
    void
    set_new_region(const char* region);

    /// \brief set new or first region for file:
    ///
    /// \param beg zero-indexed start pos
    /// \param end zero-indexed end pos
    void
    set_new_region(
        int reg, int beg, int end);

    bool next();

    const bam_record* get_record_ptr() const
    {
        if (_is_record_set) return &_brec;
        else                return nullptr;
    }

    const char* name() const
    {
        return _stream_name.c_str();
    }

    unsigned record_no() const
    {
        return _record_no;
    }

    void report_state(std::ostream& os) const;

    const char*
    target_id_to_name(const int32_t tid) const;

    int32_t
    target_name_to_id(const char* seq_name) const;

    const bam_hdr_t*
    get_header() const
    {
        return _hdr;
    }

private:
    void _load_index();

    bool _is_record_set;
    htsFile* _hfp;
    bam_hdr_t* _hdr;
    hts_idx_t* _hidx;
    hts_itr_t* _hitr;
    bam_record _brec;

    // track for debug only:
    unsigned _record_no;
    std::string _stream_name;
    bool _is_region;
    std::string _region;
};
