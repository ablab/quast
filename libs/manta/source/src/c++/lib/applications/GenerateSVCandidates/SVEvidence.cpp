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

#include "SVEvidence.hh"

#include <iostream>



std::ostream&
operator<<(
    std::ostream& os,
    const SVFragmentEvidenceAlleleBreakendPerRead& svbpr)
{
    os << "isEval: " << svbpr.isSplitEvaluated
       << " isSplitSupport: " << svbpr.isSplitSupport
       << " isTier2SplitSupport: " << svbpr.isTier2SplitSupport
       << " splitEvidence: " << svbpr.splitEvidence
       << " splitLnLhood: " << svbpr.splitLnLhood
       ;

    return os;
}




std::ostream&
operator<<(
    std::ostream& os,
    const SVFragmentEvidenceAlleleBreakend& svbp)
{
    os << "isFrag: " << svbp.isFragmentSupport << " fragProb: " << svbp.fragLengthProb << "\n";
    os << "read1ev: " << svbp.read1 << "\n";
    os << "read2ev: " << svbp.read2 << "\n";
    return os;
}



std::ostream&
operator<<(
    std::ostream& os,
    const SVFragmentEvidenceAllele& sval)
{
    os << "----BP1: " << sval.bp1;
    os << "----BP2: " << sval.bp2;
    return os;
}



std::ostream&
operator<<(
    std::ostream& os,
    const SVFragmentEvidenceRead& svr)
{
    os << "readinfo isScanned: " << svr.isScanned
       << " isAnchored: " << svr.isAnchored(false)
       << " isTier2Anchored: " << svr.isAnchored(true)
       << " isShadow: " << svr.isShadow
       << " mapq: " << svr.mapq << " size: " << svr.size;
    return os;
}



std::ostream&
operator<<(
    std::ostream& os,
    const SVFragmentEvidence& sve)
{
    os << "FRAGMENT_START\n"
       << "read1: " << sve.read1 << "\n"
       << "read2: " << sve.read2 << "\n"
       << "+++++++++++ALT\n" << sve.alt
       << "+++++++++++REF\n" << sve.ref
       << "FRAGMENT_END\n";

    return os;
}
