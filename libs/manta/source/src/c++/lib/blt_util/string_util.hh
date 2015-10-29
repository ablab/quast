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
#include <vector>


void
split_string(
    const char* str,
    const char delimiter,
    std::vector<std::string>& v);

/// insert nulls into str to create a vector of c-strs without new allocation
void
destructive_split_string(
    char* str,
    const char delimiter,
    std::vector<const char*>& v);

void
split_string(
    const std::string& str,
    const char delimiter,
    std::vector<std::string>& v,
    const bool isSkipEmpty = false);

/// check for exact match to pattern after delimiting str by delimiter
bool
split_match(
    const std::string& str,
    const char delimiter,
    const char* needle);

