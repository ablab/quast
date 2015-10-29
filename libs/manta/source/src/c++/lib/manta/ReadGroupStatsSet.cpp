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

#include "ReadGroupStatsSet.hh"

#include "blt_util/log.hh"
#include "blt_util/io_util.hh"
#include "blt_util/parse_util.hh"
#include "blt_util/string_util.hh"

// workaround intel compiler boost warnings:
#include "boost/config.hpp"
#ifdef BOOST_INTEL_CXX_VERSION
#pragma warning push
#pragma warning(disable:1944)
#endif

#include "boost/archive/xml_oarchive.hpp"
#include "boost/archive/xml_iarchive.hpp"

#ifdef BOOST_INTEL_CXX_VERSION
#pragma warning pop
#endif

#include "boost/serialization/map.hpp"
#include "boost/serialization/string.hpp"
#include "boost/serialization/vector.hpp"

#include <fstream>
#include <iostream>
#include <sstream>



/// this struct exists for the sole purpose of xml output:
struct ReadGroupStatsExporter
{
    template<class Archive>
    void serialize(
        Archive& ar,
        const unsigned /*version*/)
    {
#ifdef READ_GROUPS
        ar& boost::serialization::make_nvp("bamFile", bamFile);
        ar& boost::serialization::make_nvp("readGroup", readGroup);
#else
        ar& boost::serialization::make_nvp("groupLabel", bamFile);
#endif
        ar& boost::serialization::make_nvp("groupStats", groupStats);
    }

    std::string bamFile;
    std::string readGroup;
    ReadGroupStats groupStats;
};

BOOST_CLASS_IMPLEMENTATION(ReadGroupStatsExporter, boost::serialization::object_serializable)



void
ReadGroupStatsSet::
merge(
    const ReadGroupStatsSet& rhs)
{
    const unsigned numGroups(rhs.size());
    for (unsigned i(0); i<numGroups; ++i)
    {
        const ReadGroupLabel& mkey(rhs.getKey(i));
        if (_group.test_key(mkey))
        {
            log_os << "ERROR: Can't merge stats set objects with repeated key: '" << mkey << "'\n";
            exit(EXIT_FAILURE);
        }

        setStats(mkey,rhs.getStats(i));
    }
}



void
ReadGroupStatsSet::
save(
    const char* filename) const
{
    assert(NULL != filename);
    std::ofstream ofs(filename);
    boost::archive::xml_oarchive oa(ofs);

    const unsigned numGroups(size());
    oa << boost::serialization::make_nvp("numGroups", numGroups);
    ReadGroupStatsExporter se;
    for (unsigned i(0); i<numGroups; ++i)
    {
        const KeyType& key(getKey(i));
        se.bamFile = key.bamLabel;
        se.readGroup = key.rgLabel;
        se.groupStats = getStats(i);

        std::ostringstream oss;
        oss << "groupStats_" << i;
        oa << boost::serialization::make_nvp(oss.str().c_str(), se);
    }
}



void
ReadGroupStatsSet::
load(
    const char* filename)
{
    clear();

    assert(NULL != filename);
    std::ifstream ifs(filename);
    boost::archive::xml_iarchive ia(ifs);

    int numGroups;
    ia >> boost::serialization::make_nvp("numGroups", numGroups);
    ReadGroupStatsExporter se;
    for (int i=0; i<numGroups; i++)
    {
        ia >> boost::serialization::make_nvp("bogus", se);

        setStats(KeyType(se.bamFile.c_str(), se.readGroup.c_str()), se.groupStats);
    }
}

