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
#include "htsapi/bam_record.hh"

#include "boost/utility.hpp"

#include <cstdlib>
#include <cstring>

#include <iosfwd>
#include <string>


/// information required to uniquely identify a read:
///
struct ReadKey
{
    ReadKey(
        const bam_record& br,
        const bool isCopyPtrs = true)
        : _isCopyPtrs(isCopyPtrs)
        , _qname((_isCopyPtrs && (NULL != br.qname())) ? strdup(br.qname()) : br.qname())
        , _readNo(br.read_no())
    {
        assert(NULL != _qname);
    }

    ReadKey(
        const char* initQname,
        const int initReadNo,
        const bool isCopyPtrs = true)
        : _isCopyPtrs(isCopyPtrs)
        , _qname((_isCopyPtrs && (NULL != initQname)) ? strdup(initQname) : initQname)
        , _readNo(initReadNo)
    {
        assert(NULL != _qname);
    }

    ReadKey(
        const ReadKey& rhs)
        : _isCopyPtrs(rhs._isCopyPtrs)
        , _qname(_isCopyPtrs ? strdup(rhs._qname) : rhs._qname)
        , _readNo(rhs._readNo)
    {}

private:
    ReadKey& operator=(const ReadKey& rhs);

public:
    ~ReadKey()
    {
        if (_isCopyPtrs)
        {
            if (NULL != _qname) free(const_cast<char*>(_qname));
        }
    }

    int
    readNo() const
    {
        return _readNo;
    }

    const char*
    qname() const
    {
        return _qname;
    }

    bool operator<(
        const ReadKey& rhs) const
    {
        if (readNo() < rhs.readNo()) return true;
        if (readNo() == rhs.readNo())
        {
            return (strcmp(qname(), rhs.qname()) < 0);
        }
        return false;
    }

    bool operator==(
        const ReadKey& rhs) const
    {
        return ((readNo() == rhs.readNo()) && ((0 == strcmp(qname(), rhs.qname()))));
    }

private:
    const bool _isCopyPtrs;
    const char* _qname;
    const int _readNo;
};

std::ostream&
operator<<(std::ostream& os, const ReadKey& rk);

