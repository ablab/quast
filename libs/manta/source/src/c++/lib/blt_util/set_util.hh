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

#include <set>


/// implements B -= A
template <typename T>
void
inplaceSetSubtract(
    const std::set<T>& A,
    std::set<T>& B)
{
    typedef typename std::set<T>::const_iterator scit;
    scit ait(A.begin()), ait_end(A.end());
    scit bit(B.begin()), bit_end(B.end());
    while ( (bit != bit_end) && (ait != ait_end) )
    {
        if (*ait < *bit)
        {
            ++ait;
        }
        else
        {
            if (*ait == *bit)
            {
                const scit blast(bit);
                ++bit;
                B.erase(blast);
            }
            else
            {
                ++bit;
            }
        }
    }
}
