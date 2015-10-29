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

#include <vector>


/// random sub-sampling array
///
/// This is an array with sized fixed to S at instantiation time. The
/// array accepts N (N>=S) objects as input. For any N>=S, the array
/// contains a given input object with probability S/N.
///
/// This behavior is implemented via standard reservoir sampling
///
template <typename T, typename RNG>
struct SampleVector
{
    /// \param initrng c++11 <random> rng generator, see std::shuffle for detailed doc of similar parameter
    SampleVector(
        const unsigned initSize,
        RNG& initRng)
        : _inputCount(0),
          _data(initSize,0),
          _rng(initRng)
    {}

    void
    push(const T& val);

    const std::vector<T>&
    data() const
    {
        return _data;
    }

private:
    unsigned _inputCount;
    std::vector<T> _data;
    RNG& _rng;
};

#include "SampleVectorImpl.hh"
