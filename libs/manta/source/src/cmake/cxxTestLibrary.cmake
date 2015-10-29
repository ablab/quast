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
## Configuration file for the unit tests subdirectory
##
## author Ole Schulz-Trieglaff
##
################################################################################

set(IS_QUIET true)
include(${THIS_CXX_EXECUTABLE_CMAKE})

set(TESTCONFIGNAME "test_config.h")
set(TESTCONFIGSRC "${CMAKE_CURRENT_SOURCE_DIR}/${TESTCONFIGNAME}.in")
set(TESTCONFIGDEST "${CMAKE_CURRENT_BINARY_DIR}/${TESTCONFIGNAME}")

if (EXISTS "${TESTCONFIGSRC}")
    configure_file("${TESTCONFIGSRC}" "${TESTCONFIGDEST}" @ONLY)
    include_directories("${CMAKE_CURRENT_BINARY_DIR}")
endif ()

set(TEST_TARGET_NAME "${THIS_PROJECT_NAME}_unit_test_${THIS_LIB_DIR}")

if (THIS_LIBRARY_SOURCES)
    set(ADDITIONAL_UNITTEST_LIB ${ADDITIONAL_UNITTEST_LIB} ${THIS_PROJECT_NAME}_${THIS_LIB_DIR})
endif ()

if (WIN32)
    # create a fake library target on win32 instead of linking and running the unit test
    # this creates a project in VS that allows for interaction with the unit test code

    # add all files to TEST_SOURCE to make the IDE project more usable:
    file(GLOB TMP_TARGET_FILES *)

    add_library     (${TEST_TARGET_NAME} STATIC ${TMP_TARGET_FILES})
    #set_source_files_properties(thefile PROPERTIES HEADER_FILE_ONLY TRUE)
    add_dependencies(${TEST_TARGET_NAME} ${THIS_OPT})
else ()
    file(GLOB TEST_SOURCE *.cpp)
    add_executable(${TEST_TARGET_NAME} ${TEST_SOURCE})
    add_dependencies(${TEST_TARGET_NAME} ${THIS_OPT})

    target_link_libraries (${TEST_TARGET_NAME} ${ADDITIONAL_UNITTEST_LIB} ${THIS_AVAILABLE_LIBRARIES}
                           ${HTSLIB_LIBRARY} ${CMAKE_THREAD_LIBS_INIT} ${Boost_LIBRARIES} ${THIS_ADDITIONAL_LIB})

    set(TEST_BINARY ${CMAKE_CURRENT_BINARY_DIR}/${TEST_TARGET_NAME})

    add_test(${TEST_TARGET_NAME} ${TEST_BINARY} "--log_level=test_suite")
endif ()

# make the target project use folders when applying cmake IDE generators like Visual Studio
file(RELATIVE_PATH THIS_RELATIVE_LIBDIR "${CMAKE_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}")
set_property(TARGET ${TEST_TARGET_NAME} PROPERTY FOLDER "${THIS_RELATIVE_LIBDIR}")

add_dependencies(${THIS_UNITTESTS} ${TEST_TARGET_NAME})
