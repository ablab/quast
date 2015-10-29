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
/// \author Chris Saunders and Xiaoyu Chen
///

#pragma once

#include "htsapi/bam_record.hh"
#include "SVEvidence.hh"


void
setReadEvidence(
    const unsigned minMapQ,
    const unsigned minTier2MapQ,
    const unsigned mapq,
    const unsigned readSize,
    const bool isShadow,
    SVFragmentEvidenceRead& read);


inline
void
setReadEvidence(
    const unsigned minMapQ,
    const unsigned minTier2MapQ,
    const bam_record& bamRead,
    const bool isShadow,
    SVFragmentEvidenceRead& read)
{
    setReadEvidence(minMapQ, minTier2MapQ,
                    bamRead.map_qual(), bamRead.read_size(),
                    isShadow, read);
}
