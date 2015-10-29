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
## CMake configuration file for all the c++ executables
##
## author Roman Petrovski
##
################################################################################

include (${THIS_GLOBALS_CMAKE})

if(NOT DEFINED IS_QUIET)
    get_filename_component(CURRENT_DIR_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)
    message (STATUS "Adding c++ program subdirectory: ${CURRENT_DIR_NAME}")
endif()

include_directories (BEFORE SYSTEM ${THIS_CXX_BEFORE_SYSTEM_INCLUDES})
include_directories (${THIS_CXX_ALL_INCLUDES})
include_directories (${CMAKE_CURRENT_BINARY_DIR})
include_directories (${CMAKE_CURRENT_SOURCE_DIR})
include_directories (${THIS_CXX_CONFIG_H_DIR})
