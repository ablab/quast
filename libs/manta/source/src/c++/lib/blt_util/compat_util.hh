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

// take care of some (mostly C99) functions not available in VS C++
//

#pragma once

#include <string>


#ifdef _MSC_VER
#define snprintf _snprintf
#define strdup _strdup
#endif

#if ((defined(_MSC_VER)) && (_MSC_VER <= 1800))
#undef noexcept
#define noexcept
#endif

double
compat_round(const double x);


const char*
compat_basename(const char* s);


// gets canonical name of paths, but only when these refer to existing items
// returns false on error.
bool
compat_realpath(std::string& path);
