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


#include "manta/ReadChromDepthUtil.hh"

#include "blt_util/log.hh"
#include "blt_util/MedianDepthTracker.hh"
#include "blt_util/depth_buffer.hh"
#include "common/Exceptions.hh"
#include "htsapi/bam_header_info.hh"
#include "htsapi/bam_streamer.hh"


#include <iostream>
#include <sstream>


//#define DEBUG_DPS


/// dynamically track median read depth
///
/// assume all reads align perfectly in place
///
/// This method removes zero depth before computing the median
///
struct DepthTracker
{
    void
    setNewRegion()
    {
        if (! _isRegionInit) return;

        flushPos(_maxPos);
        _maxPos = 0;
        _isRegionInit = false;
        _depth.clear();
    }

    void
    addRead(
        const bam_record& bamRead)
    {
        const pos_t pos(bamRead.pos()-1);
        const unsigned rsize(bamRead.read_size());
        if (! _isRegionInit)
        {
            _maxPos=pos;
            _isRegionInit=true;
        }

        for (; _maxPos<pos; ++_maxPos) flushPos(_maxPos);
        _depth.inc(pos,rsize);
        _count++;
    }

    double
    getDepth() const
    {
        return _mtrack.getMedian();
    }

    uint64_t
    getReadCount() const
    {
        return _count;
    }

private:

    // flush position from depth tracker
    void
    flushPos(
        const pos_t pos)
    {
        const unsigned depth(_depth.val(pos));
        _mtrack.addObs(depth);
        _depth.clear_pos(pos);
    }

    depth_buffer_compressible _depth = depth_buffer_compressible(16); ///< track depth for the purpose of filtering high-depth regions
    MedianDepthTracker _mtrack;

    bool _isRegionInit = false;
    pos_t _maxPos = 0;

    uint64_t _count = 0;
};



/// dynamically track average read depth
///
/// we don't need a really slick estimation here because we're tracking depth over large regions,
/// all we need is:
/// 1) how much read length did we observe? (alignment doesn't matter)
/// 2) over what range of positions?
///
/// Note this method is designed to REMOVE large empty regions from the average
struct MeanDepthTracker
{
    void
    setNewRegion()
    {
        if (! _isRegionInit) return;
        _priorRegionLength += currentRegionLength();
        _minPos = 0;
        _maxPos = 0;
        _endPos = 0;
        _isRegionInit = false;
    }

    void
    addRead(
        const bam_record& bamRead)
    {
        if (_isRegionInit)
        {
            if (bamRead.pos() > _endPos + 1000)
            {
                _maxPos=_endPos;
                setNewRegion();
            }
        }

        if (! _isRegionInit)
        {
            _minPos=bamRead.pos();
            _maxPos=bamRead.pos();
            _endPos=bamRead.pos() + bamRead.read_size();
            _isRegionInit=true;
        }
        else
        {
            if (bamRead.pos() > _maxPos)
            {
                _maxPos = bamRead.pos();
                _endPos=bamRead.pos() + bamRead.read_size();
            }
        }

        _count++;
        _totalReadLength += bamRead.read_size();
    }

    double
    getMeanDepth() const
    {
        return (_totalReadLength / (_priorRegionLength + currentRegionLength()));
    }

    uint64_t
    getReadCount() const
    {
        return _count;
    }

private:

    double
    currentRegionLength() const
    {
        return (1 + _maxPos - _minPos);
    }


    bool _isRegionInit = false;
    int32_t _minPos = 0;
    int32_t _maxPos = 0;
    int32_t _endPos = 0;
    double _priorRegionLength = 0;

    uint64_t _count = 0;
    // throw this into a double so we don't worry about underflow:
    double _totalReadLength = 0;
};



/// all data required to build ChromDepth during estimation from the bam file
///
struct ChromDepthTracker
{
    explicit
    ChromDepthTracker() :
        _isFinalized(false),
        _isChecked(false),
        _isDepthConverged(false),
        _oldDepth(-1)
    {}

    void
    setNewRegion()
    {
        assert(! _isFinalized);
        _mdTracker.setNewRegion();
    }

    void
    addRead(const bam_record& bamRead)
    {
        assert(! _isFinalized);
        _mdTracker.addRead(bamRead);
    }

    unsigned
    depthObservations() const
    {
        return _mdTracker.getReadCount();
    }

    bool
    isDepthCountCheck()
    {
        static const unsigned statsCheckCnt(1000000);
        const bool isCheck((depthObservations() % statsCheckCnt) == 0);
        if (isCheck) _isChecked=true;
        return isCheck;
    }

    bool
    isChecked() const
    {
        return (_isChecked || isDepthConverged());
    }

    void
    clearChecked()
    {
        _isChecked=false;
    }

    bool
    isDepthConverged() const
    {
        return _isDepthConverged;
    }

    void
    updateDepthConvergenceTest()
    {
        // check convergence
        const double depth(_mdTracker.getDepth());
        if (_oldDepth >= 0)
        {
            _isDepthConverged=isDepthMatch(_oldDepth, depth);
        }
#ifdef DEBUG_DPS
        log_os << "Test convergence. Old: " << _oldDepth << " New: " <<  _mdTracker.getDepth() << " Pass: " << _isDepthConverged << "\n";
        log_os << "Test count. New: " <<  _mdTracker.getReadCount() << "\n";
#endif
        _oldDepth = depth;
    }

    double
    getDepth() const
    {
        assert(_isFinalized);
        return _mdTracker.getDepth();
    }

    void
    finalize(
        const bool isCompleteChrom = true)
    {
        if (_isFinalized) return;

        // finalize insert size distro:
        if (! isDepthConverged())
        {
            if (! isDepthCountCheck())
            {
                updateDepthConvergenceTest();
            }

            if (! (isCompleteChrom || isDepthConverged()))
            {
                log_os << "WARNING: chrom mean depth did not converge\n";
            }
        }

        _isFinalized=true;
    }

private:

    bool
    isDepthMatch(
        const double& d1,
        const double& d2)
    {
        static const float dPrecision(0.05f);

        return (std::abs(d1 - d2) < dPrecision);
    }

    bool _isFinalized;

    bool _isChecked;
    bool _isDepthConverged;

    double _oldDepth; // previous depth is stored to determine convergence
    DepthTracker _mdTracker;
};



/// get the start positions of chromosome segments
/// ensure that all segments are no longer than segmentSize
///
/// all are zero-indexed
static
void
getChromSegments(
    const unsigned chromSize,
    const unsigned segmentSize,
    std::vector<unsigned>& startPos)
{
    assert(chromSize>0);
    assert(segmentSize>0);

    startPos.clear();

    const unsigned chromSegments(1+((chromSize-1)/segmentSize));
    const unsigned segmentBaseSize(chromSize/chromSegments);
    const unsigned nPlusOne(chromSize%chromSegments);
    unsigned start(0);

    for (unsigned segmentIndex(0); segmentIndex<chromSegments; ++segmentIndex)
    {
        assert(start < chromSize);
        startPos.push_back(start);
        const unsigned segSize(segmentBaseSize + ((segmentIndex<nPlusOne) ? 1 : 0));
        start=std::min(start+segSize,chromSize);
    }
}



double
readChromDepthFromAlignment(
    const std::string& statsAlignmentFile,
    const std::string& chromName)
{
    bam_streamer read_stream(statsAlignmentFile.c_str());

    const bam_hdr_t& header(* read_stream.get_header());
    const bam_header_info bamHeader(header);

    const auto& chromToIndex(bamHeader.chrom_to_index);
    const auto chromIter(chromToIndex.find(chromName));
    if (chromIter == chromToIndex.end())
    {
        using namespace illumina::common;

        std::ostringstream oss;
        oss << "ERROR: Can't find chromosome name '" << chromName << "' in BAM/CRAM file: '" << statsAlignmentFile << "\n";
        BOOST_THROW_EXCEPTION(LogicException(oss.str()));
    }

    const int32_t chromIndex(chromIter->second);

    const unsigned chromSize(bamHeader.chrom_data[chromIndex].length);
    unsigned segmentSize(2000000);
    std::vector<unsigned> segmentStartPos;

    static const unsigned maxSSLoop(1000);
    for (unsigned i=0; i<=maxSSLoop; ++i)
    {
        assert(i < maxSSLoop);
        getChromSegments(chromSize, segmentSize, segmentStartPos);
        if (segmentStartPos.size() <= 20) break;

        const unsigned lastSegmentSize(segmentSize);
        segmentSize *= 2;
        assert(segmentSize > lastSegmentSize); //overflow gaurd
    }

    const unsigned totalSegments(segmentStartPos.size());

    std::vector<unsigned> segmentHeadPos = segmentStartPos;
    std::vector<bool> segmentIsEmpty(totalSegments,false);

    ChromDepthTracker cdTracker;

#ifdef DEBUG_DPS
    log_os << "INFO: Chrom depth requesting bam region starting from: chrid: " << chromIndex << "\n";
    log_os << "\tchromSize: " << chromSize << "\n";
    for (const auto startPos : segmentStartPos)
    {
        log_os << "\tstartPos: " << startPos << "\n";
    }
#endif

    /// loop through segments until convergence criteria are met, or we run out of data:
    static const unsigned maxCycle(10);
    bool isFinished(false);
    for (unsigned cycleIndex(0); cycleIndex<maxCycle; cycleIndex++)
    {
#ifdef DEBUG_DPS
        log_os << "starting cycle: " << cycleIndex << "\n";
#endif
        bool isEmpty(true);
        for (unsigned segmentIndex(0); segmentIndex<totalSegments; segmentIndex++)
        {
#ifdef DEBUG_DPS
            log_os << "starting segment: " << segmentIndex << "\n";
#endif
            if (segmentIsEmpty[segmentIndex]) continue;

            const int32_t startPos(segmentHeadPos[segmentIndex]);
            const int32_t endPos(((segmentIndex+1)<totalSegments) ? segmentStartPos[segmentIndex+1]: chromSize);
#ifdef DEBUG_DPS
            log_os << "scanning region: " << startPos << "," << endPos << "\n";
#endif
            read_stream.set_new_region(chromIndex,startPos,endPos);

            cdTracker.setNewRegion();

            static const unsigned targetSegmentReadCount=40000;
            static const int32_t minSpan(10000);
            unsigned segmentReadCount(0);
            while (read_stream.next())
            {
                // not allowed to test convergence until we've cycled through all segments once
                if ((cycleIndex>0) && cdTracker.isDepthConverged())
                {
                    isFinished=true;
                    break;
                }

                const bam_record& bamRead(*(read_stream.get_record_ptr()));
                const int32_t readPos(bamRead.pos()-1);
                if (readPos<startPos) continue;

                segmentReadCount++;

                if (readPos >= static_cast<int32_t>(segmentHeadPos[segmentIndex]))
                {
                    // cycle through to next segment:
                    // doing this here ensures that we only cycle-out at the end of a position so that
                    // no data is skipped if we come back to this segment again:
                    if ((segmentReadCount > targetSegmentReadCount) && ((readPos-startPos) >= minSpan))
                    {
                        segmentHeadPos[segmentIndex] = readPos;
                        break;
                    }
                    else
                    {
                        segmentHeadPos[segmentIndex] = readPos+1;
                    }
                }

                // apply all filters:
                if (bamRead.is_unmapped()) continue;

                cdTracker.addRead(bamRead);

                if (! cdTracker.isDepthCountCheck()) continue;

                // check convergence
                cdTracker.updateDepthConvergenceTest();
            }

            if (segmentReadCount>0)
            {
                isEmpty=false;
            }
            else
            {
                segmentIsEmpty[segmentIndex] = true;
            }
        }

        if (isFinished || isEmpty) break;
    }

    cdTracker.finalize();

    return cdTracker.getDepth();
}
