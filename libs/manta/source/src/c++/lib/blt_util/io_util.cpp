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

#include "blt_util/io_util.hh"

#include "blt_util/blt_exception.hh"

#include <cstdlib>

#include <fstream>
#include <iostream>
#include <sstream>



void
open_ifstream(
    std::ifstream& ifs,
    const char* filename)
{
    ifs.open(filename);
    if (! ifs)
    {
        std::ostringstream oss;
        oss << "ERROR: Can't open file: " << filename << "\n";
        throw blt_exception(oss.str().c_str());
    }
}



StreamScoper::
StreamScoper(
    std::ostream& os)
    : _os(os), _tmp_os(new std::ofstream)
{
    _tmp_os->copyfmt(_os);
}



StreamScoper::
~StreamScoper()
{
    _os.copyfmt(*_tmp_os);
}
