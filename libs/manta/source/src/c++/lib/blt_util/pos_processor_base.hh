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

#include "blt_util/blt_types.hh"


/// \brief base for objects designed to perform work in a single pass over a position range
///
/// Work progress is communicated via the process_pos() method. This base class is designed to
/// link the worker object with the stage_manager object
///
struct pos_processor_base
{
    pos_processor_base()
        : _is_skip_process_pos(false) {}

    virtual
    ~pos_processor_base() {}

    void
    check_process_pos(const int stage_no,
                      const pos_t pos)
    {
        if (_is_skip_process_pos) return;
        process_pos(stage_no,pos);
    }

    virtual
    void
    process_pos(const int stage_no,
                const pos_t pos) = 0;

protected:
    mutable bool _is_skip_process_pos;
};
