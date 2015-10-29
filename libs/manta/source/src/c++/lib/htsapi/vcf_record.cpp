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

#include "vcf_record.hh"
#include "blt_util/parse_util.hh"

#include <cassert>
#include <cctype>

#include <algorithm>
#include <iostream>



struct convert
{
    void operator()(char& c) const
    {
        c = toupper((unsigned char)c);
    }
};


static
void
stoupper(std::string& s)
{
    std::for_each(s.begin(), s.end(), convert());
}



bool
vcf_record::
set(const char* s)
{
    static const char sep('\t');
    static const unsigned maxword(5);

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
                pos=illumina::blt_util::parse_int(start);
                assert(start==p);
                break;
            case 2:
                // skip this field...
                break;
            case 3:
                ref=std::string(start,p-start);
                stoupper(ref);
                break;
            case 4:
                // additional parse loop for ',' character:
            {
                const char* p2(start);
                while (p2<=p)
                {
                    if ((*p2==',') || (p2==p))
                    {
                        alt.emplace_back(start,p2-start);
                        stoupper(alt.back());
                        start=p2+1;
                    }
                    p2++;
                }
            }
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



std::ostream& operator<<(std::ostream& os, const vcf_record& vcfr)
{
    os << vcfr.chrom << '\t'
       << vcfr.pos << '\t'
       << '.' << '\t'
       << vcfr.ref << '\t';

    const unsigned nalt(vcfr.alt.size());
    for (unsigned a(0); a<nalt; ++a)
    {
        if (a) os << ',';
        os << vcfr.alt[a];
    }
    os << '\t'
       << '.' << '\t'
       << '.' << '\t'
       << '.' << '\t'
       << '.' << '\n';

    return os;
}

