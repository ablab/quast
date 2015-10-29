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

#include <string>


namespace illumina
{
namespace blt_util
{

/// parse c-string to TYPE
///
/// tolerates a non-TYPE suffix, but a non-empty prefix must be parsable as a TYPE,
/// on completion the value of s will reflect the extent of the parse
///
/// if available, specify s_end for minor performance improvement (in case of extremely large string)
///
unsigned
parse_unsigned(
    const char*& s);

int
parse_int(
    const char*& s);

long
parse_long(
    const char*& s);

double
parse_double(
    const char*& s,
    const char* s_end = nullptr);



/// parse std::string to TYPE
///
/// entire string must be convertible, no trailing suffix is allowed
///
unsigned
parse_unsigned_str(
    const std::string& s);

int
parse_int_str(
    const std::string& s);

long
parse_long_str(
    const std::string& s);

double
parse_double_str(
    const std::string& s);



/// template version:
///
template <typename T>
T
parse_type(const char*&)
{
    static_assert(sizeof(T)==0, "no parse specialization available for type T");
    return T();
}


template <>
inline
unsigned
parse_type<unsigned>(const char*& s)
{
    return parse_unsigned(s);
}

template <>
inline
int
parse_type<int>(const char*& s)
{
    return parse_int(s);
}

template <>
inline
long
parse_type<long>(const char*& s)
{
    return parse_long(s);
}

template <>
inline
double
parse_type<double>(const char*& s)
{
    return parse_double(s);
}


}
}

