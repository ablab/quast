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

/// \author Chris Saunders
/// \author Mitch Bekritsky
///

#include "blt_util/binomial_test.hh"
#include "blt_util/stat_util.hh"

#include <boost/math/distributions/binomial.hpp>
#include <boost/math/distributions/complement.hpp>

using boost::math::binomial;
using boost::math::cdf;
using boost::math::complement;

#include <algorithm>



double
get_binomial_twosided_exact_pval(
    const double p,
    const unsigned n_success,
    const unsigned n_trials)
{
    assert((p>0.) && (p<1.));
    assert(n_success <= n_trials);

    const unsigned n_failure(n_trials-n_success);
    const double obs_p((double)n_success/(double)n_trials);

    double exact_prob;
    if (obs_p <= p)
    {
        exact_prob=cdf(binomial(n_trials,p),n_success);
    }
    else
    {
        exact_prob=cdf(binomial(n_trials,1.-p),n_failure);
    }

    return (2.*exact_prob);
}



bool
is_reject_binomial_twosided_exact(
    const double alpha,
    const double p,
    const unsigned n_success,
    const unsigned n_trials)
{
    return (get_binomial_twosided_exact_pval(p,n_success,n_trials)<alpha);
}



bool
is_reject_binomial_twosided_chi_sqr(
    const double alpha,
    const double p,
    const unsigned n_success,
    const unsigned n_trials)
{
    assert((p>0.) && (p<1.));
    assert(n_success <= n_trials);

    const unsigned n_failure(n_trials-n_success);
    const double e_success(p*n_trials);
    const double e_failure(((double)n_trials)-e_success);

    const double d_success(n_success-e_success);
    const double d_failure(n_failure-e_failure);

    const double xsq((d_success*d_success)/e_success+(d_failure*d_failure)/e_failure);

    return is_chi_sqr_reject(xsq,1,alpha);
}



bool
is_reject_binomial_twosided(
    const double alpha,
    const double p,
    const unsigned n_success,
    const unsigned n_trials)
{
    static const unsigned exact_test_threshold(250);

    if (n_trials > exact_test_threshold)
    {
        return is_reject_binomial_twosided_chi_sqr(alpha,p,n_success,n_trials);
    }
    else
    {
        return is_reject_binomial_twosided_exact(alpha,p,n_success,n_trials);
    }
}



double
get_binomial_gte_n_success_exact_pval(
    const double p,
    const unsigned n_success,
    const unsigned n_trials)
{
    //although binomial probabilities of
    // 0 or 1 are possible, they don't have much meaning
    assert((p >= 0.) && (p <= 1.));
    assert(n_success <= n_trials);
    if (n_success==0) return 1;

    return cdf(complement(binomial(n_trials, p), n_success - 1));
}



bool
is_reject_binomial_gte_n_success_exact(
    const double alpha,
    const double p,
    const unsigned n_success,
    const unsigned n_trials)
{
    assert(alpha >= 0);

    const double observed_pval = get_binomial_gte_n_success_exact_pval(p, n_success, n_trials);

    return (observed_pval <= alpha);
}
