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
/// \author Ole Schulz-Trieglaff
/// \author Bret Barnes
///

#pragma once

#include "blt_util/LinearScaler.hh"
#include "htsapi/bam_record.hh"
#include "htsapi/bam_record_util.hh"
#include "htsapi/bam_header_info.hh"
#include "manta/ReadGroupStatsSet.hh"
#include "manta/SVCandidate.hh"
#include "manta/SVLocusEvidenceCount.hh"
#include "svgraph/SVLocus.hh"
#include "svgraph/SVLocusSampleCounts.hh"
#include "options/ReadScannerOptions.hh"

#include <string>
#include <vector>


namespace FragmentSizeType
{
enum index_t
{
    COMPRESSED,
    NORMAL,
    VERYCLOSE,
    CLOSE,
    DISTANT
};
}


/// The counts in the SVLocus Graph represent an abstract weight of evidence supporting each edge/node.
///
/// To support large and small-scale evidence in a single graph, we need to allow for different weightings
/// for different evidence types
///
struct SVObservationWeights
{
    // noise reduction:
    static const unsigned observation = 3; ///< 'average' observation weight, this is used to scale noise filtration, but not for any evidence type

    // input evidence:
    static const unsigned readPair = observation;
    static const unsigned closeReadPair = 1;
    static const unsigned veryCloseReadPair = 1;
    static const unsigned internalReadEvent = observation; ///< indels, soft-clip, etc.
};



struct ReadScannerDerivOptions
{
    ReadScannerDerivOptions(
        const ReadScannerOptions& opt,
        const bool isRNA,
        const bool stranded) :
        isSmallCandidates(opt.minCandidateVariantSize<=opt.maxCandidateSizeForLocalAssmEvidence),
        beforeBreakend(opt.minPairBreakendSize/2),
        afterBreakend(opt.minPairBreakendSize-beforeBreakend),
        isUseOverlappingPairs(isRNA),
        isStranded(stranded)
    {}

    const bool isSmallCandidates;
    const pos_t beforeBreakend;
    const pos_t afterBreakend;

    /// TODO standardize the overlapping pair treatment to be the same for DNA/RNA modes, then
    /// remove this bit:
    const bool isUseOverlappingPairs;

    const bool isStranded;
};



/// consolidate functions which process a read to determine its
/// SV evidence value
///
/// In manta, evidence is scanned (at least) twice: once for SVLocus Graph generation
/// and then once again during hygen/scoring. We need to make sure both of these steps
/// are using the same logic to process read pairs into SV evidence. This class is
/// responsible for the shared logic
///
struct SVLocusScanner
{
    SVLocusScanner(
        const ReadScannerOptions& opt,
        const std::string& statsFilename,
        const std::vector<std::string>& alignmentFilename,
        const bool isRNA,
        const bool isStranded = false);

    /// this predicate runs isReadFiltered without the mapq components
    static
    bool
    isReadFilteredCore(
        const bam_record& bamRead)
    {
        if      (bamRead.is_filter()) return true;
        else if (bamRead.is_dup()) return true;
        else
        {
            // hack to work with bwamem '-M' formatting,
            // keep secondary reads when they contain an SA tag
            if (bamRead.is_secondary())
            {
                if (! bamRead.isSASplit()) return true;
            }
        }
        return false;
    }

    static
    bool
    isMappedReadFilteredCore(
        const bam_record& bamRead)
    {
        if (isReadFilteredCore(bamRead)) return true;
        return (bamRead.is_unmapped());
    }

    /// this predicate runs any fast tests on the acceptability of a
    /// read for the SVLocus build
    /// Tests also for low mapq
    bool
    isReadFiltered(
        const bam_record& bamRead) const
    {
        if      (isReadFilteredCore(bamRead)) return true;
        else if (bamRead.map_qual() < _opt.minMapq) return true;
        return false;
    }

    unsigned
    getMinMapQ() const
    {
        return _opt.minMapq;
    }

    unsigned
    getMinTier2MapQ() const
    {
        return _opt.minTier2Mapq;
    }

    /// custom version of proper pair bit test:
    bool
    isProperPair(
        const bam_record& bamRead,
        const unsigned defaultReadGroupIndex) const;

    /// return true if the read pair is anomalous, for any anomaly type besides being a short innie read:
    ///
    /// according to this method nothing besides mapped read pairs can be anomalous, so all single read
    /// anomalies (SA tags, CIGAR, semi-aligned) have to be detected elsewhere
    bool
    isNonCompressedAnomalous(
        const bam_record& bamRead,
        const unsigned defaultReadGroupIndex) const;

    /// large indels in CIGAR string
    bool
    isLocalIndelEvidence(
        const SimpleAlignment& bamAlign) const;

    /// semi-aligned and soft-clipped edges
    bool
    isSemiAlignedEvidence(
        const bam_record& bamRead,
        const SimpleAlignment& bamAlign,
        const reference_contig_segment& refSeq) const;

    /// \brief is the read likely to indicate the presence of a small SV?
    ///
    /// this function flags reads which could contribute to a local small-variant assembly
    /// but would not otherwise be caught by the proper pair function
    ///
    /// "small" here is relative -- it means any event at a size where read pair evidence will not be dominant
    ///
    /// Note that the thresholds in this function are more stringent than the equivalent scan used to
    /// pick up reads prior to assembly -- in this case false positives could clog up the graph and
    /// interfere with larger event discovery if not kept under control
    bool
    isLocalAssemblyEvidence(
        const bam_record& bamRead,
        const reference_contig_segment& refSeq) const;

    bool
    isSVEvidence(
        const bam_record& bamRead,
        const unsigned defaultReadGroupIndex,
        const reference_contig_segment& refSeq,
        SVLocusEvidenceCount* incountsPtr = nullptr) const;

    /// return zero to many SVLocus objects if the read supports any
    /// structural variant(s) (detectable by manta)
    ///
    /// \param defaultReadGroupIndex the read group index to use in the absence of an RG tag
    /// (for now RGs are ignored for the purpose of gathering insert stats)
    ///
    void
    getSVLoci(
        const bam_record& bamRead,
        const unsigned defaultReadGroupIndex,
        const bam_header_info& bamHeader,
        const reference_contig_segment& refSeq,
        std::vector<SVLocus>& loci,
        SampleEvidenceCounts& eCounts) const;

    /// get local and remote breakends for each SV Candidate which can be extracted from a read pair
    ///
    /// if remote read is not available, set remoteReadPtr to NULL and a best estimate will be generated for the remote breakend
    ///
    /// for all candidates, if one breakend is estimated from localRead and one is estimated from remoteRead, then
    /// the local breakend will be placed in candidate bp1 and the remote breakend will be placed in candidate.bp2
    ///
    void
    getBreakendPair(
        const bam_record& localRead,
        const bam_record* remoteReadPtr,
        const unsigned defaultReadGroupIndex,
        const bam_header_info& bamHeader,
        const reference_contig_segment& localRefSeq,
        const reference_contig_segment* remoteRefSeqPtr,
        std::vector<SVObservation>& candidates) const;

    /// this information is needed for the whole bam, not just one read group:
    int
    getShadowSearchRange(
        const unsigned defaultReadGroupIndex) const
    {
        return _stats[defaultReadGroupIndex].shadowSearchRange;
    }

    /// provide direct access to the frag distro for
    /// functions which can't be cached
    ///
    const SizeDistribution&
    getFragSizeDistro(
        const unsigned defaultReadGroupIndex) const
    {
        return _rss.getStats(defaultReadGroupIndex).fragStats;
    }

    struct Range
    {
        double min = 0;
        double max = 0;
    };

    const Range&
    getEvidencePairRange(
        const unsigned readGroupIndex) const
    {
        return _stats[readGroupIndex].evidencePair;
    }

    const Range&
    getExtremeFifthRange() const
    {
        return _fifthPerc;
    }

    struct CachedReadGroupStats
    {
        /// fragment size range assumed for the purpose of creating SVLocusGraph regions
        Range breakendRegion;

        /// fragment size range assumed for the purpose of creating SVLocusGraph regions,
        /// this range is used exclusively for large scale events (non-deletion or deletion above a threshold size):
        Range largeScaleEventBreakendRegion;

        /// fragment size range used to determine if a read is anomalous
        Range properPair;

        Range evidencePair;

        /// range fixed the 5th and 95th percentiles:
        Range fifthPerc;

        int shadowSearchRange = 0;

        int minDistantFragmentSize = 0; ///< beyond the properPair anomalous threshold, there is a threshold to distinguish close and far pairs for the purpose of evidence weight
        int minCloseFragmentSize = 0; ///< beyond the properPair anomalous threshold, there is a threshold to distinguish 'really-close' and 'close' pairs for the purpose of evidence weight
        int minVeryCloseFragmentSize = 0;

        //LinearScaler<int> veryCloseEventScaler; ///< used to scale down breakend size as fragments get smaller

        LinearScaler<int> largeEventRegionScaler; ///< used to set expanded breakend sizes for large events
    };

    bool
    isUseOverlappingPairs() const
    {
        return _dopt.isUseOverlappingPairs;
    }

private:

    /// fragments sizes get thrown is serveral pre-defined categories:
    ///
    /// assumes a mapped read pair -- check this in code if making this method non-private
    FragmentSizeType::index_t
    _getFragmentSizeType(
        const bam_record& bamRead,
        const unsigned defaultReadGroupIndex) const;

    /// test whether a fragment is significantly larger than expected
    ///
    /// this function is useful to eliminate reads which fail the ProperPair test
    /// but are still very small
    ///
    /// assumes a mapped read pair -- check this in code if making this method non-private
    bool
    _isLargeFragment(
        const bam_record& bamRead,
        const unsigned defaultReadGroupIndex) const;

    /////////////////////////////////////////////////
    // data:
    const ReadScannerOptions _opt;
    const ReadScannerDerivOptions _dopt;
    ReadGroupStatsSet _rss;

    std::vector<CachedReadGroupStats> _stats;

    /// extreme 5th-95th percentiles over all read groups:
    Range _fifthPerc;

    // cached temporary to reduce syscalls:
    mutable SimpleAlignment _bamAlign;

//    std::string lastQname;
//    uint8_t lastMapq;
};

