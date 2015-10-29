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
/// \author Xiaoyu Chen
///

#include "SizeDistribution.hh"

#include "blt_util/log.hh"

#include "boost/foreach.hpp"

#include <cassert>
#include <cmath>
#include <iostream>

//#define DEBUG_RPS


static
void
populateCdfQuantiles(
    SizeDistribution::map_type& sizeMap,
    const unsigned totalCount,
    std::vector<int>& quantiles)
{
    const unsigned quantileNum(quantiles.size());
    const float pFactor(1/static_cast<float>(totalCount));

    unsigned fillBase(0);
    unsigned cumulativeCount(0);
    BOOST_REVERSE_FOREACH(SizeDistribution::map_type::value_type& val, sizeMap)
    {
        cumulativeCount += (val.second.count);
        assert(cumulativeCount <= totalCount);

        // update the hash map with cumulative prob value
        val.second.cprob = (cumulativeCount * pFactor);

        const unsigned fillNext = static_cast<unsigned>(rint(val.second.cprob * quantileNum));
        for (; fillBase < fillNext; fillBase++)
        {
            quantiles[fillBase] = val.first;
        }
    }
}




void
SizeDistribution::
calcStats() const
{
#ifdef DEBUG_RPS
    log_os << "Calculating stats...\n"
           << "numOfSized=" << _sizeMap.size() << "\n";
#endif
    _isStatsComputed=true;
    if (_sizeMap.empty()) return;

    populateCdfQuantiles(_sizeMap, _totalCount, _quantiles);
}



int
SizeDistribution::
quantile(const float prob) const
{
    assert((prob >= 0.) && (prob <= 1.));

    static const int maxBin(_quantileNum - 1);
    if (! _isStatsComputed) calcStats();

    int bin(static_cast<int>(ceil(prob * _quantileNum) - 1));
    if (bin < 0) bin=0;
    if (bin > maxBin) bin=maxBin;
    return _quantiles[bin];
}



float
SizeDistribution::
cdf(const int size) const
{
    if (! _isStatsComputed) calcStats();

    // map uses greater<int> for comp, so lower bound is "first element not greater than" size, from a list sorted high->low
    const map_type::const_iterator sizeIter(_sizeMap.lower_bound(size));
    if (sizeIter == _sizeMap.end()) return 0;
    return sizeIter->second.cprob;
}



float
SizeDistribution::
pdf(const int size) const
{
    if (! _isStatsComputed) calcStats();

    static const unsigned targetSampleSize(5);

    unsigned count(0);
    int minSize(size);
    int maxSize(size);

    bool isMinBound(false);
    bool isMaxBound(false);

    /// scheme: get the five closest (in bin space) samples and sum them divided by the range required to find them

    // map uses greater<int> for comp, so lower bound is "first element not greater than" size, from a list sorted high->low
    map_type::const_iterator lowIter(_sizeMap.lower_bound(size));

    if (lowIter == _sizeMap.end())
    {
        isMinBound=true;
    }

    map_type::const_iterator highIter(lowIter);

    if (highIter == _sizeMap.begin())
    {
        isMaxBound=true;
    }
    else
    {
        --highIter;
    }


    for (unsigned sampleIndex(0); sampleIndex<targetSampleSize; ++sampleIndex)
    {
        // determine whether fwd or rev pointer is closer to size:
        if (isMinBound && isMaxBound) break;

        bool isChooseLow(true);
        if (isMinBound)
        {
            isChooseLow=false;
        }
        else if (isMaxBound)
        {
            isChooseLow=true;
        }
        else
        {
            isChooseLow=(std::abs(lowIter->first-size) <= std::abs(highIter->first-size));
        }

        if (isChooseLow)
        {
            minSize = lowIter->first;
            count += lowIter->second.count;
            ++lowIter;

            if (lowIter == _sizeMap.end()) isMinBound=true;
        }
        else
        {
            maxSize = highIter->first;
            count += highIter->second.count;
            if (highIter == _sizeMap.begin())
            {
                isMaxBound=true;
            }
            else
            {
                --highIter;
            }
        }
    }

    assert(maxSize >= minSize);

    return count/(static_cast<float>(_totalCount)*static_cast<float>(1+maxSize-minSize));
}



void
SizeDistribution::
filterObservationsOverQuantile(const float prob)
{
    const int maxSize(quantile(prob));
    const map_type::iterator sizeBegin(_sizeMap.begin());
    map_type::iterator sizeEnd(_sizeMap.lower_bound(maxSize));

    for (map_type::iterator sizeIter(sizeBegin); sizeIter != sizeEnd; ++sizeIter)
    {
        if (sizeIter->first <= maxSize)
        {
            sizeEnd = sizeIter;
            break;
        }
        _totalCount -= sizeIter->second.count;
    }
    _sizeMap.erase(sizeBegin,sizeEnd);

    _isStatsComputed=false;
}



std::ostream&
operator<<(std::ostream& os, const SizeDistribution& sd)
{
    os << sd.totalObservations() << '\n';
    return os;
}
