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

#include "SVFinderStats.hh"
#include "blt_util/time_util.hh"

#include "boost/serialization/nvp.hpp"
#include "boost/serialization/vector.hpp"

#include <cassert>
#include <cstdint>

#include <iosfwd>
#include <vector>


struct SimpleHist
{
    explicit
    SimpleHist(
        const unsigned size)
        : histdata(size,0)
    {
        assert (size!=0);
    }

    void
    increment(
        const unsigned val)
    {
        if (val >= histdata.size())
        {
            histdata.back()++;
            return;
        }
        histdata[val]++;
    }

    void
    merge(const SimpleHist& rhs)
    {
        assert(histdata.size() == rhs.histdata.size());
        for (unsigned i(0); i<histdata.size(); i++)
        {
            histdata[i] += rhs.histdata[i];
        }
    }

    void
    report(std::ostream& os) const;

    template<class Archive>
    void serialize(Archive& ar, const unsigned /* version */)
    {
        ar& BOOST_SERIALIZATION_NVP(histdata)
        ;
    }
    std::vector<uint64_t> histdata;
};

BOOST_CLASS_IMPLEMENTATION(SimpleHist, boost::serialization::object_serializable)


/// aggregate statistics over a group of GSV edges
struct GSCEdgeGroupStats
{
    GSCEdgeGroupStats()
        : candidatesPerEdge(6),
          assemblyCandidatesPerJunction(6),
          breaksPerJunction(4)
    {}

    void
    merge(const GSCEdgeGroupStats& rhs)
    {
        totalTime.merge(rhs.totalTime);
        candTime.merge(rhs.candTime);
        assemblyTime.merge(rhs.assemblyTime);
        scoringTime.merge(rhs.scoringTime);
        totalInputEdgeCount += rhs.totalInputEdgeCount;
        totalCandidateCount += rhs.totalCandidateCount;
        totalComplexCandidate += rhs.totalComplexCandidate;
        totalSpanningCandidateFilter += rhs.totalSpanningCandidateFilter;
        totalSpanningCandidateFilter += rhs.totalJunctionAssemblyOverlapSkips;
        totalJunctionCount += rhs.totalJunctionCount;
        totalComplexJunctionCount += rhs.totalComplexJunctionCount;
        totalAssemblyCandidates += rhs.totalAssemblyCandidates;
        totalSpanningAssemblyCandidates += rhs.totalSpanningAssemblyCandidates;
        candidatesPerEdge.merge(rhs.candidatesPerEdge);
        assemblyCandidatesPerJunction.merge(rhs.assemblyCandidatesPerJunction);
        breaksPerJunction.merge(rhs.breaksPerJunction);
        finderStats.merge(rhs.finderStats);
    }

    void
    report(std::ostream& os) const;

    template<class Archive>
    void serialize(Archive& ar, const unsigned /* version */)
    {
        ar& BOOST_SERIALIZATION_NVP(totalTime)
        & BOOST_SERIALIZATION_NVP(candTime)
        & BOOST_SERIALIZATION_NVP(assemblyTime)
        & BOOST_SERIALIZATION_NVP(scoringTime)
        & BOOST_SERIALIZATION_NVP(totalInputEdgeCount)
        & BOOST_SERIALIZATION_NVP(totalCandidateCount)
        & BOOST_SERIALIZATION_NVP(totalComplexCandidate)
        & BOOST_SERIALIZATION_NVP(totalSpanningCandidateFilter)
        & BOOST_SERIALIZATION_NVP(totalJunctionAssemblyOverlapSkips)
        & BOOST_SERIALIZATION_NVP(totalJunctionCount)
        & BOOST_SERIALIZATION_NVP(totalComplexJunctionCount)
        & BOOST_SERIALIZATION_NVP(totalAssemblyCandidates)
        & BOOST_SERIALIZATION_NVP(totalSpanningAssemblyCandidates)
        & BOOST_SERIALIZATION_NVP(candidatesPerEdge)
        & BOOST_SERIALIZATION_NVP(assemblyCandidatesPerJunction)
        & BOOST_SERIALIZATION_NVP(breaksPerJunction)
        & BOOST_SERIALIZATION_NVP(finderStats)
        ;
    }

    CpuTimes totalTime;
    CpuTimes candTime;
    CpuTimes assemblyTime;
    CpuTimes scoringTime;
    uint64_t totalInputEdgeCount = 0;
    uint64_t totalCandidateCount = 0;
    uint64_t totalComplexCandidate = 0;
    uint64_t totalSpanningCandidateFilter = 0;
    uint64_t totalJunctionAssemblyOverlapSkips = 0;
    uint64_t totalJunctionCount = 0;
    uint64_t totalComplexJunctionCount = 0;
    uint64_t totalAssemblyCandidates = 0;
    uint64_t totalSpanningAssemblyCandidates = 0;

    SimpleHist candidatesPerEdge;
    SimpleHist assemblyCandidatesPerJunction;
    SimpleHist breaksPerJunction;

    SVFinderStats finderStats;
};

BOOST_CLASS_IMPLEMENTATION(GSCEdgeGroupStats, boost::serialization::object_serializable)


struct GSCEdgeStatsData
{
    GSCEdgeStatsData() {}

    void
    merge(const GSCEdgeStatsData& rhs)
    {
        lifeTime.merge(rhs.lifeTime);
        selfEdges.merge(rhs.selfEdges);
        remoteEdges.merge(rhs.remoteEdges);
    }

    void
    report(std::ostream& os) const;

    template<class Archive>
    void serialize(Archive& ar, const unsigned /* version */)
    {
        ar& BOOST_SERIALIZATION_NVP(lifeTime)
        & BOOST_SERIALIZATION_NVP(selfEdges)
        & BOOST_SERIALIZATION_NVP(remoteEdges);
    }

    CpuTimes lifeTime;
    GSCEdgeGroupStats selfEdges;
    GSCEdgeGroupStats remoteEdges;
};

BOOST_CLASS_IMPLEMENTATION(GSCEdgeStatsData, boost::serialization::object_serializable)



struct GSCEdgeStats
{
    void
    load(const char* filename);

    void
    save(std::ostream& os) const;

    void
    save(const char* filename) const;

    void
    report(const char* filename) const;

    void
    merge(const GSCEdgeStats& rhs)
    {
        edgeData.merge(rhs.edgeData);
    }

    GSCEdgeStatsData edgeData;
};

BOOST_CLASS_IMPLEMENTATION(GSCEdgeStats, boost::serialization::object_serializable)
