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
///
///

#include "SimpleAlignment_bam_util.hh"
#include "htsapi/align_path_bam_util.hh"



void
getAlignment(
    const bam_record& bamRead,
    SimpleAlignment& al)
{
    al.is_fwd_strand=bamRead.is_fwd_strand();
    al.tid=bamRead.target_id();
    al.pos=(bamRead.pos()-1);

    bam_cigar_to_apath(bamRead.raw_cigar(),bamRead.n_cigar(),al.path);
}



SimpleAlignment
getAlignment(
    const bam_record& bamRead)
{
    SimpleAlignment al;
    getAlignment(bamRead,al);
    return al;
}


SimpleAlignment
getFakeMateAlignment(
    const bam_record& bamRead)
{
    SimpleAlignment al;
    assert(! bamRead.is_mate_unmapped());
    al.is_fwd_strand=bamRead.is_mate_fwd_strand();
    al.tid=bamRead.mate_target_id();
    al.pos=(bamRead.mate_pos()-1);
    al.path.emplace_back(ALIGNPATH::MATCH, bamRead.read_size());
    return al;
}
