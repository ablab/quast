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
///

#include "format/VcfWriterTumorSV.hh"



void
VcfWriterTumorSV::
addHeaderInfo() const
{
    _os << "##INFO=<ID=BND_DEPTH,Number=1,Type=Integer,Description=\"Read depth at local translocation breakend\">\n";
    _os << "##INFO=<ID=MATE_BND_DEPTH,Number=1,Type=Integer,Description=\"Read depth at remote translocation mate breakend\">\n";
}



void
VcfWriterTumorSV::
addHeaderFormat() const
{
    _os << "##FORMAT=<ID=PR,Number=.,Type=Integer,Description=\"Spanning paired-read support for the ref and alt alleles in the order listed\">\n";
    _os << "##FORMAT=<ID=SR,Number=.,Type=Integer,Description=\"Split reads for the ref and alt alleles in the order listed, for reads where P(allele|read)>0.999\">\n";
}



void
VcfWriterTumorSV::
addHeaderFilters() const
{
    if (_isMaxDepthFilter)
    {
        _os << "##FILTER=<ID=" << _tumorOpt.maxDepthFilterLabel << ",Description=\"Sample site depth is greater than " << _tumorOpt.maxDepthFactor << "x the median chromosome depth near one or both variant breakends\">\n";
    }

    _os << "##FILTER=<ID=" << _tumorOpt.maxMQ0FracLabel << ",Description=\"For a small variant (<1000 base), the fraction of reads with MAPQ0 around either breakend exceeds " << _tumorOpt.maxMQ0Frac << "\">\n";
}



void
VcfWriterTumorSV::
writeFilter() const
{
    writeFilters(getTumorInfo().filters, _os);
}



void
VcfWriterTumorSV::
modifySample(
    const SVCandidate& sv,
    SampleTag_t& sampletags) const
{
    const SVScoreInfo& baseInfo(getBaseInfo());
    const unsigned sampleCount(baseInfo.samples.size());

    std::vector<std::string> values(sampleCount);

    for (unsigned sampleIndex(0); sampleIndex<sampleCount; ++sampleIndex)
    {
        const SVSampleInfo& sinfo(baseInfo.samples[sampleIndex]);
        values[sampleIndex] = str( boost::format("%i,%i") % sinfo.ref.confidentSpanningPairCount % sinfo.alt.confidentSpanningPairCount);
    }
    sampletags.push_back(std::make_pair("PR",values));

    if (sv.isImprecise()) return;

    for (unsigned sampleIndex(0); sampleIndex<sampleCount; ++sampleIndex)
    {
        const SVSampleInfo& sinfo(baseInfo.samples[sampleIndex]);
        values[sampleIndex] = str( boost::format("%i,%i") % sinfo.ref.confidentSplitReadCount % sinfo.alt.confidentSplitReadCount);
    }
    sampletags.push_back(std::make_pair("SR",values));
}

void
VcfWriterTumorSV::
modifyTranslocInfo(
    const SVCandidate& /*sv*/,
    const bool isFirstOfPair,
    InfoTag_t& infotags) const
{
    const SVScoreInfo& baseInfo(getBaseInfo());

    infotags.push_back( str(boost::format("BND_DEPTH=%i") %
                            (isFirstOfPair ? baseInfo.bp1MaxDepth : baseInfo.bp2MaxDepth) ) );
    infotags.push_back( str(boost::format("MATE_BND_DEPTH=%i") %
                            (isFirstOfPair ? baseInfo.bp2MaxDepth : baseInfo.bp1MaxDepth) ) );
}



void
VcfWriterTumorSV::
writeSV(
    const SVCandidateSetData& svData,
    const SVCandidateAssemblyData& adata,
    const SVCandidate& sv,
    const SVId& svId,
    const SVScoreInfo& baseInfo,
    const SVScoreInfoTumor& tumorInfo,
    const EventInfo& event
)
{
    //TODO: this is a lame way to customize subclass behavior:
    setScoreInfo(baseInfo);
    _tumorInfoPtr=&tumorInfo;
    writeSVCore(svData, adata, sv, svId, event);

    clearScoreInfo();
    _tumorInfoPtr=nullptr;
}
