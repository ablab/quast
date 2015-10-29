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

#include "blt_util/blt_exception.hh"
#include "htsapi/bam_dumper.hh"

#include <cassert>
#include <cstdlib>

#include <iostream>
#include <sstream>


bam_dumper::
bam_dumper(const char* filename,
           const bam_hdr_t* header)
    : _hdr(header)
{
    assert(nullptr != filename);
    assert(nullptr != header);

    _hfp = hts_open(filename, "wb");

    if (nullptr == _hfp)
    {
        std::ostringstream oss;
        oss << "Failed to open SAM/BAM/CRAM file for writing: '" << filename << "'";
        throw blt_exception(oss.str().c_str());
    }

    const int retval = sam_hdr_write(_hfp,_hdr);
    if (retval != 0)
    {
        std::ostringstream oss;
        oss << "Failed to write SAM/BAM/CRAM file header for: '" << filename << "'";
        throw blt_exception(oss.str().c_str());
    }
}


bam_dumper::
~bam_dumper()
{
    if (nullptr != _hfp)
    {
        const int retval = hts_close(_hfp);
        if (retval != 0)
        {
            std::ostringstream oss;
            oss << "Failed to close SAM/BAM/CRAM file";
            throw blt_exception(oss.str().c_str());
        }
    }
}
