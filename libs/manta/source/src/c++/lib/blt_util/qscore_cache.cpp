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

#include "blt_util/blt_exception.hh"
#include "blt_util/log.hh"
#include "blt_util/math_util.hh"
#include "blt_util/qscore.hh"
#include "blt_util/qscore_cache.hh"

#include <cstdlib>
#include <iostream>
#include <sstream>



qphred_cache::
qphred_cache()
{
    static const double q2lnp(-std::log(10.)/10.);

    for (int i(0); i<=MAX_QSCORE; ++i)
    {
        q2p[i] = phred_to_error_prob(static_cast<double>(i));
        q2lncompe[i] = log1p_switch(-q2p[i]);
        q2lne[i] = static_cast<double>(i)*q2lnp;
        for (int j(0); j<=MAX_MAP; ++j)
        {
            mappedq[j][i] = error_prob_to_qphred(phred_to_mapped_error_prob(i,j));
        }
    }
}



void
qphred_cache::
invalid_qscore_error(const int qscore,
                     const char* label)
{
    std::stringstream oss;
    oss << "ERROR: Attempting to lookup invalid " << label << " score: " << qscore;
    throw blt_exception(oss.str().c_str());
}


void
qphred_cache::
high_qscore_error(const int qscore,
                  const char* label)
{
    std::stringstream oss;
    oss << "ERROR: Attempting to lookup " << label << " score " << qscore << " which exceeds the maximum cached " << label << " score of " <<  MAX_QSCORE;
    throw blt_exception(oss.str().c_str());
}

