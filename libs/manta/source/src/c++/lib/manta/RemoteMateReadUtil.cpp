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
/// \author Ole Schulz-Trieglaff
/// \author Chris Saunders
///

#include "RemoteMateReadUtil.hh"

#include <cstdlib>



bool
isMateInsertionEvidenceCandidate(
    const bam_record& bamRead,
    const unsigned minMapq)
{
    if (! bamRead.is_paired()) return false;
    if (bamRead.isNonStrictSupplement()) return false;
    if (bamRead.is_unmapped() || bamRead.is_mate_unmapped()) return false;

    if (bamRead.map_qual() < minMapq) return false;

    if (bamRead.target_id() < 0) return false;
    if (bamRead.mate_target_id() < 0) return false;

    if (bamRead.target_id() != bamRead.mate_target_id()) return true;

    /// TODO: better candidate definition based on fragment size distro:
    static const int minSize(10000);
    return (std::abs(bamRead.pos()-bamRead.mate_pos()) >= minSize);
}



bool
isMateInsertionEvidenceCandidate2(
    const bam_record& bamRead,
    const bool isSearchForLeftOpen,
    const bool isSearchForRightOpen)
{
    if ((! isSearchForLeftOpen) && (! bamRead.is_fwd_strand())) return false;
    if ((! isSearchForRightOpen) && bamRead.is_fwd_strand()) return false;
    return true;
}
