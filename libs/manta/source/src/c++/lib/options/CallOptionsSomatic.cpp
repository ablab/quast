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

#include "options/CallOptionsSomatic.hh"


boost::program_options::options_description
getOptionsDescription(CallOptionsSomatic& opt)
{
    namespace po = boost::program_options;
    po::options_description desc("somatic-variant-calling");
    desc.add_options()
    ("somatic-max-depth-factor", po::value(&opt.maxDepthFactor)->default_value(opt.maxDepthFactor),
     "Variants where the normal-sample depth around the breakpoint is greater than this factor x the chromosomal mean will be filtered out")
    ("min-somatic-score", po::value(&opt.minOutputSomaticScore)->default_value(opt.minOutputSomaticScore),
     "minimum somatic quality score for variants to be included in the somatic output vcf")
    ("min-pass-somatic-score", po::value(&opt.minPassSomaticScore)->default_value(opt.minPassSomaticScore),
     "minimum somatic quality score below which variants are marked as filtered in the somatic output vcf")
    /*
        ("noise-sv-prior", po::value(&opt.noiseSVPrior)->default_value(opt.noiseSVPrior),
         "probability of a spurious SV observation shared in the tumor and normal samples")
    */
    ;

    return desc;
}
