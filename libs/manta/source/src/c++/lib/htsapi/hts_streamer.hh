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

#include "bam_util.hh"
#include "tabix_util.hh"

#include "boost/utility.hpp"

#include <string>


struct hts_streamer : private boost::noncopyable
{
    hts_streamer(
        const char* filename,
        const char* region);

    ~hts_streamer();

    const char*
    name() const
    {
        return _stream_name.c_str();
    }

    unsigned
    record_no() const
    {
        return _record_no;
    }

protected:
    void
    _load_index();

    bool _is_record_set;
    bool _is_stream_end;
    unsigned _record_no;
    std::string _stream_name;

    htsFile* _hfp;
    tbx_t* _tidx;
    hts_itr_t* _titr;
    kstring_t _kstr;
};
