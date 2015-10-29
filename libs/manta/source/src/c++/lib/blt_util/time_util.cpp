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

#include "io_util.hh"
#include "time_util.hh"

#include <iomanip>
#include <iostream>



void
CpuTimes::
report(
    const double factor,
    const char* tlabel,
    std::ostream& os) const
{
    StreamScoper scoper(os);
    os << std::fixed << std::setprecision(4);
    const double fwall(wall*factor);
    const double fuser(user*factor);
    const double fsystem(system*factor);
    const double total(fuser+fsystem);
    const double perc(100*total/fwall);
    os << fwall << tlabel << " wall, "
       << fuser << tlabel << " user + "
       << fsystem << tlabel << " system = "
       << total << tlabel
       << " CPU (" << std::setprecision(2) << perc << "%)";
}
