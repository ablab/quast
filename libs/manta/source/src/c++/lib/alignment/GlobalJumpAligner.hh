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

#include "JumpAlignerBase.hh"



/// \brief a method to align a contig to two references
///
/// the alignment can make a single jump from reference1 to reference2
///
/// transition from insert to delete is free and allowed, but not reverse
/// transition from/to jump to insert is free and allowed TODO: more restrictive
///
template <typename ScoreType>
struct GlobalJumpAligner : public JumpAlignerBase<ScoreType>
{
    GlobalJumpAligner(
        const AlignmentScores<ScoreType>& scores,
        const ScoreType jumpScore) :
        JumpAlignerBase<ScoreType>(scores,jumpScore)
    {}

    /// returns alignment path of query to reference
    template <typename SymIter>
    void
    align(
        const SymIter queryBegin, const SymIter queryEnd,
        const SymIter ref1Begin, const SymIter ref1End,
        const SymIter ref2Begin, const SymIter ref2End,
        JumpAlignmentResult<ScoreType>& result) const;

private:

    // insert and delete are for seq1 wrt seq2
    struct ScoreVal
    {
        ScoreType
        getScore(const AlignState::index_t i) const
        {
            switch (i)
            {
            case AlignState::MATCH:
                return match;
            case AlignState::INSERT:
                return ins;
            case AlignState::DELETE:
                return del;
            case AlignState::JUMP:
                return jump;
            default:
                assert(false && "Unexpected Index Value");
                return 0;
            }
        }

        ScoreType match;
        ScoreType ins;
        ScoreType del;
        ScoreType jump;
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
            case AlignState::JUMP:
                return jump;
            default:
                assert(false && "Unexpected Index Value");
                return 0;
            }
        }

    public:
        // pack 2x4 bits into 1 byte:
        code_t match : 2;
        code_t ins : 2;
        code_t del : 2;
        code_t jump : 2;
    };

    // add the matrices here to reduce allocations over many alignment calls:
    typedef std::vector<ScoreVal> ScoreVec;
    mutable ScoreVec _score1;
    mutable ScoreVec _score2;

    typedef basic_matrix<PtrVal> PtrMat;
    mutable PtrMat _ptrMat1;
    mutable PtrMat _ptrMat2;
};


#include "alignment/GlobalJumpAlignerImpl.hh"
