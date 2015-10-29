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
## CMake configuration file for all the c++ libraries
##
## author Come Raczy
##
################################################################################
include_directories (BEFORE SYSTEM ${THIS_CXX_BEFORE_SYSTEM_INCLUDES})
include_directories (${THIS_CXX_ALL_INCLUDES})
include_directories (${CMAKE_CURRENT_BINARY_DIR})
include_directories (${CMAKE_CURRENT_SOURCE_DIR})
include_directories (${THIS_CXX_CONFIG_H_DIR})

get_filename_component(CURRENT_DIR_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)
message (STATUS "Adding c++ library subdirectory: ${CURRENT_DIR_NAME}")

##
## Some generators (VS) require all targets to be unique across the project.
## Therefore, a unique prefix is needed to create the target names which are
## shared across libraries
##

string(REGEX REPLACE ${CMAKE_SOURCE_DIR}/c[+][+]/ "" TMP1 ${CMAKE_CURRENT_SOURCE_DIR}/)
string(REGEX REPLACE "/" "_" THIS_UNIQUE_PREFIX ${TMP1})

##
## build the library
##

file(GLOB THIS_LIBRARY_SOURCES *.cpp *.c)
foreach (SOURCE_FILE ${THIS_LIBRARY_SOURCES})
    get_filename_component(SOURCE_NAME ${SOURCE_FILE} NAME_WE)
    if (${SOURCE_NAME}_COMPILE_FLAGS)
        set_source_files_properties(${SOURCE_FILE} PROPERTIES COMPILE_FLAGS ${${SOURCE_NAME}_COMPILE_FLAGS})
    endif ()
endforeach ()

# we don't need to add headers to the library for the build to work, but we
# add headers on win32 to make the Visual Studio solution easier to work with:
if (WIN32)
    file(GLOB THIS_LIBRARY_HEADERS *.hh)
    set (THIS_LIBRARY_SOURCES ${THIS_LIBRARY_SOURCES} ${THIS_LIBRARY_HEADERS})
endif ()

if (THIS_LIBRARY_SOURCES)
    set (LIB_TARGET_NAME "${THIS_PROJECT_NAME}_${CURRENT_DIR_NAME}")
    add_library     (${LIB_TARGET_NAME} STATIC ${THIS_LIBRARY_SOURCES})
    add_dependencies(${LIB_TARGET_NAME} ${THIS_OPT})

    # make the target project use folders when applying cmake IDE generators like Visual Studio
    file(RELATIVE_PATH THIS_RELATIVE_LIBDIR "${CMAKE_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}")
    set_property(TARGET ${LIB_TARGET_NAME} PROPERTY FOLDER "${THIS_RELATIVE_LIBDIR}")
endif()

##
## build the unit tests if a "test" subdirectory is found:
##
if (IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/test")
    message (STATUS "Adding c++ test subdirectory:    ${CURRENT_DIR_NAME}/test")
    add_subdirectory (test)
endif ()
