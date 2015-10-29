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

#include "common/OutStream.hh"

#include "common/Exceptions.hh"

#include <fstream>
#include <iostream>


OutStream::
OutStream(const std::string& fileName) :
    _isInit(false),
    _fileName(fileName),
    _osptr(&std::cout),
    _ofsptr(new std::ofstream)
{
    if (! _fileName.empty())
    {
        std::ofstream test;
        openFile(_fileName,test);
    }
}



// required for unique_ptr:
OutStream::
~OutStream() {}



void
OutStream::
initStream()
{
    if (! _fileName.empty())
    {
        openFile(_fileName,*_ofsptr);
        _osptr=_ofsptr.get();
    }
    _isInit=true;
}

void
OutStream::
openFile(
    const std::string& filename,
    std::ofstream& ofs)
{
    ofs.open(filename.c_str());
    if (ofs) return;
    std::ostringstream oss;
    oss << "ERROR: Can't open output file: " << filename << "\n";
    BOOST_THROW_EXCEPTION(illumina::common::LogicException(oss.str()));
}
