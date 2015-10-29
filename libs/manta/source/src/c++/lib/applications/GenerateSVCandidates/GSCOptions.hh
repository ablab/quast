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

#include "EdgeOptions.hh"
#include "manta/Program.hh"
#include "options/AlignmentFileOptions.hh"
#include "options/CallOptionsDiploid.hh"
#include "options/CallOptionsShared.hh"
#include "options/CallOptionsSomatic.hh"
#include "options/CallOptionsTumor.hh"
#include "options/ReadScannerOptions.hh"
#include "options/SVRefinerOptions.hh"

#include <string>
#include <vector>


struct GSCOptions
{
    AlignmentFileOptions alignFileOpt;
    EdgeOptions edgeOpt;
    ReadScannerOptions scanOpt;
    SVRefinerOptions refineOpt;
    CallOptionsShared callOpt;
    CallOptionsDiploid diploidOpt;
    CallOptionsSomatic somaticOpt;
    CallOptionsTumor tumorOpt;

    std::string graphFilename;
    std::string referenceFilename;
    std::string statsFilename;
    std::string chromDepthFilename;
    std::string truthVcfFilename;
    std::string edgeRuntimeFilename;
    std::string edgeStatsFilename;

    std::string candidateOutputFilename;
    std::string diploidOutputFilename;
    std::string somaticOutputFilename;
    std::string tumorOutputFilename;

    bool isVerbose = false; ///< provide some high-level log info to assist in debugging

    bool isSkipAssembly = false; ///< if true, skip assembly and run a low-resolution, breakdancer-like subset of the workflow

    bool isSkipScoring = false; ///< if true, skip quality scoring and output candidates only

    bool isSkipRemoteReads = false; ///< if true, don't search for non-local mapq0 mate pairs for assembly

    bool isRNA = false; ///< if true, RNA specific filtering on candidates and diploid scoring is used

    bool isUnstrandedRNA = false; /// For unstranded RNA data, the direction of fusion transcripts is unknown

    unsigned minCandidateSpanningCount = 3; ///< how many spanning evidence observations are required to become a candidate?

    unsigned minScoredVariantSize = 51; ///< min size for scoring and scored output following candidate generation
};


void
parseGSCOptions(
    const manta::Program& prog,
    int argc, char* argv[],
    GSCOptions& opt);
