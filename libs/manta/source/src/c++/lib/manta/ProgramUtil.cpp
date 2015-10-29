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

#include "ProgramUtil.hh"

#include <iostream>



void
usage(
    std::ostream& os,
    const manta::Program& prog,
    const boost::program_options::options_description& visible,
    const char* desc,
    const char* afteropts,
    const char* msg)
{
    os << "\n" << prog.name() << ": " << desc << "\n\n";
    os << "version: " << prog.version() << "\n";
    os << "compiler: " << prog.compiler() << "\n";
    os << "build-time: " << prog.buildTime() << "\n\n";
    os << "usage: " << prog.name() << " [options]" << afteropts << "\n\n";
    os << visible << "\n\n";

    if (nullptr != msg)
    {
        os << msg << "\n\n";
    }
    exit(2);
}
