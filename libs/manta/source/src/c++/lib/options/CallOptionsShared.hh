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


/// options shared by multiple scoring schemes:
///
/// Note that in theory these could be offered once for each scoring scheme, but
/// it would be difficult to do this efficiently because these options have an impact
/// on early scoring likelihoods.
///
struct CallOptionsShared
{
    /// This influences alignments to the ref allele when comparing ref vs alt align quality
    float snpPrior = 1e-3f;
};
