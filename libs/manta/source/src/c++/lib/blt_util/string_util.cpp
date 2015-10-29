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

/// \file

/// \author Chris Saunders
///


#include "string_util.hh"

#include <cstring>

#include <iostream>



void
split_string(
    const char* str,
    const char delimiter,
    std::vector<std::string>& v)
{
    v.clear();
    while (true)
    {
        const char* next(strchr(str,delimiter));
        if ((nullptr == next) || (delimiter == '\0'))
        {
            v.emplace_back(str);
            return;
        }
        v.emplace_back(str,next-str);
        str = next+1;
    }
}



void
destructive_split_string(
    char* str,
    const char delimiter,
    std::vector<const char*>& v)
{
    v.clear();
    while (true)
    {
        char* next(strchr(str,delimiter));
        v.push_back(str);
        if ((nullptr == next) || (delimiter == '\0')) return;
        *next = '\0';
        str = next+1;
    }
}



void
split_string(
    const std::string& str,
    const char delimiter,
    std::vector<std::string>& v,
    const bool isSkipEmpty)
{
    v.clear();

    size_t start(0);
    while (true)
    {
        size_t next(str.find(delimiter,start));
        if (! (isSkipEmpty && ((next==start) || (next==std::string::npos))))
        {
            v.emplace_back(str.substr(start,next-start));
        }
        if (next == std::string::npos) return;
        start = next+1;
    }
}



bool
split_match(const std::string& str,
            const char delimiter,
            const char* needle)
{
    size_t start(0);
    while (true)
    {
        size_t next(str.find(delimiter,start));
        if (0 == str.compare(start,next-start,needle)) return true;
        if (next == std::string::npos) break;
        start = next+1;
    }
    return false;
}
