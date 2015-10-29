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

#include "blt_util/qscore_snp.hh"
#include "blt_util/qscore.hh"
#include "blt_util/math_util.hh"



qscore_snp::
qscore_snp(
    const double snp_prob)
{
    static const int MAX_QSCORE(qphred_cache::MAX_QSCORE);

    const double comp_snp3(1.-(snp_prob/3.));

    for (int i(0); i<=MAX_QSCORE; ++i)
    {
        const double qerr(phred_to_error_prob(static_cast<double>(i)));
        _q2p[i] = (qerr * comp_snp3) + ((1-qerr) * snp_prob);
        _q2lncompe[i] = log1p_switch(-_q2p[i]);
        _q2lne[i] = std::log(_q2p[i]);
    }
}

