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

#include <iosfwd>
#include <memory>


void
open_ifstream(
    std::ifstream& ifs,
    const char* filename);


/// use this class to set scope specific stream formatting
///
/// see unit test for example usage
///
struct StreamScoper
{
    explicit
    StreamScoper(
        std::ostream& os);

    ~StreamScoper();

private:
    std::ostream& _os;
    std::unique_ptr<std::ofstream> _tmp_os;
};
