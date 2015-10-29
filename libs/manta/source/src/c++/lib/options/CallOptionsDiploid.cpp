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

#include "options/CallOptionsDiploid.hh"


boost::program_options::options_description
getOptionsDescription(CallOptionsDiploid& opt)
{
    namespace po = boost::program_options;
    po::options_description desc("germline-variant-calling");
    desc.add_options()
    ("diploid-max-depth-factor", po::value(&opt.maxDepthFactor)->default_value(opt.maxDepthFactor),
     "Variants where the depth around the breakpoint is greater than this factor x the chromosomal mean will be filtered out")
    ("min-qual-score", po::value(&opt.minOutputAltScore)->default_value(opt.minOutputAltScore),
     "minimum QUAL score for variants included in the germline output vcf")
    ("min-pass-qual-score", po::value(&opt.minPassAltScore)->default_value(opt.minPassAltScore),
     "minimum QUAL score for variants to PASS in germline output vcf")
    ("min-pass-gt-score", po::value(&opt.minPassGTScore)->default_value(opt.minPassGTScore),
     "minimum genotype quality score below which samples are filtered for a variant in the germline output vcf")
    ;

    return desc;
}

