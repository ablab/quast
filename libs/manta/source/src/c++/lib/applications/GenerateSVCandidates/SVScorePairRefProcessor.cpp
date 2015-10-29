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
/// \author Chris Saunders and Xiaoyu Chen
///

#include "SVScorePairRefProcessor.hh"
#include "SVScorerShared.hh"
#include "common/Exceptions.hh"
#include "htsapi/bam_record_util.hh"
#include "manta/SVCandidateUtil.hh"

#include <cassert>

#include <sstream>


/// standard debug output for this file:
//#define DEBUG_PAIR

/// ridiculous debug output for this file:
//#define DEBUG_MEGAPAIR

#ifdef DEBUG_PAIR
#include "blt_util/log.hh"
#endif



void
SVScorePairRefProcessor::
processClearedRecord(
    const bam_record& bamRead)
{
    using namespace illumina::common;

    assert(bamParams.isSet);

    const pos_t refPos(bamRead.pos()-1);
    if (! bamParams.interval.range.is_pos_intersect(refPos)) return;

    const bool isLargeInsert(isLargeInsertSV(sv));

#ifdef DEBUG_MEGAPAIR
    log_os << __FUNCTION__ << ": read: " << bamRead << "\n";
#endif

    /// check if fragment is too big or too small:
    const int templateSize(std::abs(bamRead.template_size()));
    if (templateSize < bamParams.minFrag) return;
    if (templateSize > bamParams.maxFrag) return;

    // count only from the down stream reads
    const bool isFirstBamRead(isFirstRead(bamRead));

    // get fragment range:
    pos_t fragBeginRefPos(refPos);
    if (! isFirstBamRead)
    {
        fragBeginRefPos=bamRead.mate_pos()-1;
    }

    const pos_t fragEndRefPos(fragBeginRefPos+templateSize);

    if (fragBeginRefPos > fragEndRefPos)
    {
        std::ostringstream oss;
        oss << "ERROR: Failed to parse fragment range from bam record. Frag begin,end: " << fragBeginRefPos << " " << fragEndRefPos << " bamRecord: " << bamRead << "\n";
        BOOST_THROW_EXCEPTION(LogicException(oss.str()));
    }

    {
        const pos_t fragOverlap(std::min((1+svParams.centerPos-fragBeginRefPos), (fragEndRefPos-svParams.centerPos)));
#ifdef DEBUG_MEGAPAIR
        log_os << __FUNCTION__ << ": frag begin/end/overlap: " << fragBeginRefPos << " " << fragEndRefPos << " " << fragOverlap << "\n";
#endif
        if (fragOverlap < pairOpt.minFragSupport) return;
    }

    SVFragmentEvidence& fragment(evidence.getSampleEvidence(bamParams.bamIndex)[bamRead.qname()]);

    static const bool isShadow(false);

    SVFragmentEvidenceRead& evRead(fragment.getRead(bamRead.is_first()));
    setReadEvidence(svParams.minMapQ, svParams.minTier2MapQ, bamRead, isShadow, evRead);

    setAlleleFrag(*bamParams.fragDistroPtr, templateSize, fragment.ref.getBp(isBp1),isLargeInsert);
}
