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

#pragma once

#include "blt_util/blt_types.hh"
#include "blt_util/pos_range.hh"
#include "blt_util/reference_contig_segment.hh"

#include <cstring>

#include <iterator>
#include <string>


namespace BASE_ID
{
enum index_t
{
    A,
    C,
    G,
    T,
    ANY,
    SIZE
};
}

enum { N_BASE=4 };

void
base_error(const char* func,
           const char a);

inline
uint8_t
base_to_id(const char a)
{
    using namespace BASE_ID;
    switch (a)
    {
    case 'A':
        return A;
    case 'C':
        return C;
    case 'G':
        return G;
    case 'T':
        return T;
    case 'N':
        return ANY;
    default:
        base_error("base_to_id",a);
        return 4;
    }
}

void
id_to_base_error(const uint8_t i);

inline
char
id_to_base(const uint8_t i)
{
    static const char base[] = "ACGTN";

    if (i>N_BASE) id_to_base_error(i);
    return base[i];
}



/// valid in the ELAND sense [ACGTN]
inline
bool
is_valid_base(char a)
{
    switch (a)
    {
    case 'A':
    case 'C':
    case 'G':
    case 'T':
    case 'N':
        return true;
    default :
        return false;
    }
}

inline
bool
is_iupac_base(char a)
{
    switch (a)
    {
    case 'A':
    case 'C':
    case 'G':
    case 'U':
    case 'T':
    case 'R':
    case 'Y':
    case 'S':
    case 'W':
    case 'K':
    case 'M':
    case 'B':
    case 'D':
    case 'H':
    case 'V':
    case '.':
    case '-':
    case 'N':
        return true;
    default :
        return false;
    }
}

/// valid in the ELAND sense [ACGTN]
bool
is_valid_seq(const char* seq);

inline
char
elandize_base(char a)
{
    switch (a)
    {
    case 'A':
        return 'A';
    case 'C':
        return 'C';
    case 'G':
        return 'G';
    case 'U':
    case 'T':
        return 'T';
    case 'R':
    case 'Y':
    case 'S':
    case 'W':
    case 'K':
    case 'M':
    case 'B':
    case 'D':
    case 'H':
    case 'V':
    case '.':
    case '-':
    case 'N':
        return 'N';
    default:
        base_error("elandize_base",a);
        return 'N';
    }
}

inline
char
comp_base(char a)
{
    switch (a)
    {
    case 'A':
        return 'T';
    case 'C':
        return 'G';
    case 'G':
        return 'C';
    case 'T':
        return 'A';
    case 'N':
        return 'N';
    default:
        base_error("comp_base",a);
        return 'N';
    }
}

inline
char
get_seq_base(const char* seq,
             const pos_t size,
             const pos_t pos)
{
    if ((pos<0) || (pos>=size))
    {
        return 'N';
    }
    else
    {
        return seq[pos];
    }
}

inline
char
get_seq_base(const std::string& seq,
             const pos_t pos)
{
    return get_seq_base(seq.c_str(),seq.size(),pos);
}

// generalized in-place revcomp -- requires bidirectional iterators
//
template <typename Iter>
void
reverseComp(Iter b,Iter e)
{
    char t;
    for (; b!=e; ++b)
    {
        if ((--e)==b)
        {
            *b=comp_base(*b);
            break;
        }
        t=comp_base(*b);
        *b=comp_base(*e);
        *e=t;
    }
}

// easy string version:
inline
void
reverseCompStr(std::string& seq)
{
    reverseComp(seq.begin(),seq.end());
}


template <typename T> void fixCstring(T) {}
inline void fixCstring(char* b)
{
    *b='\0';
}


// generalized copy revcomp -- requires bidirectional iterators
//
template <typename ConstIter,typename Iter>
void
reverseCompCopy(ConstIter cb,ConstIter ce,Iter b)
{
    while (cb!=ce)
    {
        *b++ = comp_base(*--ce);
    }
    fixCstring(b);
}

// easy char*->string version:
inline
std::string
reverseCompCopyCStr(const char* str)
{
    std::string result;
    reverseCompCopy(str,str+strlen(str),
                    std::back_insert_iterator<std::string>(result));
    return result;
}

// easy string->string version:
inline
std::string
reverseCompCopyStr(const std::string& seq)
{
    std::string result;
    reverseCompCopy(seq.begin(),seq.end(),
                    std::back_insert_iterator<std::string>(result));
    return result;
}

// Get single sequence from a fasta file, there must be only one
// sequence in the file.
//
void
get_ref_seq(const char* ref_seq_file,
            std::string& ref_seq,
            const pos_range ref_segment = pos_range());

/// Standardize reference sequence to [ACGTN]. Fail when non-IUPAC
/// character is found.
void
standardize_ref_seq(const char* ref_seq_file,
                    const char* chr_name,
                    std::string& ref_seq,
                    const pos_t offset);

inline
void
standardize_ref_seq(const char* ref_seq_file,
                    std::string& ref_seq)
{
    standardize_ref_seq(ref_seq_file,NULL,ref_seq,0);
}

//std::size_t
//get_ref_seq_known_size(const std::string& ref_seq);

std::size_t
get_ref_seq_known_size(const reference_contig_segment& ref_seq,
                       const pos_range pr);

/// Looks for the smallest possible perfect repeat in seq
///
void
get_seq_repeat_unit(const std::string& seq,
                    std::string& repeat_unit,
                    unsigned& repeat_count);

