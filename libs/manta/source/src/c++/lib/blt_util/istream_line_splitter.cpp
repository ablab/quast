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
///
/// an efficient (and slightly unsafe) class for basic tab-delimited files, etc...
///

/// \author Chris Saunders
///

#include "blt_util/blt_exception.hh"
#include "blt_util/istream_line_splitter.hh"

#include <cassert>
#include <cstring>

#include <iostream>
#include <sstream>



void
istream_line_splitter::
write_line(std::ostream& os) const
{
    for (unsigned i(0); i<_n_word; ++i)
    {
        if (i) os << _sep;
        os << word[i];
    }
    os << "\n";
}



void
istream_line_splitter::
dump(std::ostream& os) const
{
    os << "\tline_no: " << _line_no << "\n";
    os << "\tline: ";
    write_line(os);
}



void
istream_line_splitter::
increase_buffer_size()
{
    assert(_buf_size>1);
    const unsigned old_buf_size(_buf_size);
    const char* old_buf(_buf);
    _buf_size *= 2;
    _buf=new char[_buf_size];
    memcpy(_buf,old_buf,(old_buf_size-1)*sizeof(char));
    delete [] old_buf;
}



static
bool
check_istream(std::istream& is,
              unsigned& line_no)
{
    if (is)
    {
        line_no++;
        // regular successful line read:
        return true;
    }

    if     (is.eof()) return false;
    else if (is.fail())
    {
        if (is.bad())
        {
            std::ostringstream oss;
            oss << "ERROR: unexpected failure while attempting to read line " << (line_no+1) << "\n";
            throw blt_exception(oss.str().c_str());
        }
        is.clear();
    }

    // incomplete line read in this case, have to increase buffer size:
    return true;
}



bool
istream_line_splitter::
parse_line()
{
    _n_word=0;
    _is.getline(_buf,_buf_size);
    const unsigned previous_line_no(_line_no);
    if (! check_istream(_is,_line_no)) return false; // normal eof
    unsigned buflen(strlen(_buf));

    while (((buflen+1) == _buf_size) && (previous_line_no==_line_no))
    {
        increase_buffer_size();
        _is.getline(_buf+buflen,_buf_size-buflen);
        if (! check_istream(_is,_line_no))
        {
            std::ostringstream oss;
            oss << "ERROR: Unexpected read failure in parse_line() at line_no: " << _line_no << "\n";
            throw blt_exception(oss.str().c_str());
        }
        buflen=(strlen(_buf));
    }

    if ((buflen+1) >_buf_size)
    {
        std::ostringstream oss;
        oss << "ERROR: Unexpected read failure in parse_line() at line_no: " << _line_no << "\n";
        throw blt_exception(oss.str().c_str());
    }

    if (nullptr == _buf) return false;
    assert(buflen);

    // do a low-level separator parse:
    {
        char* p(_buf);
        word[0]=p;
        unsigned i(1);
        while (i<_max_word)
        {
            if ((*p == '\n') || (*p == '\0')) break;
            if (*p == _sep)
            {
                *p = '\0';
                word[i++] = p+1;
            }
            ++p;
        }
        _n_word=i;
    }
    return true;
}
