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

#include "GSCEdgeStats.hh"

#include "boost/archive/xml_iarchive.hpp"
#include "boost/archive/xml_oarchive.hpp"

#include <fstream>
#include <iostream>



template <typename A,typename B>
double
safeFrac(
    const A num,
    const B den)
{
    if (den == 0) return 0;
    return (static_cast<double>(num)/den);
}



void
SimpleHist::
report(std::ostream& os) const
{
    for (unsigned i(0); i<histdata.size(); ++i)
    {
        os << i;
        if (i+1 == histdata.size()) os << "+";
        os << "\t" << histdata[i] << "\n";
    }
}



static
void
reportTime(
    const char* label,
    const CpuTimes ltime,
    const uint64_t edgeCount,
    const uint64_t candCount,
    std::ostream& os)
{
    os << label << "Hours\t";
    ltime.reportHr(os);
    os << "\n";
    os << label << "SecsPerEdge\t";
    ltime.report(safeFrac(1,edgeCount),"s",os);
    os << "\n";
    os << label << "SecsPerCand\t";
    ltime.report(safeFrac(1,candCount),"s",os);
    os << "\n";
}


#if 0
void
reportTime(
    const char* label,
    const double ltime,
    const uint64_t edgeCount,
    const uint64_t candCount,
    std::ostream& os)
{
    static const double secPerHour(3600);
    os << label << "Hours_SecsPerEdge_SecsPerCand\t" << ltime/secPerHour
       << "\t" << safeFrac(ltime,edgeCount)
       << "\t" << safeFrac(ltime,candCount)
       << "\n";
}
#endif



void
GSCEdgeGroupStats::
report(std::ostream& os) const
{
    CpuTimes catTime(candTime);
    catTime.merge(assemblyTime);
    catTime.merge(scoringTime);
    CpuTimes nocatTime(totalTime);
    nocatTime.difference(catTime);

    os << "InputEdgeCount\t" << totalInputEdgeCount << "\n";
    os << "InputEdgeCandidatesPerEdge:\n";
    candidatesPerEdge.report(os);
    os << "CandidateCount\t" << totalCandidateCount << "\n";
    os << "ComplexCandidateCount\t" << totalComplexCandidate << "\n";
    finderStats.report(os);
    os << "SpanningComplexCandidateFiltered\t" << totalSpanningCandidateFilter << "\n";
    os << "JunctionAssemblyOverlapSkipped\t" << totalJunctionAssemblyOverlapSkips << "\n";
    os << "JunctionCount\t" << totalJunctionCount << "\n";
    os << "ComplexJunctionCount\t" << totalComplexJunctionCount << "\n";
    os << "BreaksPerJunction:\n";
    breaksPerJunction.report(os);
    os << "TotalAssemblyCandidates\t" << totalAssemblyCandidates << "\n";
    os << "TotalSpanningAssemblyCandidates\t" << totalSpanningAssemblyCandidates << "\n";
    os << "AssemblyCandidatesPerJunction:\n";
    assemblyCandidatesPerJunction.report(os);
    reportTime("total",totalTime,totalInputEdgeCount,totalCandidateCount, os);
    reportTime("candi",candTime,totalInputEdgeCount,totalCandidateCount, os);
    reportTime("assem",assemblyTime,totalInputEdgeCount,totalCandidateCount, os);
    reportTime("score",scoringTime,totalInputEdgeCount,totalCandidateCount, os);
    reportTime("nocat",nocatTime,totalInputEdgeCount,totalCandidateCount, os);
}



void
GSCEdgeStatsData::
report(std::ostream& os) const
{
    using namespace BOOST_TIMER_HELPER;
    GSCEdgeGroupStats all(remoteEdges);
    all.merge(selfEdges);
    os << "SVGenTotalHours\t";
    lifeTime.reportHr(os);
    os << "\n";
    CpuTimes nonEdge(lifeTime);
    nonEdge.difference(all.totalTime);
    os << "NonEdgeHours\t";
    nonEdge.reportHr(os);
    os << "\n";
    os << "\n[AllEdges]\n";
    all.report(os);
    os << "\n[RemoteEdges]\n";
    remoteEdges.report(os);
    os << "\n[SelfEdges]\n";
    selfEdges.report(os);
}



void
GSCEdgeStats::
load(const char* filename)
{
    assert(nullptr != filename);
    std::ifstream ifs(filename);
    boost::archive::xml_iarchive ia(ifs);
    ia >> BOOST_SERIALIZATION_NVP(edgeData);
}



void
GSCEdgeStats::
save(std::ostream& os) const
{
    boost::archive::xml_oarchive oa(os);
    oa << BOOST_SERIALIZATION_NVP(edgeData);
}



void
GSCEdgeStats::
save(const char* filename) const
{
    assert(nullptr != filename);
    std::ofstream ofs(filename);
    save(ofs);
}



void
GSCEdgeStats::
report(const char* filename) const
{
    assert(nullptr != filename);
    std::ofstream ofs(filename);
    ofs << "EdgeStatsReport\n";
    edgeData.report(ofs);
}
