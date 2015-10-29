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


struct ReadScannerOptions
{
    ReadScannerOptions() {}

    /// standard MAPQ filter applied during locus generation and some/not all subsequent steps
    unsigned minMapq = 15;

    /// a second, lower mapq threshold used only during somatic calling to disprove a
    /// somatic candidate using weak normal sample evidence
    unsigned minTier2Mapq = 5;

    /// report breakend regions with x prob regions removed from each edge
    float breakendEdgeTrimProb = 0.25f;

    /// report breakend regions with x prob regions removed from each edge
    /// used only for 'large-scale' events.
    float largeScaleEventBreakendEdgeTrimProb = 0.1f;

    /// report a pair as "proper pair" if fragment size is within x prob region removed from each edge
    float properPairTrimProb = 0.01f;

    /// add a pair to the evidence pool if frag size is within x prob region removed from each edge
    float evidenceTrimProb = 0.15f;

    /// fragment length to search upstream of a breakend for shadow read support
    float shadowSearchRangeProb = 0.05f;

    /// multiplier for fragment length
    float shadowSearchRangeFactor = 1.2f;

    /// ignore indels smaller than this when building graph, constructing candidates and scoring output:
    unsigned minCandidateVariantSize = 10;

    /// if minCandidateVariantSize is set higher than this value, then we ignore non-specific assembly evidence
    /// (like soft-clip and poor alignment) during candidate generation
    unsigned maxCandidateSizeForLocalAssmEvidence = 100;

    /// whenever a breakend is predicted from a read pair junction, the predicted breakend
    /// range should be no smaller than this:
    unsigned minPairBreakendSize = 40;

    /// whenever a breakend is predicted from an individual read split (ie. non-assembled),
    /// set the predicted breakend size to this fraction of the
    /// event size (modified by the min and max limits below)
    float splitBreakendSizeFraction = 0.1f;

    /// whenever a breakend is predicted from an individual read split (ie. non-assembled),
    /// the predicted breakend range should be no larger than this:
    unsigned maxSplitBreakendSize = 100;

    /// whenever a breakend is predicted from an individual read split (ie. non-assembled),
    /// the predicted breakend range should be no smaller than this:
    unsigned minSplitBreakendSize = 10;

    /// Semi-aligned regions (including soft-clipped) need to be at least this long to be included as SV evidence
    ///
    unsigned minSemiAlignedMismatchLen = 8;

    /// Minimal length of a cis SV candidate in RNA data
    unsigned minRNACisLength = 100000;
    /// Minimal length of any SV candidate in RNA data
    unsigned minRNALength = 1000;

    /// Accept semi-aligned reads with at least this hypothesis score
    /// different for graph and candidate generation
    double minSemiAlignedScoreGraph = 180.0;
    double minSemiAlignedScoreCandidates = 180.0;

    /// min MAPQ for shadow mate used to build SV adjacency graph
    unsigned minSingletonMapqGraph = 30;

    /// min MAPQ for shadow mate used for candidate assembly and scoring
    unsigned minSingletonMapqCandidates = 15;

    /// typically set true for RNA-Seq analysis, where proper-pair is used to signal intron-spanning pairs
    bool isIgnoreAnomProperPair = false;

    /// the maximum depth at which input reads are considered in graph creation/assembly, etc.
    /// (when avg chrom depths are provided)
    float maxDepthFactor = 12;

    /// the maximum depth for a whole locus for remote read retrieval (ie. MAPQ0 chimera mates retrieved for large insertion assembly)
    float maxDepthFactorRemoteReads = 7;
};
