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

#include "boost/optional.hpp"

#include <map>
#include <vector>


/// \brief Provides something like a set, but with sequential id numbers
/// assigned to each key starting from 0
///
template <typename K>
struct id_set
{
    /// \brief Add object to set if not present, and return id
    /// number in either case
    unsigned insert_key(const K& key)
    {
        const typename k2id_t::const_iterator i(_k2id.find(key));
        if (i==_k2id.end())
        {
            const unsigned id(_id2k.size());
            _k2id[key]=id;
            _id2k.push_back(key);
            return id;
        }
        else
        {
            return i->second;
        }
    }

    /// \brief Test if key exists in set
    bool test_key(const K& key) const
    {
        return (_k2id.find(key) != _k2id.end());
    }

    /// \brief Get id of inserted key
    boost::optional<unsigned>
    get_optional_id(const K& key) const
    {
        const typename k2id_t::const_iterator i(_k2id.find(key));
        if (i==_k2id.end())
        {
            return boost::optional<unsigned>();
        }
        return boost::optional<unsigned>(i->second);
    }

    /// \brief Get id of inserted key
    unsigned get_id(const K& key) const
    {
        const typename k2id_t::const_iterator i(_k2id.find(key));
        if (i==_k2id.end())
        {
            throw blt_exception("ERROR: id_set.get_id(): invalid key\n");
        }
        return i->second;
    }

    /// \brief Get pre-existing key
    const K& get_key(const unsigned id) const
    {
        if (id>=_id2k.size())
        {
            throw blt_exception("ERROR: id_set.get_key(): invalid id\n");
        }
        return _id2k[id];
    }

    bool
    empty() const
    {
        return _id2k.empty();
    }

    unsigned
    size() const
    {
        return _id2k.size();
    }

    void
    clear()
    {
        _k2id.clear();
        _id2k.clear();
    }

private:
    typedef std::map<K,unsigned> k2id_t;

    k2id_t _k2id;
    std::vector<K> _id2k;
};



/// \brief Provides something like a map, but with sequential id numbers
/// assigned to each key starting from 0
///
/// The id numbers can be useful for faster lookup of the value, while
/// retaining the option of doing key lookup when required
///
template <typename K, typename V>
struct id_map
{
    /// \brief Update map with (key,value) and return id
    ///
    unsigned insert(const K& key, const V& value)
    {
        const typename k2id_t::const_iterator i(_k2id.find(key));
        if (i==_k2id.end())
        {
            const unsigned id(_id2kv.size());
            _k2id[key]=id;
            _id2kv.push_back(std::make_pair(key,value));
            return id;
        }
        else
        {
            _id2kv[i->second] = std::make_pair(key,value);
            return i->second;
        }
    }

    /// \brief Test if key exists in map
    bool test_key(const K& key) const
    {
        return (_k2id.find(key) != _k2id.end());
    }

    /// \brief Get id of inserted key
    boost::optional<unsigned>
    get_optional_id(const K& key) const
    {
        const typename k2id_t::const_iterator i(_k2id.find(key));
        if (i==_k2id.end())
        {
            return boost::optional<unsigned>();
        }
        return boost::optional<unsigned>(i->second);
    }

    /// \brief Get id of inserted key
    unsigned get_id(const K& key) const
    {
        const typename k2id_t::const_iterator i(_k2id.find(key));
        if (i==_k2id.end())
        {
            throw blt_exception("ERROR: id_map.get_id(): invalid key\n");
        }
        return i->second;
    }

    /// \brief Get pre-existing key
    const K& get_key(const unsigned id) const
    {
        if (id>=_id2kv.size())
        {
            throw blt_exception("ERROR: idmap.get_key(): invalid id\n");
        }
        return _id2kv[id].first;
    }

    /// \brief Get pre-existing key
    const V& get_value(const unsigned id) const
    {
        if (id>=_id2kv.size())
        {
            throw blt_exception("ERROR: idmap.get_value(): invalid id\n");
        }
        return _id2kv[id].second;
    }

    bool
    empty() const
    {
        return _id2kv.empty();
    }

    unsigned
    size() const
    {
        return _id2kv.size();
    }

    void
    clear()
    {
        _k2id.clear();
        _id2kv.clear();
    }

private:
    typedef std::map<K,unsigned> k2id_t;

    k2id_t _k2id;
    std::vector<std::pair<K,V>> _id2kv;
};
