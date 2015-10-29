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

#include "htsapi/bam_record.hh"
#include "svgraph/GenomeInterval.hh"


/// This enables specification of different methods which
/// must traverse a range of reads in a bam file. By abstracting
/// multiple methods to this interface, we can accomplish multiple
/// tasks over a single pass of the BAM records while maintaining
/// isolation of methods
///
struct BamRegionProcessor
{
    virtual
    ~BamRegionProcessor() {}

    /// provide the index of the next bam file, must be called before switching files/samples
    ///
    /// for each bam index, return the requested interval for this operation,
    /// operations with closely related intervals will be compbined
    /// and the union of intervals will be processed
    virtual
    const GenomeInterval&
    nextBamIndex(
        const unsigned bamIndex) = 0;

    /// provide the next bam record
    virtual
    void
    processRecord(
        const bam_record& bamRead) = 0;
};
