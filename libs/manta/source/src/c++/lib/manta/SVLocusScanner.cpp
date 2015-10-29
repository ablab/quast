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

#include "blt_util/align_path_util.hh"
#include "blt_util/parse_util.hh"
#include "blt_util/string_util.hh"
#include "common/Exceptions.hh"
#include "htsapi/align_path_bam_util.hh"
#include "htsapi/bam_record_util.hh"
#include "htsapi/SimpleAlignment_bam_util.hh"
#include "manta/RemoteMateReadUtil.hh"
#include "manta/SVCandidateUtil.hh"
#include "manta/SVLocusScanner.hh"
#include "manta/SVLocusScannerSemiAligned.hh"

// #define DEBUG_SCANNER

//#define DEBUG_IS_SHADOW

#ifdef DEBUG_SCANNER
#include "blt_util/log.hh"
#include <iostream>
#endif


/// used for classifying fragments based on size so that they can be treated differently
///
namespace FragmentSizeType
{
static const float closePairFactor(4); ///< fragments within this factor of the minimum size cutoff are treated as 'close' pairs and receive a modified evidence count
static const float veryClosePairFactor(1.5); ///< fragments within this factor of the minimum size cutoff are treated as 'reallyClose' pairs and receive a modified evidence count
static const float maxNormalFactor(1.5);

static const float minLargeEventRegionFactor(10);
static const float maxLargeEventRegionFactor(20);


static
index_t
classifySize(
    const SVLocusScanner::CachedReadGroupStats& rgStats,
    const int fragmentSize)
{
    if (fragmentSize < rgStats.properPair.min) return COMPRESSED;
    if (fragmentSize > rgStats.properPair.max)
    {
        if (fragmentSize < rgStats.minDistantFragmentSize) return CLOSE;
        return DISTANT;
    }
    return NORMAL;
}

static
bool
isLarge(const index_t i)
{
    switch (i)
    {
    case NORMAL:
    case COMPRESSED:
        return false;
    default:
        return true;
    }
}
}



static
SVObservation
GetSplitSVCandidate(
    const ReadScannerDerivOptions& dopt,
    const int32_t alignTid,
    const pos_t leftPos,
    const pos_t rightPos,
    const SVEvidenceType::index_t& svSource,
    const FRAGSOURCE::index_t& fragSource,
    const bool isComplex = false)
{
    SVObservation sv;
    SVBreakend& localBreakend(sv.bp1);
    SVBreakend& remoteBreakend(sv.bp2);

    localBreakend.interval.tid = alignTid;
    remoteBreakend.interval.tid = alignTid;

    localBreakend.lowresEvidence.add(svSource);
    sv.evtype = svSource;
    sv.fragSource = fragSource;

    if (! isComplex)
    {
        remoteBreakend.lowresEvidence.add(svSource);
        localBreakend.state = SVBreakendState::RIGHT_OPEN;
        remoteBreakend.state = SVBreakendState::LEFT_OPEN;
    }
    else
    {
        localBreakend.state = SVBreakendState::COMPLEX;
        remoteBreakend.state = SVBreakendState::UNKNOWN;
    }

    localBreakend.interval.range.set_begin_pos(std::max(0,leftPos-dopt.beforeBreakend));

    if (! isComplex)
    {
        localBreakend.interval.range.set_end_pos(leftPos+dopt.afterBreakend);
    }
    else
    {
        localBreakend.interval.range.set_end_pos(rightPos+dopt.afterBreakend);
    }

    remoteBreakend.interval.range.set_begin_pos(std::max(0,rightPos-dopt.beforeBreakend));
    remoteBreakend.interval.range.set_end_pos(rightPos+dopt.afterBreakend);

    return sv;
}



/// determine, based on clipping in the cigar string, if this split alignment
/// has its breakpoint on the downstream (right) end or the upstream (left) end
static
bool
isSplitOpenDownstream(
    const ALIGNPATH::path_t& align)
{
    using namespace ALIGNPATH;
    ///TODO replace this heuristic with a better check (looking at all SA alignments at once)
    return (apath_clip_lead_size(align) < apath_clip_trail_size(align));
}



static
void
updateSABreakend(
    const ReadScannerDerivOptions& dopt,
    const SimpleAlignment& align,
    SVBreakend& breakend)
{
    // Need to use the match descriptors to determine if the split is upstream (i.e. 5' assuming fwd strand)
    // of the current alignment (i.e. we are clipped on the left side) or downstream
    // Below is the logic to convert these  to breakend candidates (everything is relative to the forward strand):
    //
    // DownStream => RIGHT_OPEN
    // Upstream => LEFT_OPEN
    //

    const bool isSplitDownstream(isSplitOpenDownstream(align.path));

    if (isSplitDownstream)
    {
        breakend.state = SVBreakendState::RIGHT_OPEN;
    }
    else
    {
        breakend.state = SVBreakendState::LEFT_OPEN;
    }

    breakend.interval.tid = align.tid;
    // get the position of the breakend implied by the split, if split
    // is downstream (see above) the split position is the end of this split
    // read segment
    int pos = align.pos;
    if (isSplitDownstream)
    {
        using namespace ALIGNPATH;
        pos += apath_ref_length(align.path);
    }
    breakend.interval.range.set_begin_pos(std::max(0,pos-dopt.beforeBreakend));
    breakend.interval.range.set_end_pos(pos+dopt.afterBreakend);
}



/// get SV candidates from SA-tag split-read alignment
static
SVObservation
GetSplitSACandidate(
    const ReadScannerDerivOptions& dopt,
    const bam_record& localRead,
    const SimpleAlignment& localAlign,
    const SimpleAlignment& remoteAlign,
    const FRAGSOURCE::index_t fragSource)
{
    using namespace SVEvidenceType;
    static const index_t svSource(SPLIT_ALIGN);

    SVObservation sv;
    sv.evtype = svSource;
    sv.fragSource = fragSource;

    SVBreakend& localBreakend(sv.bp1);
    SVBreakend& remoteBreakend(sv.bp2);

    // use single-side evidence, have to read the supp read to get the
    // reverse edge. this protects against double-count:
    localBreakend.lowresEvidence.add(svSource);

    updateSABreakend(dopt, localAlign, localBreakend);
    updateSABreakend(dopt, remoteAlign, remoteBreakend);

    // If the local (bp1) alignment is split downstream (on the right side) then this read goes from bp1 -> bp2.
    // If it is a forward read (e.g. read1 on + strand), this means it's a forward read for this event.
    const bool isSplitDownstream(isSplitOpenDownstream(localAlign.path));
    const bool isReadFw = (localRead.is_first() == localRead.is_fwd_strand());
    if (dopt.isStranded)
    {
        if (isReadFw == isSplitDownstream)
        {
            sv.fwReads += 1;
        }
        else
        {
            sv.rvReads += 1;
        }
    }
    return sv;
}



typedef std::map<std::string, int32_t> chromMap_t;



static
void
parseSACandidatesFromRead(
    const ReadScannerOptions& opt,
    const bam_record& bamRead,
    const chromMap_t& chromToIndex,
    std::vector<SimpleAlignment>& splitAlign)
{
    using namespace ALIGNPATH;

    splitAlign.clear();

    std::vector<std::string> saVec;
    {
        static const char satag[] = {'S','A'};
        const char* saStr(bamRead.get_string_tag(satag));
        if (nullptr == saStr) return;

        split_string(saStr, ';', saVec);
        if ( (! saVec.empty()) && saVec.back().empty())
        {
            saVec.pop_back();
        }
    }

    // Only handle a single split alignment right now.
    // In the future we could sort the SA tags by order on the template, possibly
    // also removing segments that map to two different areas,

    if (saVec.size() > 1) return;

    for (const std::string& sa : saVec)
    {
#ifdef DEBUG_SCANNER
        log_os << __FUNCTION__ << ": SA STRING: " << sa << "\n";
#endif
        std::vector<std::string> saDat;
        split_string(sa, ',', saDat);

        assert((saDat.size() == 6) && "Unexpected number of SA tag values");

        /// filter split reads with low MAPQ:
        const unsigned saMapq(illumina::blt_util::parse_unsigned_str(saDat[4]));
        if (saMapq < opt.minMapq) continue;

        const chromMap_t::const_iterator ci(chromToIndex.find(saDat[0]));
        assert(ci != chromToIndex.end());

        splitAlign.emplace_back();
        SimpleAlignment& sal(splitAlign.back());
        sal.tid=(ci->second); // convert chr to int32_t via new bam header map
        sal.pos = (illumina::blt_util::parse_int_str(saDat[1])-1);
        {
            const char saStrand(saDat[2][0]); // convert to char
            assert((saStrand=='-') || (saStrand=='+'));
            sal.is_fwd_strand = (saStrand == '+');
        }

        cigar_to_apath(saDat[3].c_str(), sal.path);
    }
}



static
void
getSACandidatesFromRead(
    const ReadScannerOptions& opt,
    const ReadScannerDerivOptions& dopt,
    const bam_record& localRead,
    const SimpleAlignment& localAlign,
    const FRAGSOURCE::index_t fragSource,
    const chromMap_t& chromToIndex,
    std::vector<SVObservation>& candidates)
{
    using namespace ALIGNPATH;

    std::vector<SimpleAlignment> remoteAlign;
    parseSACandidatesFromRead(opt, localRead, chromToIndex, remoteAlign);

    if (remoteAlign.empty()) return;

    // Only handle a single split alignment right now.
    // In the future we could sort the SA tags by order on the template, possibly
    // also removing segments that map to two different areas,
    if (remoteAlign.size() > 1) return;

    for (const auto& ral : remoteAlign)
    {
        candidates.push_back(GetSplitSACandidate(dopt, localRead, localAlign, ral, fragSource));
#ifdef DEBUG_SCANNER
        log_os << __FUNCTION__ << ": evaluating SA sv for inclusion: " << candidates.data.back() << "\n";
#endif
    }
}



/// extract large indels in alignment cigar string to internal candidate format
static
void
getSVCandidatesFromReadIndels(
    const ReadScannerOptions& opt,
    const ReadScannerDerivOptions& dopt,
    const SimpleAlignment& align,
    const FRAGSOURCE::index_t fragSource,
    std::vector<SVObservation>& candidates)
{
    using namespace SVEvidenceType;
    static const index_t svSource(CIGAR);

    using namespace ALIGNPATH;
    const std::pair<unsigned,unsigned> ends(get_match_edge_segments(align.path));

    unsigned pathIndex(0);
    unsigned readOffset(0);
    pos_t refHeadPos(align.pos);

    const unsigned pathSize(align.path.size());
    while (pathIndex<pathSize)
    {
        const path_segment& ps(align.path[pathIndex]);
        const bool isBeginEdge(pathIndex<ends.first);
        const bool isEndEdge(pathIndex>ends.second);
        const bool isEdgeSegment(isBeginEdge || isEndEdge);

        // in this case, swap means combined insertion/deletion
        const bool isSwapStart(is_segment_swap_start(align.path,pathIndex));

        if (isEdgeSegment && isSwapStart)
        {
            using namespace illumina::common;

            std::ostringstream oss;
            oss << "Can't process unexpected alignment pattern: " << align << "\n";
            BOOST_THROW_EXCEPTION(LogicException(oss.str()));
        }

        unsigned nPathSegments(1); // number of path segments consumed
        if (isEdgeSegment)
        {
            // edge inserts are allowed for intron adjacent and grouper reads, edge deletions for intron adjacent only

            if (ps.type == INSERT)
            {
                if (ps.length >= opt.minCandidateVariantSize)
                {
                    static const bool isComplex(true);
                    candidates.push_back(GetSplitSVCandidate(dopt, align.tid, refHeadPos, refHeadPos, svSource, fragSource, isComplex));
                }
            }
        }
        else if (isSwapStart)
        {
            const swap_info sinfo(align.path,pathIndex);
            if ((sinfo.delete_length >= opt.minCandidateVariantSize) || (sinfo.insert_length >= opt.minCandidateVariantSize))
            {
                candidates.push_back(GetSplitSVCandidate(dopt, align.tid, refHeadPos, refHeadPos+sinfo.delete_length, svSource, fragSource));
            }

            nPathSegments = sinfo.n_seg;
        }
        else if (is_segment_type_indel(align.path[pathIndex].type))
        {
            // regular indel:

            if (ps.type == DELETE)
            {
                if (ps.length >= opt.minCandidateVariantSize)
                {
                    candidates.push_back(GetSplitSVCandidate(dopt, align.tid, refHeadPos, refHeadPos+ps.length, svSource, fragSource));
                }
            }
            else if (ps.type == INSERT)
            {
                if (ps.length >= opt.minCandidateVariantSize)
                {
                    candidates.push_back(GetSplitSVCandidate(dopt, align.tid, refHeadPos, refHeadPos, svSource, fragSource));
                }
            }
        }

        for (unsigned i(0); i<nPathSegments; ++i)
        {
            increment_path(align.path, pathIndex, readOffset, refHeadPos);
        }
    }
}



/// extract poorly aligned read ends (semi-aligned and/or soft-clipped)
/// to internal candidate format
static
void
getSVCandidatesFromSemiAligned(
    const ReadScannerOptions& opt,
    const ReadScannerDerivOptions& dopt,
    const bam_record& bamRead,
    const SimpleAlignment& bamAlign,
    const FRAGSOURCE::index_t fragSource,
    const reference_contig_segment& refSeq,
    std::vector<SVObservation>& candidates)
{
    unsigned leadingMismatchLen(0);
    unsigned trailingMismatchLen(0);
    pos_t leadingRefPos(0), trailingRefPos(0);
    getSVBreakendCandidateSemiAligned(bamRead, bamAlign, refSeq,
                                      dopt.isUseOverlappingPairs,
                                      leadingMismatchLen, leadingRefPos,
                                      trailingMismatchLen, trailingRefPos);

    if ((leadingMismatchLen + trailingMismatchLen) >= bamRead.read_size()) return;

    using namespace SVEvidenceType;
    static const index_t svSource(SEMIALIGN);

    // semi-aligned reads don't define a full hypothesis, so they're always evidence for a 'complex' ie. undefined, event
    // in a fashion analogous to clipped reads
    static const bool isComplex(true);

    if (leadingMismatchLen >= opt.minSemiAlignedMismatchLen)
    {
        const pos_t pos(leadingRefPos);
        candidates.push_back(GetSplitSVCandidate(dopt,bamRead.target_id(),pos,pos,svSource, fragSource,isComplex));
    }

    if (trailingMismatchLen >= opt.minSemiAlignedMismatchLen)
    {
        const pos_t pos(trailingRefPos);
        candidates.push_back(GetSplitSVCandidate(dopt,bamRead.target_id(),pos,pos,svSource, fragSource,isComplex));
    }
}



/// local utility class to analyze read pair relationship as lazily as possible
struct AlignmentPairAnalyzer
{
    AlignmentPairAnalyzer(
        const ReadScannerOptions& opt,
        const ReadScannerDerivOptions& dopt,
        const SVLocusScanner::CachedReadGroupStats& rstats)
        : _opt(opt),
          _dopt(dopt),
          _rstats(rstats)
    {}

    void
    reset(
        const SimpleAlignment& local,
        const SimpleAlignment& remote,
        const bool isRemoteObserved,
        const bool isForward) //Is the local read 1st in pair
    {
        _local= &local;
        _remote= &remote;
        _isRemote = isRemoteObserved;
        _isForward = isForward;
        _isScale = false;
        _scale = 0;
        _totalNonInsertSize = 0;
        _localEndRefPos = 0;
        _remoteEndRefPos = 0;
    }

    /// returns true if scale is valid (ie. the pair is anomalous)
    bool
    computeLargeEventRegionScale()
    {
        assert(isInit());
        if (! _isScale) setLargeEventRegionScale();
        return (_scale >= 0.);
    }

    void
    getSVObservation(
        SVObservation& sv)
    {
        assert(_isScale);
        assert((_scale >= 0.) && (_scale <= 1.));

        using namespace SVEvidenceType;
        static const index_t svLocalPair(LOCAL_PAIR);
        static const index_t svPair(PAIR);

        sv.evtype = svLocalPair;
        sv.fragSource = FRAGSOURCE::PAIR;

        SVBreakend& localBreakend(sv.bp1);
        SVBreakend& remoteBreakend(sv.bp2);

        localBreakend.lowresEvidence.add(svLocalPair);

        if (_dopt.isStranded)
        {
            if (_isForward)
            {
                sv.fwReads++;
            }
            else
            {
                sv.rvReads++;
            }
        }
        if (_isRemote)
        {
            remoteBreakend.lowresEvidence.add(svLocalPair);
            localBreakend.lowresEvidence.add(svPair);
            remoteBreakend.lowresEvidence.add(svPair);
            sv.evtype = svPair;
        }

        // set state and interval for each breakend:
        const double breakendRegionMax(
            (_scale*_rstats.largeScaleEventBreakendRegion.max) +
            ((1.-_scale)*_rstats.breakendRegion.max));

        const pos_t breakendSize(std::max(
                                     static_cast<pos_t>(_opt.minPairBreakendSize),
                                     static_cast<pos_t>(breakendRegionMax-_totalNonInsertSize)));

        const pos_t localStartRefPos(localAlign().pos);
        const pos_t remoteStartRefPos(remoteAlign().pos);

        localBreakend.interval.tid = localAlign().tid;
        // expected breakpoint range is from the end of the localRead alignment to the (probabilistic) end of the fragment:
        if (localAlign().is_fwd_strand)
        {
            localBreakend.state = SVBreakendState::RIGHT_OPEN;
            localBreakend.interval.range.set_begin_pos(_localEndRefPos);
            localBreakend.interval.range.set_end_pos(_localEndRefPos + breakendSize);
        }
        else
        {
            localBreakend.state = SVBreakendState::LEFT_OPEN;
            localBreakend.interval.range.set_end_pos(localStartRefPos);
            localBreakend.interval.range.set_begin_pos(localStartRefPos - breakendSize);
        }

        remoteBreakend.interval.tid = remoteAlign().tid;
        if (remoteAlign().is_fwd_strand)
        {
            remoteBreakend.state = SVBreakendState::RIGHT_OPEN;
            remoteBreakend.interval.range.set_begin_pos(_remoteEndRefPos);
            remoteBreakend.interval.range.set_end_pos(_remoteEndRefPos + breakendSize);
        }
        else
        {
            remoteBreakend.state = SVBreakendState::LEFT_OPEN;
            remoteBreakend.interval.range.set_end_pos(remoteStartRefPos);
            remoteBreakend.interval.range.set_begin_pos(remoteStartRefPos - breakendSize);
        }
    }

    /// return the amount of unaligned sequence proceding the pair insert:
    static
    unsigned
    distanceFromInsert(
        const SimpleAlignment& al)
    {
        if (al.is_fwd_strand) return apath_read_trail_size(al.path);
        else                  return apath_read_lead_size(al.path);
    }

private:

    static
    unsigned
    getNonInsertSize(
        const SimpleAlignment& al)
    {
        const unsigned readSize(apath_read_length(al.path));
        return readSize - distanceFromInsert(al);
    }

    static
    pos_t
    getEndPos(
        const SimpleAlignment& al)
    {
        return (al.pos + apath_ref_length(al.path));
    }


    void
    setLargeEventRegionScale()
    {
        // different breakend sizes are used for long-range pairings vs short-ish range deletions,
        // because of occasional long-fragment noise. This ramps from 0 to 1 as we go from short to
        // long deletions sizes:
        _isScale = true;
        _scale = 1.0;

        // find the read size excluding soft-clip/edge-insert on the 'inside' of the fragment
        const unsigned localNoninsertSize(getNonInsertSize(localAlign()));
        const unsigned remoteNoninsertSize(getNonInsertSize(remoteAlign()));

        // total the 'used' read span of read1 and read2 (ie. the elements of the
        // fragment that are not part of the insert between the reads)
        //
        _totalNonInsertSize = (localNoninsertSize+remoteNoninsertSize);

        const pos_t localStartRefPos(localAlign().pos);
        const pos_t remoteStartRefPos(remoteAlign().pos);
        _localEndRefPos=getEndPos(localAlign());
        _remoteEndRefPos=getEndPos(remoteAlign());

        // check if fragment size is still anomalous after accounting for read alignment patterns:
        if ((localAlign().tid != remoteAlign().tid) ||
            (localAlign().is_fwd_strand == remoteAlign().is_fwd_strand)) return;

        known_pos_range2 insertRange;
        if (localAlign().is_fwd_strand)
        {
            insertRange.set_range(_localEndRefPos, remoteStartRefPos);
        }
        else
        {
            insertRange.set_range(_remoteEndRefPos, localStartRefPos);
        }

        // get length of fragment after accounting for any variants described directly in either read alignment:
        // note insertRange can be negative, so don't use insertRange.size()
        const pos_t cigarAdjustedFragmentSize(_totalNonInsertSize + (insertRange.end_pos() - insertRange.begin_pos()));

        // this is an arbitrary point to start officially tagging 'outties' -- for now  we just want to avoid conventional small fragments from FFPE
        const bool isOuttie(cigarAdjustedFragmentSize < 0);

        if (isOuttie) return;

        const bool isLargeFragment(cigarAdjustedFragmentSize > (_rstats.properPair.max + _opt.minCandidateVariantSize));

        if (isLargeFragment)
        {
            _scale = _rstats.largeEventRegionScaler.getScale(cigarAdjustedFragmentSize);
        }
        else
        {
            _scale = -1.;
        }
    }

    bool
    isInit() const
    {
        return (_local != nullptr);
    }

    const SimpleAlignment&
    localAlign() const
    {
        return *_local;
    }

    const SimpleAlignment&
    remoteAlign() const
    {
        return *_remote;
    }

    const ReadScannerOptions& _opt;
    const ReadScannerDerivOptions& _dopt;
    const SVLocusScanner::CachedReadGroupStats& _rstats;
    const SimpleAlignment* _local = nullptr;
    const SimpleAlignment* _remote = nullptr;
    bool _isRemote = false;
    bool _isForward = false;
    bool _isScale = false;
    double _scale = 0.;
    unsigned _totalNonInsertSize = 0;
    pos_t _localEndRefPos = 0;
    pos_t _remoteEndRefPos = 0;
};



/// get SV candidates from anomalous read pairs
static
void
getSVCandidatesFromPair(
    const ReadScannerOptions& opt,
    const ReadScannerDerivOptions& dopt,
    const SVLocusScanner::CachedReadGroupStats& rstats,
    const bam_record& localRead,
    const SimpleAlignment& localAlign,
    const bam_record* remoteReadPtr,
    std::vector<SVObservation>& candidates)
{
    if (! localRead.is_paired()) return;

    // don't count paired end evidence from SA-split reads twice:
    if (localRead.isNonStrictSupplement()) return;

    if (localRead.is_unmapped() || localRead.is_mate_unmapped()) return;

    // special case typically used for RNA-Seq analysis:
    if (opt.isIgnoreAnomProperPair && localRead.is_proper_pair()) return;

    // abstract remote alignment to SimpleAlignment object:
    const bool isRemote(nullptr != remoteReadPtr);
    const SimpleAlignment remoteAlign(isRemote ? getAlignment(*remoteReadPtr) : getFakeMateAlignment(localRead));

    AlignmentPairAnalyzer pairInspector(opt, dopt, rstats);
    pairInspector.reset(localAlign, remoteAlign, isRemote, localRead.is_first());

    if (! pairInspector.computeLargeEventRegionScale()) return;

    candidates.emplace_back();
    pairInspector.getSVObservation(candidates.back());

#ifdef DEBUG_SCANNER
    log_os << __FUNCTION__ << " evaluating pair sv for inclusion: " << candidates.back() << "\n";
#endif
}



#if 0
/// get SV candidates from shadow/singleton pairs
/// look for singletons, create candidateSV around conf. interval of shadow position
/// cache singletons? might be needed to remove poor quality shadows.
/// should be able to re-use code, follow soft-clipping example.
static
void
getSVCandidatesFromShadow(
    const ReadScannerOptions& opt,
    const SVLocusScanner::CachedReadGroupStats& rstats,
    const bam_record& localRead,
    const SimpleAlignment& localAlign,
    const bam_record* remoteReadPtr,
    TrackedCandidates& candidates)
{
    using namespace SVEvidenceType;
    static const index_t svSource(SHADOW);

    static const bool isComplex(true);
    pos_t singletonGenomePos(0);
    int targetId(0);
    if (NULL == remoteReadPtr)
    {
        if (!localRead.is_unmapped()) return;
        // need to take care of this case
        // need to rely on cached mapq and qname
        return;
        if (!isGoodShadow(localRead,lastMapq,lastQname,opt.minSingletonMapqGraph))
        {
            return;
        }
        singletonGenomePos = localAlign.pos;
        targetId           = localRead.target_id();
    }
    else
    {
        // have both reads, straightforward from here
        const bam_record& remoteRead(*remoteReadPtr);
        const SimpleAlignment remoteAlign(remoteRead);

        if (localRead.is_mate_unmapped())
        {
            // remote read is shadow candidate
            if (!isGoodShadow(remoteRead,localRead.map_qual(),localRead.qname(),opt.minSingletonMapqGraph))
            {
                return;
            }
            singletonGenomePos = localAlign.pos;
            targetId = remoteRead.target_id();
        }
        else if (localRead.is_unmapped())
        {
            // local is shadow candidate
            if (!isGoodShadow(localRead,remoteRead.map_qual(),remoteRead.qname(),opt.minSingletonMapqGraph))
            {
                return;
            }
            singletonGenomePos = remoteAlign.pos;
            targetId = localRead.target_id();
        }
        else
        {
            // none unmapped, skip this one
            return;
        }
    }
    const pos_t properPairRangeOffset = static_cast<pos_t>(rstats.properPair.min + (rstats.properPair.max-rstats.properPair.min)/2);
    const pos_t shadowGenomePos = singletonGenomePos + properPairRangeOffset;
    candidates.push_back(GetSplitSVCandidate(opt,targetId,shadowGenomePos,shadowGenomePos, svSource, isComplex));
}
#endif



static
void
getSingleReadSVCandidates(
    const ReadScannerOptions& opt,
    const ReadScannerDerivOptions& dopt,
    const bam_record& localRead,
    const SimpleAlignment& localAlign,
    const chromMap_t& chromToIndex,
    const reference_contig_segment& refSeq,
    std::vector<SVObservation>& candidates)
{
    using namespace illumina::common;

    const bool isRead2(localRead.is_paired() && (localRead.read_no() == 2));
    const FRAGSOURCE::index_t fragSource(isRead2 ? FRAGSOURCE::READ2 : FRAGSOURCE::READ1);

    // - process any large indels in the localRead:
    getSVCandidatesFromReadIndels(opt, dopt, localAlign, fragSource, candidates);
#ifdef DEBUG_SCANNER
    log_os << __FUNCTION__ << ": post-indels candidate_size: " << candidates.size() << "\n";
#endif

    // a read can provide SA split evidence or semi-aligned/soft-clip, but not both.
    // this prevents split reads from triggering spurious local assembles. It is
    // possible for a read to genuinely contain evidence of both, but this should
    // be very rare.
    if (localRead.isSASplit())
    {
        getSACandidatesFromRead(opt, dopt, localRead, localAlign, fragSource, chromToIndex,
                                candidates);
#ifdef DEBUG_SCANNER
        log_os << __FUNCTION__ << ": post-split read candidate_size: " << candidates.size() << "\n";
#endif
    }
    else
    {
        if (dopt.isSmallCandidates)
        {
            getSVCandidatesFromSemiAligned(opt, dopt, localRead, localAlign, fragSource, refSeq,
                                           candidates);
        }
#ifdef DEBUG_SCANNER
        log_os << __FUNCTION__ << ": post-semialigned candidate_size: " << candidates.size() << "\n";
#endif
    }
}



/// scan read record (and optionally its mate record) for SV evidence.
//
/// note that estimation is improved by the mate record (because we have the mate cigar string in this case)
///
static
void
getReadBreakendsImpl(
    const ReadScannerOptions& opt,
    const ReadScannerDerivOptions& dopt,
    const SVLocusScanner::CachedReadGroupStats& rstats,
    const bam_record& localRead,
    const bam_record* remoteReadPtr,
    const bam_header_info& bamHeader,
    const reference_contig_segment& localRefSeq,
    const reference_contig_segment* remoteRefSeqPtr,
    std::vector<SVObservation>& candidates,
    known_pos_range2& localEvidenceRange)
{
    using namespace illumina::common;

#ifdef DEBUG_SCANNER
    log_os << __FUNCTION__ << ": Starting read: " << localRead.qname() << "\n";
#endif

    const chromMap_t& chromToIndex(bamHeader.chrom_to_index);

    candidates.clear();

    /// get some basic derived information from the bam_record:
    const SimpleAlignment localAlign(getAlignment(localRead));

    try
    {
        getSingleReadSVCandidates(opt, dopt, localRead, localAlign, chromToIndex,
                                  localRefSeq, candidates);

        // run the same check on the read's mate if we have access to it
        if (nullptr != remoteReadPtr)
        {
            const bam_record& remoteRead(*remoteReadPtr);
            const SimpleAlignment remoteAlign(getAlignment(remoteRead));

            if (nullptr == remoteRefSeqPtr)
            {
                static const char msg[] = "ERROR: remoteRefSeqPtr cannot be null";
                BOOST_THROW_EXCEPTION(LogicException(msg));
            }
            getSingleReadSVCandidates(opt, dopt, remoteRead, remoteAlign,
                                      chromToIndex, (*remoteRefSeqPtr),
                                      candidates);
        }

        // process shadows:
        //getSVCandidatesFromShadow(opt, rstats, localRead, localAlign,remoteReadPtr,candidates);

        // - process anomalous read pairs:
        getSVCandidatesFromPair(opt, dopt, rstats, localRead, localAlign, remoteReadPtr,
                                candidates);
    }
    catch (...)
    {
        std::cerr << "ERROR: Exception caught while processing ";
        if (nullptr == remoteReadPtr)
        {
            std::cerr << "single read record:\n"
                      << '\t' << localRead << "\n";
        }
        else
        {
            std::cerr << " read pair records:\n"
                      << '\t'  << localRead << "\n"
                      << '\t' << (*remoteReadPtr) << "\n";
        }
        throw;
    }

#ifdef DEBUG_SCANNER
    log_os << __FUNCTION__ << ": post-pair candidate_size: " << candidates.size() << "\n";
#endif

    // update localEvidence range:
    // note this is only used if candidates were added, so there's no harm in setting it every time:
    const unsigned localRefLength(apath_ref_length(localAlign.path));
    const pos_t startRefPos(localRead.pos()-1);
    const pos_t endRefPos(startRefPos+localRefLength);

    localEvidenceRange.set_range(startRefPos,endRefPos);

    const int maxTid(chromToIndex.size());

    /// final chance to QC candidate set:
    ///
    for (const SVCandidate& sv : candidates)
    {
        bool isInvalidTid(false);
        if ((sv.bp1.interval.tid < 0) || (sv.bp1.interval.tid >= maxTid))
        {
            isInvalidTid=true;
        }
        else if (sv.bp2.state != SVBreakendState::UNKNOWN)
        {
            if ((sv.bp2.interval.tid < 0) || (sv.bp2.interval.tid >= maxTid))
            {
                isInvalidTid=true;
            }
        }

        bool isInvalidPos(false);
        if (! isInvalidTid)
        {
            // note in the 'off-chromosome edge' test below we check for cases which are obviously way off
            // the edge, but allow for a bit of over-edge mistakes to occur for the circular chromosomes
            //
            static const int offEdgePad(500);
            const pos_t tid1Length(bamHeader.chrom_data[sv.bp1.interval.tid].length);
            if ((sv.bp1.interval.range.end_pos() <= -offEdgePad) || (sv.bp1.interval.range.begin_pos() >= (tid1Length+offEdgePad)))
            {
                isInvalidPos=true;
            }
            else if (sv.bp2.state != SVBreakendState::UNKNOWN)
            {
                const pos_t tid2Length(bamHeader.chrom_data[sv.bp2.interval.tid].length);
                if ((sv.bp2.interval.range.end_pos() <= -offEdgePad) || (sv.bp2.interval.range.begin_pos() >= (tid2Length+offEdgePad)))
                {
                    isInvalidPos=true;
                }
            }
        }

        if (isInvalidTid || isInvalidPos)
        {
            std::ostringstream oss;
            if (isInvalidTid)
            {
                oss << "SVbreakend has unknown or invalid chromosome id in candidate sv.\n";
            }
            else
            {
                oss << "Cannot interpret BAM record: candidate SV breakend from BAM record is off chromosome edge.\n";
            }

            oss << "\tlocal_bam_record: " <<  localRead << "\n"
                << "\tremote_bam record: ";
            if (NULL==remoteReadPtr)
            {
                oss << "NONE";
            }
            else
            {
                oss << (*remoteReadPtr);
            }
            oss << "\n"
                << "\tSVCandidate: " << sv << "\n";
            BOOST_THROW_EXCEPTION(LogicException(oss.str()));
        }
    }
}



/// Create an SVLocus for each potential SV event supported by the BAM record
///
/// the loci count should almost always be one (or, depending on input filtration, zero).
/// multiple suggested loci from one read is more of a theoretical possibility than an
/// expectation.
///
static
void
getSVLociImpl(
    const ReadScannerOptions& opt,
    const ReadScannerDerivOptions& dopt,
    const SVLocusScanner::CachedReadGroupStats& rstats,
    const bam_record& bamRead,
    const bam_header_info& bamHeader,
    const reference_contig_segment& refSeq,
    std::vector<SVLocus>& loci,
    SampleEvidenceCounts& eCounts)
{
    using namespace illumina::common;

    loci.clear();
    std::vector<SVObservation> candidates;
    known_pos_range2 localEvidenceRange;

    getReadBreakendsImpl(opt, dopt, rstats, bamRead, nullptr, bamHeader,
                         refSeq, nullptr, candidates, localEvidenceRange);

#ifdef DEBUG_SCANNER
    log_os << __FUNCTION__ << ": candidate_size: " << candidates.size() << "\n";
#endif

    // translate SVCandidate to a simpler form for use
    // in the SV locus graph:
    for (const SVCandidate& cand : candidates)
    {
        const bool isCandComplex(isComplexSV(cand));

        const SVBreakend& localBreakend(cand.bp1);
        const SVBreakend& remoteBreakend(cand.bp2);

        if ((0==localBreakend.interval.range.size()) ||
            ((! isCandComplex) && (0==remoteBreakend.interval.range.size())))
        {
            std::ostringstream oss;
            oss << "Unexpected breakend pattern proposed from bam record.\n"
                << "\tlocal_breakend: " << localBreakend << "\n"
                << "\tremote_breakend: " << remoteBreakend << "\n"
                << "\tbam_record: " << bamRead << "\n";
            BOOST_THROW_EXCEPTION(LogicException(oss.str()));
        }

        // update evidence stats:
        for (int i(0); i< SVEvidenceType::SIZE; ++i)
        {
            eCounts.eType[i] += localBreakend.lowresEvidence.getVal(i);
        }

        // determine the evidence weight of this candidate:
        unsigned localEvidenceWeight(0);
        unsigned remoteEvidenceWeight(0);

        if (localBreakend.getAnyNonPairCount() != 0)
        {
            localEvidenceWeight = SVObservationWeights::internalReadEvent;
            if (remoteBreakend.getAnyNonPairCount() != 0)
            {
                remoteEvidenceWeight = SVObservationWeights::internalReadEvent;
            }
        }
        else if (localBreakend.getLocalPairCount() != 0)
        {
            bool isClose(false);
            if (is_innie_pair(bamRead))
            {
                isClose = (std::abs(bamRead.template_size()) < rstats.minDistantFragmentSize);
            }

            unsigned thisWeight(SVObservationWeights::readPair);
            if (isClose)
            {
                thisWeight = SVObservationWeights::closeReadPair;
                eCounts.closeCount += 1;
            }

            localEvidenceWeight = thisWeight;
            if (remoteBreakend.getLocalPairCount() != 0)
            {
                remoteEvidenceWeight = thisWeight;
            }
        }

        // finally, create the graph locus:
        SVLocus locus;
        // set local breakend estimate:
        const NodeIndexType localBreakendNode(locus.addNode(localBreakend.interval));
        locus.setNodeEvidence(localBreakendNode,localEvidenceRange);

        if (isCandComplex)
        {
            locus.linkNodes(localBreakendNode,localBreakendNode,localEvidenceWeight);
        }
        else
        {
            // set remote breakend estimate:
            const NodeIndexType remoteBreakendNode(locus.addNode(remoteBreakend.interval));
            locus.linkNodes(localBreakendNode,remoteBreakendNode,localEvidenceWeight,remoteEvidenceWeight);

            locus.mergeSelfOverlap();
        }

#ifdef DEBUG_SCANNER
        log_os << __FUNCTION__ << ": adding Locus: " << locus << "\n";
#endif
        loci.push_back(locus);
    }
}



/// compute one of the scanner's fragment ranges:
static
void
setRGRange(
    const SizeDistribution& fragStats,
    const float qprob,
    SVLocusScanner::Range& range)
{
    range.min=fragStats.quantile(qprob);
    range.max=fragStats.quantile((1-qprob));
    if (range.min<0.) range.min = 0;
    assert(range.max>0.);
}



SVLocusScanner::
SVLocusScanner(
    const ReadScannerOptions& opt,
    const std::string& statsFilename,
    const std::vector<std::string>& /*alignmentFilename*/,
    const bool isRNA,
    const bool isStranded) :
    _opt(opt),
    _dopt(opt, isRNA, isStranded)
{
    using namespace illumina::common;

    // pull in insert stats:
    _rss.load(statsFilename.c_str());

    // precompute frequently used insert stats for each rg:
    const unsigned rgCount(_rss.size());
    for (unsigned rgIndex(0); rgIndex<rgCount; rgIndex++)
    {
        /// TODO: add check that the filenames in the stats file are a complete match to alignmentFilename

        const SizeDistribution& rgDistro(getFragSizeDistro(rgIndex));

        _stats.resize(_stats.size()+1);
        CachedReadGroupStats& rgStats(_stats.back());
        setRGRange(rgDistro, _opt.breakendEdgeTrimProb, rgStats.breakendRegion);
        setRGRange(rgDistro, _opt.largeScaleEventBreakendEdgeTrimProb, rgStats.largeScaleEventBreakendRegion);
        setRGRange(rgDistro, _opt.properPairTrimProb, rgStats.properPair);
        setRGRange(rgDistro, _opt.evidenceTrimProb, rgStats.evidencePair);
        setRGRange(rgDistro, 0.05f, rgStats.fifthPerc);

        if ((rgIndex==0) || (rgStats.fifthPerc.min < _fifthPerc.min))
        {
            _fifthPerc.min = rgStats.fifthPerc.min;
        }
        if ((rgIndex==0) || (rgStats.fifthPerc.max > _fifthPerc.max))
        {
            _fifthPerc.max = rgStats.fifthPerc.max;
        }

        rgStats.shadowSearchRange = rgDistro.quantile(1-(_opt.shadowSearchRangeProb))*_opt.shadowSearchRangeFactor;

        assert(rgStats.shadowSearchRange > 0);

        rgStats.minVeryCloseFragmentSize = static_cast<int>(rgStats.properPair.max*FragmentSizeType::maxNormalFactor);
        rgStats.minCloseFragmentSize = static_cast<int>(rgStats.properPair.max*FragmentSizeType::veryClosePairFactor);
        rgStats.minDistantFragmentSize = static_cast<int>(rgStats.properPair.max*FragmentSizeType::closePairFactor);

        assert(rgStats.minDistantFragmentSize > rgStats.properPair.max);

        //rgStats.veryCloseEventScaler.init(rgStats.minVeryCloseFragmentSize, rgStats.minCloseFragmentSize);

        const int largeEventRegionMin(rgStats.properPair.max*FragmentSizeType::minLargeEventRegionFactor);
        const int largeEventRegionMax(rgStats.properPair.max*FragmentSizeType::maxLargeEventRegionFactor);

        rgStats.largeEventRegionScaler.init(largeEventRegionMin, largeEventRegionMax);
    }
}



bool
SVLocusScanner::
isProperPair(
    const bam_record& bamRead,
    const unsigned defaultReadGroupIndex) const
{
    if (! is_innie_pair(bamRead)) return false;

    const Range& ppr(_stats[defaultReadGroupIndex].properPair);
    const int32_t fragmentSize(std::abs(bamRead.template_size()));

    // we're seeing way to much large fragment garbage in cancers to use
    // vanilla proper pair criteria, push the max fragment size out a bit for now:
    static const float maxAnomFactor(1.5);
    if (fragmentSize > static_cast<int32_t>(maxAnomFactor*ppr.max)) return false;
    if (fragmentSize < ppr.min) return false;

    return true;
}



FragmentSizeType::index_t
SVLocusScanner::
_getFragmentSizeType(
    const bam_record& bamRead,
    const unsigned defaultReadGroupIndex) const
{
    using namespace FragmentSizeType;
    if (bamRead.target_id() != bamRead.mate_target_id()) return DISTANT;
    const int32_t fragmentSize(std::abs(bamRead.template_size()));
    return classifySize(_stats[defaultReadGroupIndex], fragmentSize);
}


bool
SVLocusScanner::
_isLargeFragment(
    const bam_record& bamRead,
    const unsigned defaultReadGroupIndex) const
{
    return FragmentSizeType::isLarge(_getFragmentSizeType(bamRead,defaultReadGroupIndex));
}



bool
SVLocusScanner::
isNonCompressedAnomalous(
    const bam_record& bamRead,
    const unsigned defaultReadGroupIndex) const
{
    if (! is_mapped_pair(bamRead)) return false;
    const bool isAnomalous(! isProperPair(bamRead,defaultReadGroupIndex));
    const bool isInnie(is_innie_pair(bamRead));
    const bool isLarge(_isLargeFragment(bamRead,defaultReadGroupIndex));

    // exclude innie read pairs which are anomalously short:
    return (isAnomalous && ((! isInnie) || isLarge));
}



bool
SVLocusScanner::
isLocalIndelEvidence(
    const SimpleAlignment& bamAlign) const
{
    using namespace ALIGNPATH;
    for (const path_segment& ps : bamAlign.path)
    {
        if (ps.type == INSERT || ps.type == DELETE)
        {
            if (ps.length>=_opt.minCandidateVariantSize) return true;
        }
    }
    return false;
}



bool
SVLocusScanner::
isSemiAlignedEvidence(
    const bam_record& bamRead,
    const SimpleAlignment& bamAlign,
    const reference_contig_segment& refSeq) const
{
    unsigned leadingMismatchLen(0), trailingMismatchLen(0);
    getSVBreakendCandidateSemiAlignedSimple(bamRead, bamAlign, refSeq, _dopt.isUseOverlappingPairs,
                                            leadingMismatchLen, trailingMismatchLen);
    return ((leadingMismatchLen >= _opt.minSemiAlignedMismatchLen) || (trailingMismatchLen >= _opt.minSemiAlignedMismatchLen));
}



bool
SVLocusScanner::
isLocalAssemblyEvidence(
    const bam_record& bamRead,
    const reference_contig_segment& refSeq) const
{
    const SimpleAlignment bamAlign(getAlignment(bamRead));
    if (isLocalIndelEvidence(bamAlign)) return true;
    if (isSemiAlignedEvidence(bamRead, bamAlign, refSeq)) return true;
    /// TODO Add shadow evidence -- complexity here is keeping locus merging under control due to the large breakend location variance
    /// suggested by shadows

    return false;
}



bool
SVLocusScanner::
isSVEvidence(
    const bam_record& bamRead,
    const unsigned defaultReadGroupIndex,
    const reference_contig_segment& refSeq,
    SVLocusEvidenceCount* incountsPtr) const
{
    // exclude innie read pairs which are anomalously short:
    const bool isAnom(isNonCompressedAnomalous(bamRead,defaultReadGroupIndex));
    const bool isSplit(bamRead.isSASplit());
    getAlignment(bamRead,_bamAlign);
    const bool isIndel(isLocalIndelEvidence(_bamAlign));
    const bool isAssm((_dopt.isSmallCandidates) && ((!isSplit) && isSemiAlignedEvidence(bamRead, _bamAlign, refSeq)));

    const bool isEvidence(isAnom || isSplit || isIndel || isAssm);

    if (nullptr != incountsPtr)
    {
        SVLocusEvidenceCount& incounts(*incountsPtr);
        incounts.total++;
        if (isAnom) incounts.anom++;
        if (isSplit) incounts.split++;
        if (isIndel) incounts.indel++;
        if (isAssm) incounts.assm++;

        if (! isEvidence) incounts.ignored++;

        if (isAnom)
        {
            if (isMateInsertionEvidenceCandidate(bamRead, getMinMapQ()))
            {
                // these counts are used to generate background noise rates in later candidate generation stages:
                incounts.remoteRecoveryCandidates++;
            }
        }
    }

    return isEvidence;
}



void
SVLocusScanner::
getSVLoci(
    const bam_record& bamRead,
    const unsigned defaultReadGroupIndex,
    const bam_header_info& bamHeader,
    const reference_contig_segment& refSeq,
    std::vector<SVLocus>& loci,
    SampleEvidenceCounts& eCounts) const
{
    loci.clear();

    const CachedReadGroupStats& rstats(_stats[defaultReadGroupIndex]);
    getSVLociImpl(_opt, _dopt, rstats, bamRead, bamHeader, refSeq, loci,
                  eCounts);
}



void
SVLocusScanner::
getBreakendPair(
    const bam_record& localRead,
    const bam_record* remoteReadPtr,
    const unsigned defaultReadGroupIndex,
    const bam_header_info& bamHeader,
    const reference_contig_segment& localRefSeq,
    const reference_contig_segment* remoteRefSeqPtr,
    std::vector<SVObservation>& candidates) const
{
    const CachedReadGroupStats& rstats(_stats[defaultReadGroupIndex]);

    // throw evidence range away in this case
    known_pos_range2 evidenceRange;
    getReadBreakendsImpl(_opt, _dopt, rstats, localRead, remoteReadPtr,
                         bamHeader, localRefSeq, remoteRefSeqPtr,
                         candidates, evidenceRange);
}
