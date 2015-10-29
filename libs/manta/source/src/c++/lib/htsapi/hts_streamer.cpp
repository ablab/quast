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


#include "hts_streamer.hh"
#include "blt_util/blt_exception.hh"
#include "blt_util/log.hh"

#include <cstdlib>

#include <iostream>


static const kstring_t kinit = {0,0,0};


hts_streamer::
hts_streamer(
    const char* filename,
    const char* region) :
    _is_record_set(false),
    _is_stream_end(false),
    _record_no(0),
    _stream_name(filename),
    _hfp(nullptr),
    _tidx(nullptr),
    _titr(nullptr),
    _kstr(kinit)
{
    if (nullptr == filename)
    {
        throw blt_exception("hts filename is null ptr");
    }

    if (nullptr == region)
    {
        throw blt_exception("hts region is null ptr");
    }

    if ('\0' == *filename)
    {
        throw blt_exception("hts filename is empty string");
    }

    _hfp = hts_open(filename, "r");
    if (nullptr == _hfp)
    {
        log_os << "ERROR: Failed to open hts file: '" << filename << "'\n";
        exit(EXIT_FAILURE);
    }

    _load_index();

    // read only a region of HTS file:
    _titr = tbx_itr_querys(_tidx, region);
    if (nullptr == _titr)
    {
        _is_stream_end=true;
    }
}



hts_streamer::
~hts_streamer()
{
    if (nullptr != _titr) tbx_itr_destroy(_titr);
    if (nullptr != _tidx) tbx_destroy(_tidx);
    if (nullptr != _hfp) hts_close(_hfp);
}



// load index if it hasn't been set already:
void
hts_streamer::
_load_index()
{
    if (nullptr != _tidx) return;

    _tidx = tbx_index_load(name());
    if (nullptr == _tidx)
    {
        log_os << "ERROR: Failed to load index for hts file: '" << name() << "'\n";
        exit(EXIT_FAILURE);
    }
}
