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

#include "common/Exceptions.hh"
#include "htsapi/samtools_fasta_util.hh"
#include "manta/SVReferenceUtil.hh"



#if 0
static
void
trimOverlappingRange(
    known_pos_range2& rA,
    known_pos_range2& rB)
{
    // put ranges in order:
    known_pos_range2* r1(&rA);
    known_pos_range2* r2(&rB);

    if (r1->begin_pos() > r2->begin_pos()) std::swap(r1, r2);

    const pos_t overlap(r1->end_pos()-r2->begin_pos());
    if (overlap <= 0) return;

    r1->set_end_pos(r1->end_pos()-(overlap/2));
    r2->set_begin_pos(r1->end_pos());
}
#endif



/// produce the reference extraction interval only
///
/// \param leadingTrim how much was not returned from the front of the sequence compared to what was requested?
/// \param trailingTrim how much was not returned from the back of the sequence compared to what was requested?
///
static
void
getBpReferenceInterval(
    const bam_header_info& header,
    const pos_t extraRefEdgeSize,
    const GenomeInterval& bpInterval,
    GenomeInterval& refInterval,
    unsigned& leadingTrim,
    unsigned& trailingTrim)
{
    const bam_header_info::chrom_info& chromInfo(header.chrom_data[bpInterval.tid]);

    const pos_t chromSize(static_cast<pos_t>(chromInfo.length));

    assert(bpInterval.range.begin_pos() <= bpInterval.range.end_pos());
    if ((bpInterval.range.begin_pos() >= chromSize) || (bpInterval.range.end_pos() <= 0))
    {
        using namespace illumina::common;

        std::ostringstream oss;
        oss << __FUNCTION__ << ": requested reference range has no overlap with chromosome\n"
            << "\tinterval: " << bpInterval
            << "\tchromSize: " << chromSize
            << "\n";

        BOOST_THROW_EXCEPTION(LogicException(oss.str()));
    }

    pos_t beginPos(bpInterval.range.begin_pos()-extraRefEdgeSize);
    if (beginPos < 0)
    {
        leadingTrim = -beginPos;
        beginPos = 0;
    }
    else
    {
        leadingTrim = 0;
    }

    pos_t endPos(bpInterval.range.end_pos()+extraRefEdgeSize);
    if (endPos > chromSize)
    {
        trailingTrim = (endPos - chromSize);
        endPos = chromSize;
    }
    else
    {
        trailingTrim = 0;
    }

    refInterval.tid = bpInterval.tid;
    refInterval.range.set_begin_pos(beginPos);
    refInterval.range.set_end_pos(endPos);
}



/// simpler calling convention which throws away trim values
static
void
getBpReferenceInterval(
    const bam_header_info& header,
    const pos_t extraRefEdgeSize,
    const GenomeInterval& bpInterval,
    GenomeInterval& refInterval)
{
    unsigned leadingTrim;
    unsigned trailingTrim;
    getBpReferenceInterval(header, extraRefEdgeSize, bpInterval, refInterval, leadingTrim, trailingTrim);
}



/// given a reference extraction interval, produce the corresponding ref contig segment
static
void
getIntervalReferenceSegment(
    const std::string& referenceFilename,
    const bam_header_info& header,
    const GenomeInterval& refInterval,
    reference_contig_segment& intervalRefSeq)
{
    const bam_header_info::chrom_info& chromInfo(header.chrom_data[refInterval.tid]);
    const std::string& chrom(chromInfo.label);

    // get REF
    const known_pos_range2& range(refInterval.range);
    intervalRefSeq.set_offset(range.begin_pos());

    // note: begin and end pos follow Manta's closed-open bpInterval conventions (a la bedtools,
    // but the ref function below takes closed-closed endpoints, so we subract one from endPos
    get_standardized_region_seq(referenceFilename, chrom, range.begin_pos(), (range.end_pos()-1), intervalRefSeq.seq());

    if (intervalRefSeq.seq().size() != range.size())
    {
        using namespace illumina::common;

        std::ostringstream oss;
        oss << "getIntervalReferenceSegment: Unexpected reference sequence\n"
            << "\t" << referenceFilename
            << "\t" << chrom
            << "\t" << range
            << "\n";

        oss << "\texpected_size: " << range.size()
            << "\treturned_size: " << intervalRefSeq.seq().size()
            << "\n";

        BOOST_THROW_EXCEPTION(LogicException(oss.str()));
    }
}



void
getIntervalReferenceSegment(
    const std::string& referenceFilename,
    const bam_header_info& header,
    const pos_t extraRefEdgeSize,
    const GenomeInterval& bpInterval,
    reference_contig_segment& intervalRefSeq,
    unsigned& leadingTrim,
    unsigned& trailingTrim)
{
    GenomeInterval refInterval;
    getBpReferenceInterval(header, extraRefEdgeSize, bpInterval, refInterval, leadingTrim, trailingTrim);

    getIntervalReferenceSegment(referenceFilename, header, refInterval, intervalRefSeq);
}



bool
isRefRegionOverlap(
    const bam_header_info& header,
    const pos_t extraRefEdgeSize,
    const SVCandidate& sv)
{
    if (sv.bp1.interval.tid != sv.bp2.interval.tid) return false;

    GenomeInterval bp1RefInterval;
    GenomeInterval bp2RefInterval;
    getBpReferenceInterval(header, extraRefEdgeSize, sv.bp1.interval,bp1RefInterval);
    getBpReferenceInterval(header, extraRefEdgeSize, sv.bp2.interval,bp2RefInterval);

    return (bp1RefInterval.isIntersect(bp2RefInterval));
}



bool
isRefRegionValid(
    const bam_header_info& header,
    const GenomeInterval& bpInterval)
{
    const bam_header_info::chrom_info& chromInfo(header.chrom_data[bpInterval.tid]);
    const pos_t chromSize(static_cast<pos_t>(chromInfo.length));

    assert(bpInterval.range.begin_pos() <= bpInterval.range.end_pos());
    return (! ((bpInterval.range.begin_pos() >= chromSize) || (bpInterval.range.end_pos() <= 0)));
}



void
getSVReferenceSegments(
    const std::string& referenceFilename,
    const bam_header_info& header,
    const pos_t extraRefEdgeSize,
    const SVCandidate& sv,
    reference_contig_segment& bp1ref,
    reference_contig_segment& bp2ref,
    unsigned& bp1LeadingTrim,
    unsigned& bp1TrailingTrim,
    unsigned& bp2LeadingTrim,
    unsigned& bp2TrailingTrim)
{
    getIntervalReferenceSegment(referenceFilename, header, extraRefEdgeSize, sv.bp1.interval, bp1ref, bp1LeadingTrim, bp1TrailingTrim);
    getIntervalReferenceSegment(referenceFilename, header, extraRefEdgeSize, sv.bp2.interval, bp2ref, bp2LeadingTrim, bp2TrailingTrim);
}
