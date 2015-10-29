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

#include "options/optionsUtil.hh"
#include "options/AlignmentFileOptionsParser.hh"

#include <set>


typedef std::vector<std::string> files_t;



boost::program_options::options_description
getOptionsDescription(
    AlignmentFileOptions& /*opt*/)
{
    namespace po = boost::program_options;
    po::options_description desc("alignment-files");
    desc.add_options()
    ("align-file", po::value<files_t>(),
     "alignment file in BAM or CRAM format (may be specified multiple times, assumed to be non-tumor if tumor file(s) provided)")
    ("tumor-align-file", po::value<files_t>(),
     "tumor sample alignment file in BAM or CRAM format (may be specified multiple times)")
    ;
    return desc;
}


bool
parseOptions(
    const boost::program_options::variables_map& vm,
    AlignmentFileOptions& opt,
    std::string& errorMsg)
{
    // paste together tumor and normal:
    {
        files_t normal;
        files_t tumor;
        if (vm.count("align-file"))
        {
            normal=(boost::any_cast<files_t>(vm["align-file"].value()));
        }
        if (vm.count("tumor-align-file"))
        {
            tumor=(boost::any_cast<files_t>(vm["tumor-align-file"].value()));
        }
        opt.alignmentFilename = normal;
        opt.alignmentFilename.insert(opt.alignmentFilename.end(),
                                     tumor.begin(),
                                     tumor.end());
        opt.isAlignmentTumor.clear();
        opt.isAlignmentTumor.resize(normal.size(), false);
        opt.isAlignmentTumor.resize(opt.alignmentFilename.size(), true);
    }

    errorMsg.clear();
    if (opt.alignmentFilename.empty())
    {
        errorMsg="Must specify at least one input alignment file";
    }
    else
    {
        // check that alignment files exist, and names do not repeat
        std::set<std::string> nameCheck;
        for (std::string& afile : opt.alignmentFilename)
        {
            if (checkStandardizeInputFile(afile,"alignment file",errorMsg)) break;
            if (nameCheck.count(afile))
            {
                std::ostringstream oss;
                oss << "Repeated alignment filename: " << afile << "\n";
                errorMsg = oss.str();
                break;
            }
            nameCheck.insert(afile);
        }
    }

    return (! errorMsg.empty());
}
