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

#ifdef DEBUG_ALN
#include "blt_util/log.hh"
#include <iostream>
#endif



template <typename ScoreType>
template <typename SymIter>
void
GlobalJumpAligner<ScoreType>::
align(
    const SymIter queryBegin, const SymIter queryEnd,
    const SymIter ref1Begin, const SymIter ref1End,
    const SymIter ref2Begin, const SymIter ref2End,
    JumpAlignmentResult<ScoreType>& result) const
{
    result.clear();

    const AlignmentScores<ScoreType>& scores(this->getScores());

    const size_t querySize(std::distance(queryBegin, queryEnd));
    const size_t ref1Size(std::distance(ref1Begin, ref1End));
    const size_t ref2Size(std::distance(ref2Begin, ref2End));

    assert(0 != querySize);
    assert(0 != ref1Size);
    assert(0 != ref2Size);

    _score1.resize(querySize+1);
    _score2.resize(querySize+1);
    _ptrMat1.resize(querySize+1, ref1Size+1);
    _ptrMat2.resize(querySize+1, ref2Size+1);

    ScoreVec* thisSV(&_score1);
    ScoreVec* prevSV(&_score2);

    static const ScoreType badVal(-10000);

    // global alignment of query -- disallow start from insertion or deletion
    // state, query can 'fall-off' the end of a short reference, in which case it will
    // be soft-clipped and each base off the end will count as an 'offEdge' state:
    for (unsigned queryIndex(0); queryIndex<=querySize; queryIndex++)
    {
        ScoreVal& val((*thisSV)[queryIndex]);
        val.match = queryIndex * scores.offEdge;
        val.del = badVal;
        val.ins = badVal;
        val.jump = badVal;
    }

#ifdef DEBUG_ALN_MATRIX
    // store full matrix of scores to print out later, don't turn this debug option on for large references!
    std::vector<ScoreVec> storeScores;

    storeScores.push_back(*thisSV);
#endif


    BackTrace<ScoreType> btrace;

    {
        unsigned ref1Index(0);
        for (SymIter ref1Iter(ref1Begin); ref1Iter != ref1End; ++ref1Iter, ++ref1Index)
        {
            std::swap(thisSV,prevSV);

            {
                // disallow start from the insert or delete state:
                ScoreVal& val((*thisSV)[0]);
                val.match = 0;
                val.del = badVal;
                val.ins = badVal;
                val.jump = badVal;
            }

            unsigned queryIndex(0);
            for (SymIter queryIter(queryBegin); queryIter != queryEnd; ++queryIter, ++queryIndex)
            {
                // update match
                ScoreVal& headScore((*thisSV)[queryIndex+1]);
                PtrVal& headPtr(_ptrMat1.val(queryIndex+1,ref1Index+1));
                {
                    const ScoreVal& sval((*prevSV)[queryIndex]);
                    headPtr.match = this->max3(
                                        headScore.match,
                                        sval.match,
                                        sval.del,
                                        sval.ins);

                    headScore.match += ((*queryIter==*ref1Iter) ? scores.match : scores.mismatch);
                }

                // update delete
                {
                    const ScoreVal& sval((*prevSV)[queryIndex+1]);
                    headPtr.del = this->max3(
                                      headScore.del,
                                      sval.match + scores.open,
                                      sval.del,
                                      sval.ins);

                    headScore.del += scores.extend;
                    if (0==queryIndex) headScore.del = badVal;
                }

                // update insert
                {
                    const ScoreVal& sval((*thisSV)[queryIndex]);
                    headPtr.ins = this->max3(
                                      headScore.ins,
                                      sval.match + scores.open,
                                      badVal,
                                      sval.ins);

                    headScore.ins += scores.extend;
                    if (0==queryIndex) headScore.ins = badVal;
                }

                // update jump
                {
                    const ScoreVal& sval((*prevSV)[queryIndex+1]);
                    headPtr.jump = this->max4(
                                       headScore.jump,
                                       headScore.match + this->getJumpScore(),
                                       badVal,
                                       headScore.ins + this->getJumpScore(),
                                       sval.jump);
                }

#ifdef DEBUG_ALN
                log_os << "queryIdx refIdx ref1Idx: " << queryIndex+1 << " " << ref1Index+1 << " " << ref1Index+1 << "\n";
                log_os << "MIDJ: " << headScore.match << ":" << headScore.ins << ":" << headScore.del << ":" << headScore.jump << "/"
                       << static_cast<int>(headPtr.match) << static_cast<int>(headPtr.ins) << static_cast<int>(headPtr.del) << static_cast<int>(headPtr.jump) << "\n";
                log_os << "QuerySymbol:" << *queryIter << " RefSymbol:" << *ref1Iter << "\n";
#endif
            }
#ifdef DEBUG_ALN
            log_os << "\n";
#endif

#ifdef DEBUG_ALN_MATRIX
            storeScores.push_back(*thisSV);
#endif

            // get backtrace info:
            {
                const ScoreVal& sval((*thisSV)[querySize]);
                updateBacktrace(sval.match, ref1Index+1, querySize, btrace);
            }
        }
    }

    // in the backtrace start search, also allow for the case where the query falls-off the end of the reference:
    for (unsigned queryIndex(0); queryIndex<querySize; queryIndex++)
    {
        const ScoreVal& sval((*thisSV)[queryIndex]);
        const ScoreType thisMax(sval.match + (querySize-queryIndex) * scores.offEdge);
        updateBacktrace(thisMax, ref1Size, queryIndex, btrace);
    }


    {
        for (unsigned queryIndex(0); queryIndex<=querySize; queryIndex++)
        {
            ScoreVal& val((*thisSV)[queryIndex]);
            val.match = queryIndex * scores.offEdge;
            val.del = badVal;
            val.ins = badVal;
            //val.jump = badVal; // preserve jump setting from last iteration of ref1
        }

#ifdef DEBUG_ALN_MATRIX
        storeScores.push_back(*thisSV);
#endif

        unsigned ref2Index(0);
        for (SymIter ref2Iter(ref2Begin); ref2Iter != ref2End; ++ref2Iter, ++ref2Index)
        {
            std::swap(thisSV,prevSV);

            {
                // disallow start from the insert or delete state:
                ScoreVal& val((*thisSV)[0]);
                val.match = 0;
                val.del = badVal;
                val.ins = badVal;
                val.jump = badVal;
            }

            unsigned queryIndex(0);
            for (SymIter queryIter(queryBegin); queryIter != queryEnd; ++queryIter, ++queryIndex)
            {
                // update match
                ScoreVal& headScore((*thisSV)[queryIndex+1]);
                PtrVal& headPtr(_ptrMat2.val(queryIndex+1,ref2Index+1));
                {
                    const ScoreVal& sval((*prevSV)[queryIndex]);
                    headPtr.match = this->max4(
                                        headScore.match,
                                        sval.match,
                                        sval.del,
                                        sval.ins,
                                        sval.jump);

                    headScore.match += ((*queryIter==*ref2Iter) ? scores.match : scores.mismatch);
                }

                // update delete
                {
                    const ScoreVal& sval((*prevSV)[queryIndex+1]);
                    headPtr.del = this->max3(
                                      headScore.del,
                                      sval.match + scores.open,
                                      sval.del,
                                      sval.ins);

                    headScore.del += scores.extend;
                }

                // update insert
                {
                    const ScoreVal& sval((*thisSV)[queryIndex]);
                    headPtr.ins = this->max4(
                                      headScore.ins,
                                      sval.match + scores.open,
                                      badVal,
                                      sval.ins,
                                      sval.jump); // jump->ins moves get a pass on the gap-open penalty, to support breakend insertions

                    headScore.ins += scores.extend;
                }

                // update jump
                {
                    const ScoreVal& sval((*prevSV)[queryIndex+1]);
                    headPtr.jump = AlignState::JUMP;
                    headScore.jump = sval.jump;
                }

#ifdef DEBUG_ALN
                log_os << "queryIdx refIdx ref2Idx: " << queryIndex+1 << " " << ref1Size+ref2Index+1 << " " << ref2Index+1 << "\n";
                log_os << "MIDJ: " << headScore.match << ":" << headScore.ins << ":" << headScore.del << ":" << headScore.jump << "/"
                       << static_cast<int>(headPtr.match) << static_cast<int>(headPtr.ins) << static_cast<int>(headPtr.del) << static_cast<int>(headPtr.jump) << "\n";
                log_os << "QuerySymbol:" << *queryIter << " RefSymbol:" << *ref2Iter << "\n";
#endif
            }
#ifdef DEBUG_ALN
            log_os << "\n";
#endif

#ifdef DEBUG_ALN_MATRIX
            storeScores.push_back(*thisSV);
#endif

            // get backtrace start info:
            {
                const ScoreVal& sval((*thisSV)[querySize]);
                updateBacktrace(sval.match, ref1Size+ref2Index+1, querySize, btrace);
            }
        }

    }

    // in the backtrace start search, also allow for the case where the query falls-off the end of the reference:
    for (unsigned queryIndex(0); queryIndex<querySize; queryIndex++)
    {
        const ScoreVal& sval((*thisSV)[queryIndex]);
        const ScoreType thisMax(sval.match + (querySize-queryIndex) * scores.offEdge);
        updateBacktrace(thisMax, ref1Size+ref2Size, queryIndex, btrace);
    }

#ifdef DEBUG_ALN
    log_os << "bt-start queryIndex: " << btrace.queryBegin << " refIndex: " << btrace.refBegin << " state: " << AlignState::label(btrace.state) << " maxScore: " << btrace.max << "\n";
#endif

#ifdef DEBUG_ALN_MATRIX
    std::vector<AlignState::index_t> dumpStates {AlignState::MATCH, AlignState::DELETE, AlignState::INSERT, AlignState::JUMP};
    this->dumpTables(queryBegin, queryEnd,
                     ref1Begin, ref1End,
                     ref2Begin, ref2End,
                     querySize,
                     _ptrMat1, _ptrMat2,
                     dumpStates,storeScores);
#endif

    this->backTraceAlignment(
        queryBegin, queryEnd,
        ref1Begin, ref1End,
        ref2Begin, ref2End,
        querySize, ref1Size, ref2Size,
        _ptrMat1, _ptrMat2, btrace, result);
}

