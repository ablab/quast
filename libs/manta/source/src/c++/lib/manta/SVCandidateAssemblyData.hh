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

#pragma once

#include "assembly/AssembledContig.hh"
#include "alignment/GlobalAligner.hh"
#include "alignment/GlobalJumpAligner.hh"
#include "blt_util/reference_contig_segment.hh"
#include "manta/SVCandidate.hh"

#include <iosfwd>
#include <unordered_map>
#include <vector>


/// minimum set of information required to describe bp transformations between SVCandidate and its
/// corresponding contig alignment
///
struct BPOrientation
{
    void
    clear()
    {
        isBp2AlignedFirst=false;
        isBp1Reversed=false;
        isBp2Reversed=false;
        isBp1First=true;
        isStranded=false;
    }

    bool isBp2AlignedFirst = false; ///< should the contig on the fwd strand align bp2->bp1 (true) or bp1->bp2 (false)
    bool isBp1Reversed = false; ///< should all bp1 reads be reversed for the contig to assemble correctly?
    bool isBp2Reversed = false; ///< should all bp2 reads be reversed for the contig to assemble correctly?
    bool isBp1First = true; ///< Is this candidate oriented from bp1 to bp2 (used in RNA)? Valid if isStranded==true
    bool isStranded = false; /// Do we know the strand for this candidate (RNA)
};



struct LargeInsertionInfo
{
    void
    clear()
    {
        isLeftCandidate=false;
        isRightCandidate=false;
        contigOffset=0;
        refOffset=0;
        score=0;
    }

    bool isLeftCandidate = false;
    bool isRightCandidate = false;
    unsigned contigOffset = 0; // if candidate, how far into the contig is the breakend?
    unsigned refOffset = 0; // if candidate, how far from the start of the contig alignment is the breakend on reference?
    int score = 0; // what is the alignment score of the contig up to the insertion breakpoint?
};

std::ostream&
operator<<(std::ostream& os, const LargeInsertionInfo& lii);



struct RemoteReadPayload
{
    RemoteReadPayload() :
        readNo(0)
    {}

    RemoteReadPayload(
        const int initReadNo,
        const std::string& initReadSeq) :
        readNo(initReadNo),
        readSeq(initReadSeq)
    {}

    uint8_t readNo; // read no of the remote read, ie. the readno matching readSeq
    std::string readSeq;
};



typedef std::unordered_map<std::string,RemoteReadPayload> RemoteReadCache;



/// \brief Assembly data pertaining to a specific SV candidate
///
/// Assembly starts from a low-resolution SV candidate. This holds
/// any persistent data related to the assembly process, such as data
/// useful for scoring the candidate.
///
/// As a future design note -- it may be possible that the candidate is broken
/// into multiple candidates during assembly-based hypothesis refinement, so
/// this struct may cover multiple refined candidates (but always only one input
/// candidate.
///
/// Also note that this class is representing both large scale/spanning SV's using the locus 'jump' aligner
/// and small-scale local assemblies. This should probably be refactored into two parts, but
/// it's hard to see the right strategy/interface until the scoring modules reach greater maturity
/// (the scoring modules are the primary non-local consumer of information from this struct)
///
struct SVCandidateAssemblyData
{
    void
    clear()
    {
        contigs.clear();
        isCandidateSpanning=false;
        isSpanning=false;
        bporient.clear();
        extendedContigs.clear();
        smallSVAlignments.clear();
        spanningAlignments.clear();
        smallSVSegments.clear();
        largeInsertInfo.clear();
        remoteReads.clear();
        bestAlignmentIndex=0;
        bp1ref.clear();
        bp2ref.clear();
        svs.clear();
        isOverlapSkip=false;
    }

    typedef AlignmentResult<int> SmallAlignmentResultType;
    typedef JumpAlignmentResult<int> JumpAlignmentResultType;

    typedef std::pair<unsigned,unsigned> CandidateSegmentType; ///< 'segments' only pertain to small-event alignments
    typedef std::vector<CandidateSegmentType> CandidateSegmentSetType; ///< 'segments' only pertain to small-event alignments

    Assembly contigs; ///< assembled contigs for both breakpoints

    /// note that isCandidateSpanning can be != isSpanning in cases where the breakends are so close that the assembler decides to fuse
    /// them and treat a spanning hypothesis as non-spanning:
    bool isCandidateSpanning = false; ///< before assembly, was this a 2-locus event (spanning), or a local-assembly?
    bool isSpanning = false; ///< at assembly time, was this treated as a 2-locus event (spanning), or a local-assembly?

    BPOrientation bporient;

    std::vector<std::string> extendedContigs; ///extended each contig's sequence by padding reference sequences on each end

    std::vector<SmallAlignmentResultType> smallSVAlignments; ///< contig smallSV alignments, one per contig, may be empty
    std::vector<JumpAlignmentResultType> spanningAlignments; ///< contig spanning alignments, one per contig, may be empty
    std::vector<CandidateSegmentSetType> smallSVSegments; ///< list of indel sets, one per small alignment

    std::vector<LargeInsertionInfo> largeInsertInfo; ///< data specific to searching for a large insertion candidate

    RemoteReadCache remoteReads; ///< remote reads retrieved to improve assembly and scoring for this locus

    unsigned bestAlignmentIndex = 0; ///< if non-empty sv candidate set, which contig/alignment produced them?

    // expanded reference regions around the candidate SV breakend regions, for small events we use only bp1ref:
    reference_contig_segment bp1ref;
    reference_contig_segment bp2ref;

    std::vector<SVCandidate> svs; ///< summarize candidate refined sv candidates

    /// if true, assembly was skipped for this case because of an overlapping assembly
    bool isOverlapSkip = false;
};
