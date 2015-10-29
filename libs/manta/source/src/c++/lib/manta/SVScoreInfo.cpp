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

#include "manta/SVScoreInfo.hh"
#include "blt_util/log.hh"
#include "blt_util/seq_util.hh"
#include "blt_util/seq_printer.hh"

#include <iostream>



SVAlignmentInfo::
SVAlignmentInfo(
    const SVCandidate& sv,
    const SVCandidateAssemblyData& assemblyData) :
    _isSpanning(assemblyData.isSpanning),
    _bp1ContigReversed(assemblyData.bporient.isBp1Reversed),
    _bp2ContigReversed(assemblyData.bporient.isBp2Reversed)
{
    // for imprecise SVs, split-read evidence won't be assigned
    if (sv.isImprecise()) return;

    const pos_t bp1HomLength(sv.bp1.interval.range.size()-1);
    const pos_t bp2HomLength(sv.bp2.interval.range.size()-1);
    assert(bp1HomLength >= 0);
    assert(bp2HomLength >= 0);

    contigSeq = assemblyData.extendedContigs[sv.assemblyAlignIndex];

    if (_isSpanning)
    {
        const JumpAlignmentResult<int>& alignment(assemblyData.spanningAlignments[sv.assemblyAlignIndex]);

        // get offsets of breakpoints in the extended contig
        const pos_t align1Size(apath_read_length(alignment.align1.apath));

        // the beginPos of align1 is the length of reference padding in the extended contig
        // |ref padding| + |align1| + |insert| + |align2|
        // both bp1 and bp2 include the insert and micro-homology,
        // which can avoid false split-read evidence from normal sample when the micorhomology is long

        // csaunders: leave homology size out until we're prepared downstream to
        // deal with each breakpoint as a range instead of a point value

        const pos_t bp1ContigBeginPos(alignment.align1.beginPos + align1Size -1);
        const pos_t bp2ContigBeginPos(bp1ContigBeginPos + alignment.jumpInsertSize);

        bp1ContigOffset.set_begin_pos(bp1ContigBeginPos);
        bp2ContigOffset.set_begin_pos(bp2ContigBeginPos);

        // before swap, bp[1/2]ContigOffset are in alignment order (like alignment.align[12])
        // after swap, they are in the same bp order indicated on the sv (like sv.bp[12])
        if (assemblyData.bporient.isBp2AlignedFirst)
        {
            std::swap(bp1ContigOffset, bp2ContigOffset);
        }

        /// add micro-homology after swapping so that we can extract information from sv
        bp1ContigOffset.set_end_pos(bp1ContigOffset.begin_pos() + bp1HomLength);
        bp2ContigOffset.set_end_pos(bp2ContigOffset.begin_pos() + bp2HomLength);

        if (_bp1ContigReversed || _bp2ContigReversed)
        {
            assert(! (_bp1ContigReversed && _bp2ContigReversed));

            revContigSeq = reverseCompCopyStr(contigSeq);
            // reset offset w.r.t. the reversed contig
            // note this is -2 and not -1 because we're jumping to the other "side" of the breakend:
            const pos_t revSize(contigSeq.size()-2);
            if (_bp1ContigReversed)
            {
                const known_pos_range2 tmpRange(bp1ContigOffset);
                bp1ContigOffset.set_begin_pos(revSize - tmpRange.end_pos());
                bp1ContigOffset.set_end_pos(revSize - tmpRange.begin_pos());
            }
            else
            {
                const known_pos_range2 tmpRange(bp2ContigOffset);
                bp2ContigOffset.set_begin_pos(revSize - tmpRange.end_pos());
                bp2ContigOffset.set_end_pos(revSize - tmpRange.begin_pos());
            }
        }
        assert(bp1ContigOffset.begin_pos() <= bp1ContigOffset.end_pos());
        assert(bp2ContigOffset.begin_pos() <= bp2ContigOffset.end_pos());

        // get reference regions
        const reference_contig_segment& bp1Ref = assemblyData.bp1ref;
        const reference_contig_segment& bp2Ref = assemblyData.bp2ref;
        bp1RefSeq = bp1Ref.seq();
        bp2RefSeq = bp2Ref.seq();
        // get offsets of breakpoints in the reference regions
        // again, both bp1 and bp2 include the micro-homology

        // csaunders: see above regarding micro-homology handling

        bp1RefOffset.set_begin_pos(sv.bp1.interval.range.begin_pos() - bp1Ref.get_offset());
        bp1RefOffset.set_end_pos(bp1RefOffset.begin_pos() + bp1HomLength);

        bp2RefOffset.set_begin_pos(sv.bp2.interval.range.begin_pos() - bp2Ref.get_offset());
        bp2RefOffset.set_end_pos(bp2RefOffset.begin_pos() + bp2HomLength);
    }
    else
    {
        // get offsets of breakpoints in the extended contig
        const AlignmentResult<int>& alignment(assemblyData.smallSVAlignments[sv.assemblyAlignIndex]);
        const std::pair<unsigned, unsigned>& alignSegment(assemblyData.smallSVSegments[sv.assemblyAlignIndex][sv.assemblySegmentIndex]);

        const ALIGNPATH::path_t apathTillSvStart(&alignment.align.apath[0], &alignment.align.apath[alignSegment.first]);
        const ALIGNPATH::path_t apathTillSvEnd(&alignment.align.apath[0], &alignment.align.apath[alignSegment.second+1]);

        // the beginPos of align is the length of reference padding in the extended contig
        // |ref padding| + |alignment segments|
        // both bp1 and bp2 include the insert and micro-homology,
        // which can avoid false split-read evidence from normal sample when the micorhomology is long

        bp1ContigOffset.set_begin_pos(alignment.align.beginPos + apath_read_length(apathTillSvStart) - 1);
        bp1ContigOffset.set_end_pos(bp1ContigOffset.begin_pos() + bp1HomLength);
        bp2ContigOffset.set_begin_pos(alignment.align.beginPos + apath_read_length(apathTillSvEnd) - 1);
        bp2ContigOffset.set_end_pos(bp2ContigOffset.begin_pos() + bp2HomLength);

        // get reference regions
        // only bp1ref is used for small events
        const reference_contig_segment& bp1Ref = assemblyData.bp1ref;
        bp1RefSeq = bp1Ref.seq();

        // get offsets of breakpoints in the reference regions
        // again, both bp1 and bp2 include the micro-homology
        bp1RefOffset.set_range(
            sv.bp1.interval.range.begin_pos() - bp1Ref.get_offset(),
            sv.bp1.interval.range.end_pos() -bp1Ref.get_offset());
        bp2RefOffset.set_range(
            sv.bp2.interval.range.begin_pos() - bp1Ref.get_offset(),
            sv.bp2.interval.range.end_pos() -bp1Ref.get_offset());
    }
}



bool
SVAlignmentInfo::
isMinBpEdge(
    const unsigned minEdge) const
{
    const int iminEdge(minEdge);
    if ((bp1ContigOffset.begin_pos()+1) < iminEdge) return false;
    if ((bp2ContigOffset.begin_pos()+1) < iminEdge) return false;
    if ((bp1RefOffset.begin_pos()+1) < iminEdge) return false;
    if ((bp2RefOffset.begin_pos()+1) < iminEdge) return false;

    const pos_t contigBpSize(contigSeq.size()-1);
    if ((contigBpSize - bp1ContigOffset.end_pos()) < iminEdge) return false;
    if ((contigBpSize - bp2ContigOffset.end_pos()) < iminEdge) return false;

    const pos_t bp1RefSize(bp1ReferenceSeq().size());
    if ((bp1RefSize - 1 - bp1RefOffset.end_pos()) < iminEdge) return false;

    const pos_t bp2RefSize(bp2ReferenceSeq().size());
    if ((bp2RefSize - 1 - bp2RefOffset.end_pos()) < iminEdge) return false;

    return true;
}



static
void
dumpSeq(
    const char* label,
    const std::string& seq,
    std::ostream& os)
{
    os << label << " size/seq: " << seq.size() << '\n';
    printSeq(seq,os);
    os << '\n';
}



std::ostream&
operator<<(
    std::ostream& os,
    const SVAlignmentInfo& ai)
{
    os << "SVAlignmentInfo: isSpanning: " << ai.isSpanning() << '\n';
    dumpSeq("Contig",ai.contigSeq,os);
    dumpSeq("Rev Contig",ai.revContigSeq,os);
    os << "bp1 contig offset = " << ai.bp1ContigOffset << " bp1 contig reversed = " << ai._bp1ContigReversed << '\n';
    os << "bp2 contig offset = " << ai.bp2ContigOffset << " bp2 contig reversed = " << ai._bp2ContigReversed << '\n';
    dumpSeq("bp1RefSeq",ai.bp1RefSeq,os);
    if (ai.isSpanning())
    {
        dumpSeq("bp2RefSeq",ai.bp2RefSeq,os);
    }
    os << "bp1 reference offset = " << ai.bp1RefOffset << '\n';
    os << "bp2 reference offset = " << ai.bp2RefOffset << '\n';
    return os;
}



std::ostream&
operator<<(
    std::ostream& os,
    const SVSampleAlleleInfo& sai)
{
    static const char indent('\t');
    os << "SVSampleAlleleInfo:\n"
       << indent << "confidentSpanningPairCount: " << sai.confidentSpanningPairCount << '\n'
       << indent << "confidentSemiMappedSpanningPairCount: " << sai.confidentSemiMappedSpanningPairCount << '\n'
       << indent << "splitReadCount: " << sai.splitReadCount << '\n'
       << indent << "confidentSplitReadCount: " << sai.confidentSplitReadCount << '\n'
       ;
    return os;
}



std::ostream&
operator<<(
    std::ostream& os,
    const SVSampleInfo& si)
{
    os << "SVSampleInfo:\n"
       << "Alt Allele " << si.alt
       << "Ref Allele " << si.ref
       ;
    return os;
}



std::ostream&
operator<<(
    std::ostream& os,
    const SVScoreInfo& ssi)
{
    os << "SVScoreInfo bp1MaxDepth=" << ssi.bp1MaxDepth << " bp2MaxDepth=" << ssi.bp2MaxDepth << '\n'
       << "SVScoreInfo bp1MQ0Frac=" << ssi.bp1MQ0Frac << " bp2MQ0Frac=" << ssi.bp2MQ0Frac << '\n';

    for (const auto& sample : ssi.samples)
    {
        os << "Sample info " << sample;
    }
    return os;
}

