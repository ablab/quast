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

#include "GSCOptions.hh"
#include "EdgeRuntimeTracker.hh"

#include "alignment/GlobalAligner.hh"
#include "alignment/GlobalLargeIndelAligner.hh"
#include "alignment/GlobalJumpAligner.hh"
#include "alignment/GlobalJumpIntronAligner.hh"
#include "htsapi/bam_header_info.hh"
#include "manta/SVCandidateAssemblyData.hh"
#include "manta/SVCandidate.hh"
#include "manta/SVCandidateAssembler.hh"
#include "options/SmallAssemblerOptions.hh"
#include "svgraph/GenomeIntervalTracker.hh"


/// \brief methods to improve low-resolution SVCandidates via assembly and contig alignment
///
struct SVCandidateAssemblyRefiner
{
    SVCandidateAssemblyRefiner(
        const GSCOptions& opt,
        const bam_header_info& header,
        const AllCounts& counts,
        EdgeRuntimeTracker& edgeTracker);

    /// \brief add assembly and assembly post-processing data to SV candidate
    ///
    /// \param[in] isRNA if true add intron logic to the contig jump aligner
    /// \param[in] isFindLargeInsertions if true search for insertions which can't be completely assembled, and conduct more expensive search for assembly insertion evidence
    void
    getCandidateAssemblyData(
        const SVCandidate& sv,
        const SVCandidateSetData& svData,
        const bool isRNA,
        const bool isFindLargeInsertions,
        SVCandidateAssemblyData& assemblyData) const;

    void
    clearEdgeData()
    {
        _spanToComplexAssmRegions.clear();
    }

private:

    /// large SV assembler
    void
    getJumpAssembly(
        const SVCandidate& sv,
        const bool isRNA,
        const bool isFindLargeInsertions,
        SVCandidateAssemblyData& assemblyData) const;

    /// small SV/indel assembler
    ///
    /// \param[in] isFindLargeInsertions if true search for insertions which can't be completely assembled, and conduct more expensive search for assembly insertion evidence
    ///
    void
    getSmallSVAssembly(
        const SVCandidate& sv,
        const bool isFindLargeInsertions,
        SVCandidateAssemblyData& assemblyData) const;

    //////////////////////////////// data:
    const GSCOptions& _opt;
    const bam_header_info& _header;
    const SVCandidateAssembler _smallSVAssembler;
    const SVCandidateAssembler _spanningAssembler;
    const GlobalAligner<int> _smallSVAligner;
    const GlobalLargeIndelAligner<int> _largeSVAligner;
    const GlobalAligner<int> _largeInsertEdgeAligner;
    const GlobalAligner<int> _largeInsertCompleteAligner;
    const GlobalJumpAligner<int> _spanningAligner;
    const GlobalJumpIntronAligner<int> _RNASpanningAligner;
    mutable GenomeIntervalTracker _spanToComplexAssmRegions;
};
