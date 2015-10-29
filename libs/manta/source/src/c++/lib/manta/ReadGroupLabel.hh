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

#include "blt_util/compat_util.hh"

#include <cassert>
#include <cstdlib>
#include <cstring>

#include <iosfwd>


struct ReadGroupLabel
{
    /// if isCopyPtrs then the strings are copied and alloced/de-alloced by
    /// the object, if false the client is responsible these pointers over
    /// the lifetime of the label:
    explicit
    ReadGroupLabel(
        const char* bamLabelInit,
        const char* rgLabelInit,
        const bool isCopyPtrsInit = true) :
        isCopyPtrs(isCopyPtrsInit),
        bamLabel((isCopyPtrs && (NULL != bamLabelInit)) ? strdup(bamLabelInit) : bamLabelInit),
        rgLabel((isCopyPtrs && (NULL != rgLabelInit)) ? strdup(rgLabelInit) : rgLabelInit)
    {
        assert(NULL != bamLabel);
        assert(NULL != rgLabel);
    }

    ReadGroupLabel(const ReadGroupLabel& rhs) :
        isCopyPtrs(rhs.isCopyPtrs),
        bamLabel(isCopyPtrs ? strdup(rhs.bamLabel) : rhs.bamLabel),
        rgLabel(isCopyPtrs ? strdup(rhs.rgLabel) : rhs.rgLabel)
    {}

    ReadGroupLabel&
    operator=(const ReadGroupLabel& rhs)
    {
        if (this == &rhs) return *this;
        clear();
        isCopyPtrs = rhs.isCopyPtrs;
        bamLabel = (isCopyPtrs ? strdup(rhs.bamLabel) : rhs.bamLabel);
        rgLabel = (isCopyPtrs ? strdup(rhs.rgLabel) : rhs.rgLabel);
        return *this;
    }

public:

    ~ReadGroupLabel()
    {
        clear();
    }

    /// sort allowing for NULL string pointers in primary and secondary key:
    bool
    operator<(
        const ReadGroupLabel& rhs) const
    {
        const int scval(strcmp(bamLabel,rhs.bamLabel));
        if (scval < 0) return true;
        if (scval == 0)
        {
            return (strcmp(rgLabel,rhs.rgLabel) < 0);
        }

        return false;
    }

private:
    void
    clear()
    {
        if (isCopyPtrs)
        {
            if (NULL != bamLabel) free(const_cast<char*>(bamLabel));
            if (NULL != rgLabel) free(const_cast<char*>(rgLabel));
        }
    }

    bool isCopyPtrs;

public:
    const char* bamLabel;
    const char* rgLabel;
};


std::ostream&
operator<<(
    std::ostream& os,
    const ReadGroupLabel& rgl);
