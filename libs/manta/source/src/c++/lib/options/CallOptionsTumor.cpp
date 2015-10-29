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
/// \author Xiaoyu Chen
///

#include "options/CallOptionsTumor.hh"


boost::program_options::options_description
getOptionsDescription(CallOptionsTumor& opt)
{
    namespace po = boost::program_options;
    po::options_description desc("tumor-only-variant-calling");
    desc.add_options()
    ("tumor-max-depth-factor", po::value(&opt.maxDepthFactor)->default_value(opt.maxDepthFactor),
     "Variants where the tumor-sample depth around the breakpoint is greater than this factor x the chromosomal mean will be filtered out")
    ;

    return desc;
}





