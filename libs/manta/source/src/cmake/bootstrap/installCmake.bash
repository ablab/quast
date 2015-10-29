#!/usr/bin/env bash
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

################################################################################
##
## Installation script for cmake
##
## author Come Raczy
##
################################################################################

set -o nounset
set -o pipefail

REDIST_DIR=$1
INSTALL_DIR=$2
if [[ $# -ge 3 ]] ; then PARALLEL=$3 ; else PARALLEL=1 ; fi

script_dir="$(dirname "$0")"
source $script_dir/common.bash

BUILD_DIR=${INSTALL_DIR}/build
BIN_DIR=${INSTALL_DIR}/bin
INCLUDE_DIR=${INSTALL_DIR}/include

CMAKE_MAJOR=2
CMAKE_MINOR=8
CMAKE_PATCH=12
CMAKE_PATCH_MIN=4
CMAKE_REQUIRED="$CMAKE_MAJOR.$CMAKE_MINOR.$CMAKE_PATCH_MIN"
TARBALL_VERSION="$CMAKE_MAJOR.$CMAKE_MINOR.$CMAKE_PATCH"
SCRIPT=`basename "$0"`
SOURCE_TARBALL=${REDIST_DIR}/cmake-$TARBALL_VERSION.tar.bz2
TARBALL_COMPRESSION=j
SOURCE_DIR=${BUILD_DIR}/cmake-$TARBALL_VERSION
CMAKE_DIR=cmake-$CMAKE_MAJOR.$CMAKE_MINOR

common_options $@

if [[ $CLEAN ]] ; then
    ilog "removing $SOURCE_DIR"
    rm -rf $SOURCE_DIR
    rm -rf ${INSTALL_DIR}/{doc,share}/$CMAKE_DIR
    rm -f ${BIN_DIR}/{ccmake,cmake,cpack,ctest}
    rm -f ${INSTALL_DIR}/man/man1/{ccmake,cmake,cmakecommands,cmakecompat,cmakemodules,cmakeprops,cmakevars,cpack,ctest}.1
    exit 0
fi

AVAILABLE_CMAKE_VERSION=`cmake --version 2> /dev/null`
if [[ "${AVAILABLE_CMAKE_VERSION}" =~ ^cmake\ version\ ([0-9]+)\.([0-9]+)\.([0-9]+) && ! $FORCE ]] ; then
    MAJOR=${BASH_REMATCH[1]}
    MINOR=${BASH_REMATCH[2]}
    PATCH=${BASH_REMATCH[3]}
    if [[ "$MAJOR" -gt "$CMAKE_MAJOR" || ( "$MAJOR" -eq "$CMAKE_MAJOR" && ( "$MINOR" -gt "$CMAKE_MINOR" || ( "$MINOR" -eq "$CMAKE_MINOR" && "$PATCH" -ge "$CMAKE_PATCH_MIN"  ) ) ) ]] ; then
        ilog "${BASH_REMATCH[0]} (>= $CMAKE_REQUIRED) is already installed"
        echo "cmake"
        exit 0
    fi
fi

OLD_CMAKE_VERSION=`${BIN_DIR}/cmake --version 2> /dev/null`;
if [[ $OLD_CMAKE_VERSION == "cmake version $TARBALL_VERSION" && ! $FORCE ]] ; then
    ilog "cmake version \"$TARBALL_VERSION\" is already installed at ${BIN_DIR}/cmake"
    echo "${BIN_DIR}/cmake"
    exit 0
elif [[ $OLD_CMAKE_VERSION != "" ]] ; then
    ilog "ERROR: unable to install cmake version \"$TARBALL_VERSION\" in ${BIN_DIR}"
    ilog "\tcmake version \"$OLD_CMAKE_VERSION\" is in the way."
    ilog "\tPlease use an empty location to build the product."
    exit 1
fi


##
## cleanup all existing source directory before proceeding
##
rm -rf $SOURCE_DIR

common_create_source

ilog "Extracted cmake version $TARBALL_VERSION source code into $SOURCE_DIR"
cmd="cd $SOURCE_DIR && ./bootstrap --prefix=\"${INSTALL_DIR}\" --parallel=$PARALLEL && make -j $PARALLEL && make install"
ilog "Installing cmake using: '$cmd'"
eval $cmd 1>&2

if [ $? != 0 ] ; then ilog "ERROR: cmake build failed: Terminating..."; exit 1 ; fi

ilog "Cleaning up ${SOURCE_DIR}"
rm -rf ${SOURCE_DIR}

ilog "CMake-$TARBALL_VERSION installed successfully"
echo "${BIN_DIR}/cmake"
