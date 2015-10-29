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

#include "SVScorePairProcessor.hh"

#include "manta/ShadowReadFinder.hh"
#include "manta/SVCandidateAssemblyData.hh"
#include "options/ReadScannerOptions.hh"
#include "options/SVRefinerOptions.hh"


struct ContigParams
{
    ContigParams(
        const SVCandidateAssemblyData& assemblyData,
        const SVCandidate& sv);

    /// extended contig:
    const std::string& extSeq;

    /// where does the sv segment begin,end in reference coordinates?:
    known_pos_range2 segmentSpan;

    known_pos_range2 bpAOffset;
    known_pos_range2 bpBOffset;
};


/// estimate pair support for an sv candidate
/// restricted to simple indel style svs
struct SVScorePairAltProcessor : public SVScorePairProcessor
{
    SVScorePairAltProcessor(
        const ReadScannerOptions& scanOpt,
        const SVRefinerOptions& refineOpt,
        const std::vector<bool>& initIsAlignmentTumor,
        const SVLocusScanner& initReadScanner,
        const PairOptions& initPairOpt,
        const SVCandidateAssemblyData& initAssemblyData,
        const SVCandidate& initSv,
        const bool initIsBp1,
        SVEvidence& initEvidence) :
        SVScorePairProcessor(initIsAlignmentTumor, initReadScanner, initPairOpt, initSv, initIsBp1, initEvidence),
        assemblyData(initAssemblyData),
        _shadowAligner(refineOpt.spanningAlignScores),
        _shadow(scanOpt.minSingletonMapqCandidates,
                (! initIsBp1), /// search for left-open shadows
                (  initIsBp1)), /// search for right-open shadows
        _contig(initAssemblyData,initSv)
    {
        checkInput(sv);
    }

    /// what to skip in addition to the core skip test?
    ///
    /// override to allow for shadow and chimera re-maps for large insertions:
    ///
    virtual
    bool
    isSkipRecord(
        const bam_record& bamRead)
    {
        if (! isLargeInsertSV(sv)) return SVScorePairProcessor::isSkipRecord(bamRead);

        if (! bamRead.is_paired()) return true;
        else if (bamRead.is_unmapped() && bamRead.is_mate_unmapped()) return true;
        return false;
    }

    void
    processClearedRecord(
        const bam_record& bamRead);

private:
    static
    void
    checkInput(
        const SVCandidate& sv);

    /// \param[in] bam record used for debug printout only
    /// \param[in] isLeftOfInsert is the anchor on the left or right side of the insertion
    /// \param[in] floatRead the read to be realigned, already revcomped to expected orientation
    /// \param[in] anchorPos the alignment position of the anchoring (ie. non-relaigned) read of the pair
    ///
    /// \return true for usable alignment
    bool
    realignPairedRead(
        const bam_record& bamRead,
        const bool isLeftOfInsert,
        const std::string& floatRead,
        const pos_t anchorPos,
        int& altTemplateSize);

    bool
    alignShadowRead(
        const bam_record& bamRead,
        int& altTemplateSize);

    /// test whether a frag reference span provides sufficient support for a breakpoint of this sv:
    bool
    testFragOverlap(
        const int fragBeginRefPos,
        const int fragEndRefPos) const;

    ///////////////////////
    const SVCandidateAssemblyData& assemblyData;

    const GlobalAligner<int> _shadowAligner;
    ShadowReadFinder _shadow;

    ContigParams _contig;
};
