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

#include "AlignerBase.hh"
#include "AlignerUtil.hh"
#include "Alignment.hh"

#include "blt_util/basic_matrix.hh"

//#define DEBUG_ALN // Standard debug output
//#define DEBUG_ALN_MATRIX // Dump full edit-matrix tables to stderr. Does not scale to non-trivial ref/query size!


/// represents alignment of a query sequence which can switch over from reference1 to reference2
///
/// an empty alignment to one reference indicates that the entire alignment is to the other reference
///
template <typename ScoreType>
struct JumpAlignmentResult
{
    JumpAlignmentResult()
    {
        clear();
    }

    void
    clear()
    {
        score=0;
        jumpInsertSize=0;
        jumpRange=0;
        align1.clear();
        align2.clear();
    }


    ScoreType score;
    unsigned jumpInsertSize;
    unsigned jumpRange; ///< length of sequence over which jump would have the same score (left-most on align1 is reported)
    Alignment align1;
    Alignment align2;
};

template <typename ScoreType>
std::ostream& operator<<(std::ostream& os, JumpAlignmentResult<ScoreType>& alignment);



/// \brief a method to align a contig to two references
///
/// the alignment can make a single jump from reference1 to reference2
///
template <typename ScoreType>
struct JumpAlignerBase : public AlignerBase<ScoreType>
{
    JumpAlignerBase(
        const AlignmentScores<ScoreType>& scores,
        const ScoreType jumpScore) :
        AlignerBase<ScoreType>(scores),
        _jumpScore(jumpScore)
    {}

    /// read-only access to the aligner's scores:
    const ScoreType&
    getJumpScore() const
    {
        return _jumpScore;
    }

protected:

    // backtrace logic shared with the intron jump aligner:
    template <typename SymIter, typename MatrixType>
    void
    backTraceAlignment(
        const SymIter queryBegin, const SymIter queryEnd,
        const SymIter ref1Begin, const SymIter ref1End,
        const SymIter ref2Begin, const SymIter ref2End,
        const size_t querySize, const size_t ref1Size, const size_t ref2Size,
        const MatrixType& ptrMatrix1, const MatrixType& ptrMatrix2,
        const BackTrace<ScoreType>& btraceInput,
        JumpAlignmentResult<ScoreType>& result) const;

    static
    uint8_t
    max4(
        ScoreType& max,
        const ScoreType v0,
        const ScoreType v1,
        const ScoreType v2,
        const ScoreType v3)
    {
        max=v0;
        uint8_t ptr=0;
        if (v1>v0)
        {
            max=v1;
            ptr=1;
        }
        if (v2>max)
        {
            max=v2;
            ptr=2;
        }
        if (v3>max)
        {
            max=v3;
            ptr=3;
        }
        return ptr;
    }

#ifdef DEBUG_ALN_MATRIX

    /// write out matrix of scores back-trace pointers for debug:
    template <typename SymIter, typename MatrixType, typename ScoreValType>
    void
    dumpTables(
        const SymIter queryBegin, const SymIter queryEnd,
        const SymIter ref1Begin, const SymIter ref1End,
        const SymIter ref2Begin, const SymIter ref2End,
        const size_t querySize,
        const MatrixType& ptrMatrix1, const MatrixType& ptrMatrix2,
        const std::vector<AlignState::index_t>& dumpStates,
        const std::vector<std::vector<ScoreValType>>& storeScores) const;

private:
    /// write out subset of matrix of scores back-trace pointers for debug, for
    /// one of the two references
    template <typename SymIter, typename MatrixType, typename ScoreValType>
    void
    dumpSingleRefTable(
        const SymIter refBegin, const SymIter refEnd,
        const size_t querySize,
        const MatrixType& ptrMatrix,
        const std::vector<std::vector<ScoreValType>>& storeScores,
        const char refSym,
        const AlignState::index_t sIndex,
        unsigned& storeIndex) const;

protected:
#endif

    const ScoreType _jumpScore;
};


#include "alignment/JumpAlignerBaseImpl.hh"


/// Convenience function to run alignment from multiple aligner classes:
template <typename JumpAligner, typename SymIter, typename ScoreType>
void
jumpAlign(
    const JumpAligner& jumpAligner,
    const SymIter queryBegin, const SymIter queryEnd,
    const SymIter ref1Begin, const SymIter ref1End,
    const SymIter ref2Begin, const SymIter ref2End,
    JumpAlignmentResult<ScoreType>& result)
{
    jumpAligner.align(queryBegin,queryEnd,ref1Begin,ref1End,ref2Begin,ref2End,result);
}


