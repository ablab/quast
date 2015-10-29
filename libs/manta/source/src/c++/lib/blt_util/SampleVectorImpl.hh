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

#include <random>



template <typename T, typename RNG>
void
SampleVector<T,RNG>::
push(const T& val)
{
    if (_inputCount < _data.size())
    {
        // initial fill of the reservoir array is deterministic:
        _data[_inputCount] = val;
    }
    else
    {
        // replace elements with gradually decreasing probability:
        std::uniform_int_distribution<unsigned> rdist(0,_inputCount);
        const unsigned rval(rdist(_rng));

        if (rval < _data.size())
        {
            _data[rval] = val;
        }
    }
    _inputCount++;
}
