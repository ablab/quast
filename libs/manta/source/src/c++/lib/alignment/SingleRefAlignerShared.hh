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

/// common boilerplate for single-reference sequence aligners

#pragma once

#include "AlignerBase.hh"
#include "AlignerUtil.hh"
#include "Alignment.hh"

#include "blt_util/basic_matrix.hh"

#include <iosfwd>


template <typename ScoreType>
struct AlignmentResult
{
    AlignmentResult()
    {
        clear();
    }

    void
    clear()
    {
        score=0;
        align.clear();
    }

    ScoreType score;
    Alignment align;
};


template <typename ScoreType>
std::ostream&
operator<<(std::ostream& os, AlignmentResult<ScoreType>& alignment);



template <typename ScoreType>
struct SingleRefAlignerBase : public AlignerBase<ScoreType>
{
    SingleRefAlignerBase(
        const AlignmentScores<ScoreType>& scores) :
        AlignerBase<ScoreType>(scores)
    {}

protected:

    /// returns alignment path of query to reference
    template <typename SymIter, typename MatrixType>
    void
    backTraceAlignment(
        const SymIter queryBegin, const SymIter queryEnd,
        const SymIter refBegin, const SymIter refEnd,
        const size_t querySize, const size_t refSize,
        const MatrixType& ptrMatrix,
        const BackTrace<ScoreType>& btraceInput,
        AlignmentResult<ScoreType>& result) const;
};


#include "SingleRefAlignerSharedImpl.hh"
