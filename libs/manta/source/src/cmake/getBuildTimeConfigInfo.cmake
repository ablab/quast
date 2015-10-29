#
# Manta - Structural Variant and Indel Caller
# Copyright (c) 2013-2015 Illumina, Inc.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#
#

# \author Chris Saunders

#
# get various build-time configuration values -- this is information not available
# at cmake configuration time
#
# requires SRC_DIR, REDIST_DIR and CONFIG_FILE
#

# generate git describe tag

set (GETGIT_CMAKE "${REDIST_DIR}/cmake-modules-c99fd3/GetGitRevisionDescription.cmake")
include ("${GETGIT_CMAKE}")
git_describe(GIT_VERSION "${SRC_DIR}" --match "v[0-9]*" --dirty)
if (NOT GIT_VERSION)
    # try again without the --dirty flag, might be an older git:
    git_describe(GIT_VERSION "${SRC_DIR}" --match "v[0-9]*")
endif()

if (NOT GIT_VERSION)
    set(GIT_VERSION "UNKNOWN")
else ()
    STRING(REGEX REPLACE "^v" "" GIT_VERSION ${GIT_VERSION})
endif ()
set (WORKFLOW_VERSION "0.29.0")
file(WRITE ${CONFIG_FILE} "WORKFLOW_VERSION\t${WORKFLOW_VERSION}\n")

#
# get build timestamp
#
# python is a cross platform way to do this without newer cmake,
# we have compile and runtime python req anyway.
#
find_package(PythonInterp QUIET)
if (PYTHONINTERP_FOUND)
    execute_process(
        COMMAND ${PYTHON_EXECUTABLE} -c "import datetime;print(datetime.datetime.utcnow().isoformat())"
        OUTPUT_VARIABLE BUILD_TIME
        OUTPUT_STRIP_TRAILING_WHITESPACE)

    set (BUILD_TIME "${BUILD_TIME}Z")
else ()
    set (BUILD_TIME "UNKNOWN")
endif ()
file(APPEND ${CONFIG_FILE} "BUILD_TIME\t${BUILD_TIME}\n")
