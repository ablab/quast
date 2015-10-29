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

/// \author Chris Saunders
///

#include "blt_util/blt_exception.hh"
#include "blt_util/log.hh"
#include "blt_util/sig_handler.hh"
#include "common/Exceptions.hh"
#include "manta/Program.hh"
#include "manta/version.hh"

#include <cstdlib>

#include <iostream>


static
void
dump_cl(int argc,
        char* argv[],
        std::ostream& os)
{
    os << "cmdline:";
    for (int i(0); i<argc; ++i)
    {
        os << ' ' << argv[i];
    }
    os << "\n";
}


namespace manta
{


const char*
Program::
version() const
{
    return getVersion();
}

const char*
Program::
compiler() const
{
    static const std::string _str(
        cxxCompilerName() +
        std::string("-") +
        compilerVersion());
    return _str.c_str();

}

const char*
Program::
buildTime() const
{
    return getBuildTime();
}


void
Program::
post_catch(
    int argc,
    char* argv[],
    std::ostream& os) const
{
    os << "...caught in program.run()\n";
    dump_cl(argc,argv,log_os);
    os << "version:\t" << version() << "\n";
    os << "buildTime:\t" << buildTime() << "\n";
    os << "compiler:\t" << compiler() << "\n";
    os << std::flush;
    exit(EXIT_FAILURE);
}


int
Program::
run(int argc, char* argv[]) const
{
    try
    {
        std::ios_base::sync_with_stdio(false);

        std::string cmdline;
        for (int i(0); i<argc; ++i)
        {
            if (i) cmdline += ' ';
            cmdline += argv[i];
        }

        initialize_blt_signals(name(),cmdline.c_str());

        runInternal(argc,argv);
    }
    catch (const blt_exception& e)
    {
        log_os << "FATAL_ERROR: " << name() << " EXCEPTION: " << e.what() << "\n";
        post_catch(argc,argv,log_os);
    }
    catch (const illumina::common::ExceptionData& e)
    {
        log_os << "FATAL_ERROR: " << name() << " EXCEPTION: "
               << e.getContext() << ": " << e.getMessage() << "\n";
        post_catch(argc,argv,log_os);
    }
    catch (const boost::exception& e)
    {
        log_os << "FATAL_ERROR: " << name() << " EXCEPTION: "
               << boost::diagnostic_information(e) << "\n";
        post_catch(argc,argv,log_os);
    }
    catch (const std::exception& e)
    {
        log_os << "FATAL_ERROR: EXCEPTION: " << e.what() << "\n";
        post_catch(argc,argv,log_os);
    }
    catch (...)
    {
        log_os << "FATAL_ERROR: UNKNOWN EXCEPTION\n";
        post_catch(argc,argv,log_os);
    }
    return EXIT_SUCCESS;
}

}
