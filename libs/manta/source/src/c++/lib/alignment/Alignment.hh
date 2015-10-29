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

#pragma once

#include "blt_util/align_path.hh"

#include <iosfwd>

/// minimal summary of a query sequence aligned to a reference, roughly
/// following bam conventions for describing the alignment (apath trivially
/// maps to CIGAR string segments)
struct Alignment
{
    void
    clear()
    {
        beginPos = 0;
        apath.clear();
    }

    bool
    isAligned()
    const
    {
        return (! apath.empty());
    }

    pos_t beginPos;
    ALIGNPATH::path_t apath;
};

std::ostream&
operator<<(std::ostream& os, const Alignment& align);



struct AlignState
{
    // note the order of this enumerator is important for bit packing in client code, in particular
    // we rely on fitting the [MATCH->JUMP] states in 2 bits for the standard jump aligner
    enum index_t
    {
        MATCH,
        DELETE,
        INSERT,
        JUMP, // allows for an arbitrarily large hop between two reference regions
        SPLICE,
        JUMPINS = SPLICE, // analogous to jump state, but for very large insertions, reuse SPLICE state, in current applications we don't need both states in the same model.
        SIZE
    };

    static
    const char*
    label(const index_t i)
    {
        switch (i)
        {
        case MATCH:
            return "MATCH";
        case DELETE:
            return "DELETE";
        case INSERT:
            return "INSERT";
        case JUMP:
            return "JUMP";
        case SPLICE:
            return "SPLICE/JUMPINS";
        default:
            return "UNKNOWN";
        }
    }

    static
    char
    symbol(const index_t i)
    {
        switch (i)
        {
        case MATCH:
            return 'M';
        case DELETE:
            return 'D';
        case INSERT:
            return 'I';
        case JUMP:
            return 'J';
        case SPLICE:
            return 'N';
        default:
            return '?';
        }
    }

};
