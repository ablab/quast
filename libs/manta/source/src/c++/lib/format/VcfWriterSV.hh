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

#pragma once

#include "manta/EventInfo.hh"
#include "manta/JunctionIdGenerator.hh"
#include "manta/SVCandidate.hh"
#include "manta/SVCandidateAssemblyData.hh"
#include "manta/SVCandidateSetData.hh"
#include "svgraph/SVLocusSet.hh"

#include <iosfwd>


struct VcfWriterSV
{
    VcfWriterSV(
        const std::string& referenceFilename,
        const bool isRNA,
        const SVLocusSet& set,
        std::ostream& os);

    virtual
    ~VcfWriterSV() {}

    void
    writeHeader(
        const char* progName,
        const char* progVersion,
        const std::vector<std::string>& sampleNames)
    {
        writeHeaderPrefix(progName, progVersion);
        writeHeaderColumnKey(sampleNames);
    }

    typedef std::vector<std::string> InfoTag_t;
    typedef std::vector<std::pair<std::string,std::vector<std::string> > > SampleTag_t;

protected:
    void
    writeHeaderPrefix(
        const char* progName,
        const char* progVersion);

    void
    writeHeaderColumnKey(
        const std::vector<std::string>& sampleNames) const;

    virtual
    void
    addHeaderInfo() const {}

    virtual
    void
    addHeaderFormat() const {}

    virtual
    void
    addHeaderFilters() const {}

    void
    writeSVCore(
        const SVCandidateSetData& svData,
        const SVCandidateAssemblyData& adata,
        const SVCandidate& sv,
        const SVId& svId,
        const EventInfo& event);

    /// add info tags which can be customized by sub-class
    virtual
    void
    modifyInfo(
        const EventInfo& /*event*/,
        InfoTag_t& /*infotags*/) const
    {}

    /// add info tags specific to translocations:
    virtual
    void
    modifyTranslocInfo(
        const SVCandidate& /*sv*/,
        const bool /*isFirstOfPair*/,
        InfoTag_t& /*infoTags*/) const
    {}

    /// add info tags specific to non-translocations:
    virtual
    void
    modifyInvdelInfo(
        const SVCandidate& /*sv*/,
        const bool /*isBp1First*/,
        InfoTag_t& /*infoTags*/) const
    {}

    virtual
    void
    writeQual() const
    {
        _os << '.';
    }

    virtual
    void
    writeFilter() const
    {
        _os << '.';
    }

    virtual
    void
    modifySample(
        const SVCandidate& /*sv*/,
        SampleTag_t& /*sampletags*/) const
    {}

    static
    void
    writeFilters(
        const std::set<std::string>& filters,
        std::ostream& os);

    static
    void
    writeFilters(
        const std::set<std::string>& filters,
        std::string& s);

private:

    /// \param[in] isFirstBreakend if true report bp1, else report bp2
    void
    writeTransloc(
        const SVCandidate& sv,
        const SVId& svId,
        const bool isFirstBreakend,
        const SVCandidateSetData& svData,
        const SVCandidateAssemblyData& adata,
        const EventInfo& event);

    void
    writeTranslocPair(
        const SVCandidate& sv,
        const SVId& svId,
        const SVCandidateSetData& svData,
        const SVCandidateAssemblyData& adata,
        const EventInfo& event);

    /// \param isIndel if true, the variant is a simple right/left breakend insert/delete combination
    void
    writeInvdel(
        const SVCandidate& sv,
        const SVId& svId,
        const bool isIndel,
        const EventInfo& event);

protected:
    const std::string& _referenceFilename;
    const bool _isRNA;

private:
    const bam_header_info& _header;
protected:
    std::ostream& _os;
};

