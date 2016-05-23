// -*- mode: c++; indent-tabs-mode: nil; -*-
//
// Manta - Structural Variant and Indel Caller
// Copyright (c) 2013-2016 Illumina, Inc.
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

#include "MESOptions.hh"
#include "blt_util/log.hh"
#include "common/ProgramUtil.hh"

#include "boost/filesystem.hpp"
#include "boost/program_options.hpp"

#include <iostream>
#include <fstream>
#include <set>
#include <sstream>



static
void
usage(
    std::ostream& os,
    const illumina::Program& prog,
    const boost::program_options::options_description& visible,
    const char* msg = nullptr)
{
    usage(os, prog, visible, "merge sv locus edge stats", "", msg);
}



void
parseMESOptions(
    const illumina::Program& prog,
    int argc, char* argv[],
    MESOptions& opt)
{
    namespace po = boost::program_options;
    po::options_description req("configuration");

    req.add_options()
    ("stats-file", po::value(&opt.statsFilename),
     "input sv edge stats file (may be specified multiple times)")
    ("stats-file-list", po::value(&opt.statsFilenameList),
     "file listing all input sv edge stats files, one filename per line (specified only once)")
    ("output-file", po::value(&opt.outputFilename),
     "merged output sv edge stats file (required)")
    ("report-file", po::value(&opt.reportFilename),
     "provide a summary report based on the merged edge stats");

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
    catch (const boost::program_options::error& e)
    {
        // todo:: find out what is the more specific exception class thrown by program options
        log_os << "\nERROR: Exception thrown by option parser: " << e.what() << "\n";
        po_parse_fail=true;
    }

    if ((argc<=1) || (vm.count("help")) || po_parse_fail)
    {
        usage(log_os,prog,visible);
    }

    //read stat file names from a user-defined file
    if (! opt.statsFilenameList.empty())
    {
        std::ifstream parFile(opt.statsFilenameList.c_str(), std::ios_base::in | std::ios_base::binary);
        if (! parFile.good())
        {
            std::ostringstream osfl;
            osfl << "Stats file list does not exist: '" << opt.statsFilenameList << "'";
            usage(log_os, prog, visible, osfl.str().c_str());
        }

        std::string lineIn;
        while (getline(parFile, lineIn))
        {
            if (lineIn.size() == 0) continue;
            const unsigned sm1(lineIn.size()-1);
            if (lineIn[sm1] == '\r')
            {
                if (sm1 == 0) continue;
                lineIn.resize(sm1);
            }
            opt.statsFilename.push_back(lineIn);
        };
    }

    // fast check of config state:
    if (opt.statsFilename.empty())
    {
        usage(log_os,prog,visible, "Must specify at least 1 input sv edge stats file");
    }

    std::set<std::string> dupCheck;
    for (const std::string& statsFilename : opt.statsFilename)
    {
        if (! boost::filesystem::exists(statsFilename))
        {
            std::ostringstream oss;
            oss << "SV edge stats file does not exist: '" << statsFilename << "'";
            usage(log_os,prog,visible,oss.str().c_str());
        }

        if (dupCheck.find(statsFilename) != dupCheck.end())
        {
            std::ostringstream oss;
            oss << "Same SV edge stats file submitted multiple times: '" << statsFilename << "'";
            usage(log_os,prog,visible,oss.str().c_str());
        }
        dupCheck.insert(statsFilename);
    }

    if (opt.outputFilename.empty())
    {
        usage(log_os,prog,visible, "Must specify sv edges stats output file");
    }
}

