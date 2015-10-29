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

#include "GenomeInterval.hh"
#include "blt_util/RegionTracker.hh"

#include <iosfwd>


struct GenomeIntervalTracker
{
    void
    clear()
    {
        for (auto& r : _regions)
        {
            r.clear();
        }
    }

    void
    addInterval(
        const GenomeInterval& gi)
    {
        assert(gi.tid >= 0);
        if (static_cast<unsigned>(gi.tid) >= _regions.size()) _regions.resize(gi.tid+1);
        _regions[gi.tid].addRegion(gi.range);
    }

    bool
    isSubsetOfRegion(
        const GenomeInterval& gi) const
    {
        assert(gi.tid >= 0);
        if (static_cast<unsigned>(gi.tid) >= _regions.size()) return false;
        return _regions[gi.tid].isSubsetOfRegion(gi.range);
    }

private:
    std::vector<RegionTracker> _regions;
};
