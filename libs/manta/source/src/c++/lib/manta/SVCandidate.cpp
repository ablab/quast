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

#include "manta/SVCandidate.hh"

#include <iostream>



std::ostream&
operator<<(
    std::ostream& os,
    const SVCandidate& svc)
{
    static const char indent('\t');
    os << "SVCandidate:\n"
       << indent << "isImprecise?: " << svc.isImprecise() << "\n"
       << indent << "fwReads: " << svc.fwReads << " ; rvReads: " << svc.rvReads << "\n"
       << indent << "index candidate:assemblyAlign:assemblySegment: " << svc.candidateIndex
       << ":" << svc.assemblyAlignIndex
       << ":" << svc.assemblySegmentIndex
       << "\n";
    if (! svc.isImprecise())
    {
        os << "\tAlignment: " << svc.insertAlignment << "\n"
           << "\tBreakendInsertSeq: " << svc.insertSeq << "\n";
    }
    os << "\t" << svc.bp1 << "\n"
       << "\t" << svc.bp2 << "\n";
    return os;
}



std::ostream&
operator<<(
    std::ostream& os,
    const SVObservation& svc)
{
    os << static_cast<SVCandidate>(svc);
    os << "SVObservation etype: " << SVEvidenceType::label(svc.evtype)
       << " fragtype: " << FRAGSOURCE::label(svc.fragSource) << "\n";
    return os;
}
