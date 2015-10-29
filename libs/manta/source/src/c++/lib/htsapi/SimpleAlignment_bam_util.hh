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

#pragma once

#include "blt_util/SimpleAlignment.hh"
#include "htsapi/bam_record.hh"


void
getAlignment(
    const bam_record& bamRead,
    SimpleAlignment& al);

SimpleAlignment
getAlignment(
    const bam_record& bamRead);

/// generate a mate alignment, assuming same read length and perfect alignment
SimpleAlignment
getFakeMateAlignment(
    const bam_record& bamRead);


