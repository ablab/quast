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

/// derived from ELAND implementation by Tony Cox

#pragma once

#include "SingleRefAlignerShared.hh"


/// \brief Implementation of global alignment with affine gap costs
///
/// alignment outputs start positions and CIGAR-style alignment
/// expression of query to reference. Alignment of
/// query is global -- query can "fall-off" either end of the reference,
/// in this case, each unaligned position is given an "off-edge" score and
/// the base is soft-clipped in the alignment
///
/// transition from insert to delete is free and allowed, but not reverse
///
template <typename ScoreType>
struct GlobalAligner : public SingleRefAlignerBase<ScoreType>
{
    GlobalAligner(
        const AlignmentScores<ScoreType>& scores) :
        SingleRefAlignerBase<ScoreType>(scores)
    {}

    /// returns alignment path of query to reference
    template <typename SymIter>
    void
    align(
        const SymIter queryBegin, const SymIter queryEnd,
        const SymIter refBegin, const SymIter refEnd,
        AlignmentResult<ScoreType>& result) const;

private:

    // insert and delete are for query wrt reference
    struct ScoreVal
    {
        ScoreType match;
        ScoreType ins;
        ScoreType del;
    };

    struct PtrVal
    {
        typedef uint8_t code_t;

        /// for state i, return the highest scoring previous state
        /// to use during the backtrace?
        AlignState::index_t
        getStatePtr(const AlignState::index_t i) const
        {
            return static_cast<AlignState::index_t>(getStateCode(i));
        }
    private:
        code_t
        getStateCode(const AlignState::index_t i) const
        {
            switch (i)
            {
            case AlignState::MATCH:
                return match;
            case AlignState::INSERT:
                return ins;
            case AlignState::DELETE:
                return del;
            default:
                assert(false && "Unexpected Index Value");
                return 0;
            }
        }

    public:
        code_t match : 2;
        code_t ins : 2;
        code_t del : 2;
    };

    // add the matrices here to reduce allocations over many alignment calls:
    typedef std::vector<ScoreVal> ScoreVec;
    mutable ScoreVec _score1;
    mutable ScoreVec _score2;
    mutable basic_matrix<PtrVal> _ptrMat;
};


#include "alignment/GlobalAlignerImpl.hh"
