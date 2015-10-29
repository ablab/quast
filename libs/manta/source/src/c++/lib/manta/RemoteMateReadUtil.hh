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

#pragma once

#include "htsapi/bam_record.hh"

#include <string>

/// this is the first of 2 components to determine mate
/// insert candidacy. Note that we assume base filtration
/// (PCR dups, etc.) has already occurred.
///
bool
isMateInsertionEvidenceCandidate(
    const bam_record& bamRead,
    const unsigned minMapq);


bool
isMateInsertionEvidenceCandidate2(
    const bam_record& bamRead,
    const bool isSearchForLeftOpen,
    const bool isSearchForRightOpen);


/// information recorded for reads where we need to grab the mate from a remote locus
///
/// typically these are chimeras with a MAPQ0 mate used to assemble a large insertion
///
struct RemoteReadInfo
{
    explicit
    RemoteReadInfo(
        const bam_record& bamRead)
        : qname(bamRead.qname()),
          readNo(bamRead.read_no()==1 ? 2 : 1),
          tid(bamRead.mate_target_id()),
          pos(bamRead.mate_pos() - 1),
          localPos(bamRead.pos() - 1),
          readSize(bamRead.read_size()),
          isLocalFwd(bamRead.is_fwd_strand()),
          isFound(false),
          isUsed(false)
    {}

    bool
    operator<(
        const RemoteReadInfo& rhs) const
    {
        if (tid < rhs.tid) return true;
        if (tid == rhs.tid)
        {
            return (pos < rhs.pos);
        }
        return false;
    }

    std::string qname;
    int readNo; // this is read number of the target
    int tid;
    int pos;
    int localPos;
    int readSize;
    bool isLocalFwd;
    bool isFound;
    bool isUsed;
};
