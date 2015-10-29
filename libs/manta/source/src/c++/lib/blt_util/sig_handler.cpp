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

#include "blt_util/log.hh"
#include "blt_util/sig_handler.hh"

#include <cstdlib>
#include <signal.h>

#include <iostream>
#include <string>


static std::string _progname;
static std::string _cmdline;



static
void
blt_sig_handler (int sig)
{
    switch (sig)
    {
    case SIGTERM:
        log_os << "ERROR: " << _progname << " received termination signal. cmdline: " << _cmdline << std::endl;
        exit(EXIT_FAILURE);
#ifndef _WIN32
    case SIGINT:
        log_os << "ERROR: " << _progname << " received interrupt signal. cmdline: " << _cmdline << std::endl;
        exit(EXIT_FAILURE);
#endif
    default:
        log_os << "INFO: " << _progname << " received signal no: " << sig << std::endl;
        break;
    }
}



void
initialize_blt_signals(const char* progname,
                       const char* cmdline)
{
    _progname=progname;
    _cmdline=cmdline;

    signal(SIGTERM, blt_sig_handler);
#ifndef _WIN32
    signal(SIGINT, blt_sig_handler);
#endif
}
