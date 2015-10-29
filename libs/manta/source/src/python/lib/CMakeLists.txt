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

#
# \author Chris Saunders
#

# final installation path:
set(INSTALL_TO_DIR "${THIS_PYTHON_LIBDIR}")

# clean staging area for all py and pyc files:
set(PYSTAGE_DIR "${CMAKE_CURRENT_BINARY_DIR}/pystage")

file(RELATIVE_PATH THIS_RELATIVE_LIBEXECDIR "${INSTALL_TO_DIR}" "${THIS_LIBEXECDIR}")

set(BUILD_CONFIG_PY "configBuildTimeInfo.py")
file(GLOB PYTHON_FILES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "${CMAKE_CURRENT_SOURCE_DIR}/*.py")
foreach(PYTHON_FILE ${PYTHON_FILES})
    if (NOT ${PYTHON_FILE} STREQUAL ${BUILD_CONFIG_PY})
        configure_file("${CMAKE_CURRENT_SOURCE_DIR}/${PYTHON_FILE}" "${PYSTAGE_DIR}/${PYTHON_FILE}" @ONLY)
    endif ()
endforeach()

# build-time configuration:
#
# buildConfig.py needs to be configured at build time, this actually doubles
#
set (PY_BUILD_CONFIG_TARGET "${THIS_PROJECT_NAME}_python_buildtime_config")
add_custom_target(
    ${PY_BUILD_CONFIG_TARGET}
    DEPENDS ${THIS_BUILDTIME_CONFIG_TARGET}
    COMMAND ${CMAKE_COMMAND}
    -D CONFIG_FILE=${THIS_BUILDTIME_CONFIG_FILE}
    -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/${BUILD_CONFIG_PY}
    -D DEST_FILE=${PYSTAGE_DIR}/${BUILD_CONFIG_PY}
    -P ${THIS_MODULE_DIR}/buildTimeConfigure.cmake)


if (PYTHONINTERP_FOUND)
    #
    # compile all py to pyc for:
    # (1) build-time error-checking (primarily)
    # (2) faster run-time response (in theory??? -- I don't observe)
    #
    file(GLOB STAGED_PYTHON_FILES "${PYSTAGE_DIR}/*.py")
    foreach(PYFILE ${STAGED_PYTHON_FILES})
        list(APPEND STAGED_PYTHONC_FILES "${PYFILE}c")
    endforeach()

    add_custom_command(
        OUTPUT ${STAGED_PYTHONC_FILES}
        COMMAND ${PYTHON_EXECUTABLE} -m compileall -q ${PYSTAGE_DIR}
        COMMENT "Compiling python library source")

    set(THIS_PYTHONLIB_COMPILE "${THIS_PROJECT_NAME}_pythonlib_compile")
    add_custom_target(${THIS_PYTHONLIB_COMPILE} ALL
        DEPENDS ${STAGED_PYTHONC_FILES}
        DEPENDS ${PY_BUILD_CONFIG_TARGET})
endif ()



#
# install
#
include("${THIS_MACROS_CMAKE}")
install_python_lib_dir("${PYSTAGE_DIR}" "${INSTALL_TO_DIR}")

