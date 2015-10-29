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

#include "SingleRefAlignerShared.hh"


/// \brief align a contig to reference and allow very large insert or deletion events
///
/// this is essentially a regular global alignment with affine gap penalties for small
/// indels and a non-affine gap state for large indels, the open cost for large indels
/// is (typically) much higher but the extension cost is zero.
///
/// transition from insert to delete is free and allowed
/// transition from jumpDel to insert is free and allowed
/// transition from jumpIns to delete is free and allowed
///
template <typename ScoreType>
struct GlobalLargeIndelAligner : public SingleRefAlignerBase<ScoreType>
{
    /// \param largeIndelScore is the 'gap open' for the large indels
    ///
    GlobalLargeIndelAligner(
        const AlignmentScores<ScoreType>& scores,
        const ScoreType largeIndelScore) :
        SingleRefAlignerBase<ScoreType>(scores),
        _largeIndelScore(largeIndelScore)
    {}

    /// returns alignment path of query to reference
    template <typename SymIter>
    void
    align(
        const SymIter queryBegin, const SymIter queryEnd,
        const SymIter refBegin, const SymIter refEnd,
        AlignmentResult<ScoreType>& result) const;

private:

    // insert and delete are for seq1 wrt seq2
    struct ScoreVal
    {
        ScoreType match;
        ScoreType ins;
        ScoreType del;
        ScoreType jumpIns;
        ScoreType jumpDel;
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
                return jumpDel;
            case AlignState::JUMPINS:
                return jumpIns;
            default:
                assert(false && "Unexpected Index Value");
                return 0;
            }
        }

    public:
        /// pack 3x5 bits into 2 bytes:
        code_t match : 3;
        code_t ins : 3;
        code_t del : 3;
        code_t jumpDel : 3;
        code_t jumpIns : 3;
    };

    static
    uint8_t
    max5(
        ScoreType& max,
        const ScoreType v0,
        const ScoreType v1,
        const ScoreType v2,
        const ScoreType v3,
        const ScoreType v4)
    {
        max=v0;
        uint8_t ptr=0;
        if (v1>max)
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
        if (v4>max)
        {
            max=v4;
            ptr=4;
        }
        return ptr;
    }

    // add the matrices here to reduce allocations over many alignment calls:
    typedef std::vector<ScoreVal> ScoreVec;
    mutable ScoreVec _score1;
    mutable ScoreVec _score2;

    typedef basic_matrix<PtrVal> PtrMat;
    mutable PtrMat _ptrMat;

    const ScoreType _largeIndelScore;
};


#include "alignment/GlobalLargeIndelAlignerImpl.hh"
