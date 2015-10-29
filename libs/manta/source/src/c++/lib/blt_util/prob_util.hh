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

#include <cassert>
#include <cmath>

#include <iterator>


/// find more accurate complement of posterior_probability:
///
///
template <typename It>
typename std::iterator_traits<It>::value_type
prob_comp(It begin,
          const It end,
          const unsigned cgt)
{
    typedef typename std::iterator_traits<It>::value_type float_type;

    unsigned i(0);
    float_type val(0.);
    for (; begin!=end; ++begin,++i)
    {
        if (i == cgt) continue;
        val = val + *begin;
    }
    return val;
}


/// given a log() transformed distribution, transform it to a standard
/// distro, set max_idx to the index of the most probable component
template <typename It>
void
normalize_ln_distro(const It pbegin,
                    const It pend,
                    unsigned& max_idx)
{
    typedef typename std::iterator_traits<It>::value_type float_type;

    // scale and exp pprob values:
    max_idx=0;
    if (pbegin==pend) return;
    float_type max(*pbegin);
    unsigned i(1);
    for (It p(pbegin+1); p!=pend; ++p,++i)
    {
        if (*p > max)
        {
            max = *p;
            max_idx = i;
        }
    }

    float_type sum(0.);
    for (It p(pbegin); p!=pend; ++p)
    {
        *p = std::exp(*p-max);
        sum += *p;
    }

    // normalize:
    sum = 1./sum;
    for (It p(pbegin); p!=pend; ++p)
    {
        *p *= sum;
    }
}


// optimized version of probability normalization
//
// values significantly less than opt-max will be treated as zero probability
//
// opt-max is found within the subset of the distribution where the predicate
// iterator is true
//
template <typename It,typename It2>
void
opt_normalize_ln_distro(const It pbegin,
                        const It pend,
                        const It2 pred_begin,
                        unsigned& max_idx)
{
    typedef typename std::iterator_traits<It>::value_type float_type;

    max_idx=0;
    if (pbegin==pend) return;

    bool is_max(false), is_opt_max(false);
    float_type max(0), opt_max(0);

    unsigned i(0);
    It2 pred(pred_begin);
    for (It p(pbegin); p!=pend; ++p,++pred,++i)
    {
        if ((! is_max) || (*p > max))
        {
            max = *p;
            max_idx = i;
            is_max=true;
        }
        if (((! is_opt_max) || (*p > max)) && *pred)
        {
            opt_max = *p;
            is_opt_max=true;
        }
    }

    assert(is_opt_max);

    static const float_type norm_thresh(20);
    static const float_type opt_thresh(5);

    float_type sum(0.);
    pred=(pred_begin);
    for (It p(pbegin); p!=pend; ++p,++pred)
    {
        float_type mdiff(max-*p);
        const bool is_mdiff_skip(mdiff>norm_thresh);
        if (is_mdiff_skip)
        {
            if (! *pred)
            {
                *p=0;
                continue;
            }
            float_type optdiff(opt_max-*p);
            if (optdiff>opt_thresh)
            {
                *p=0;
                continue;
            }
        }
        *p = std::exp(-mdiff);
        sum += *p;
    }

    // normalize:
    sum = 1./sum;
    for (It p(pbegin); p!=pend; ++p)
    {
        *p *= sum;
    }
}


void
check_ln_distro_invalid_value(const char* label,
                              const double val,
                              const unsigned n);


void
check_ln_distro_invalid_sum(const char* label,
                            const double sum);


template <typename It>
double
check_ln_distro(It i,
                const It i_end,
                const char* label,
                const double tol = 0.00001,
                const double target = 1)
{
    unsigned n(1);
    double sum(0);
    for (; i!=i_end; ++i,++n)
    {
        const double val(std::exp(*i));
        if ((val<0.) || (val>1.))
        {
            check_ln_distro_invalid_value(label,val,n);
        }
        sum += val;
    }
    if (std::abs(sum-target) > tol)
    {
        check_ln_distro_invalid_sum(label,sum);
    }
    return sum;
}
