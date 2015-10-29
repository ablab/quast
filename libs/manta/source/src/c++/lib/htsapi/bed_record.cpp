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

#include "bed_record.hh"
#include "blt_util/parse_util.hh"

#include <cassert>

#include <iostream>



bool
bed_record::
set(const char* s)
{
    static const char sep('\t');
    static const unsigned maxword(3);

    clear();

    line = s;

    // simple tab parse:
    const char* start(s);
    const char* p(start);

    unsigned wordindex(0);
    while (wordindex<maxword)
    {
        if ((*p == sep) || (*p == '\n') || (*p == '\0'))
        {
            switch (wordindex)
            {
            case 0:
                chrom=std::string(start,p-start);
                break;
            case 1:
                begin=illumina::blt_util::parse_int(start);
                assert(start==p);
                break;
            case 2:
                end=illumina::blt_util::parse_int(start);
                assert(start==p);
                break;
            default:
                assert(0);
                break;
            }
            start=p+1;
            wordindex++;
        }
        if ((*p == '\n') || (*p == '\0')) break;
        ++p;
    }

    return (wordindex >= maxword);
}



std::ostream&
operator<<(
    std::ostream& os,
    const bed_record& bedr)
{
    os << bedr.chrom << '\t'
       << bedr.begin << '\t'
       << bedr.end << '\n';

    return os;
}

