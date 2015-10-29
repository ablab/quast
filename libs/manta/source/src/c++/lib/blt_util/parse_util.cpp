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

#include "blt_util/blt_exception.hh"
#include "blt_util/parse_util.hh"

#include "boost/spirit/include/qi.hpp"

#include <cerrno>
#include <climits>
#include <cstdlib>

#include <limits>
#include <sstream>



static
void
parse_exception(const char* type_label,
                const char* parse_str)
{
    std::ostringstream oss;
    oss << "ERROR: Can't parse " << type_label << " from string: '" << parse_str << "'";
    throw blt_exception(oss.str().c_str());
}


namespace illumina
{
namespace blt_util
{

unsigned
parse_unsigned(
    const char*& s)
{
    static const int base(10);

    errno = 0;

    char* endptr;
    const unsigned long val(strtoul(s, &endptr, base));
    if ((errno == ERANGE && (val == ULONG_MAX || val == 0))
        || (errno != 0 && val == 0) || (endptr == s))
    {
        parse_exception("unsigned long",s);
    }

    if (val > std::numeric_limits<unsigned>::max())
    {
        parse_exception("unsigned",s);
    }

    s = endptr;

    return static_cast<unsigned>(val);
}



unsigned
parse_unsigned_str(
    const std::string& s)
{
    const char* s2(s.c_str());
    const char* const s2_end(s2+s.size());
    const unsigned val(parse_unsigned(s2));
    if (s2 != s2_end)
    {
        parse_exception("unsigned",s.c_str());
    }
    return val;
}



int
parse_int(
    const char*& s)
{
    const char* endptr(s);
    const long val(parse_long(endptr));

    if ((val > std::numeric_limits<int>::max()) ||
        (val < std::numeric_limits<int>::min()))
    {
        parse_exception("int",s);
    }

    s = endptr;

    return static_cast<int>(val);
}



int
parse_int_str(
    const std::string& s)
{
    const char* s2(s.c_str());
    const char* const s2_end(s2+s.size());
    const int val(parse_int(s2));
    if (s2 != s2_end)
    {
        parse_exception("int",s.c_str());
    }
    return val;
}



long
parse_long(
    const char*& s)
{
    static const int base(10);

    errno = 0;

    char* endptr;
    const long val(strtol(s, &endptr, base));
    if ((errno == ERANGE && (val == LONG_MIN || val == LONG_MAX))
        || (errno != 0 && val == 0) || (endptr == s))
    {
        parse_exception("long int",s);
    }

    s = endptr;

    return val;
}



long
parse_long_str(
    const std::string& s)
{
    const char* s2(s.c_str());
    const char* const s2_end(s2+s.size());
    const long val(parse_long(s2));
    if (s2 != s2_end)
    {
        parse_exception("long int",s.c_str());
    }
    return val;
}



double
parse_double(
    const char*& s,
    const char* s_end)
{
    double val;
    const char* s_start(s);
    if (s_end == nullptr) s_end=s+strlen(s);
    bool isPass(boost::spirit::qi::parse(s, s_end, boost::spirit::double_, val));
    if (isPass)
    {
        isPass = (s_start != s);
    }
    if (! isPass)
    {
        parse_exception("double",s_start);
    }
    return val;
}



double
parse_double_str(
    const std::string& s)
{
    const char* s2(s.c_str());
    const char* const s2_end(s2+s.size());
    const double val(parse_double(s2,s2_end));
    if (s2 != s2_end)
    {
        parse_exception("double",s.c_str());
    }
    return val;
}

}
}
