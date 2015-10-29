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

#include "blt_util/blt_exception.hh"

#include "boost/dynamic_bitset.hpp"

#include <algorithm>
#include <sstream>
#include <vector>


/// two predefined options for the ValClear type parameter to RangeMap:
template <typename T>
struct ZeroT
{
    void
    operator()(T& val) const
    {
        val = 0;
    }
};

template <typename T>
struct ClearT
{
    void
    operator()(T& val) const
    {
        val.clear();
    }
};


/// this object provides map-like storage for a set of positions which are
/// assumed to cluster in a small range
///
/// in practice this is very similar to an unbounded circular buffer, but heavily customized
/// for our specific application
///
template <typename KeyType, typename ValType, typename ValClear = ZeroT<ValType>>
struct RangeMap
{
    ///\TODO automate this w/ static assert/concepts:
    //Keytype must implement operator < +/-

    RangeMap() :
        _isEmpty(true),
        _minKeyIndex(0),
        _data(_minChunk),
        _occup(_minChunk)
    {}

    void
    clear()
    {
        _isEmpty=true;
        _minKeyIndex=0;
        _occup.reset();
    }

    bool
    empty() const
    {
        return _isEmpty;
    }

    bool
    isKeyPresent(
        const KeyType& k) const
    {
        return (! ((_isEmpty) || (k < _minKey) || (k > _maxKey) || (! _occup.test(getKeyIndex(k)))));
    }

    /// get a mutable reference for the value associated with key k
    ///
    /// if k does not exist then it is initialized according to ValClear type parameter
    ValType&
    getRef(
        const KeyType& k)
    {
        if (_isEmpty)
        {
            _minKey=k;
        }
        else if (k < _minKey)
        {
            expand(_maxKey-k);
            const unsigned dataSize(_data.size());
            _minKeyIndex = ((_minKeyIndex + dataSize)-(_minKey-k) ) % dataSize;
            _minKey = k;
        }

        if ((_isEmpty) || (k > _maxKey))
        {
            expand(k-_minKey);
            _maxKey = k;
            _isEmpty=false;
        }


        const unsigned kindex(getKeyIndex(k));
        if (! _occup.test(kindex))
        {
            _clearFunc(_data[kindex]);
            _occup.set(kindex);
        }

        return _data[kindex];
    }

    /// get a const reference to key's associated value
    ///
    /// exception thrown in key is absent
    const ValType&
    getConstRef(
        const KeyType& k) const
    {
        enforceKeyPresent(k);
        return _data[getKeyIndex(k)];
    }

    /// get a const reference to key's associated value, or provide reference to specified default
    const ValType&
    getConstRefDefault(
        const KeyType& k,
        const ValType& defaultVal) const
    {
        if (! isKeyPresent(k)) return defaultVal;
        return _data[getKeyIndex(k)];
    }

    void
    erase(
        const KeyType& k)
    {
        enforceKeyPresent(k);
        const unsigned kindex(getKeyIndex(k));
        _occup.reset(kindex);

        if (k != _minKey) return;

        resetMinKey();
    }

    /// erase all contents with keys sorting less than or equal to k
    void
    eraseTo(
        const KeyType& k)
    {
        // special cases:
        if (_isEmpty) return;
        if (_minKey > k) return;
        if (_maxKey <= k)
        {
            clear();
            return;
        }

        if (_minKey == k)
        {
            /// accelerate/simplify _occup setting for common use case of erasing
            /// a single position off the end of contiguous key block
            _occup.reset(_minKeyIndex);
        }
        else
        {
            boost::dynamic_bitset<>& mask(_occup_mask_helper);
            mask.resize(1+k-_minKey);
            mask.reset();
            mask.resize(_occup.size(),true);
            rotateLeft(mask,_minKeyIndex);
            _occup &= mask;
        }

        resetMinKey();
    }

#ifdef DEBUG_RMAP
    /// debug dumper:
    void
    dump(const char* msg, std::ostream& os) const
    {
        os << "rangeMap dump: " << msg << "\n"
           << "\tempty: " << _isEmpty << "\n"
           << "\tminKeyIndex: " << _minKeyIndex << "\n"
           << "\tminKey: " << _minKey << "\n"
           << "\tmaxKey: " << _maxKey << "\n"
           << "\tdatasize: " << _data.size() << "\n";
    }
#endif

private:
    /// update minKey based on new occupy values:
    void
    resetMinKey()
    {
        // we have to shift minKey up to the next valid value:
        const unsigned keySize(_maxKey-_minKey);
        for (unsigned offset(1); offset<=keySize; ++offset)
        {
            const unsigned testIndex(getKeyIndexOffset(offset));
            if (! _occup.test(testIndex)) continue;
            _minKeyIndex = testIndex;
            _minKey += offset;
            return;
        }

        _isEmpty=true;
    }

    /// assumes offset has already been validated!
    unsigned
    getKeyIndexOffset(
        const unsigned offset) const
    {
        // the following should be faster than a modulus but
        // still handle all cases:
        const unsigned i(_minKeyIndex + offset);
        const unsigned d(_data.size());
        if (i < d) return i;
        return (i-d);
    }

    /// assumes key has already been validated!
    unsigned
    getKeyIndex(
        const KeyType& k) const
    {
        return getKeyIndexOffset(k-_minKey);
    }

    /// rotate data so that minKeyIndex is 0
    void
    normRotate()
    {
        if (_minKeyIndex==0) return;
        std::rotate(_data.begin(),_data.begin()+_minKeyIndex,_data.end());
        rotateRight(_occup,_minKeyIndex);
        _minKeyIndex=0;
    }

    // expand to larger of 2x current size or minSize+minChunk:
    void
    expand(
        const unsigned minSize)
    {
        if (minSize <= _data.size()) return;
        const unsigned newSize(std::max(static_cast<unsigned>(2*_data.size()),minSize+_minChunk));
        normRotate();
        _data.resize(newSize);
        _occup.resize(newSize);
    }

    void
    enforceKeyPresent(
        const KeyType& k) const
    {
        if (isKeyPresent(k)) return;
        std::ostringstream oss;
        oss << "Attempting to retrieve an invalid key '" << k << "'\n";
        throw blt_exception(oss.str().c_str());
    }

    void
    rotateLeft(
        boost::dynamic_bitset<>& a,
        unsigned n)
    {
        if (n==0) return;
        const unsigned s(a.size());
        assert(n<s);
        boost::dynamic_bitset<>& b(_occup_rotate_helper);
        b = a;
        b >>= (s-n);
        a <<= n;
        a |= b;
    }

    void
    rotateRight(
        boost::dynamic_bitset<>& a,
        unsigned n)
    {
        if (n==0) return;
        const unsigned s(a.size());
        assert(n<s);
        rotateLeft(a, (s-n));
    }

    static const unsigned _minChunk = 1024;

    bool _isEmpty;
    unsigned _minKeyIndex;
    KeyType _minKey;
    KeyType _maxKey;
    std::vector<ValType> _data;
    boost::dynamic_bitset<> _occup;

    ///< used to cache the copy needed for masking:
    boost::dynamic_bitset<> _occup_mask_helper;

    ///< used to cache the copy needed for rotate:
    boost::dynamic_bitset<> _occup_rotate_helper;

    ValClear _clearFunc;
};
