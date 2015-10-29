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

#include "ChromDepthOptions.hh"

#include "blt_util/log.hh"
#include "manta/ProgramUtil.hh"
#include "options/optionsUtil.hh"

#include "boost/program_options.hpp"

#include <iostream>

typedef std::vector<std::string> chroms_t;


static
void
usage(
    std::ostream& os,
    const manta::Program& prog,
    const boost::program_options::options_description& visible,
    const char* msg = nullptr)
{
    usage(os, prog, visible, "get chromosome depth information from alignment files", " [ > output ]", msg);
}



void
parseChromDepthOptions(
    const manta::Program& prog,
    int argc, char* argv[],
    ChromDepthOptions& opt)
{
    namespace po = boost::program_options;
    po::options_description req("configuration");
    req.add_options()
    ("align-file", po::value(&opt.alignmentFilename),
     "alignment file in BAM or CRAM format")
    ("chrom", po::value<chroms_t>(),
     "chromosome name. May be supplied more than once. At least one entry required.")
    ("output-file", po::value(&opt.outputFilename),
     "write stats to filename (default: stdout)")
    ;

    po::options_description help("help");
    help.add_options()
    ("help,h","print this message");

    po::options_description visible("options");
    visible.add(req).add(help);

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
        log_os << "\n" << prog.name() << ": get chromosome depth from alignment file\n\n";
        log_os << "version: " << prog.version() << "\n\n";
        log_os << "usage: " << prog.name() << " [options] > stats\n\n";
        log_os << visible << "\n";
        exit(EXIT_FAILURE);
    }

    if (vm.count("chrom"))
    {
        opt.chromNames=(boost::any_cast<chroms_t>(vm["chrom"].value()));
    }

    std::string errorMsg;
    if      (checkStandardizeInputFile(opt.alignmentFilename, "alignment", errorMsg))
    {
    }
    else if (opt.chromNames.empty())
    {
        errorMsg = "Need at least one chromosome name";
    }
    else
    {
        for (const std::string& chrom : opt.chromNames)
        {
            if (chrom.empty())
            {
                errorMsg = "Empty chromosome name";
                break;
            }
        }
    }

    if (! errorMsg.empty())
    {
        usage(log_os, prog, visible, errorMsg.c_str());
    }
}
