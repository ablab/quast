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

#include "blt_util/time_util.hh"
#include "svgraph/EdgeInfo.hh"

#include "boost/utility.hpp"


#include <iosfwd>



/// simple edge time tracker and reporter
struct EdgeRuntimeTracker : private boost::noncopyable
{
    explicit
    EdgeRuntimeTracker(const std::string& outputFile);

    ~EdgeRuntimeTracker();

    void
    start()
    {
        edgeTime.clear();
        candTime.clear();
        assmTime.clear();
        scoreTime.clear();
        remoteTime.clear();

        edgeTime.resume();
        _cand = 0;
        _compCand = 0;
        _assmCand = 0;
        _assmCompCand = 0;
    }

    void
    stop(const EdgeInfo& edge);

    CpuTimes
    getLastEdgeTime() const
    {
        return edgeTime.getTimes();
    }

    void
    addCand(const bool isComplex)
    {
        if (isComplex) _compCand++;
        else           _cand++;
    }

    void
    addAssm(const bool isComplex)
    {
        if (isComplex) _assmCompCand++;
        else           _assmCand++;
    }

    TimeTracker candTime;
    TimeTracker assmTime;
    TimeTracker scoreTime;
    TimeTracker remoteTime;
private:
    std::ostream* _osPtr;
    TimeTracker edgeTime;

    unsigned _cand;
    unsigned _compCand;
    unsigned _assmCand;
    unsigned _assmCompCand;
};
