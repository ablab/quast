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

///
/// \brief simple observer/notifier pattern
///
/// see unit test for demonstration, note this is not meant to be used across threads
///

#pragma once

#include <set>



template <typename T>
struct notifier;


template <typename T>
struct observer
{
    friend struct notifier<T>;

    typedef observer self_t;

    observer() {}

    observer(const self_t&)  {} // do not copy notifier set

    virtual ~observer()
    {
        for (typename nots_t::value_type val : _nots)
        {
            val->unregister_observer(this);
        }
    }

protected:
    void
    observe_notifier(const notifier<T>& n)
    {
        n.register_observer(this);
        _nots.insert(&n);
    }

private:
    self_t& operator=(const self_t&);

    virtual void
    recieve_notification(const notifier<T>&,
                         const T&) = 0;

    void
    register_notifier(const notifier<T>* n) const
    {
        _nots.insert(n);
    }

    void
    unregister_notifier(const notifier<T>* n)
    {
        const typename nots_t::iterator i(_nots.find(n));
        if (i != _nots.end()) _nots.erase(i);
    }

    ////////// data:
    typedef typename std::set<const notifier<T>*> nots_t;
    mutable nots_t _nots;
};


template <typename T>
struct notifier
{
    friend struct observer<T>;

    typedef notifier self_t;

    notifier() {}

    notifier(const self_t& rhs) :
        _obss(rhs._obss)
    {
        for (typename obss_t::value_type val : _obss)
        {
            val->register_notifier(this);
        }
    }

    self_t&
    operator=(const self_t& rhs)
    {
        if (this == &rhs) return *this;
        self_unregister();
        _obss=rhs._obss;
        for (typename obss_t::value_type val : _obss)
        {
            val->register_notifier(this);
        }
        return *this;
    }

    virtual ~notifier()
    {
        self_unregister();
    }

protected:
    void
    notify_observers(const T& msg) const
    {
        for (typename obss_t::value_type val : _obss)
        {
            val->recieve_notification(*this, msg);
        }
    }

private:

    void
    self_unregister() const
    {
        for (typename obss_t::value_type val : _obss)
        {
            val->unregister_notifier(this);
        }
    }

    void
    register_observer(observer<T>* n) const
    {
        _obss.insert(n);
    }

    void
    unregister_observer(observer<T>* n) const
    {
        const typename obss_t::iterator i(_obss.find(n));
        if (i != _obss.end()) _obss.erase(i);
    }

    ////////// data:
    typedef typename std::set<observer<T>*> obss_t;
    mutable obss_t _obss;
};

