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

#include "format/VcfWriterCandidateSV.hh"




void
VcfWriterCandidateSV::
addHeaderInfo() const
{
    _os << "##INFO=<ID=PAIR_COUNT,Number=1,Type=Integer,Description=\"Read pairs supporting this variant where both reads are confidently mapped\">\n";
    _os << "##INFO=<ID=BND_PAIR_COUNT,Number=1,Type=Integer,Description=\"Confidently mapped reads supporting this variant at this breakend (mapping may not be confident at remote breakend)\">\n";
    _os << "##INFO=<ID=UPSTREAM_PAIR_COUNT,Number=1,Type=Integer,Description=\"Confidently mapped reads supporting this variant at the upstream breakend (mapping may not be confident at downstream breakend)\">\n";
    _os << "##INFO=<ID=DOWNSTREAM_PAIR_COUNT,Number=1,Type=Integer,Description=\"Confidently mapped reads supporting this variant at this downstream breakend (mapping may not be confident at upstream breakend)\">\n";
}



void
VcfWriterCandidateSV::
modifyTranslocInfo(
    const SVCandidate& sv,
    const bool isFirstOfPair,
    InfoTag_t& infoTags) const
{
    const SVBreakend& bpA( isFirstOfPair ? sv.bp1 : sv.bp2);

    infoTags.push_back( str(boost::format("BND_PAIR_COUNT=%i") % bpA.getLocalPairCount()) );
    infoTags.push_back( str(boost::format("PAIR_COUNT=%i") % bpA.getPairCount()) );
}



void
VcfWriterCandidateSV::
modifyInvdelInfo(
    const SVCandidate& sv,
    const bool isBp1First,
    InfoTag_t& infoTags) const
{
    const SVBreakend& bpA( isBp1First ? sv.bp1 : sv.bp2);
    const SVBreakend& bpB( isBp1First ? sv.bp2 : sv.bp1);

    infoTags.push_back( str(boost::format("UPSTREAM_PAIR_COUNT=%i") % bpA.getLocalPairCount()) );
    infoTags.push_back( str(boost::format("DOWNSTREAM_PAIR_COUNT=%i") % bpB.getLocalPairCount()) );
    infoTags.push_back( str(boost::format("PAIR_COUNT=%i") % bpA.getPairCount()) );
}



void
VcfWriterCandidateSV::
writeSV(
    const SVCandidateSetData& svData,
    const SVCandidateAssemblyData& adata,
    const SVCandidate& sv,
    const SVId& svId)
{
    static const EventInfo event;
    writeSVCore( svData, adata, sv, svId, event);
}
