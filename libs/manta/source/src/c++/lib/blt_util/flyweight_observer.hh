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
/// \brief extremely minimal observer pattern
///
/// this class is designed to assist setting up an observer pattern which has zero memory overhead, but
/// has extremely limited abilities, see blt_util/observer.hh for a more general observer pattern support
///

#pragma once

template <typename T>
struct flyweight_notifier;


template <typename T>
struct flyweight_observer
{
    friend struct flyweight_notifier<T>;

    flyweight_observer& operator=(const flyweight_observer&) = default;

    virtual ~flyweight_observer() {}

private:
    virtual void
    recieve_flyweight_notification(const T&) = 0;
};


template <typename T>
struct flyweight_notifier
{
    typedef flyweight_observer<T> flyweight_observer_t;

protected:
    void
    notify_flyweight_observer(
        flyweight_observer_t* val,
        const T& msg) const
    {
        val->recieve_flyweight_notification(msg);
    }
};

