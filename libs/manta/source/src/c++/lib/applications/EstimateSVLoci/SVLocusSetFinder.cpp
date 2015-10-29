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

#include "SVLocusSetFinder.hh"

#include "blt_util/log.hh"
#include "htsapi/align_path_bam_util.hh"
#include "manta/ChromDepthFilterUtil.hh"

#include <iostream>



namespace STAGE
{
enum index_t
{
    HEAD,
    DENOISE,
    CLEAR_DEPTH
};


static
stage_data
getStageData(
    const unsigned denoiseBorderSize)
{
    static const unsigned clearDepthBorderSize(10);

    stage_data sd;
    sd.add_stage(HEAD);
    sd.add_stage(DENOISE, HEAD, denoiseBorderSize);
    sd.add_stage(CLEAR_DEPTH, HEAD, clearDepthBorderSize);

    return sd;
}
}



static const unsigned depthBufferCompression = 16;



SVLocusSetFinder::
SVLocusSetFinder(
    const ESLOptions& opt,
    const GenomeInterval& scanRegion,
    const bam_header_info& bamHeader,
    const reference_contig_segment& refSeq) :
    _isAlignmentTumor(opt.alignFileOpt.isAlignmentTumor),
    _scanRegion(scanRegion),
    _stageman(
        STAGE::getStageData(REGION_DENOISE_BORDER),
        pos_range(
            scanRegion.range.begin_pos(),
            scanRegion.range.end_pos()),
        *this),
    _svLoci(opt.graphOpt),
    _depth(depthBufferCompression),
    _isScanStarted(false),
    _isInDenoiseRegion(false),
    _denoisePos(0),
    _readScanner(opt.scanOpt,opt.statsFilename,opt.alignFileOpt.alignmentFilename, opt.isRNA),
    _isMaxDepth(false),
    _maxDepth(0),
    _bamHeader(bamHeader),
    _refSeq(refSeq)
{
    const ChromDepthFilterUtil dFilter(opt.chromDepthFilename, opt.scanOpt.maxDepthFactor, bamHeader);
    _isMaxDepth=dFilter.isMaxDepthFilter();
    if (_isMaxDepth)
    {
        _maxDepth=dFilter.maxDepth(scanRegion.tid);
    }

    const unsigned sampleCount(opt.alignFileOpt.alignmentFilename.size());
    _svLoci.getCounts().setSampleCount(sampleCount);

    for (unsigned sampleIndex(0); sampleIndex<sampleCount; ++sampleIndex)
    {
        _svLoci.getCounts().getSampleCounts(sampleIndex).sampleSource = opt.alignFileOpt.alignmentFilename[sampleIndex];
    }

    _svLoci.header = _bamHeader;
    updateDenoiseRegion();
}



void
SVLocusSetFinder::
updateDenoiseRegion()
{
    _denoiseRegion=_scanRegion;

    known_pos_range2& range(_denoiseRegion.range);
    if (range.begin_pos() > 0)
    {
        range.set_begin_pos(range.begin_pos()+REGION_DENOISE_BORDER);
    }

    bool isEndBorder(true);
    if (static_cast<int32_t>(_svLoci.header.chrom_data.size()) > _denoiseRegion.tid)
    {
        const pos_t chromEndPos(_svLoci.header.chrom_data[_denoiseRegion.tid].length);
        isEndBorder=(range.end_pos() < chromEndPos);
    }

    if (isEndBorder)
    {
        range.set_end_pos(range.end_pos()-REGION_DENOISE_BORDER);
    }

#ifdef DEBUG_SFINDER
    log_os << __FUNCTION__ << ": " << _denoiseRegion << "\n";
#endif

}



void
SVLocusSetFinder::
process_pos(const int stage_no,
            const pos_t pos)
{
#ifdef DEBUG_SFINDER
    log_os << __FUNCTION__ << ": stage_no: " << stage_no << " pos: " << pos << "\n";
#endif

    if     (stage_no == STAGE::HEAD)
    {
        // pass
    }
    else if (stage_no == STAGE::DENOISE)
    {
        static const pos_t denoiseMinChunk(1000);

        if (_denoiseRegion.range.is_pos_intersect(pos))
        {

#ifdef DEBUG_SFINDER
            log_os << __FUNCTION__ << ": pos intersect. pos: " << pos << " dnRegion: " << _denoiseRegion << " is in region: " << _isInDenoiseRegion << "\n";
#endif

            if (! _isInDenoiseRegion)
            {
                _denoisePos=_denoiseRegion.range.begin_pos();
                _isInDenoiseRegion=true;
            }

            if ( (1 + pos-_denoisePos) >= denoiseMinChunk)
            {
                _svLoci.cleanRegion(GenomeInterval(_denoiseRegion.tid, _denoisePos, (pos+1)));
                _denoisePos = (pos+1);
            }
        }
        else
        {

#ifdef DEBUG_SFINDER
            log_os << __FUNCTION__ << ": no pos intersect. pos: " << pos << " dnRegion: " << _denoiseRegion << " is in region: " << _isInDenoiseRegion << "\n";
#endif

            if (_isInDenoiseRegion)
            {
                if ( (_denoiseRegion.range.end_pos()-_denoisePos) > 0)
                {
                    _svLoci.cleanRegion(GenomeInterval(_denoiseRegion.tid, _denoisePos, _denoiseRegion.range.end_pos()));
                    _denoisePos = _denoiseRegion.range.end_pos();
                }
                _isInDenoiseRegion=false;
            }
        }
    }
    else if (stage_no == STAGE::CLEAR_DEPTH)
    {
        _depth.clear_pos(pos);
    }
    else
    {
        assert(false && "Unexpected stage id");
    }
}



void
SVLocusSetFinder::
addToDepthBuffer(
    const unsigned defaultReadGroupIndex,
    const bam_record& bamRead)
{
    if (! _isMaxDepth) return;

    // estimate depth from normal sample only:
    if (_isAlignmentTumor[defaultReadGroupIndex]) return;

    // depth estimation relies on a simple filtration criteria to stay in sync with the chromosome mean
    // depth estimates:
    if (bamRead.is_unmapped()) return;

    const pos_t refPos(bamRead.pos()-1);

    /// stick to a simple approximation -- ignore CIGAR string and just look at the read length:
    const pos_t readSize(bamRead.read_size());
    _depth.inc(refPos,readSize);
}



void
SVLocusSetFinder::
update(
    const bam_record& bamRead,
    const unsigned defaultReadGroupIndex)
{
    _isScanStarted=true;

    const bool isTumor(_isAlignmentTumor[defaultReadGroupIndex]);
    if (! isTumor)
    {
        // depth estimation relies on a simple filtration criteria to stay in sync with the chromosome mean
        // depth estimates using samtools idxstats:
        if (! bamRead.is_unmapped())
        {
            addToDepthBuffer(defaultReadGroupIndex, bamRead);
        }
    }

    // note we currently filter unmapped but allow unpaired/unmapped mate to come through this screen
    // these reads can still show an assembly signal as individual reads, or by contributing shadows
    if (SVLocusScanner::isMappedReadFilteredCore(bamRead)) return;

    if (_isMaxDepth)
    {
        if (_depth.val(bamRead.pos()-1) > _maxDepth) return;
    }

    SampleCounts& counts(_svLoci.getCounts().getSampleCounts(defaultReadGroupIndex));
    SampleReadInputCounts& incounts(counts.input);
    if (bamRead.map_qual() < _readScanner.getMinMapQ())
    {
        incounts.minMapq++;
        return;
    }

    if (! _readScanner.isSVEvidence(bamRead,defaultReadGroupIndex,_refSeq,&(incounts.evidenceCount))) return;

#ifdef DEBUG_SFINDER
    log_os << __FUNCTION__ << ": Accepted read. isNonCompressedAnomalous "  << isNonCompressedAnomalous << " is Local assm evidence: " << isLocalAssemblyEvidence << " read: " << bamRead << "\n";
#endif

    // check that this read starts in our scan region:
    if (! _scanRegion.range.is_pos_intersect(bamRead.pos()-1)) return;

    _stageman.handle_new_pos_value(bamRead.pos()-1);

    std::vector<SVLocus> loci;

    SampleEvidenceCounts& eCounts(counts.evidence);

    _readScanner.getSVLoci(bamRead, defaultReadGroupIndex, _bamHeader,
                           _refSeq, loci, eCounts);

    for (const SVLocus& locus : loci)
    {
        if (locus.empty()) continue;
        _svLoci.merge(locus);
    }
}
