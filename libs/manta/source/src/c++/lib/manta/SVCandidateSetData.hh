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

#include "alignment/Alignment.hh"
#include "htsapi/bam_record.hh"
#include "manta/SVBreakend.hh"
#include "svgraph/GenomeInterval.hh"

#include <iosfwd>
#include <map>
#include <vector>

//#define DEBUG_SVDATA

#ifdef DEBUG_SVDATA
#include "blt_util/log.hh"
#include <iostream>
#endif


/// A read associated with an SV associated set of regions
///
/// note this read could be linked with zero to many specific SVCandidates
///
struct SVCandidateSetRead
{
    bool
    isSet() const
    {
        return (! bamrec.empty());
    }

    bool
    isAnchored() const
    {
        return (isSet() && (!isSubMapped));
    }

    //realignment info, etc...
    bam_record bamrec;
    bool isNode1 = true; ///< used to link this read to node1 or node2 in the original graph ordering, note this is not the same as read1 and read2
    bool isSubMapped = false; ///< is mapq below the minimum normally required to use this read
    double mappedReadCount = 0;
    double subMappedReadCount = 0;
};

std::ostream&
operator<<(std::ostream& os, const SVCandidateSetRead& svr);


/// capture details of the link between a sequence fragment and an SV
struct SVSequenceFragmentAssociation
{
    typedef uint16_t index_t;

    explicit
    SVSequenceFragmentAssociation(
        const index_t initIndex = 0,
        const SVEvidenceType::index_t initEvtype = SVEvidenceType::UNKNOWN) :
        index(initIndex),
        evtype(initEvtype)
    {}

    index_t index;

    /// is the association from anom read pair, split read, CIGAR, etc?
    SVEvidenceType::index_t evtype;
};

std::ostream&
operator<<(std::ostream& os, const SVSequenceFragmentAssociation& sva);


/// A DNA/RNA fragment associated with an SV-associated set of regions
///
/// note this read could be linked with zero to many specific SVCandidates
///
struct SVCandidateSetSequenceFragment
{
    const char*
    qname() const
    {
        if      (read1.isSet()) return read1.bamrec.qname();
        else if (read2.isSet()) return read2.bamrec.qname();
        else if ((! read1Supplemental.empty()) && read1Supplemental.front().isSet())
        {
            return read1Supplemental.front().bamrec.qname();
        }
        else if ((! read2Supplemental.empty()) && read2Supplemental.front().isSet())
        {
            return read2Supplemental.front().bamrec.qname();
        }
        return nullptr;
    }

    bool
    isAnchored() const
    {
        return (read1.isAnchored() || read2.isAnchored());
    }

    std::vector<SVSequenceFragmentAssociation> svLink; ///< which SVs from the set are this molecule associated with?
    SVCandidateSetRead read1;
    std::vector<SVCandidateSetRead> read1Supplemental;
    SVCandidateSetRead read2;
    std::vector<SVCandidateSetRead> read2Supplemental;
};

std::ostream&
operator<<(std::ostream& os, const SVCandidateSetSequenceFragment& svp);



/// SVCandidateSet data associated with a specific bam-file/read-group
///
struct SVCandidateSetSequenceFragmentSampleGroup
{
    typedef std::vector<SVCandidateSetSequenceFragment> pair_t;
    typedef pair_t::iterator iterator;
    typedef pair_t::const_iterator const_iterator;

    /// increment once for each eligible read considered
    ///
    /// this information is used to determine signal rate
    /// so that it can be compared to sample specific noise
    /// levels
    void
    increment(
        const bool /*isNode1*/,
        const bool isSubMapped)
    {
        if (isSubMapped)
        {
            _subMappedReadCount++;
        }
        else
        {
            _mappedReadCount++;
        }
    }

    /// add a new bam record to the set:
    void
    add(const bam_record& bamRead,
        const bool isExpectRepeat,
        const bool isNode1,
        const bool isSubMapped);

    iterator
    begin()
    {
        return _pairs.begin();
    }

    iterator
    end()
    {
        return _pairs.end();
    }

    const_iterator
    begin() const
    {
        return _pairs.begin();
    }

    const_iterator
    end() const
    {
        return _pairs.end();
    }

    unsigned
    size() const
    {
        return _pairs.size();
    }

    bool
    isFull() const
    {
        return _isFull;
    }

    void
    setFull()
    {
        _isFull = true;
    }

private:
    typedef std::string bamqname_t;
    typedef std::map<bamqname_t,unsigned> pindex_t;

    /// get existing fragment or return pointer for a new fragment
    ///
    /// this will return null for new fragments when isFull() is true
    ///
    SVCandidateSetSequenceFragment*
    getSequenceFragment(const pindex_t::key_type& key);

    pair_t _pairs;
    pindex_t _pairIndex;

    bool _isFull = false; ///< this flag can be set if the object grows too large to insert more data into it

    double _mappedReadCount = 0;
    double _subMappedReadCount = 0;
};


/// Data gathered from a set of regions implicated to contain one or more SVs
///
/// Note these data are used for initial hypothesis generation, therefore the
/// reads are potentially associated with zero to many specific SV candidates
/// (although we expect any one read to usually be associated with no more
/// one).
///
struct SVCandidateSetData
{
    /// get evidence associated with a specific sample group:
    SVCandidateSetSequenceFragmentSampleGroup&
    getDataGroup(const unsigned bamIndex)
    {
        data_t::iterator diter(_data.find(bamIndex));
        if (diter != _data.end()) return diter->second;

        std::pair<data_t::iterator,bool> diter2 = _data.insert(std::make_pair(bamIndex,SVCandidateSetSequenceFragmentSampleGroup()));
        return diter2.first->second;
    }

    /// get evidence associated with a specific sample group:
    const SVCandidateSetSequenceFragmentSampleGroup&
    getDataGroup(const unsigned bamIndex) const
    {
        data_t::const_iterator diter(_data.find(bamIndex));
        assert(diter != _data.end());
        return diter->second;
    }

    void
    clear()
    {
        _data.clear();
        _searchIntervals.clear();
    }

    /// return true if this search interval overlaps with any previous
    /// intervals
    ///
    /// if true, we know to expect repeated qnames
    ///
    bool
    setNewSearchInterval(const GenomeInterval& newSearch);

private:
    typedef std::map<unsigned,SVCandidateSetSequenceFragmentSampleGroup> data_t;
    data_t _data;

    std::vector<GenomeInterval> _searchIntervals;
};
