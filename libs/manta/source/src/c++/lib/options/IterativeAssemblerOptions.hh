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
/// \author Xiaoyu Chen
/// \author Ole Schulz-Trieglaff
///

#pragma once


/// Input parameters for IterativeAssembler
///
struct IterativeAssemblerOptions
{
    /// the symbol set used during assembly
    std::string alphabet = "ACGT";

    /// minimum basecall quality for assembly input
    int minQval = 5;

    /// initial word (kmer) length
    unsigned minWordLength = 41;

    unsigned maxWordLength = 76;
    unsigned wordStepSize = 5;
    unsigned minContigLength = 15;

    /// min. coverage required for contig extension
    unsigned minCoverage = 1;

    /// coverage required for conservative contig sub-range
    unsigned minConservativeCoverage = 2;

    /// max error rates allowed during contig extension
    double maxError = 0.35;

    /// min. number of unused reads to enable search for more contigs
    unsigned minUnusedReads = 3;

    /// min. number of reads required to start assembly
    unsigned minSupportReads = 2;

    /// Max. number of assembly returned for a given set of reads
    unsigned maxAssemblyCount = 10;
};
