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

#include "format/VcfWriterDiploidSV.hh"



void
VcfWriterDiploidSV::
addHeaderInfo() const
{
    _os << "##INFO=<ID=BND_DEPTH,Number=1,Type=Integer,Description=\"Read depth at local translocation breakend\">\n";
    _os << "##INFO=<ID=MATE_BND_DEPTH,Number=1,Type=Integer,Description=\"Read depth at remote translocation mate breakend\">\n";
    _os << "##INFO=<ID=JUNCTION_QUAL,Number=1,Type=Integer,Description=\"If the SV junction is part of an EVENT (ie. a multi-adjacency variant), this field provides the QUAL value for the adjacency in question only\">\n";
    if (_isRNA)
    {
        _os << "##INFO=<ID=REF_COUNT,Number=1,Type=Integer,Description=\"For RNA fusions, the number of reads supporting the reference allele at this breakend\">\n";
        _os << "##INFO=<ID=MATE_REF_COUNT,Number=1,Type=Integer,Description=\"For RNA fusions, the number of reads supporting the reference allele at the other breakend\">\n";
    }
}



void
VcfWriterDiploidSV::
addHeaderFormat() const
{
    _os << "##FORMAT=<ID=GT,Number=1,Type=String,Description=\"Genotype\">\n";
    _os << "##FORMAT=<ID=FT,Number=1,Type=String,Description=\"Sample filter, 'PASS' indicates that all filters have passed for this sample\">\n";
    _os << "##FORMAT=<ID=GQ,Number=1,Type=Float,Description=\"Genotype Quality\">\n";
    _os << "##FORMAT=<ID=PL,Number=G,Type=Integer,Description=\"Normalized, Phred-scaled likelihoods for genotypes as defined in the VCF specification\">\n";
    _os << "##FORMAT=<ID=PR,Number=.,Type=Integer,Description=\"Spanning paired-read support for the ref and alt alleles in the order listed\">\n";
    _os << "##FORMAT=<ID=SR,Number=.,Type=Integer,Description=\"Split reads for the ref and alt alleles in the order listed, for reads where P(allele|read)>0.999\">\n";
    if (_isRNA)
    {
        _os << "##FORMAT=<ID=FS,Number=2,Type=Integer,Description=\"For RNA variants split reads supporting the ref and alt alleles in the order listed\">\n";
        _os << "##FORMAT=<ID=FP,Number=2,Type=Integer,Description=\"For RNA variants spanning paired reads supporting the ref and alt alleles in the order listed\">\n";
    }
}



void
VcfWriterDiploidSV::
addHeaderFilters() const
{
    if (_isMaxDepthFilter)
    {
        _os << "##FILTER=<ID=" << _diploidOpt.maxDepthFilterLabel << ",Description=\"Depth is greater than " << _diploidOpt.maxDepthFactor << "x the median chromosome depth near one or both variant breakends\">\n";
    }
    _os << "##FILTER=<ID=" << _diploidOpt.maxMQ0FracLabel << ",Description=\"For a small variant (<1000 bases), the fraction of reads in all samples with MAPQ0 around either breakend exceeds " << _diploidOpt.maxMQ0Frac << "\">\n";
    _os << "##FILTER=<ID=" << _diploidOpt.noPairSupportLabel << ",Description=\"For variants significantly larger than the paired read fragment size, no paired reads support the alternate allele in any sample.\">\n";
    _os << "##FILTER=<ID=" << _diploidOpt.minAltFilterLabel << ",Description=\"QUAL score is less than " << _diploidOpt.minPassAltScore << "\">\n";
    _os << "##FILTER=<ID=" << _diploidOpt.minGTFilterLabel << ",Description=\"GQ score is less than " << _diploidOpt.minPassGTScore << " (filter applied at sample level and record level if all samples are filtered)\">\n";
    if (_isRNA)
    {
        _os << "##FILTER=<ID=" << _diploidOpt.rnaFilterLabel << ",Description=\"RNA fusion variants without split read and split pair support\">\n";
    }
}



void
VcfWriterDiploidSV::
modifyInfo(
    const EventInfo& event,
    InfoTag_t& infotags) const
{
    if (event.isEvent())
    {
        infotags.push_back( str(boost::format("JUNCTION_QUAL=%i") % getSingleJunctionDiploidInfo().altScore) );
    }
}



void
VcfWriterDiploidSV::
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
    if (_isRNA)
    {
        ///TODO better multisample handler here:
        const unsigned sampleIndex(0);

        const SVSampleAlleleInfo& refinfo(baseInfo.samples[sampleIndex].ref);
        infotags.push_back(str(boost::format("REF_COUNT=%i") %
                               (isFirstOfPair ? refinfo.confidentSplitReadAndPairCountRefBp1 : refinfo.confidentSplitReadAndPairCountRefBp2)));
        infotags.push_back(str(boost::format("MATE_REF_COUNT=%i") %
                               (isFirstOfPair ? refinfo.confidentSplitReadAndPairCountRefBp2 : refinfo.confidentSplitReadAndPairCountRefBp1)));
    }
}



void
VcfWriterDiploidSV::
writeQual() const
{
    _os << getDiploidInfo().altScore;
}



void
VcfWriterDiploidSV::
writeFilter() const
{
    writeFilters(getDiploidInfo().filters,_os);
}



static
const char*
gtLabel(
    const DIPLOID_GT::index_t id)
{
    using namespace DIPLOID_GT;
    switch (id)
    {
    case REF :
        return "0/0";
    case HET :
        return "0/1";
    case HOM :
        return "1/1";
    default :
        return "";
    }
}



void
VcfWriterDiploidSV::
modifySample(
    const SVCandidate& sv,
    SampleTag_t& sampletags) const
{
    const SVScoreInfo& baseInfo(getBaseInfo());
    const SVScoreInfoDiploid& diploidInfo(getDiploidInfo());
    const unsigned diploidSampleCount(diploidInfo.samples.size());

    std::vector<std::string> values(diploidSampleCount);
    for (unsigned diploidSampleIndex(0); diploidSampleIndex<diploidSampleCount; ++diploidSampleIndex)
    {
        const SVScoreInfoDiploidSample& diploidSampleInfo(diploidInfo.samples[diploidSampleIndex]);
        values[diploidSampleIndex] = gtLabel(diploidSampleInfo.gt);
    }
    sampletags.push_back(std::make_pair("GT",values));

    for (unsigned diploidSampleIndex(0); diploidSampleIndex<diploidSampleCount; ++diploidSampleIndex)
    {
        const SVScoreInfoDiploidSample& diploidSampleInfo(diploidInfo.samples[diploidSampleIndex]);

        writeFilters(diploidSampleInfo.filters, values[diploidSampleIndex]);
    }
    sampletags.push_back(std::make_pair("FT",values));

    for (unsigned diploidSampleIndex(0); diploidSampleIndex<diploidSampleCount; ++diploidSampleIndex)
    {
        const SVScoreInfoDiploidSample& diploidSampleInfo(diploidInfo.samples[diploidSampleIndex]);

        values[diploidSampleIndex] = str( boost::format("%s") % diploidSampleInfo.gtScore);
    }
    sampletags.push_back(std::make_pair("GQ",values));

    for (unsigned diploidSampleIndex(0); diploidSampleIndex<diploidSampleCount; ++diploidSampleIndex)
    {
        const SVScoreInfoDiploidSample& diploidSampleInfo(diploidInfo.samples[diploidSampleIndex]);

        values[diploidSampleIndex] =  str( boost::format("%s,%s,%s")
                                           % diploidSampleInfo.phredLoghood[DIPLOID_GT::REF]
                                           % diploidSampleInfo.phredLoghood[DIPLOID_GT::HET]
                                           % diploidSampleInfo.phredLoghood[DIPLOID_GT::HOM]);
    }
    sampletags.push_back(std::make_pair("PL",values));

    for (unsigned diploidSampleIndex(0); diploidSampleIndex<diploidSampleCount; ++diploidSampleIndex)
    {
        const SVSampleInfo& sampleInfo(baseInfo.samples[diploidSampleIndex]);
        values[diploidSampleIndex] =  str( boost::format("%i,%i")
                                           % sampleInfo.ref.confidentSpanningPairCount
                                           % sampleInfo.alt.confidentSpanningPairCount);
    }
    sampletags.push_back(std::make_pair("PR",values));

    if (sv.isImprecise()) return;

    for (unsigned diploidSampleIndex(0); diploidSampleIndex<diploidSampleCount; ++diploidSampleIndex)
    {
        const SVSampleInfo& sampleInfo(baseInfo.samples[diploidSampleIndex]);
        values[diploidSampleIndex] =  str( boost::format("%i,%i")
                                           % sampleInfo.ref.confidentSplitReadCount
                                           % sampleInfo.alt.confidentSplitReadCount);
    }
    sampletags.push_back(std::make_pair("SR",values));

    if (! _isRNA) return;

    for (unsigned diploidSampleIndex(0); diploidSampleIndex<diploidSampleCount; ++diploidSampleIndex)
    {
        const SVSampleInfo& sampleInfo(baseInfo.samples[diploidSampleIndex]);
        values[diploidSampleIndex] =  str( boost::format("%i,%i")
                                           % sampleInfo.ref.splitReadCount
                                           % sampleInfo.alt.splitReadCount);
    }
    sampletags.push_back(std::make_pair("FS",values));

    for (unsigned diploidSampleIndex(0); diploidSampleIndex<diploidSampleCount; ++diploidSampleIndex)
    {
        const SVSampleInfo& sampleInfo(baseInfo.samples[diploidSampleIndex]);
        values[diploidSampleIndex] =  str( boost::format("%i,%i")
                                           % sampleInfo.ref.spanningPairCount
                                           % sampleInfo.alt.spanningPairCount);
    }
    sampletags.push_back(std::make_pair("FP",values));
}



void
VcfWriterDiploidSV::
writeSV(
    const SVCandidateSetData& svData,
    const SVCandidateAssemblyData& adata,
    const SVCandidate& sv,
    const SVId& svId,
    const SVScoreInfo& baseInfo,
    const SVScoreInfoDiploid& diploidInfo,
    const EventInfo& event,
    const SVScoreInfoDiploid& singleJunctionDiploidInfo)
{
    //TODO: this is a lame way to customize subclass behavior:
    setScoreInfo(baseInfo);
    _diploidInfoPtr=&diploidInfo;
    _singleJunctionDiploidInfoPtr=&singleJunctionDiploidInfo;

    writeSVCore(svData, adata, sv, svId, event);

    clearScoreInfo();
    _diploidInfoPtr=nullptr;
    _singleJunctionDiploidInfoPtr=nullptr;
}
