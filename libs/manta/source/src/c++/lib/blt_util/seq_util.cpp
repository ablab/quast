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

/// \author Chris Saunders
///
#include "blt_util/log.hh"
#include "blt_util/seq_util.hh"

#include <cassert>
#include <cstdlib>

#include <algorithm>
#include <fstream>
#include <iostream>



void
base_error(const char* func, const char a)
{
    log_os << "ERROR:: Invalid base in " << func << ".\n"
           << "\t\tinvalid base (char): '" << a << "'\n"
           << "\t\tinvalid base (int): " << static_cast<int>(a) << "\n";
    exit(EXIT_FAILURE);
}



void
id_to_base_error(const uint8_t i)
{
    log_os << "ERROR:: Invalid id in id_to_base. id: " << i << "\n";
    exit(EXIT_FAILURE);
}



bool
is_valid_seq(const char* seq)
{
    assert(NULL != seq);

    while (*seq !=  '\0')
    {
        if (! is_valid_base(*seq)) return false;
        seq++;
    }
    return true;
}



void
get_ref_seq(const char* ref_seq_file,
            std::string& ref_seq,
            const pos_range ref_segment)
{
    static const unsigned buff_size(50000);
    char buff[buff_size];

    std::ifstream ref_is(ref_seq_file);

    if ( ! ref_is )
    {
        log_os << "ERROR: Can't open reference sequence file: " << ref_seq_file << "\n";
        exit(EXIT_FAILURE);
    }

    unsigned line_no(1);
#ifdef SEQ_UTIL_VERBOSE
    log_os << "Reading from reference sequence file " << ref_seq_file << "\n";
#endif
    ref_is.getline(buff,buff_size);
#ifdef SEQ_UTIL_VERBOSE
    log_os << "First line: " << buff << "\n";
#endif

    if (buff[0] != '>')
    {
        log_os << "ERROR: Unexpected format in reference sequence file: " << ref_seq_file << " line_no: " << line_no << "\n"
               << "\tline: '" << buff << "'\n";
        exit(EXIT_FAILURE);
    }

    const pos_t begin_pos(ref_segment.is_begin_pos ? ref_segment.begin_pos : 0);
    pos_t ref_pos(0);

    ref_seq.clear();
    while (true)
    {
        ref_is.getline(buff,buff_size);
        if (! ref_is)
        {
            if     (ref_is.eof()) break;
            else if (ref_is.fail())
            {
                if (ref_is.bad())
                {
                    log_os << "ERROR: unexpected failure while attempting to read sequence file: " << ref_seq_file << "\n";
                    exit(EXIT_FAILURE);
                }
                ref_is.clear();
            }
        }
        line_no++;
        if (buff[0] == '>')
        {
            log_os << "ERROR: Unexpected format in reference sequence file: " << ref_seq_file << " line_no: " << line_no << "\n"
                   << "\tline: '" << buff << "'\n";
            exit(EXIT_FAILURE);
        }

        // correct for '\r' if present:
        pos_t rc(ref_is.gcount()-1);
        if (rc && (buff[rc-1]=='\r'))
        {
            buff[--rc]='\0';
        }
        if (ref_segment.is_end_pos)
        {
            if (ref_pos>=ref_segment.end_pos) break;
            if ((ref_pos+rc)>ref_segment.end_pos)
            {
                rc=(ref_segment.end_pos-ref_pos);
                buff[rc]='\0';
            }
        }
        if (ref_pos<begin_pos)
        {
            if ((ref_pos+rc) > begin_pos)
            {
                ref_seq += (buff+(begin_pos-ref_pos));
            }
        }
        else
        {
            ref_seq += buff;
        }
        ref_pos += rc;
    }

#ifdef SEQ_UTIL_VERBOSE
    log_os << "Finished reading from reference sequence file " << ref_seq_file << "\n";
    log_os << "Reference sequence size: " << ref_seq.size() << "\n";
#endif
}



void
standardize_ref_seq(const char* ref_seq_file,
                    const char* chr_name,
                    std::string& ref_seq,
                    const pos_t offset)
{
    const std::string::size_type ref_size(ref_seq.size());
    for (std::string::size_type i(0); i<ref_size; ++i)
    {
        const char old_ref(ref_seq[i]);
        char c(old_ref);
        if (islower(c)) c = toupper(c);
        if (! is_valid_base(c))
        {
            if (! is_iupac_base(c))
            {
                static const char def_chr_name[] = "first-sequence-in-file";
                const char* seq_name(NULL != chr_name ? chr_name : def_chr_name);

                log_os << "ERROR:: Unexpected character in reference sequence.\n";
                log_os << "\treference_sequence_file: '" << ref_seq_file << "'\n";
                log_os << "\tchromosome: '" << seq_name << "'\n";
                log_os << "\tcharacter: '" << old_ref << "'\n";
                log_os << "\tcharacter_decimal_index: " << static_cast<int>(old_ref) << "\n";
                log_os << "\tcharacter_position_in_chromosome: " << (i+1+offset) << "\n";
                exit(EXIT_FAILURE);
            }
            c=elandize_base(c);
        }
        if (c != old_ref) ref_seq[i] = c;
    }
}



#if 0
std::size_t
get_ref_seq_known_size(const std::string& ref_seq)
{
    std::string::const_iterator i(ref_seq.begin());
    const std::string::const_iterator i_end(ref_seq.end());
    std::size_t size(0);
    for (; i != i_end; ++i)
    {
        if (*i != 'N') size++;
    }
    return size;
}
#endif


std::size_t
get_ref_seq_known_size(const reference_contig_segment& ref,
                       const pos_range pr)
{
    pos_t b(0);
    pos_t end(ref.end());
    if (pr.is_begin_pos && (pr.begin_pos>0)) b=pr.begin_pos;
    if (pr.is_end_pos && (pr.end_pos>0)) end=std::min(end,pr.end_pos);
    std::size_t size(0);
    for (; b<end; ++b)
    {
        if (ref.get_base(b) != 'N') size++;
    }
    return size;
}



void
get_seq_repeat_unit(const std::string& seq,
                    std::string& repeat_unit,
                    unsigned& repeat_count)
{
    const std::string::size_type sg(seq.find('-'));
    const unsigned seq_size((sg!=std::string::npos) ? sg : seq.size());

    // check all divisors of seq_size until a repeat is found:
    for (unsigned i(1); i<seq_size; ++i)
    {
        /// TODO -- find a real way to get the divisor list, this
        /// isn't very important because indels are so small it
        /// almost doesn't matter.
        if ((seq_size%i) != 0) continue;

        bool is_repeat(true);
        for (unsigned j(i); j<seq_size; j += i)
        {
            for (unsigned k(0); k<i; ++k)
            {
                if (seq[j+k] != seq[k])
                {
                    is_repeat=false;
                    break;
                }
            }
            if (! is_repeat) break;
        }
        if (is_repeat)
        {
            repeat_unit = seq.substr(0,i);
            repeat_count = seq_size/i;
            return;
        }
    }

    repeat_unit = seq;
    repeat_count = 1;
}
