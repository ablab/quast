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

#include "DSLOptions.hh"

#include "blt_util/log.hh"
#include "manta/ProgramUtil.hh"

#include "boost/filesystem.hpp"
#include "boost/program_options.hpp"



static
void
usage(
    std::ostream& os,
    const manta::Program& prog,
    const boost::program_options::options_description& visible,
    const char* msg = nullptr)
{
    usage(os, prog, visible, "write binary sv locus graph to stdout", " > graph_dump", msg);
}



void
parseDSLOptions(const manta::Program& prog,
                int argc, char* argv[],
                DSLOptions& opt)
{
    namespace po = boost::program_options;
    po::options_description req("configuration");
    req.add_options()
    ("graph-file", po::value(&opt.graphFilename),
     "sv locus graph file")
    ("region", po::value(&opt.region),
     "list nodes in the specified region only. region in samtools format, eg. 'chr1:20-30' (optional)")
    ("locus-index", po::value(&opt.locusIndex),
     "dump only the specified locus")
    ("locus-file", po::value(&opt.locusFilename),
     "Write a binary sv locus file if locus-index is specified")
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
    }

    // fast check of config state:
    if (opt.graphFilename.empty())
    {
        usage(log_os,prog,visible,"Must specify sv locus graph file");
    }
    if (! boost::filesystem::exists(opt.graphFilename))
    {
        usage(log_os,prog,visible,"SV locus graph file does not exist");
    }
    if (vm.count("locus-index"))
    {
        if (! opt.region.empty()) usage(log_os,prog,visible,"locus-index and region cannot be used together");
        opt.isLocusIndex=true;
    }
    if (! opt.locusFilename.empty())
    {
        if (! opt.isLocusIndex) usage(log_os,prog,visible,"Must specify sv locus index with locus file");
    }
}

