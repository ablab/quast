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
/// \author Chris Saunders and Xiaoyu Chen
///

#pragma once

#include "manta/SVScoreInfo.hh"
#include "manta/SVScoreInfoDiploid.hh"
#include "manta/SVScoreInfoSomatic.hh"
#include "manta/SVScoreInfoTumor.hh"


/// all scoring info for one sv candidate, including data related to specific scoring models
///
///
struct SVModelScoreInfo
{
    void
    setSampleCount(
        const unsigned sampleCount,
        const unsigned diploidSampleCount)
    {
        base.setSampleCount(sampleCount);
        diploid.setSampleCount(diploidSampleCount);
    }

    void
    clear()
    {
        base.clear();
        diploid.clear();
        somatic.clear();
        tumor.clear();
    }

    SVScoreInfo base;
    SVScoreInfoDiploid diploid;
    SVScoreInfoSomatic somatic;
    SVScoreInfoTumor tumor;
};
