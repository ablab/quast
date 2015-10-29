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

#include "ESLOptions.hh"

#include "blt_util/log.hh"
#include "manta/ProgramUtil.hh"
#include "options/AlignmentFileOptionsParser.hh"
#include "options/ReadScannerOptionsParser.hh"
#include "options/SVLocusSetOptionsParser.hh"
#include "options/optionsUtil.hh"

#include "boost/program_options.hpp"

#include <iostream>


typedef std::vector<std::string> regions_t;


static
void
usage(
    std::ostream& os,
    const manta::Program& prog,
    const boost::program_options::options_description& visible,
    const char* msg = nullptr)
{
    usage(os, prog, visible, "partition sv evidence regions", "", msg);
}



static
void
checkStandardizeUsageFile(
    std::ostream& os,
    const manta::Program& prog,
    const boost::program_options::options_description& visible,
    std::string& filename,
    const char* fileLabel)
{
    std::string errorMsg;
    if ( checkStandardizeInputFile(filename, fileLabel, errorMsg))
    {
        usage(os,prog,visible,errorMsg.c_str());
    }
}



void
parseESLOptions(
    const manta::Program& prog,
    int argc, char* argv[],
    ESLOptions& opt)
{
    namespace po = boost::program_options;
    po::options_description req("configuration");
    req.add_options()
    ("output-file", po::value(&opt.outputFilename),
     "write SV Locus graph to file (required)")
    ("ref", po::value(&opt.referenceFilename),
     "fasta reference sequence (required)")
    ("align-stats", po::value(&opt.statsFilename),
     "pre-computed alignment statistics for the input alignment files (required)")
    ("chrom-depth", po::value(&opt.chromDepthFilename),
     "average depth estimate for each chromosome")
    ("truth-vcf", po::value(&opt.truthVcfFilename),
     "optional truth VCF file (for testing)")
    ("region", po::value<regions_t>(),
     "samtools formatted region, eg. 'chr1:20-30'. May be supplied more than once but regions must not overlap. At least one entry required.")
    ("rna", po::value(&opt.isRNA)->zero_tokens(),
     "For RNA input. Changes small fragment handling.")
    ;

    po::options_description alignDesc(getOptionsDescription(opt.alignFileOpt));
    po::options_description scanDesc(getOptionsDescription(opt.scanOpt));
    po::options_description graphDesc(getOptionsDescription(opt.graphOpt));

    po::options_description help("help");
    help.add_options()
    ("help,h","print this message");

    po::options_description visible("options");
    visible.add(alignDesc).add(scanDesc).add(graphDesc).add(req).add(help);

    bool po_parse_fail(false);
    po::variables_map vm;
    try
    {
        po::store(po::parse_command_line(argc, argv, visible,
                                         po::command_line_style::unix_style ^ po::command_line_style::allow_short), vm);
        po::notify(vm);
    }
    catch (const boost::program_options::error& e)     // todo:: find out what is the more specific exception class thrown by program options
    {
        log_os << "\nERROR: Exception thrown by option parser: " << e.what() << "\n";
        po_parse_fail=true;
    }

    if ((argc<=1) || (vm.count("help")) || po_parse_fail)
    {
        usage(log_os,prog,visible);
    }

    if (vm.count("region"))
    {
        opt.regions=(boost::any_cast<regions_t>(vm["region"].value()));
    }

    std::string errorMsg;
    if (parseOptions(vm, opt.alignFileOpt, errorMsg))
    {
        usage(log_os,prog,visible,errorMsg.c_str());
    }
    else if (parseOptions(vm, opt.scanOpt, errorMsg))
    {
        usage(log_os,prog,visible,errorMsg.c_str());
    }
    else if (opt.outputFilename.empty())
    {
        usage(log_os,prog,visible,"Must specify a graph output file");
    }
    else if (opt.referenceFilename.empty())
    {
        usage(log_os,prog,visible,"Need the FASTA reference file");
    }
    else if (opt.regions.empty())
    {
        usage(log_os,prog,visible,"Need at least one samtools formatted region");
    }

    for (const auto& region : opt.regions)
    {
        if (region.empty())
        {
            usage(log_os,prog,visible,"Empty region argument");
        }
    }

    checkStandardizeUsageFile(log_os,prog,visible,opt.statsFilename,"alignment statistics");
    checkStandardizeUsageFile(log_os,prog,visible,opt.referenceFilename,"reference fasta");

    if (! opt.chromDepthFilename.empty())
    {
        checkStandardizeUsageFile(log_os,prog,visible,opt.chromDepthFilename,"chromosome depth");
    }
}
