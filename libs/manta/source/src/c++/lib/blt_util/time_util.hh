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

#include "boost/serialization/nvp.hpp"
#include "boost/timer/timer.hpp"
#include "boost/utility.hpp"

#include <iosfwd>


namespace BOOST_TIMER_HELPER
{
inline
double
getTimerSeconds(
    const boost::timer::nanosecond_type& ns)
{
    using namespace boost::chrono;
    return static_cast<double>(duration_cast<microseconds>(nanoseconds(ns)).count())/1000000.;
}
}

/// this is a replacement for boost::timer cpu_times
/// with serialization/merge, etc...
struct CpuTimes
{
    CpuTimes() {}

    explicit
    CpuTimes(
        const boost::timer::cpu_times& t)
        : wall(BOOST_TIMER_HELPER::getTimerSeconds(t.wall)),
          user(BOOST_TIMER_HELPER::getTimerSeconds(t.user)),
          system(BOOST_TIMER_HELPER::getTimerSeconds(t.system))
    {}

    void
    merge(
        const CpuTimes& rhs)
    {
        wall += rhs.wall;
        user += rhs.user;
        system += rhs.system;
    }

    void
    difference(
        const CpuTimes& rhs)
    {
        wall -= rhs.wall;
        user -= rhs.user;
        system -= rhs.system;
    }

    template<class Archive>
    void serialize(Archive& ar, const unsigned /*version*/)
    {
        ar& BOOST_SERIALIZATION_NVP(wall)
        & BOOST_SERIALIZATION_NVP(user)
        & BOOST_SERIALIZATION_NVP(system)
        ;
    }

    void
    reportSec(
        std::ostream& os) const
    {
        static const char tlabel('s');
        static const double factor(1.);
        report(factor,&tlabel,os);
    }

    void
    reportHr(
        std::ostream& os) const
    {
        static const char tlabel('h');
        static const double factor(1./3600.);
        report(factor,&tlabel,os);
    }

    void
    report(
        const double factor,
        const char* tlabel,
        std::ostream& os) const;

    double wall = 0.;
    double user = 0.;
    double system = 0.;
};

BOOST_CLASS_IMPLEMENTATION(CpuTimes, boost::serialization::object_serializable)



/// simple time track utility
struct TimeTracker
{
    TimeTracker()
    {
        _timer.stop();
    }

    void
    clear()
    {
        _isReset = true;
    }

    /// starts clock without reset to accumulate total time
    void
    resume()
    {
        //assert((! _isStart) && "clock is running");
        if (_isReset)
        {
            _timer.start();
            _isReset = false;
        }
        else _timer.resume();
    }

    /// stop clock
    void
    stop()
    {
        _timer.stop();
    }

    CpuTimes
    getTimes() const
    {
        static const CpuTimes zero;
        if (_isReset) return zero;
        return CpuTimes(_timer.elapsed());
    }

    /// DEPRECATED get user cpu time in seconds
    ///
    /// timer must be stopped
    double
    getUserSeconds() const
    {
        return getTimes().user;
    }

    /// DEPRECATED get user cpu time in seconds
    ///
    /// timer must be stopped
    double
    getWallSeconds() const
    {
        return getTimes().wall;
    }

private:
    bool _isReset = true;
    boost::timer::cpu_timer _timer;
};


/// utility for timetracker for scope based start-stop scenarios:
struct TimeScoper : private boost::noncopyable
{
    explicit
    TimeScoper(TimeTracker& t) : _t(t)
    {
        _t.resume();
    }

    ~TimeScoper()
    {
        _t.stop();
    }
private:
    TimeTracker& _t;
};
