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
## Configuration file for the c++ subfolder
##
## author Come Raczy
##
################################################################################

set(CMAKE_SKIP_BUILD_RPATH  FALSE)
set(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

set (THIS_CXX_EXECUTABLE_CMAKE "${CMAKE_SOURCE_DIR}/cmake/cxxExecutable.cmake")
set (THIS_CXX_LIBRARY_CMAKE "${CMAKE_SOURCE_DIR}/cmake/cxxLibrary.cmake")
set (THIS_CXX_CONFIGURE_CMAKE "${CMAKE_SOURCE_DIR}/cmake/cxxConfigure.cmake")
set (THIS_CXX_TEST_LIBRARY_CMAKE "${CMAKE_SOURCE_DIR}/cmake/cxxTestLibrary.cmake")

include ("${THIS_CXX_CONFIGURE_CMAKE}")

##
## refresh git label at build-time
##
add_dependencies(${THIS_OPT} ${CXX_BUILDTIME_CONFIG_TARGET})

##
## run cppcheck if any cxx source has been updated:
##
if (PYTHONINTERP_FOUND)
    if(${DEVELOPER_MODE})
        file(GLOB_RECURSE ALL_CXX_FILES *.cpp *.cc *.hh)

        add_custom_command(
            OUTPUT cppcheck.done
            DEPENDS ${ALL_CXX_FILES}
            COMMAND ${PYTHON_EXECUTABLE} ${THIS_SOURCE_QC_DIR}/run_cppcheck.py ${CMAKE_CURRENT_SOURCE_DIR}
            COMMENT "Running c++ static analyzer")

        set(THIS_CPPCHECK "${THIS_PROJECT_NAME}_cppcheck")
        add_custom_target(${THIS_CPPCHECK} ALL DEPENDS cppcheck.done)

        add_dependencies(${THIS_OPT} ${THIS_CPPCHECK})
    endif()
endif ()

##
## Build all the libraries for the project
##
add_subdirectory (lib)


##
## build all the applications for the project
##
if (NOT WIN32)
    add_subdirectory (bin)
endif ()

##
## build the documentation when available
##
include  (FindDoxygen)
message (STATUS "Doxygen: ${DOXYGEN_EXECUTABLE}. Dot: ${DOXYGEN_DOT_EXECUTABLE}.")
if (DOXYGEN_FOUND)
    set (DOXYFILE_SRC ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in)
    set (DOXYFILE_SRC2 ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile.in)
    set (DOXYFILE ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)
    message (STATUS "Creating Doxygen config file: ${DOXYFILE}")

    # configure doxyfile twice, once at config time and once at build time:
    set (BUILDTIME_WORKFLOW_VERSION "@WORKFLOW_VERSION@")
    configure_file(${DOXYFILE_SRC} ${DOXYFILE_SRC2} @ONLY)

    # set doxyfile to update from build-time configuration info:
    set (THIS_BUILDTIME_DOXYGEN_TARGET "${THIS_PROJECT_NAME}_doxygen_buildtime_config")
    add_custom_target(${THIS_BUILDTIME_DOXYGEN_TARGET}
        DEPENDS ${THIS_BUILDTIME_CONFIG_TARGET}
        COMMAND ${CMAKE_COMMAND}
            -D CONFIG_FILE=${THIS_BUILDTIME_CONFIG_FILE}
            -D SOURCE_FILE=${DOXYFILE_SRC2}
            -D DEST_FILE=${DOXYFILE}
            -P ${THIS_MODULE_DIR}/buildTimeConfigure.cmake)
    add_custom_target(doc
        DEPENDS ${THIS_BUILDTIME_DOXYGEN_TARGET}
        COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYFILE})
endif ()

