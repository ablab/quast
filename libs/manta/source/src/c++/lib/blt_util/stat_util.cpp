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

#include "blt_util/stat_util.hh"

#include "boost/math/distributions/chi_squared.hpp"



bool
is_chi_sqr_reject(
    const double xsq,
    const unsigned df,
    const double alpha)
{
    assert(xsq>=0);
    assert(df>0);

    boost::math::chi_squared dist(df);
    return ((1.-boost::math::cdf(dist,xsq)) < alpha);

#if 0
    // alternate implementation (is one faster?):
    const double xsq_crit_val(boost::math::quantile(dist,1.-alpha));
    return xsq>xsq_crit_val;
#endif
}



bool
is_lrt_reject_null(
    const double null_loghood,
    const double alt_loghood,
    const unsigned df,
    const double alpha)
{
    if (df == 0) return false;
    if (null_loghood>alt_loghood) return false;

    const double log_lrt(-2.*(null_loghood-alt_loghood));

    return is_chi_sqr_reject(log_lrt,df,alpha);
}
