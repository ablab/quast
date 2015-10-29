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
/// \brief align a contig across two breakend regions
///


#pragma once

#include "Alignment.hh"
#include "blt_util/align_path.hh"


struct AlignerUtil
{
    static
    void
    updatePath(
        ALIGNPATH::path_t& path,
        ALIGNPATH::path_segment& ps,
        ALIGNPATH::align_t atype)
    {
        if (ps.type == atype) return;
        if (ps.type != ALIGNPATH::NONE) path.push_back(ps);
        ps.type = atype;
        ps.length = 0;
    }
};



/// bookkeeping variables used during alignment backtrace
template <typename ScoreType>
struct BackTrace
{
    ScoreType max = 0;
    AlignState::index_t state = AlignState::MATCH;
    unsigned queryBegin = 0;
    unsigned refBegin = 0;
    bool isInit = false;
};



/// track values needed to run the alignment backtrace:
template <typename ScoreType>
void
updateBacktrace(
    const ScoreType thisMax,
    const unsigned refIndex,
    const unsigned queryIndex,
    BackTrace<ScoreType>& btrace,
    const AlignState::index_t state = AlignState::MATCH
)
{
    if ( (! btrace.isInit) || (thisMax>btrace.max))
    {
        btrace.max=thisMax;
        btrace.refBegin=refIndex;
        btrace.queryBegin=queryIndex;
        btrace.isInit=true;
        btrace.state = state;
    }
}




