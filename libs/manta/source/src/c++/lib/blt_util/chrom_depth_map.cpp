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
///
/// \author Chris Saunders
///


#include "blt_util/blt_exception.hh"
#include "blt_util/chrom_depth_map.hh"
#include "blt_util/log.hh"
#include "blt_util/parse_util.hh"

#include <cstdlib>
#include <cstring>

#include <fstream>
#include <iostream>



// parse the chrom depth file
void
parse_chrom_depth(const std::string& chrom_depth_file,
                  cdmap_t& chrom_depth)
{

    if (chrom_depth_file.empty()) return;

    std::ifstream depth_is(chrom_depth_file.c_str());
    if (! depth_is)
    {
        log_os << "ERROR: Failed to open chrom depth file '" << chrom_depth_file << "'\n";
        exit(EXIT_FAILURE);
    }

    static const unsigned buff_size(1024);
    char buff[buff_size];

    unsigned line_no(0);

    while (true)
    {
        depth_is.getline(buff,buff_size);
        if (! depth_is)
        {
            if     (depth_is.eof()) break;
            else
            {
                log_os << "ERROR: unexpected failure while attempting to read chrom depth file line " << (line_no+1) << "\n";
                exit(EXIT_FAILURE);
            }
        }
        else
        {
            ++line_no;
        }

        char* word2(strchr(buff,'\t'));
        if (NULL == word2)
        {
            log_os << "ERROR: unexpected format in read chrom depth file line " << (line_no) << "\n";
            exit(EXIT_FAILURE);
        }
        *(word2++) = '\0';
        try
        {
            const char* s(word2);
            chrom_depth[buff] = illumina::blt_util::parse_double(s);
        }
        catch (const blt_exception&)
        {
            log_os << "ERROR: unexpected format in read chrom depth file line " << (line_no) << "\n";
            throw;
        }
        if (chrom_depth[buff] < 0)
        {
            log_os << "ERROR: Chromosome depth estimate is negative. Chromosome: '" << buff
                   << "' Depth: " << chrom_depth[buff] << "\n";
            exit(EXIT_FAILURE);
        }
    }
}

