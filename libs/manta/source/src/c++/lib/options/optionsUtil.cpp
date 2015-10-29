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

#include "optionsUtil.hh"

#include "boost/filesystem.hpp"

#include <sstream>



bool
checkStandardizeInputFile(
    std::string& filename,
    const char* fileLabel,
    std::string& errorMsg)
{
    errorMsg.clear();

    if (filename.empty())
    {
        std::ostringstream oss;
        oss << "Must specify " << fileLabel << " file";
        errorMsg = oss.str();
    }
    else if (! boost::filesystem::exists(filename))
    {
        std::ostringstream oss;
        oss << "Can't find " << fileLabel << " file '" << filename << "'";
        errorMsg = oss.str();
    }
    else
    {
        filename = boost::filesystem::canonical(filename).string();
    }

    return (! errorMsg.empty());
}
