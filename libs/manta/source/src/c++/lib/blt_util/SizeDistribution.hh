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

#pragma once

#include "boost/foreach.hpp"
#include "boost/serialization/access.hpp"
#include "boost/serialization/nvp.hpp"
#include "boost/serialization/split_member.hpp"

#include <functional>
#include <iosfwd>
#include <map>
#include <vector>


struct SizeData
{
    SizeData(
        unsigned initCount = 0,
        float initCprob = 0.) :
        count(initCount),
        cprob(initCprob)
    {}

    unsigned count;
    float cprob;
};


/// this structure's only purpose is to provide neat xml output.
/// it is not used outside of serialize/deserialize steps
struct SizeMapXmlElement
{
    template<class Archive>
    void serialize(Archive& ar, const unsigned /*version*/)
    {
        ar& boost::serialization::make_nvp("size", size);
        ar& boost::serialization::make_nvp("count", count);
    }

    int size;
    unsigned count;
};

BOOST_CLASS_IMPLEMENTATION(SizeMapXmlElement, object_serializable)


/// accumulate size observations and provide cdf/quantile/smoothed-pdf for the distribution
///
struct SizeDistribution
{
    SizeDistribution() :
        _isStatsComputed(false),
        _totalCount(0),
        _quantiles(_quantileNum,0)
    {}

    /// return size value for which we observe size value or less with prob p
    int
    quantile(const float prob) const;

    /// return prob of observing this size or less
    float
    cdf(const int x) const;

    /// provide smoothed prob of observing this size
    float
    pdf(const int x) const;

    unsigned
    totalObservations() const
    {
        return _totalCount;
    }

    void
    addObservation(const int size)
    {
        _isStatsComputed = false;
        _totalCount++;
        _sizeMap[size].count++;
    }

    /// filter high value outliers:
    void
    filterObservationsOverQuantile(const float prob);

    typedef std::map<int, SizeData, std::greater<int> > map_type;

private:
    void
    calcStats() const;

    friend class boost::serialization::access;
    template<class Archive>
    void save(Archive& ar, const unsigned /*version*/) const
    {
        ar << boost::serialization::make_nvp("totalObservationCount", _totalCount);
        unsigned mapSize(_sizeMap.size());
        ar << boost::serialization::make_nvp("elementCount", mapSize);

        SizeMapXmlElement xe;
        BOOST_REVERSE_FOREACH(const map_type::value_type& val, _sizeMap)
        {
            xe.size = val.first;
            xe.count = val.second.count;
            ar << boost::serialization::make_nvp("element", xe);
        }
    }

    template<class Archive>
    void load(Archive& ar, const unsigned /*version*/)
    {
        ar >> boost::serialization::make_nvp("totalObservationCount", _totalCount);
        unsigned mapSize(0);
        ar >> boost::serialization::make_nvp("elementCount", mapSize);

        SizeMapXmlElement xe;
        _sizeMap.clear();

        for (unsigned i(0); i<mapSize; ++i)
        {
            ar >> boost::serialization::make_nvp("element", xe);
            _sizeMap[xe.size].count = xe.count;
        }
        _isStatsComputed = false;
    }

    BOOST_SERIALIZATION_SPLIT_MEMBER()

    ///////////////////////////////////// data:

    static const int _quantileNum = 1000;

    mutable bool _isStatsComputed;
    unsigned _totalCount;
    mutable std::vector<int> _quantiles;
    mutable map_type _sizeMap;
};

BOOST_CLASS_IMPLEMENTATION(SizeDistribution, object_serializable)


std::ostream&
operator<<(std::ostream& os, const SizeDistribution& sd);
