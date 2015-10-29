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
/// \author Chris Saunders and Felix Schlesinger
///

#pragma once

#include "JumpAlignerBase.hh"


/// \brief a method to align a contig to two references
///
/// the alignment can make a single jump from reference1 to reference2
///
template <typename ScoreType>
struct GlobalJumpIntronAligner : public JumpAlignerBase<ScoreType>
{
    GlobalJumpIntronAligner(
        const AlignmentScores<ScoreType>& scores,
        const ScoreType jumpScore,
        const ScoreType intronOpenScore,
        const ScoreType intronOffEdgeScore) :
        JumpAlignerBase<ScoreType>(scores,jumpScore),
        _intronOpenScore(intronOpenScore),
        _intronOffEdgeScore(intronOffEdgeScore)
    {}

    /// returns alignment path of query to reference
    template <typename SymIter>
    void
    align(
        const SymIter queryBegin, const SymIter queryEnd,
        const SymIter ref1Begin, const SymIter ref1End,
        const SymIter ref2Begin, const SymIter ref2End,
        bool ref1Fw, bool ref2Fw, bool isStranded,
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
            case AlignState::SPLICE:
                return intron;
            default:
                assert(false && "Unexpected Index Value");
                return 0;
            }
        }

        ScoreType match;
        ScoreType ins;
        ScoreType del;
        ScoreType jump;
        ScoreType intron;
    };

    struct PtrVal
    {
        typedef uint16_t code_t;

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
            case AlignState::SPLICE:
                return intron;
            default:
                assert(false && "Unexpected Index Value");
                return 0;
            }
        }

    public:
        /// pack 3x5 bits into a single uint16_t:
        code_t match : 3;
        code_t ins : 3;
        code_t del : 3;
        code_t jump : 3;
        code_t intron : 3;
    };

    const ScoreType _intronOpenScore; ///< gap open for introns (i.e. deletions starting with splice motif) (should be negative)
    const ScoreType _intronOffEdgeScore; ///< As offEdge but only of the last aligned bases match a splice motif (should be negative) todo: not implemented

    // add the matrices here to reduce allocations over many alignment calls:
    typedef std::vector<ScoreVal> ScoreVec;
    mutable ScoreVec _score1;
    mutable ScoreVec _score2;

    typedef basic_matrix<PtrVal> PtrMat;
    mutable PtrMat _ptrMat1;
    mutable PtrMat _ptrMat2;
};


#include "alignment/GlobalJumpIntronAlignerImpl.hh"
