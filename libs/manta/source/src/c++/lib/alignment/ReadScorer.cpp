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
/// \author Ole Schulz-Trieglaff
///

#include "ReadScorer.hh"
#include "blt_util/math_util.hh"

#include <cassert>

//#define DEBUG_RS


#ifdef DEBUG_RS
#include "blt_util/log.hh"
#include <iostream>
#endif



ReadScorer::
ReadScorer()
{
#ifdef DEBUG_RS
    log_os << "Filling logpcorrectratio table" << std::endl;
#endif
    // below we take log(1+(-i)) so set a dummy value for i = 0
    assert(MAX_QSCORE>=0);
    _logpcorrectratio[0] = 0.99;
    for (unsigned i(1); i<=MAX_QSCORE; ++i)
    {
        const double eprob(qphred_to_error_prob(i));
#ifdef DEBUG_RS
        log_os << "i=" << i << " " << eprob << " " << (log1p_switch(-eprob) - std::log(eprob/3.)) << std::endl;
#endif
        _logpcorrectratio[i] = log1p_switch(-eprob) - std::log(eprob/3);
    }
}



double
ReadScorer::
getSemiAlignedMetricImpl(
    const unsigned readLen,
    const ALIGNPATH::path_t& apath,
    const uint8_t* qual) const
{
    using namespace ALIGNPATH;

    unsigned posInRead = 0;
    double score(0.);

    for (const path_segment& ps : apath)
    {
        assert((ps.type != MATCH) && "Incorrect CIGAR type, matches must be converted to SEQ_MATCH/SEQ_MISMATCH");

        if ((ps.type==SOFT_CLIP) || (ps.type==SEQ_MISMATCH))
        {
            assert((posInRead+ps.length) <= readLen);
            for (unsigned j(0); j<ps.length; ++j)
            {
                score += getLogRatio(qual[posInRead+j]);
            }
        }
        if (is_segment_type_read_length(ps.type)) posInRead += ps.length;
    }

    return score;
}
