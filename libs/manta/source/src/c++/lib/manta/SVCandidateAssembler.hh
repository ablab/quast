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
/// \author Ole Schulz-Trieglaff
///

#pragma once

#include "applications/GenerateSVCandidates/GSCOptions.hh"
#include "assembly/IterativeAssembler.hh"
#include "assembly/SmallAssembler.hh"
#include "blt_util/time_util.hh"
#include "htsapi/bam_streamer.hh"
#include "manta/ChromDepthFilterUtil.hh"
#include "manta/SVCandidate.hh"
#include "manta/SVCandidateAssemblyData.hh"
#include "manta/SVCandidateSetData.hh"
#include "manta/SVLocusScanner.hh"

#include <vector>


//#define ITERATIVE_ASSEMBLER
#ifdef ITERATIVE_ASSEMBLER
typedef IterativeAssemblerOptions AssemblerOptions;
#else
typedef SmallAssemblerOptions AssemblerOptions;
#endif

/// Assembles SV-candidate reads for single and paired SVBreakend objects
///
struct SVCandidateAssembler
{
    SVCandidateAssembler(
        const ReadScannerOptions& scanOpt,
        const AssemblerOptions& assembleOpt,
        const AlignmentFileOptions& alignFileOpt,
        const std::string& statsFilename,
        const std::string& chromDepthFilename,
        const bam_header_info& bamHeader,
        const AllCounts& counts,
        const bool isRNA,
        TimeTracker& remoteTIme);

    /**
     * @brief Performs a de-novo assembly of a set of reads crossing a breakpoint.
     *
     * Iterates over a range of word lengths until the first successful assembly.
     *
     * If unused reads remain, the assembly is re-started using this subset.
     */
    void
    assembleSingleSVBreakend(
        const SVBreakend& bp,
        const reference_contig_segment& refSeq,
        const bool isSearchRemoteInsertionReads,
        RemoteReadCache& remoteReads,
        Assembly& as) const;

    void
    assembleSVBreakends(
        const SVBreakend& bp1,
        const SVBreakend& bp2,
        const bool isBp1Reversed,
        const bool isBp2Reversed,
        const reference_contig_segment& refSeq1,
        const reference_contig_segment& refSeq2,
        Assembly& as) const;

    const AssemblerOptions&
    getAssembleOpt() const
    {
        return _assembleOpt;
    }


    typedef std::map<std::string,unsigned> ReadIndexType;

private:
    typedef std::shared_ptr<bam_streamer> streamPtr;

    /// Collect reads crossing an SV breakpoint and add them to 'reads'
    ///
    /// \param[in] isReversed if true revcomp all reads on input
    /// \param[in] refSeq this is used to find reads which have poorly aligned ends, such reads are added to the breakend assembly pool
    /// \param[in] isSearchRemoteInsertionReads if true search the remote end of chimeric pairs for MAPQ0 insertion support
    /// \param[out] remoteReadsCache stores any discovered remote reads so that these can be reused during scoring
    /// \param[out] reads collected breakend assembly candidate reads
    void
    getBreakendReads(
        const SVBreakend& bp,
        const bool isReversed,
        const reference_contig_segment& refSeq,
        const bool isSearchRemoteInsertionReads,
        RemoteReadCache& remoteReadsCache,
        ReadIndexType& readIndex,
        AssemblyReadInput& reads) const;

    const ReadScannerOptions _scanOpt;
    const AssemblerOptions _assembleOpt;
    const std::vector<bool> _isAlignmentTumor;
    const ChromDepthFilterUtil _dFilter;
    const ChromDepthFilterUtil _dFilterRemoteReads;

    // contains functions to detect/classify anomalous reads
    SVLocusScanner _readScanner;
    std::vector<streamPtr> _bamStreams;
    TimeTracker& _remoteTime;

    std::vector<double> _sampleBackgroundRemoteRate;
};
