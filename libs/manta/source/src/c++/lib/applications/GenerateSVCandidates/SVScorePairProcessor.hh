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

#include "SVEvidence.hh"
#include "SVScorerPairOptions.hh"
#include "blt_util/SizeDistribution.hh"
#include "manta/BamRegionProcessor.hh"
#include "manta/SVCandidate.hh"
#include "manta/SVLocusScanner.hh"


struct SVScorePairInitParams
{
    SVScorePairInitParams(
        const SVLocusScanner& readScanner,
        const SVCandidate& sv,
        const bool isBp1);

    pos_t centerPosA;
    pos_t centerPosB;
    pos_t centerPos;

    // total impact of the alt allele on template size:
    pos_t altShift;
    unsigned minMapQ;
    unsigned minTier2MapQ; // a second, lower mapq threshold used to disprove a somatic allele during tumor/normal calling
};


struct SVScorePairBamParams
{
    bool isSet = false;
    unsigned bamIndex = 0;
    bool isTumor = false;
    pos_t minFrag = 0;
    pos_t maxFrag = 0;
    const SizeDistribution* fragDistroPtr = nullptr;
    GenomeInterval interval;
};


struct SVScorePairProcessor : public BamRegionProcessor
{
    SVScorePairProcessor(
        const std::vector<bool>& initIsAlignmentTumor,
        const SVLocusScanner& initReadScanner,
        const PairOptions& initPairOpt,
        const SVCandidate& initSv,
        const bool initIsBp1,
        SVEvidence& initEvidence) :
        isAlignmentTumor(initIsAlignmentTumor),
        readScanner(initReadScanner),
        pairOpt(initPairOpt),
        sv(initSv),
        isBp1(initIsBp1),
        evidence(initEvidence),
        svParams(readScanner, sv, isBp1),
        bamParams()
    {}

    const GenomeInterval&
    nextBamIndex(
        const unsigned bamIndex);

    void
    processRecord(
        const bam_record& bamRead)
    {
        if (isSkipRecordCore(bamRead)) return;
        if (isSkipRecord(bamRead)) return;
        processClearedRecord(bamRead);
    }

    // alternate interface
    static
    bool
    isSkipRecordCore(
        const bam_record& bamRead)
    {
        return (SVLocusScanner::isReadFilteredCore(bamRead) || bamRead.isNonStrictSupplement());
    }

    /// what to skip in addition to the core skip test?
    virtual
    bool
    isSkipRecord(
        const bam_record& bamRead)
    {
        if (bamRead.is_unmapped() || (bamRead.is_paired() && bamRead.is_mate_unmapped())) return true;
        else if (! is_innie_pair(bamRead)) return true;
        return false;
    }

    // process a record for which isSkipRecord() == false
    virtual
    void
    processClearedRecord(
        const bam_record& bamRead) = 0;

    static
    bool
    isLargeInsertSV(
        const SVCandidate& sv)
    {
        return (sv.insertSeq.size() >= 100 );
    }

protected:

    static
    void
    setAlleleFrag(
        const SizeDistribution& fragDistro,
        const int size,
        SVFragmentEvidenceAlleleBreakend& bp,
        const bool /*isPdf*/ = false)
    {
        float fragProb(0);
#if 0
        if (isPdf)
        {
            fragProb = fragDistro.pdf(size);
        }
        else
#endif
        {
            fragProb = fragDistro.cdf(size);
            fragProb = std::min(fragProb, (1-fragProb));
        }
#ifdef DEBUG_MEGAPAIR
        log_os << __FUNCTION__ << ": fraglen,prob " << size << " " << fragProb << "\n";
#endif

        bp.isFragmentSupport = true;
        bp.fragLengthProb = fragProb;
    }

    const std::vector<bool> isAlignmentTumor;
    const SVLocusScanner& readScanner;
    const PairOptions& pairOpt;
    const SVCandidate& sv;
    const bool isBp1;
    SVEvidence& evidence;

    const SVScorePairInitParams svParams;
    SVScorePairBamParams bamParams;
};
