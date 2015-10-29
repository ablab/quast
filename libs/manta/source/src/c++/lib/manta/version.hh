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

/// \brief provide access to cmake project version numbers

#pragma once

#include "common/config.h"

namespace manta
{

inline
const char*
getVersion()
{
    return WORKFLOW_VERSION;
}

inline
const char*
getBuildTime()
{
    return BUILD_TIME;
}

inline
const char*
cxxCompilerName()
{
    return CXX_COMPILER_NAME;
}

inline
const char*
compilerVersion()
{
    return COMPILER_VERSION;
}

}
