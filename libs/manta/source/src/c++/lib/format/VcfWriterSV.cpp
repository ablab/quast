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
/// \author Felix Schlesinger
///

#include "format/VcfWriterSV.hh"

#include "blt_util/log.hh"
#include "blt_util/seq_util.hh"
#include "blt_util/string_util.hh"
#include "htsapi/samtools_fasta_util.hh"
#include "htsapi/vcf_util.hh"
#include "common/Exceptions.hh"
#include "manta/SVCandidateUtil.hh"

#include <iostream>
#include <sstream>


//#define DEBUG_VCF


#ifdef DEBUG_VCF
#include "blt_util/log.hh"
#endif



VcfWriterSV::
VcfWriterSV(
    const std::string& referenceFilename,
    const bool isRNA,
    const SVLocusSet& set,
    std::ostream& os) :
    _referenceFilename(referenceFilename),
    _isRNA(isRNA),
    _header(set.header),
    _os(os)
{
}



void
VcfWriterSV::
writeHeaderPrefix(
    const char* progName,
    const char* progVersion)
{
    _os << "##fileformat=VCFv4.1\n";
    _os << "##fileDate=" << vcf_fileDate << "\n";
    _os << "##source=" << progName << " " << progVersion << "\n";
    _os << "##reference=file://" << _referenceFilename << "\n";

    for (const bam_header_info::chrom_info& cdata : _header.chrom_data)
    {
        _os << "##contig=<ID=" << cdata.label << ",length=" << cdata.length << ">\n";
    }

    /// vcf 4.1 reserved/suggested INFO tags:
    _os << "##INFO=<ID=IMPRECISE,Number=0,Type=Flag,Description=\"Imprecise structural variation\">\n";
    _os << "##INFO=<ID=SVTYPE,Number=1,Type=String,Description=\"Type of structural variant\">\n";
    _os << "##INFO=<ID=SVLEN,Number=.,Type=Integer,Description=\"Difference in length between REF and ALT alleles\">\n";
    _os << "##INFO=<ID=END,Number=1,Type=Integer,Description=\"End position of the variant described in this record\">\n";
    _os << "##INFO=<ID=CIPOS,Number=2,Type=Integer,Description=\"Confidence interval around POS\">\n";
    _os << "##INFO=<ID=CIEND,Number=2,Type=Integer,Description=\"Confidence interval around END\">\n";
    _os << "##INFO=<ID=CIGAR,Number=A,Type=String,Description=\"CIGAR alignment for each alternate indel allele\">\n";
    _os << "##INFO=<ID=MATEID,Number=.,Type=String,Description=\"ID of mate breakend\">\n";
    _os << "##INFO=<ID=EVENT,Number=1,Type=String,Description=\"ID of event associated to breakend\">\n";
    _os << "##INFO=<ID=HOMLEN,Number=.,Type=Integer,Description=\"Length of base pair identical homology at event breakpoints\">\n";
    _os << "##INFO=<ID=HOMSEQ,Number=.,Type=String,Description=\"Sequence of base pair identical homology at event breakpoints\">\n";

    /// custom INFO tags:
    _os << "##INFO=<ID=SVINSLEN,Number=.,Type=Integer,Description=\"Length of insertion\">\n";
    _os << "##INFO=<ID=SVINSSEQ,Number=.,Type=String,Description=\"Sequence of insertion\">\n";
    _os << "##INFO=<ID=LEFT_SVINSSEQ,Number=.,Type=String,Description=\"Known left side of insertion for an insertion of unknown length\">\n";
    _os << "##INFO=<ID=RIGHT_SVINSSEQ,Number=.,Type=String,Description=\"Known right side of insertion for an insertion of unknown length\">\n";
    _os << "##INFO=<ID=INV3,Number=0,Type=Flag,Description=\"Inversion breakends open 3' of reported location\">\n";
    _os << "##INFO=<ID=INV5,Number=0,Type=Flag,Description=\"Inversion breakends open 5' of reported location\">\n";

    if (_isRNA)
    {
        _os << "##INFO=<ID=RNA_FIRST,Number=0,Type=Flag,Description=\"For RNA fusions, this break-end is 5' in the fusion transcript\">\n";
        _os << "##INFO=<ID=RNA_STRANDED,Number=0,Type=Flag,Description=\"For RNA fusions, the direction of transcription is known\">\n";
        _os << "##INFO=<ID=RNA_FwRvReads,Number=2,Type=Integer,Description=\"For RNA fusions, number of stranded reads supporting forward or reverse direction of transcription\">\n";
        _os << "##INFO=<ID=RNA_CONTIG,Number=1,Type=String,Description=\"For RNA fusions, the sequence of the breakend spanning contig\">\n";
        _os << "##INFO=<ID=RNA_CONTIG_ALN,Number=2,Type=Integer,Description=\"For RNA fusions, length of the spanning contig alignment on each breakend\">\n";
    }
    addHeaderInfo();

    addHeaderFormat();

    addHeaderFilters();

    _os << "##ALT=<ID=BND,Description=\"Translocation Breakend\">\n";
    _os << "##ALT=<ID=INV,Description=\"Inversion\">\n";
    _os << "##ALT=<ID=DEL,Description=\"Deletion\">\n";
    _os << "##ALT=<ID=INS,Description=\"Insertion\">\n";
    _os << "##ALT=<ID=DUP:TANDEM,Description=\"Tandem Duplication\">\n";
}


static
void
writeHeaderColKeyPrefix(std::ostream& os)
{
    os << "#CHROM\tPOS\tID\tREF\tALT\tQUAL\tFILTER\tINFO";
}



void
VcfWriterSV::
writeHeaderColumnKey(
    const std::vector<std::string>& sampleNames) const
{
    writeHeaderColKeyPrefix(_os);
    if (!sampleNames.empty())
    {
        _os << "\tFORMAT";

        for (const std::string& sampleName : sampleNames)
        {
            _os << '\t' << sampleName;
        }
    }
    _os << '\n';
}



static
void
makeInfoField(
    const VcfWriterSV::InfoTag_t& info,
    std::ostream& os)
{
    static const char sep(';');
    bool isFirst(true);
    for (const std::string& is : info)
    {
        if (! isFirst) os << sep;
        else           isFirst = false;
        os << is;
    }
}



static
void
makeFormatSampleField(
    const VcfWriterSV::SampleTag_t& sample,
    std::ostream& os)
{
    static const char sep(':');

    if (sample.empty()) return;

    {
        // first write FORMAT field:
        os << '\t';

        bool isFirst(true);
        for (const VcfWriterSV::SampleTag_t::value_type& fs : sample)
        {
            if (! isFirst) os << sep;
            else           isFirst = false;

            assert(! fs.first.empty());
            os << fs.first;
        }
    }

    unsigned nSamples(0);
    for (const VcfWriterSV::SampleTag_t::value_type& fs : sample)
    {
        const unsigned ns(fs.second.size());
        nSamples = std::max(nSamples, ns);
    }

    for (unsigned sampleIndex(0); sampleIndex < nSamples; ++sampleIndex)
    {
        os << '\t';

        // next write SAMPLE field:
        {
            bool isFirst(true);
            for (const VcfWriterSV::SampleTag_t::value_type& fs : sample)
            {
                if (! isFirst) os << sep;
                else           isFirst = false;

                if (fs.second.size() <= sampleIndex)
                {
                    os << '.';
                }
                else if (fs.second[sampleIndex].empty())
                {
                    os << '.';
                }
                else
                {
                    os << fs.second[sampleIndex];
                }
            }
        }
    }
}



static
void
addRNAInfo(
    const bool isFirstOfPair,
    const SVCandidate& sv,
    const SVCandidateAssemblyData& assemblyData,
    VcfWriterSV::InfoTag_t& infotags)
{
    if (! assemblyData.isSpanning) return;

    const bool isFirst = (assemblyData.bporient.isBp1First == isFirstOfPair);
    if (isFirst) infotags.push_back("RNA_FIRST");
    if (assemblyData.bporient.isStranded) infotags.push_back("RNA_STRANDED");

    if (!isFirstOfPair) return; // only the first breakpoint gets the additional RNA info attached to its VCF entry

    infotags.push_back(str(boost::format("RNA_FwRvReads=%i,%i") % sv.fwReads % sv.rvReads));

    const unsigned numContigs(assemblyData.contigs.size());
    if (numContigs > 0)
    {
        if (numContigs != assemblyData.spanningAlignments.size())
            infotags.push_back(str(boost::format("ERROR=%i,%i") % numContigs % assemblyData.spanningAlignments.size()));
        if (numContigs <= assemblyData.bestAlignmentIndex)
            infotags.push_back(str(boost::format("ERROR2=%i,%i") % numContigs % assemblyData.bestAlignmentIndex));
        infotags.push_back(str(boost::format("RNA_CONTIG=%s") % assemblyData.contigs[assemblyData.bestAlignmentIndex].seq));
        infotags.push_back(str(boost::format("RNA_CONTIG_ALN=%i,%i")
                               % apath_matched_length(assemblyData.spanningAlignments[assemblyData.bestAlignmentIndex].align1.apath)
                               % apath_matched_length(assemblyData.spanningAlignments[assemblyData.bestAlignmentIndex].align2.apath)));
    }
}

#ifdef DEBUG_VCF
static
void
addRNADebugInfo(
    const bool isFirstOfPair,
    const SVCandidate& sv,
    const SVCandidateAssemblyData& assemblyData,
    VcfWriterSV::InfoTag_t& infotags)
{
    if (! assemblyData.isSpanning) return;

    const bool isFirst = (assemblyData.bporient.isBp1First == isFirstOfPair);
    const bool isRightOpen = (isFirstOfPair ? sv.bp1.state : sv.bp2.state) == SVBreakendState::RIGHT_OPEN;
    infotags.push_back(str(boost::format("FOOBAR_FW=%1%") % (isFirst == isRightOpen)));

    if (!isFirst) return; // only the first breakpoint gets the alignments attached to its VCF entry

    infotags.push_back(str(boost::format("FOOBAR_bp1=%i;bp2=%i") % sv.bp1.interval.tid % sv.bp2.interval.tid));

    // there can be several contigs per breakend, so we iterate over all of them.
    const unsigned numContigs(assemblyData.contigs.size());
    // cppcheck-suppress zerodivcond
    infotags.push_back(str(boost::format("FOOBAR_NCONTIGS=%i") % numContigs));
    if (numContigs > 0)
    {
        if (numContigs != assemblyData.spanningAlignments.size())
            infotags.push_back(str(boost::format("FOOBAR_ERROR=%i;%i") % numContigs % assemblyData.spanningAlignments.size()));
        if (numContigs <= assemblyData.bestAlignmentIndex)
            infotags.push_back(str(boost::format("FOOBAR_ERROR2=%i;%i") % numContigs % assemblyData.bestAlignmentIndex));

        infotags.push_back(str(boost::format("FOOBAR_BEST=%i") % assemblyData.bestAlignmentIndex));
        //infotags.push_back(str(boost::format("FOOBAR_EXTCONTIG=%s") % assemblyData.extendedContigs[assemblyData.bestAlignmentIndex]));
        infotags.push_back(str(boost::format("FOOBAR_CONTIGcount=%i") % assemblyData.contigs[assemblyData.bestAlignmentIndex].supportReads.size()));
    }
}
#endif

#ifdef DEBUG_VCF
static
void
addDebugInfo(
    const SVBreakend& bp1,
    const SVBreakend& bp2,
    const bool isFirstOfPair,
    const SVCandidateAssemblyData& assemblyData,
    VcfWriterSV::InfoTag_t& infotags)
{
    if (! isFirstOfPair) return;

    // store alignment start + cigar string for each section of the jumping alignment.
    // there can be several contigs per breakend, so we iterate over all of them.
    // only the first breakpoint gets the alignments attached to its VCF entry

    if (assemblyData.isSpanning)
    {
        const unsigned numAlign(assemblyData.spanningAlignments.size());
        std::string cigar1;
        std::string cigar2;
        for (unsigned alignIndex(0); alignIndex<numAlign; ++alignIndex)
        {
            const SVCandidateAssemblyData::JumpAlignmentResultType align(assemblyData.spanningAlignments[alignIndex]);
            infotags.push_back( str(boost::format("CTG_JALIGN_%i_POS_A=%d") %
                                    alignIndex %
                                    (bp1.interval.range.begin_pos()+align.align1.beginPos)) );
            infotags.push_back( str(boost::format("CTG_JALIGN_%i_POS_B=%d") %
                                    alignIndex %
                                    (bp2.interval.range.begin_pos()+align.align2.beginPos)) );

            apath_to_cigar(align.align1.apath,cigar1);
            apath_to_cigar(align.align2.apath,cigar2);

            infotags.push_back( str(boost::format("CTG_JALIGN_%i_CIGAR_A=%s") % alignIndex % cigar1) );
            infotags.push_back( str(boost::format("CTG_JALIGN_%i_CIGAR_B=%s") % alignIndex % cigar2) );
        }
        const unsigned numContigs(assemblyData.contigs.size());

        infotags.push_back(str(boost::format("DEBUG_NCONTIGS=%i") % numContigs));
        infotags.push_back(str(boost::format("DEBUG_BESTContig=%s") % assemblyData.contigs[assemblyData.bestAlignmentIndex].seq));
        infotags.push_back(str(boost::format("DEBUG_CONTIGReads=%i") % assemblyData.contigs[assemblyData.bestAlignmentIndex].supportReads.size()));
        infotags.push_back(str(boost::format("DEBUG_CONTIGLeftAln=%i") % apath_matched_length(assemblyData.spanningAlignments[assemblyData.bestAlignmentIndex].align1.apath)));
        infotags.push_back(str(boost::format("DEBUG_CONTIGRightAln=%i") % apath_matched_length(assemblyData.spanningAlignments[assemblyData.bestAlignmentIndex].align2.apath)));
    }
}
#endif


static
void
addSharedInfo(
    const EventInfo& event,
    VcfWriterSV::InfoTag_t& infoTags)
{
    if (event.isEvent())
    {
        infoTags.push_back( str(boost::format("EVENT=%i") % event.label));
    }
}



void
VcfWriterSV::
writeTransloc(
    const SVCandidate& sv,
    const SVId& svId,
    const bool isFirstBreakend,
    const SVCandidateSetData& /*svData*/,
    const SVCandidateAssemblyData& adata,
    const EventInfo& event)
{
    const bool isImprecise(sv.isImprecise());
    const bool isBreakendRangeSameShift(sv.isBreakendRangeSameShift());

    const SVBreakend& bpA( isFirstBreakend ? sv.bp1 : sv.bp2);
    const SVBreakend& bpB( isFirstBreakend ? sv.bp2 : sv.bp1);

    InfoTag_t infotags;
    SampleTag_t sampletags;

    // get CHROM
    const std::string& chrom(_header.chrom_data[bpA.interval.tid].label);
    const std::string& mateChrom(_header.chrom_data[bpB.interval.tid].label);

    const known_pos_range2& bpArange(bpA.interval.range);
    const known_pos_range2& bpBrange(bpB.interval.range);

    if (! isImprecise)
    {
        assert(bpArange.size() == bpBrange.size());
    }

    // get POS
    pos_t pos(bpArange.center_pos()+1);
    pos_t matePos(bpBrange.center_pos()+1);
    if (! isImprecise)
    {
        pos = bpArange.begin_pos()+1;
        if (isBreakendRangeSameShift)
        {
            matePos = bpBrange.begin_pos()+1;
        }
        else
        {
            matePos = bpBrange.end_pos();
        }
    }

    // TODO: improve circular genome handler:
    if ((pos<1) || (matePos<1)) return;

    // get ID
    const std::string& localId(isFirstBreakend ? svId.localId : svId.mateId);
    const std::string& mateId(isFirstBreakend ? svId.mateId : svId.localId);

    // get REF
    std::string ref;
    get_standardized_region_seq(_referenceFilename,chrom,pos-1,pos-1,ref);

    assert(1 == ref.size());

    const bool isReverseInsertSeq(! (isFirstBreakend || (bpA.state != bpB.state)));
    std::string tmpString;
    const std::string* insertSeqPtr(&sv.insertSeq);
    if (isReverseInsertSeq)
    {
        tmpString = reverseCompCopyStr(sv.insertSeq);
        insertSeqPtr = &tmpString;
    }
    const std::string& insertSeq(*insertSeqPtr);

    // build alt:
    boost::format altFormat("%4%%3%%1%:%2%%3%%5%");
    {
        std::string altPrefix;
        std::string altSuffix;
        if     (bpA.state == SVBreakendState::RIGHT_OPEN)
        {
            altPrefix = ref + insertSeq;
        }
        else if (bpA.state == SVBreakendState::LEFT_OPEN)
        {
            altSuffix = insertSeq + ref;
        }
        else
        {
            assert(false && "Unexpected bpA.state");
        }


        char altSep('?');
        if     (bpB.state == SVBreakendState::RIGHT_OPEN)
        {
            altSep=']';
        }
        else if (bpB.state == SVBreakendState::LEFT_OPEN)
        {
            altSep='[';
        }
        else
        {
            assert(false && "Unexpected bpB.state");
        }

        altFormat % mateChrom % matePos % altSep % altPrefix % altSuffix;
    }

    // build INFO field
    infotags.push_back("SVTYPE=BND");
    infotags.push_back("MATEID="+mateId);
    if (isImprecise)
    {
        infotags.push_back("IMPRECISE");
    }

    if (bpArange.size() > 1)
    {
        infotags.push_back( str( boost::format("CIPOS=%i,%i") % ((bpArange.begin_pos()+1) - pos) % (bpArange.end_pos() - pos) ));
    }

    if (! isImprecise)
    {
        if (bpArange.size() > 1)
        {
            infotags.push_back( str( boost::format("HOMLEN=%i") % (bpArange.size()-1) ));
            std::string homref;
            get_standardized_region_seq(_referenceFilename,chrom,bpArange.begin_pos()+1,bpArange.end_pos()-1,homref);
            infotags.push_back( str( boost::format("HOMSEQ=%s") % (homref) ));
        }
    }

    if (! insertSeq.empty())
    {
        infotags.push_back( str( boost::format("SVINSLEN=%i") % (insertSeq.size()) ));
        infotags.push_back( str( boost::format("SVINSSEQ=%s") % (insertSeq) ));
    }

    addSharedInfo(event, infotags);

    modifyInfo(event, infotags);
    modifyTranslocInfo(sv, isFirstBreakend, infotags);

    modifySample(sv, sampletags);
#ifdef DEBUG_VCF
    addDebugInfo(bpA, bpB, isFirstBreakend, adata, infotags);
#endif

    if (_isRNA)
    {
        addRNAInfo(isFirstBreakend, sv, adata, infotags);
#ifdef DEBUG_VCF
        addRNADebugInfo(isFirstBreakend, sv, adata, infotags);
#endif
    }

    // write out record:
    _os << chrom
        << '\t' << pos
        << '\t' << localId // ID
        << '\t' << ref // REF
        << '\t' << str( altFormat ) // ALT
        << '\t';
    writeQual();
    _os << '\t';
    writeFilter();
    _os << '\t';
    makeInfoField(infotags,_os); // INFO
    makeFormatSampleField(sampletags, _os); // FORMAT + SAMPLE
    _os << '\n';
}



void
VcfWriterSV::
writeTranslocPair(
    const SVCandidate& sv,
    const SVId& svId,
    const SVCandidateSetData& svData,
    const SVCandidateAssemblyData& adata,
    const EventInfo& event)
{
    writeTransloc(sv, svId, true, svData, adata, event);
    writeTransloc(sv, svId, false, svData, adata, event);
}



void
VcfWriterSV::
writeInvdel(
    const SVCandidate& sv,
    const SVId& svId,
    const bool isIndel,
    const EventInfo& event)
{
    const bool isImprecise(sv.isImprecise());
    const bool isBreakendRangeSameShift(sv.isBreakendRangeSameShift());

    const bool isBp1First(sv.bp1.interval.range.begin_pos()<=sv.bp2.interval.range.begin_pos());

    const SVBreakend& bpA(isBp1First ? sv.bp1 : sv.bp2);
    const SVBreakend& bpB(isBp1First ? sv.bp2 : sv.bp1);

    InfoTag_t infoTags;
    SampleTag_t sampleTags;

    // get CHROM
    const std::string& chrom(_header.chrom_data[sv.bp1.interval.tid].label);

    const known_pos_range2& bpArange(bpA.interval.range);
    const known_pos_range2& bpBrange(bpB.interval.range);

    if (! isImprecise)
    {
        assert(bpArange.size() == bpBrange.size());
    }

    // above this size all records use symbolic alleles (ie. <DEL>):
    static const unsigned maxNonSymbolicRecordSize(1000);

    // if the variant is a combination of simple insertion and deletions, and below
    // a large-event size threshold, it is classified as a small variant. In this case
    // we report the event using full REF and ALT sequences, plus a CIGAR string for
    // complex in/del combinations
    //
    bool isSmallVariant(false);
    if ((! isImprecise) && isIndel && (! sv.isUnknownSizeInsertion))
    {
        const unsigned deleteSize(bpBrange.begin_pos() - bpArange.begin_pos());
        const unsigned insertSize(sv.insertSeq.size());

        const bool isSmallDelete(deleteSize<=maxNonSymbolicRecordSize);
        const bool isSmallInsert(insertSize<=maxNonSymbolicRecordSize);

        isSmallVariant = (isSmallDelete && isSmallInsert);
    }

    // get POS and endPos,
    // first compute internal coordinates and then transform per vcf conventions:
    pos_t internal_pos(bpArange.center_pos());
    pos_t internal_endPos(bpBrange.center_pos());
    if (! isImprecise)
    {
        internal_pos = bpArange.begin_pos();
        if (isBreakendRangeSameShift)
        {
            internal_endPos = bpBrange.begin_pos();
        }
        else
        {
            internal_endPos = (bpBrange.end_pos()- 1);
        }
    }

    // now create external pos values for vcf only
    // everything is +1'd to get out zero-indexed coordinates:
    pos_t pos(internal_pos+1);
    pos_t endPos(internal_endPos+1);

    // variants are adjusted by up to one base according to breakend direction to match vcf spec:
    const pos_t bpABkptAdjust(bpA.getLeftSideOfBkptAdjustment());
    const pos_t bpBBkptAdjust(bpB.getLeftSideOfBkptAdjustment());
    pos += bpABkptAdjust;
    endPos += bpBBkptAdjust;

    if (isImprecise)
    {
        // check against the rare IMPRECISE case arising when CIEND is a subset of CIPOS:
        endPos=std::max(endPos,pos+1);
    }

    if (pos<1) return;

    // get REF
    std::string ref;
    {
        const pos_t beginRefPos(pos-1);
        pos_t endRefPos(beginRefPos);
        if (isSmallVariant) endRefPos=endPos-1;

        get_standardized_region_seq(_referenceFilename, chrom, beginRefPos, endRefPos, ref);

        if (static_cast<unsigned>(1+endRefPos-beginRefPos) != ref.size())
        {
            using namespace illumina::common;

            std::ostringstream oss;
            oss << "ERROR: Unexpected reference allele size: " << ref.size() << "\n";
            oss << "\tExpected: " << (1+endRefPos-beginRefPos) << "\n";
            oss << "\tbeginRefPos: " << beginRefPos << " endRefPos: " << endRefPos << " isSmallVariant: " << isSmallVariant << "\n";
            BOOST_THROW_EXCEPTION(LogicException(oss.str()));
        }
    }

    // build alt:
    std::string alt;
    if (isSmallVariant)
    {
        alt = ref[0] + sv.insertSeq;
    }
    else
    {
        alt = str( boost::format("<%s>") % svId.getLabel());
    }

    // build INFO field
    std::vector<std::string> words;
    split_string(svId.getLabel(),':',words);
    {
        // note that there's a reasonable argument for displaying these tags only when a
        // symbolic allele is used (by a strict reading of the vcf spec) -- we instead
        // print these fields for all variants for uniformity within the manta vcf:
        //
        infoTags.push_back( str(boost::format("END=%i") % endPos));
        infoTags.push_back( str(boost::format("SVTYPE=%s") % words[0]));
        const pos_t refLen(endPos-pos);
        pos_t svLen(refLen);

        if (! sv.isUnknownSizeInsertion)
        {
            if (isIndel)
            {
                const pos_t insertLen(static_cast<pos_t>(sv.insertSeq.size()));
                if ( insertLen > refLen )
                {
                    svLen = insertLen;
                }
                else
                {
                    svLen = -refLen;
                }
            }
            infoTags.push_back( str(boost::format("SVLEN=%i") % (svLen)));
        }
    }

    if (isSmallVariant)
    {
        if (! sv.insertAlignment.empty())
        {
            std::string cigar;
            apath_to_cigar(sv.insertAlignment,cigar);

            // add the 1M to signify the leading reference base:
            infoTags.push_back( str(boost::format("CIGAR=1M%s") % cigar));
        }
    }

    if (isImprecise)
    {
        infoTags.push_back("IMPRECISE");
    }

    if (bpArange.size() > 1)
    {
        infoTags.push_back( str( boost::format("CIPOS=%i,%i") % (bpArange.begin_pos() - internal_pos) % ((bpArange.end_pos()-1) - internal_pos) ));
    }

    if (! isSmallVariant)
    {
        if (bpBrange.size() > 1)
        {
            infoTags.push_back( str( boost::format("CIEND=%i,%i") % (bpBrange.begin_pos() - internal_endPos) % ((bpBrange.end_pos()-1) - internal_endPos) ));
        }
    }

    if (! isImprecise)
    {
        if (bpArange.size() > 1)
        {
            infoTags.push_back( str( boost::format("HOMLEN=%i") % (bpArange.size()-1) ));
            std::string homref;
            get_standardized_region_seq(_referenceFilename,chrom,bpArange.begin_pos()+1,bpArange.end_pos()-1,homref);
            infoTags.push_back( str( boost::format("HOMSEQ=%s") % (homref) ));
        }
    }

    if (! isSmallVariant)
    {
        if (! (sv.insertSeq.empty() || sv.isUnknownSizeInsertion))
        {
            infoTags.push_back( str( boost::format("SVINSLEN=%i") % (sv.insertSeq.size()) ));
            if (isBp1First || (bpA.state != bpB.state))
            {
                infoTags.push_back( str( boost::format("SVINSSEQ=%s") % (sv.insertSeq) ));
            }
            else
            {
                infoTags.push_back( str( boost::format("SVINSSEQ=%s") % reverseCompCopyStr(sv.insertSeq) ));
            }
        }
    }

    if (sv.isUnknownSizeInsertion)
    {
        if (! sv.unknownSizeInsertionLeftSeq.empty())
        {
            infoTags.push_back( str( boost::format("LEFT_SVINSSEQ=%s") % (sv.unknownSizeInsertionLeftSeq) ));
        }

        if (! sv.unknownSizeInsertionRightSeq.empty())
        {
            infoTags.push_back( str( boost::format("RIGHT_SVINSSEQ=%s") % (sv.unknownSizeInsertionRightSeq) ));
        }
    }

    if (svId.svType == EXTENDED_SV_TYPE::INVERSION)
    {
        if (sv.bp1.state == SVBreakendState::RIGHT_OPEN)
        {
            infoTags.push_back("INV3");
        }
        else if (sv.bp1.state == SVBreakendState::LEFT_OPEN)
        {
            infoTags.push_back("INV5");
        }
        else
        {
            assert(false && "Unexpected inversion configuration");
        }
    }

    addSharedInfo(event, infoTags);

    modifyInfo(event, infoTags);
    modifyInvdelInfo(sv, isBp1First, infoTags);

    modifySample(sv, sampleTags);

    // write out record:
    _os << chrom
        << '\t' << pos
        << '\t' << svId.localId // ID
        << '\t' << ref // REF
        << '\t' << alt // ALT
        << '\t';
    writeQual();
    _os << '\t';
    writeFilter();
    _os << '\t';
    makeInfoField(infoTags,_os); // INFO
    makeFormatSampleField(sampleTags, _os); // FORMAT + SAMPLE
    _os << '\n';
}



static
bool
isAcceptedSVType(
    const EXTENDED_SV_TYPE::index_t svType)
{
    using namespace EXTENDED_SV_TYPE;

    switch (svType)
    {
    case INTERTRANSLOC:
    case INTRATRANSLOC:
    case INVERSION:
    case INSERT:
    case DELETE:
    case TANDUP:
        return true;
    default:
        return false;
    }
}



void
VcfWriterSV::
writeSVCore(
    const SVCandidateSetData& svData,
    const SVCandidateAssemblyData& adata,
    const SVCandidate& sv,
    const SVId& svId,
    const EventInfo& event)
{
    using namespace EXTENDED_SV_TYPE;
    const index_t svType(getExtendedSVType(sv, _isRNA));

#ifdef DEBUG_VCF
    log_os << "VcfWriterSV::writeSVCore svType: " << EXTENDED_SV_TYPE::label(svType) << "\n";
#endif

    if (! isAcceptedSVType(svType))
    {
        using namespace illumina::common;

        std::ostringstream oss;
        oss << "ERROR: sv candidate cannot be classified: " << sv << "\n";
        BOOST_THROW_EXCEPTION(LogicException(oss.str()));
    }

    try
    {
        if (isSVTransloc(svType))
        {
            writeTranslocPair(sv, svId, svData, adata, event);
        }
        else
        {
            const bool isIndel(isSVIndel(svType));
            writeInvdel(sv, svId, isIndel, event);
        }
    }
    catch (...)
    {
        log_os << "Exception caught while attempting to write sv candidate to vcf: " << sv << "\n";
        log_os << "\tsvId: " << svId.getLabel() << " ext-svType: " << EXTENDED_SV_TYPE::label(svType) << "\n";
        throw;
    }
}



void
VcfWriterSV::
writeFilters(
    const std::set<std::string>& filters,
    std::ostream& os)
{
    if (filters.empty())
    {
        os << "PASS";
    }
    else
    {
        bool isFirst(true);
        for (const std::string& filter : filters)
        {
            if (isFirst)
            {
                isFirst=false;
            }
            else
            {
                os << ';';
            }
            os << filter;
        }
    }
}



void
VcfWriterSV::
writeFilters(
    const std::set<std::string>& filters,
    std::string& s)
{
    std::ostringstream oss;
    writeFilters(filters,oss);
    s = oss.str();
}
