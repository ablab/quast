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

/// \file

/// \author Chris Saunders
///

#include "blt_util/log.hh"
#include "blt_util/prob_util.hh"

#include <cstdlib>

#include <iomanip>
#include <iostream>



void
check_ln_distro_invalid_value(const char* label,
                              const double val,
                              const unsigned n)
{
    log_os << std::setprecision(14) << std::fixed;
    log_os << "ERROR: " << label << " element [" << n << "] has invalid value: '" << val << "'\n";
    log_os.unsetf(std::ios::fixed);
    exit(EXIT_FAILURE);
}



void
check_ln_distro_invalid_sum(const char* label,
                            const double sum)
{
    log_os << std::setprecision(14) << std::fixed;
    log_os << "ERROR: " << label << " sum is: '" << sum << "'\n";
    log_os.unsetf(std::ios::fixed);
    exit(EXIT_FAILURE);
}
