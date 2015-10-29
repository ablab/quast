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

#include "GenerateSVCandidates.hh"
#include "EdgeRetrieverBin.hh"
#include "EdgeRetrieverLocus.hh"
#include "GSCOptions.hh"
#include "SVCandidateProcessor.hh"
#include "SVFinder.hh"

#include "blt_util/log.hh"
#include "common/Exceptions.hh"
#include "manta/MultiJunctionUtil.hh"
#include "manta/SVCandidateUtil.hh"

#include <iostream>

//#define DEBUG_GSV


/// provide additional edge details, intended for attachment to an in-flight exception:
static
void
dumpEdgeInfo(
    const EdgeInfo& edge,
    const SVLocusSet& set,
    std::ostream& os)
{
    os << edge;
    os << "\tnode1:" << set.getLocus(edge.locusIndex).getNode(edge.nodeIndex1);
    os << "\tnode2:" << set.getLocus(edge.locusIndex).getNode(edge.nodeIndex2);
}



/// we can either traverse all edges in a single locus (disjoint subgraph) of the graph
/// OR
/// traverse all edges in one "bin" -- that is, one out of binCount subsets of the total
/// graph edges. Each bin is designed to be of roughly equal size in terms of total
/// anticipated workload, so that we have good parallel processing performance.
///
static
EdgeRetriever*
edgeRFactory(
    const SVLocusSet& set,
    const EdgeOptions& opt)
{
    if (opt.isLocusIndex)
    {
        return (new EdgeRetrieverLocus(set, opt.graphNodeMaxEdgeCount, opt.locusOpt));
    }
    else
    {
        return (new EdgeRetrieverBin(set, opt.graphNodeMaxEdgeCount, opt.binCount, opt.binIndex));
    }
}


/// TODO temporarily shoved here, needs a better home:
struct MultiJunctionFilter
{
    MultiJunctionFilter(
        const GSCOptions& opt,
        GSCEdgeStatsManager& edgeStatMan)
        : _opt(opt),
          _edgeStatMan(edgeStatMan)
    {}

    void
    filterGroupCandidateSV(
        const EdgeInfo& edge,
        const std::vector<SVCandidate>& svs,
        std::vector<SVMultiJunctionCandidate>& mjSVs)
    {
        unsigned mjComplexCount(0);
        unsigned mjSpanningFilterCount(0);
        findMultiJunctionCandidates(svs, _opt.minCandidateSpanningCount, mjComplexCount, mjSpanningFilterCount, mjSVs);
        _edgeStatMan.updateMJFilter(edge, mjComplexCount, mjSpanningFilterCount);

        if (_opt.isVerbose)
        {
            unsigned junctionCount(mjSVs.size());
            unsigned candidateCount(0);
            for (const SVMultiJunctionCandidate& mj : mjSVs)
            {
                candidateCount += mj.junction.size();
            }
            log_os << __FUNCTION__ << ": Low-resolution candidate filtration complete. "
                   << "candidates: " << candidateCount << " "
                   << "junctions: " << junctionCount << " "
                   << "complex: " << mjComplexCount << " "
                   << "spanningfilt: " << mjSpanningFilterCount << "\n";
        }
#ifdef DEBUG_GSV
        log_os << __FUNCTION__ << ": final candidate list";
        const unsigned junctionCount(mjSVs.size());
        for (unsigned junctionIndex(0); junctionIndex< junctionCount; ++junctionIndex)
        {
            const auto& mj(mjSVs[junctionIndex]);
            const unsigned junctionCandCount(mj.junction.size());
            log_os << __FUNCTION__ << ": JUNCTION " << junctionIndex << " with " << junctionCandCount << " candidates\n";
            for (unsigned junctionCandIndex(0); junctionCandIndex< junctionCandCount; ++junctionCandIndex)
            {
                log_os << __FUNCTION__ << ":  JUNCTION " << junctionIndex << " Candidate "
                       << junctionCandIndex << " " << mj.junction[junctionCandIndex] << "\n";
            }
        }
#endif
    }

private:
    const GSCOptions& _opt;
    GSCEdgeStatsManager& _edgeStatMan;
};


#if 0
/// edge indices+graph evidence counts and regions:
///
/// this is designed to be useful even when the locus graph is not present
struct EhancedEdgeInfo
{

};

/// reduce the full (very-large) graph down to just the information we need during SVCandidate generation:
struct ReducedGraphInfo
{
    ReducedGraphInfo(const GSCOptions& opt)

    bam_header_info header;

    std::vector<EnhancedEdgeInfo> edges;
};
#endif



static
void
runGSC(
    const GSCOptions& opt,
    const char* progName,
    const char* progVersion)
{
#if 0
    {
        // to save memory, load the graph and process/store only the information we need from it:
    }
#endif

    EdgeRuntimeTracker edgeTracker(opt.edgeRuntimeFilename);
    GSCEdgeStatsManager edgeStatMan(opt.edgeStatsFilename);

    const SVLocusScanner readScanner(opt.scanOpt, opt.statsFilename, opt.alignFileOpt.alignmentFilename, opt.isRNA, !opt.isUnstrandedRNA);

    SVFinder svFind(opt, readScanner, edgeTracker,edgeStatMan);
    MultiJunctionFilter svMJFilter(opt,edgeStatMan);
    const SVLocusSet& cset(svFind.getSet());

    SVCandidateProcessor svProcessor(opt, readScanner, progName, progVersion, cset, edgeTracker, edgeStatMan);

    std::unique_ptr<EdgeRetriever> edgerPtr(edgeRFactory(cset, opt.edgeOpt));
    EdgeRetriever& edger(*edgerPtr);

    SVCandidateSetData svData;
    std::vector<SVCandidate> svs;
    std::vector<SVMultiJunctionCandidate> mjSVs;

    if (opt.isVerbose)
    {
        log_os << __FUNCTION__ << ": " << cset.header << "\n";
    }

    while (edger.next())
    {
        const EdgeInfo& edge(edger.getEdge());

        try
        {
            edgeTracker.start();

            if (opt.isVerbose)
            {
                log_os << __FUNCTION__ << ": starting analysis of edge: ";
                dumpEdgeInfo(edge,cset,log_os);
            }

            // find number, type and breakend range (or better: breakend distro) of SVs on this edge:
            svFind.findCandidateSV(edge, svData, svs);

            // filter long-range junctions outside of the candidate finder so that we can evaluate
            // junctions which are part of a larger event (like a reciprocal translocation)
            svMJFilter.filterGroupCandidateSV(edge, svs, mjSVs);

            // determine if this is the only edge for this node:
            svProcessor.evaluateCandidates(edge, mjSVs, svData);
        }
        catch (illumina::common::ExceptionData& e)
        {
            std::ostringstream oss;
            dumpEdgeInfo(edge,cset,oss);
            e << illumina::common::ExceptionMsg(oss.str());
            throw;
        }
        catch (...)
        {
            log_os << "Exception caught while processing graph component: ";
            dumpEdgeInfo(edge,cset,log_os);
            throw;
        }

        edgeTracker.stop(edge);
        if (opt.isVerbose)
        {
            log_os << __FUNCTION__ << ": Time to process last edge: ";
            edgeTracker.getLastEdgeTime().reportSec(log_os);
            log_os << "\n";
        }

        edgeStatMan.updateScoredEdgeTime(edge, edgeTracker);
    }
}



void
GenerateSVCandidates::
runInternal(int argc, char* argv[]) const
{
    GSCOptions opt;

    parseGSCOptions(*this,argc,argv,opt);
#ifdef DEBUG_GSV
    opt.isVerbose=true;
#endif
    runGSC(opt, name(), version());
}
