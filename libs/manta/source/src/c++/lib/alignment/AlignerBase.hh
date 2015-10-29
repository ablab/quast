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

#include "alignment/AlignmentScores.hh"
#include "blt_util/align_path.hh"

#include <cassert>


/// base class containing a few shared aligner functions
///
template <typename ScoreType>
struct AlignerBase
{
    AlignerBase(
        const AlignmentScores<ScoreType>& scores) :
        _scores(scores)
    {}

    /// read-only access to the aligner's scores:
    const AlignmentScores<ScoreType>&
    getScores() const
    {
        return _scores;
    }

    /// recover a path alignment score without aligning, requires SEQ_MATCH style CIGAR
    ///
    ScoreType
    getPathScore(
        const ALIGNPATH::path_t& apath,
        const bool isScoreOffEdge = true) const
    {
        using namespace ALIGNPATH;

        ScoreType val(0);

        // note that the intent of this function was to replicate the underling aligner and thus
        // not penalize insert-delete transitions, however as written the open score is added twice for
        // such an event. This turns out to perform much better for the performance of this tool as
        // a variant 'arm' validator. so the unintended code is staying in place for now.
        //
        /// TODO: reevaluate policy for insertion-deletion state transition score

        for (const path_segment& ps : apath)
        {
            bool isIndel(false); // placement of isIndel inside of this loop is the 'bug'
            switch (ps.type)
            {
            case MATCH:
                assert(false && "Unexpected MATCH segment"); // if MATCH segments exist, then you're using the wrong type of CIGAR for this function
                break;
            case SEQ_MATCH:
                val += (_scores.match * ps.length);
                isIndel = false;
                break;
            case SEQ_MISMATCH:
                val += (_scores.mismatch * ps.length);
                isIndel = false;
                break;
            case INSERT:
            case DELETE:
                if (! isIndel) val += _scores.open;
                val += (_scores.extend * ps.length);
                isIndel = true;
                break;
            case SOFT_CLIP:
                if (isScoreOffEdge) val += (_scores.offEdge * ps.length);
                isIndel = false;
                break;
            default:
                break;
            }
        }
        return val;
    }

    /// recover the maximum partial path alignment score (going left->right) without aligning, requires SEQ_MATCH style CIGAR
    ///
    ScoreType
    getMaxPathScore(
        const ALIGNPATH::path_t& apath,
        unsigned& maxReadOffset,
        unsigned& maxRefOffset,
        const bool isScoreOffEdge = true) const
    {
        using namespace ALIGNPATH;

        ScoreType val(0);
        unsigned readOffset(0);
        unsigned refOffset(0);

        ScoreType maxVal(0);
        maxReadOffset=0;
        maxRefOffset=0;

        for (const path_segment& ps : apath)
        {
            bool isIndel(false); // unintended 'bug' with positive results, see TODO note above
            switch (ps.type)
            {
            case MATCH:
                assert(false && "Unexpected MATCH segment"); // if MATCH segments exist, then you're using the wrong type of CIGAR for this function
                break;
            case SEQ_MATCH:
                val += (_scores.match * ps.length);
                readOffset += ps.length;
                refOffset += ps.length;
                isIndel = false;
                break;
            case SEQ_MISMATCH:
                val += (_scores.mismatch * ps.length);
                readOffset += ps.length;
                refOffset += ps.length;
                isIndel = false;
                break;
            case INSERT:
                if (! isIndel) val += _scores.open;
                val += (_scores.extend * ps.length);
                readOffset += ps.length;
                isIndel = true;
                break;
            case DELETE:
                if (! isIndel) val += _scores.open;
                val += (_scores.extend * ps.length);
                refOffset += ps.length;
                isIndel = true;
                break;
            case SOFT_CLIP:
                if (isScoreOffEdge) val += (_scores.offEdge * ps.length);
                readOffset += ps.length;
                isIndel = false;
                break;
            default:
                break;
            }

            if (val>maxVal)
            {
                maxVal = val;
                maxReadOffset = readOffset;
                maxRefOffset = refOffset;
            }
        }
        return maxVal;
    }

protected:

    static
    uint8_t
    max3(
        ScoreType& max,
        const ScoreType v0,
        const ScoreType v1,
        const ScoreType v2)
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
        return ptr;
    }

    const AlignmentScores<ScoreType> _scores;
};
