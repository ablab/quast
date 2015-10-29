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
 ** \brief Declaration of the common exception mechanism.
 **
 ** All exceptions must carry the same data (independently of the
 ** exception type) to homogenixe the reporting and processing of
 ** errors.
 **
 ** \author Come Raczy
 **/

#pragma once


#include "blt_util/thirdparty_push.h"

#include "boost/cerrno.hpp"
#include "boost/lexical_cast.hpp"
#include "boost/exception/all.hpp"
#include "boost/throw_exception.hpp"

#include "blt_util/thirdparty_pop.h"

#include <ios>
#include <stdexcept>
#include <string>

namespace illumina
{
namespace common
{

/// this type can be used to add more info onto an in-flight exception:
///
typedef boost::error_info<struct extra_exception_message,std::string> ExceptionMsg;


/**
 ** \brief Virtual base class to all the exception classes
 **
 ** Use BOOST_THROW_EXCEPTION to get the contect info (file, function, line)
 ** at the throw site.
 **/
class ExceptionData : public boost::exception
{
public:
    ExceptionData(int errorNumber=0, const std::string& message="");
    ExceptionData(const ExceptionData&) = default;
    ExceptionData& operator=(const ExceptionData&) = delete;

    int getErrorNumber() const
    {
        return errorNumber_;
    }
    const std::string& getMessage() const
    {
        return message_;
    }
    std::string getContext() const;
private:
    const int errorNumber_;
    const std::string message_;
};

class IlluminaException : public std::exception, public ExceptionData
{
public:
    IlluminaException(int errorNumber, const std::string& message) : ExceptionData(errorNumber, message) {}
    IlluminaException(const IlluminaException& e) : std::exception(e), ExceptionData(e) {}
private:
    IlluminaException& operator=(const IlluminaException&);
};

/**
 * \brief Exception thrown when there are problems with the IO operations
 */
class IoException: public std::ios_base::failure, public ExceptionData
{
public:
    IoException(int errorNumber, const std::string& message);
};

/**
 * \brief Exception thrown when there is insufficient resources to perform an operation. For example
 *        if the adjusting the soft ulimit fails due to a set hard limit
 */
class ResourceException: public std::exception, public ExceptionData
{
public:
    ResourceException(int errorNumber, const std::string& message);
};

/**
 * \brief Same as bad_alloc but with a message
 */
class MemoryException: public std::bad_alloc, public ExceptionData
{
public:
    MemoryException(const std::string& message);
};

/**
 ** \brief Exception thrown when the client supplied and unsupported version number.
 **
 ** Particularly relevant to data format and software versions
 ** (Pipeline, IPAR, Phoenix, etc.). It should not be used in
 ** situations where the client didn't have the possibility to check
 ** the version (for instance when reading the version of a data
 ** format from the header of a file).
 **
 **/
class UnsupportedVersionException: public std::logic_error, public ExceptionData
{
public:
    UnsupportedVersionException(const std::string& message);
};

/**
 ** \brief Thrown when the requested functionality is not available.
 **
 **/
class FeatureNotAvailable: public std::logic_error, public ExceptionData
{
public:
    FeatureNotAvailable(const std::string& message);
};

/**
 ** \brief Exception thrown when the client supplied an invalid parameter.
 **
 **/
class InvalidParameterException: public std::logic_error, public ExceptionData
{
public:
    InvalidParameterException(const std::string& message);
};

/**
 ** \brief Exception thrown when an invalid command line option was detected.
 **
 **/
class InvalidOptionException: public std::logic_error, public ExceptionData
{
public:
    InvalidOptionException(const std::string& message);
};

/**
 ** \brief Exception thrown when a method invocation violates the pre-conditions.
 **
 **/
class PreConditionException: public std::logic_error, public ExceptionData
{
public:
    PreConditionException(const std::string& message);
};

/**
 ** \brief Exception thrown when a method invocation violates the post-conditions.
 **
 **/
class PostConditionException: public std::logic_error, public ExceptionData
{
public:
    PostConditionException(const std::string& message);
};

/**
 ** \brief Exception thrown when a method invocation generates an out-of-limits situation.
 **
 **/
class OutOfBoundsException: public std::out_of_range, public ExceptionData
{
public:
    OutOfBoundsException(const std::string& message);
};

/// General purpose exception for all other cases:
///
struct LogicException: public std::logic_error, public ExceptionData
{
    LogicException(const std::string& message) :
        std::logic_error(message),
        ExceptionData(EPERM, message)
    {}
};


/**
 ** \brief Exception thrown when a VCF violation is encountered.
 **
 **/
class VcfException: public IoException
{
public:
    VcfException(const std::string& message);
};


}
}
