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

#include "blt_util/qscore_cache.hh"


/// this object provides a variation on regular qscores by incorporating SNP probability
///
/// SNV probability limits the error if we have to explain the difference
/// between a read and the reference, however it should not be used in cases where the
/// biological variation has already been accounted for in the comparison.
///
struct qscore_snp
{
    qscore_snp(const double snp_prob);

    double
    qphred_to_error_prob(const int qscore) const
    {
        qphred_cache::qscore_check(qscore, "basecall quality");
        return _q2p[qscore];
    }

    double
    qphred_to_ln_comp_error_prob(const int qscore) const
    {
        qphred_cache::qscore_check(qscore, "basecall quality");
        return _q2lncompe[qscore];
    }

    double
    qphred_to_ln_error_prob(const int qscore) const
    {
        qphred_cache::qscore_check(qscore, "basecall quality");
        return _q2lne[qscore];
    }

private:
    double _q2p[qphred_cache::MAX_QSCORE+1];
    double _q2lncompe[qphred_cache::MAX_QSCORE+1];
    double _q2lne[qphred_cache::MAX_QSCORE+1];
};
