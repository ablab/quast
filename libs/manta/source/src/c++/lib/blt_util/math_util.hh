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

#include "boost/math/special_functions/log1p.hpp"

#include <cmath>

#include <algorithm>


/// returns log(1+x), switches to special libc function when abs(x) is small
///
template <typename FloatType>
FloatType
log1p_switch(const FloatType x)
{
    // better number??
    static const FloatType smallx_thresh(0.01);

    if (std::abs(x)<smallx_thresh)
    {
        return boost::math::log1p(x);
    }
    else
    {
        return std::log(1+x);
    }
}


/// returns equiv of log(exp(x1)+exp(x2))
///
template <typename FloatType>
FloatType
log_sum(FloatType x1, FloatType x2)
{
    if (x1<x2) std::swap(x1,x2);
    return x1 + log1p_switch(std::exp(x2-x1));
}


// helper for median() below
template <typename Iter>
typename std::iterator_traits<Iter>::value_type
_ne_median(
    Iter begin,
    Iter end)
{
    assert(begin != end);
    const auto size(std::distance(begin,end));
    std::nth_element(begin,begin+size/2, end);
    return *(begin+size/2);
}

// helper for median() below
template <typename Iter>
typename std::iterator_traits<Iter>::value_type
_ps_median(
    Iter begin,
    Iter end)
{
    assert(begin != end);
    const auto size(std::distance(begin,end));
    std::partial_sort(begin,begin+size/2+1, end);
    return *(begin+size/2);
}


/// returns median, partially reorders elements in specified range
///
template <typename Iter>
typename std::iterator_traits<Iter>::value_type
median(
    Iter begin,
    Iter end)
{
    // Dispatch median call so as to avoid common broken libstdc++ impl

#ifndef BROKEN_NTH_ELEMENT
    // this is the preferred way to do it, it is optionally disabled because of common gcc bug:
    // https://gcc.gnu.org/bugzilla/show_bug.cgi?id=58800
    return _ne_median(begin,end);
#else
    return _ps_median(begin,end);
#endif
}
