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

//
// \author Chris Saunders
//

#include <cassert>

#include <iostream>

#if defined(DEBUG_ALN) || defined(DEBUG_ALN_MATRIX)
#include "blt_util/log.hh"
#endif

#ifdef DEBUG_ALN_MATRIX
#include <iomanip>
#endif



#ifdef DEBUG_ALN_MATRIX


template <typename ScoreType>
template <typename SymIter, typename MatrixType, typename ScoreValType>
void
JumpAlignerBase<ScoreType>::
dumpSingleRefTable(
    const SymIter refBegin, const SymIter refEnd,
    const size_t querySize,
    const MatrixType& ptrMatrix,
    const std::vector<std::vector<ScoreValType>>& storeScores,
    const char refSym,
    const AlignState::index_t sIndex,
    unsigned& storeIndex) const
{
    auto printVal = [](
                        const ScoreType& val,
                        const char fromSym,
                        std::ostream& os)
    {
        if (val<-900)
        {
            os << " XX";
        }
        else
        {
            os << std::setfill(' ') << std::setw(3) << val;
        }
        os << fromSym;
    };

    {
        log_os << "# N ";
        for (unsigned queryIndex(0); queryIndex<=querySize; ++queryIndex)
        {
            const auto& val(storeScores[storeIndex][queryIndex].getScore(sIndex));
            static const char fromSym('.');
            printVal(val,fromSym, log_os);
        }
        log_os << "\n";
    }
    unsigned refIndex(0);
    for (SymIter refIter(refBegin); refIter != refEnd; ++refIter, ++refIndex)
    {
        log_os << refSym << " " << *refIter << " ";
        storeIndex++;
        for (unsigned queryIndex(0); queryIndex<=querySize; ++queryIndex)
        {
            const auto& val(storeScores[storeIndex][queryIndex].getScore(sIndex));
            const char fromSym(queryIndex==0 ? '.' : AlignState::symbol(ptrMatrix.val(queryIndex,refIndex+1).getStatePtr(sIndex)));
            printVal(val,fromSym, log_os);
        }
        log_os << "\n";
    }
}



template <typename ScoreType>
template <typename SymIter, typename MatrixType, typename ScoreValType>
void
JumpAlignerBase<ScoreType>::
dumpTables(
    const SymIter queryBegin, const SymIter queryEnd,
    const SymIter ref1Begin, const SymIter ref1End,
    const SymIter ref2Begin, const SymIter ref2End,
    const size_t querySize,
    const MatrixType& ptrMatrix1, const MatrixType& ptrMatrix2,
    const std::vector<AlignState::index_t>& dumpStates,
    const std::vector<std::vector<ScoreValType>>& storeScores) const
{
    const unsigned stateSize(dumpStates.size());
    for (unsigned stateIndex(0); stateIndex<stateSize; ++stateIndex)
    {
        const AlignState::index_t sIndex(dumpStates[stateIndex]);
        log_os << "******** Dumping matrix for state: " << AlignState::label(sIndex) << " ********\n";
        {
            unsigned queryIndex(0);
            log_os << "REF    -";
            for (SymIter queryIter(queryBegin); queryIter != queryEnd; ++queryIter, ++queryIndex)
            {
                log_os << "   " << *queryIter;
            }
            log_os << "\n";
        }

        // dump state before refIndex 0
        unsigned storeIndex(0);
        dumpSingleRefTable(ref1Begin,ref1End,querySize,ptrMatrix1,storeScores, '1', sIndex, storeIndex);

        storeIndex++;
        dumpSingleRefTable(ref2Begin,ref2End,querySize,ptrMatrix2,storeScores, '2', sIndex, storeIndex);
    }
}
#endif



template <typename ScoreType>
std::ostream&
operator<<(std::ostream& os, JumpAlignmentResult<ScoreType>& alignment)
{
    os << "JumpAligner: score: " << alignment.score << "\n"
       << "\talign1: " << alignment.align1 << "\n"
       << "\talign2: " << alignment.align2 << "\n"
       << "\tjumpInsertSize " << alignment.jumpInsertSize << "\n"
       << "\tjumpRange " << alignment.jumpRange << "\n";
    return os;
}



// traceback:
template <typename ScoreType>
template <typename SymIter, typename MatrixType>
void
JumpAlignerBase<ScoreType>::
backTraceAlignment(
    const SymIter queryBegin, const SymIter queryEnd,
    const SymIter ref1Begin, const SymIter ref1End,
    const SymIter ref2Begin, const SymIter ref2End,
    const size_t querySize, const size_t ref1Size, const size_t ref2Size,
    const MatrixType& ptrMatrix1, const MatrixType& ptrMatrix2,
    const BackTrace<ScoreType>& btraceInput,
    JumpAlignmentResult<ScoreType>& result) const
{
    BackTrace<ScoreType> btrace(btraceInput);

    assert(btrace.isInit);
    assert(btrace.refBegin <= ref1Size+ref2Size);
    assert(btrace.queryBegin <= querySize);

#ifdef DEBUG_ALN
    log_os << "qSize: " << querySize << " ref1Size: " << ref1Size << " ref2Size: " << ref2Size << "\n";
    log_os << "bt-start max: " << btrace.max << " refBegin: " << btrace.refBegin << " qBegin: " << btrace.queryBegin << " state: " << AlignState::label(btrace.state) << "\n";
#endif

    result.score = btrace.max;

    // traceback:
    ALIGNPATH::path_t& apath1(result.align1.apath);
    ALIGNPATH::path_t& apath2(result.align2.apath);
    ALIGNPATH::path_segment ps;

    // add any trailing soft-clip if we go off the end of the reference:
    if (btrace.queryBegin < querySize)
    {
        ps.type = ALIGNPATH::SOFT_CLIP;
        ps.length = (querySize-btrace.queryBegin);
    }

    bool isRef2End(false);

    while ((btrace.queryBegin>0) && (btrace.refBegin>0))
    {
        if (isRef2End) break;
        const bool isRef1(btrace.refBegin<=ref1Size);
        ALIGNPATH::path_t& apath( isRef1 ? apath1 : apath2 );
        const unsigned refXBegin(btrace.refBegin - (isRef1 ? 0 : ref1Size));
        const MatrixType* ptrMatrixX(isRef1 ? &ptrMatrix1 : &ptrMatrix2 );
        const AlignState::index_t nextState(ptrMatrixX->val(btrace.queryBegin,refXBegin).getStatePtr(btrace.state));

#ifdef DEBUG_ALN
        log_os << "bt-iter queryIndex: " << btrace.queryBegin
               << " refIndex: " << btrace.refBegin
               << " state: " << AlignState::label(btrace.state)
               << " next: " << AlignState::label(nextState)
               << "\n";
        log_os << "\tisref1: " << isRef1 << " refXBegin: " << refXBegin << "\n";
#endif

        if      (btrace.state==AlignState::MATCH)
        {
            if ((!isRef1) && (refXBegin==1) && (nextState==AlignState::MATCH)) isRef2End=true;

            AlignerUtil::updatePath(apath,ps,ALIGNPATH::MATCH);
            btrace.queryBegin--;
            btrace.refBegin--;
        }
        else if (btrace.state==AlignState::DELETE)
        {
            AlignerUtil::updatePath(apath,ps,ALIGNPATH::DELETE);
            btrace.refBegin--;
        }
        else if (btrace.state==AlignState::SPLICE)
        {
            if ((!isRef1) && (refXBegin == 1) && (nextState == AlignState::SPLICE)) isRef2End = true;

            AlignerUtil::updatePath(apath,ps,ALIGNPATH::SKIP);
            btrace.refBegin--;
        }
        else if (btrace.state==AlignState::INSERT)
        {
            AlignerUtil::updatePath(apath,ps,ALIGNPATH::INSERT);
            btrace.queryBegin--;
        }
        else if (btrace.state==AlignState::JUMP)
        {
            if (ps.type != ALIGNPATH::NONE)
            {
                assert(btrace.refBegin>=ref1Size);
                result.align2.beginPos = btrace.refBegin-ref1Size;
                if (ps.type == ALIGNPATH::INSERT)
                {
                    result.jumpInsertSize += ps.length;
                    ps.type = ALIGNPATH::NONE;
                    ps.length = 0;
                }
                else
                {
                    AlignerUtil::updatePath(apath2,ps,ALIGNPATH::NONE);
                }
            }
            else
            {
                if (nextState == AlignState::JUMP) btrace.refBegin--;
            }
        }
        else
        {
            assert(false && "Unknown align state");
        }
        btrace.state=nextState;
        ps.length++;
    }

    const bool isRef1(btrace.refBegin<ref1Size);
    ALIGNPATH::path_t& apath( isRef1 ? apath1 : apath2 );

    if (ps.type != ALIGNPATH::NONE) apath.push_back(ps);

    // soft-clip beginning of read if we fall off the end of the reference
    if (btrace.queryBegin!=0)
    {
        ps.type = ALIGNPATH::SOFT_CLIP;
        ps.length = btrace.queryBegin;
        apath.push_back(ps);
    }

    if (isRef1)
    {
        result.align1.beginPos = btrace.refBegin;
    }
    else
    {
        result.align2.beginPos = btrace.refBegin-ref1Size;
    }

    std::reverse(apath1.begin(),apath1.end());
    std::reverse(apath2.begin(),apath2.end());

    // figure out jumpRange:
    if (result.align1.isAligned() && result.align2.isAligned())
    {
        // find the distance over which ref1 and ref2 are equal following the start of the breakpoint
        SymIter ref1JumpIter(ref1Begin + result.align1.beginPos + apath_ref_length(apath1));
        SymIter ref2JumpIter(ref2Begin + result.align2.beginPos);
        while (true)
        {
            if (ref1JumpIter == ref1End) break;
            if (ref2JumpIter == ref2End) break;
            if ((*ref1JumpIter) != (*ref2JumpIter)) break;

            result.jumpRange++;
            ref1JumpIter++;
            ref2JumpIter++;
        }
    }

    // if true, output final cigars using seq match '=' and mismatch 'X' symbols:
    static const bool isOutputSeqMatch(true);

    if (isOutputSeqMatch)
    {
        apath_add_seqmatch(queryBegin, queryEnd, (ref1Begin+result.align1.beginPos), ref1End, apath1);

        const unsigned queryOffset = apath_read_length(apath1) + result.jumpInsertSize;
        apath_add_seqmatch(queryBegin + queryOffset, queryEnd, (ref2Begin+result.align2.beginPos), ref2End, apath2);
    }
}
