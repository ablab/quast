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
/// \brief functions to convert alignments to/from the match descriptor (MD) format
///
/// Note MD is an older format which should be considered deprecated.
///

#pragma once

#include "blt_util/align_path.hh"



namespace ALIGNPATH
{

void
export_md_to_apath(const char* md,
                   const bool is_fwd_strand,
                   path_t& apath,
                   const bool is_edge_deletion_error=true);

void
apath_to_export_md(path_t& apath,
                   const char* ref_seq,
                   const char* ref_end,
                   const int32_t ref_pos,
                   const std::string& read_bases,
                   const bool is_fwd_strand,
                   std::string& md);

}
