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

#include "EstimateSVLoci.hh"
#include "ESLOptions.hh"
#include "SVLocusSetFinder.hh"

#include "blt_util/input_stream_handler.hh"
#include "blt_util/log.hh"
#include "common/OutStream.hh"
#include "htsapi/bam_header_util.hh"
#include "manta/SVReferenceUtil.hh"

#include <iostream>
#include <vector>

//#define DEBUG_ESL



static
void
runESLRegion(
    const ESLOptions& opt,
    const std::string& region,
    SVLocusSet& mergedSet)
{
    TimeTracker timer;
    timer.resume();

    typedef std::shared_ptr<bam_streamer> stream_ptr;
    std::vector<stream_ptr> bamStreams;

    // setup all data for main alignment loop:
    for (const std::string& afile : opt.alignFileOpt.alignmentFilename)
    {
        stream_ptr tmp(new bam_streamer(afile.c_str(),
                                        (region.empty()
                                         ? nullptr
                                         : region.c_str())));
        bamStreams.push_back(tmp);
    }

    const unsigned bamCount(bamStreams.size());

    assert(0 != bamCount);

    // check bam header compatibility:
    if (bamCount > 1)
    {
        /// TODO: provide a better error exception for failed bam header check:
        const bam_hdr_t* compareHeader(bamStreams[0]->get_header());
        for (unsigned bamIndex(1); bamIndex<bamCount; ++bamIndex)
        {
            const bam_hdr_t* indexHeader(bamStreams[bamIndex]->get_header());
            if (! check_header_compatibility(compareHeader,indexHeader))
            {
                log_os << "ERROR: incompatible bam headers between files:\n"
                       << "\t" << opt.alignFileOpt.alignmentFilename[0] << "\n"
                       << "\t" << opt.alignFileOpt.alignmentFilename[bamIndex] << "\n";
                exit(EXIT_FAILURE);
            }
        }
    }

    // assume headers compatible after this point....

    const bam_hdr_t& header(*(bamStreams[0]->get_header()));
    const bam_header_info bamHeader(header);

    int32_t tid(0), beginPos(0), endPos(0);
    parse_bam_region(bamHeader,region.c_str(),tid,beginPos,endPos);

    const GenomeInterval scanRegion(tid,beginPos,endPos);
#ifdef DEBUG_ESL
    static const std::string log_tag("EstimateSVLoci");
    log_os << log_tag << " scanRegion= " << scanRegion << "\n";
#endif

    // grab the reference for segment we're estimating plus a buffer around the segment edges:
    static const unsigned refEdgeBufferSize(500);

    reference_contig_segment refSegment;
    getIntervalReferenceSegment(opt.referenceFilename, bamHeader, refEdgeBufferSize, scanRegion, refSegment);

    SVLocusSetFinder locusFinder(opt, scanRegion, bamHeader, refSegment);

    input_stream_data sdata;
    for (unsigned bamIndex(0); bamIndex<bamCount; ++bamIndex)
    {
        sdata.register_reads(*bamStreams[bamIndex],bamIndex);
    }

    // loop through alignments:
    input_stream_handler sinput(sdata);
    while (sinput.next())
    {
        const input_record_info current(sinput.get_current());

        if (current.itype != INPUT_TYPE::READ)
        {
            log_os << "ERROR: invalid input condition.\n";
            exit(EXIT_FAILURE);
        }

        const bam_streamer& readStream(*bamStreams[current.sample_no]);
        const bam_record& read(*(readStream.get_record_ptr()));

        locusFinder.update(read, current.sample_no);
    }

    // finished updating:
    locusFinder.flush();
    timer.stop();
    const CpuTimes totalTimes(timer.getTimes());
#ifdef DEBUG_ESL
    log_os << log_tag << " found " << locusFinder.getLocusSet().size() << " loci. \n";
    log_os << log_tag << " totalTime: ";
    totalTimes.reportHr(log_os);
    log_os << "\n";
#endif
    locusFinder.setBuildTime(totalTimes);

    const bool isMultiRegion(opt.regions.size()>1);

    if (! isMultiRegion)
    {
        locusFinder.getLocusSet().save(opt.outputFilename.c_str());
    }
    else
    {
        if (mergedSet.empty())
        {
            mergedSet = locusFinder.getLocusSet();
        }
        else
        {
            mergedSet.merge(locusFinder.getLocusSet());
        }
    }
}



static
void
runESL(const ESLOptions& opt)
{
    {
        // early test that we have permission to write to output file
        OutStream outs(opt.outputFilename);
    }

    SVLocusSet mergedSet;

    for (const auto& region : opt.regions)
    {
        runESLRegion(opt, region, mergedSet);
    }

    if (! mergedSet.empty())
    {
        mergedSet.save(opt.outputFilename.c_str());
    }
}



void
EstimateSVLoci::
runInternal(int argc, char* argv[]) const
{
    ESLOptions opt;

    parseESLOptions(*this,argc,argv,opt);
    runESL(opt);
}
