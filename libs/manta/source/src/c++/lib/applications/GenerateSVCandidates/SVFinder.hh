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

#include "EdgeRuntimeTracker.hh"
#include "FatSVCandidate.hh"
#include "GSCOptions.hh"
#include "GSCEdgeStatsManager.hh"
#include "appstats/SVFinderStats.hh"
#include "htsapi/bam_streamer.hh"
#include "manta/ChromDepthFilterUtil.hh"
#include "manta/SVCandidateSetData.hh"
#include "manta/SVLocusScanner.hh"
#include "svgraph/EdgeInfo.hh"
#include "svgraph/SVLocusSet.hh"

#include <vector>


struct SVFinder
{
    SVFinder(
        const GSCOptions& opt,
        const SVLocusScanner& readScanner,
        EdgeRuntimeTracker& edgeTracker,
        GSCEdgeStatsManager& edgeStatMan);

    ~SVFinder();

    const SVLocusSet&
    getSet() const
    {
        return _set;
    }

    void
    findCandidateSV(
        const EdgeInfo& edge,
        SVCandidateSetData& svData,
        std::vector<SVCandidate>& svs);

    void
    checkResult(
        const SVCandidateSetData& svData,
        const std::vector<SVCandidate>& svs) const;

private:

    void
    addSVNodeData(
        const bam_header_info& bamHeader,
        const SVLocus& locus,
        const NodeIndexType node1,
        const NodeIndexType node2,
        const GenomeInterval& searchInterval,
        const reference_contig_segment& refSeq,
        const bool isNode1,
        SVCandidateSetData& svData);

    void
    assignFragmentObservationsToSVCandidates(
        const SVLocusNode& node1,
        const SVLocusNode& node2,
        const std::vector<SVObservation>& readCandidates,
        const bool isExpandSVCandidateSet,
        SVCandidateSetSequenceFragment& fragment,
        std::vector<FatSVCandidate>& svs);

    /// we either process the fragment to discover new SVs and expand existing SVs,
    /// or we go through and add pairs to existing SVs without expansion
    ///
    void
    processSequenceFragment(
        const SVLocusNode& node1,
        const SVLocusNode& node2,
        const bam_header_info& bamHeader,
        const reference_contig_segment& refSeq1,
        const reference_contig_segment& refSeq2,
        const unsigned bamIndex,
        const bool isExpandSVCandidateSet,
        std::vector<FatSVCandidate>& svs,
        SVCandidateSetSequenceFragment& fragment);

    void
    getCandidatesFromData(
        const SVLocusNode& node1,
        const SVLocusNode& node2,
        const bam_header_info& bamHeader,
        const reference_contig_segment& refSeq1,
        const reference_contig_segment& refSeq2,
        SVCandidateSetData& svData,
        std::vector<SVCandidate>& svs,
        SVFinderStats& stats);

    void
    findCandidateSVImpl(
        const EdgeInfo& edge,
        SVCandidateSetData& svData,
        std::vector<SVCandidate>& svs,
        SVFinderStats& stats);

    const ChromDepthFilterUtil&
    dFilter() const
    {
        return *(_dFilterPtr);
    }

    const ReadScannerOptions _scanOpt;
    const std::vector<bool> _isAlignmentTumor;
    SVLocusSet _set;
    std::unique_ptr<ChromDepthFilterUtil> _dFilterPtr;
    const SVLocusScanner& _readScanner;

    const std::string _referenceFilename;

    const bool _isRNA;
    const bool _isVerbose;
    bool _isSomatic;

    typedef std::shared_ptr<bam_streamer> streamPtr;
    std::vector<streamPtr> _bamStreams;

    /// this is only here as syscall cache:
    std::vector<SVObservation> _readCandidates;

    /// throwaway stats tracker...
    SampleEvidenceCounts _eCounts;

    double _spanningNoiseRate;
    double _assemblyNoiseRate;

    EdgeRuntimeTracker& _edgeTracker;
    GSCEdgeStatsManager& _edgeStatMan;
};
