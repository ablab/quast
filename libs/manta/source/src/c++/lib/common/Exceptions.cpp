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

/**
 ** \brief Implementation of the common exception mechanism.
 **
 ** \author Come Raczy
 **/

#include <cstring>
#include <cerrno>
#include <boost/date_time.hpp>

#include "common/Exceptions.hh"

namespace illumina
{
namespace common
{

ExceptionData::ExceptionData(int errorNumber, const std::string& message) : boost::exception(),
    errorNumber_(errorNumber), message_(message)
{
}

std::string ExceptionData::getContext() const
{
    const std::string now = boost::posix_time::to_simple_string(boost::posix_time::second_clock::local_time());
    return now + ": " + std::string(strerror(errorNumber_)) + ": " + boost::diagnostic_information(*this);
}

IoException::IoException(int errorNumber, const std::string& message)
    : std::ios_base::failure(message)
    , ExceptionData(errorNumber, message)
{
}

ResourceException::ResourceException(int errorNumber, const std::string& message)
    : ExceptionData(errorNumber, message)
{
}


MemoryException::MemoryException(const std::string& message)
    : std::bad_alloc(),
      ExceptionData(ENOMEM, message)
{
}

UnsupportedVersionException::UnsupportedVersionException(const std::string& message)
    : std::logic_error(message)
    , ExceptionData(EINVAL, message)
{
}

FeatureNotAvailable::FeatureNotAvailable(const std::string& message)
    : std::logic_error(message)
    , ExceptionData(EINVAL, message)
{
}

InvalidParameterException::InvalidParameterException(const std::string& message)
    : std::logic_error(message)
    , ExceptionData(EINVAL, message)
{
}

InvalidOptionException::InvalidOptionException(const std::string& message)
    : std::logic_error(message)
    , ExceptionData(EINVAL, message)
{
}

PreConditionException::PreConditionException(const std::string& message)
    : std::logic_error(message)
    , ExceptionData(EINVAL, message)
{
}

PostConditionException::PostConditionException(const std::string& message)
    : std::logic_error(message)
    , ExceptionData(EINVAL, message)
{
}

OutOfBoundsException::OutOfBoundsException(const std::string& message)
    : std::out_of_range("OutOfBoundsException: " + message)
    , ExceptionData(EINVAL, message)
{
}

VcfException::VcfException(const std::string& message)
    : IoException(EPROTO, std::string("VCF failure: ") + message)
{
}


}
}
