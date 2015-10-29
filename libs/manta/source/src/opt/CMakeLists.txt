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
## Configuration file for the opt subfolder (external tools)
##
## author Come Raczy
##
################################################################################

message(STATUS "Creating external tools in subdirectory opt")

include ("${THIS_GLOBALS_CMAKE}")
include ("${THIS_MACROS_CMAKE}")

# convenience macro to set in both current and parent scope:
macro(superset symbol value)
    set(${symbol} "${value}")
    set(${symbol} "${value}" PARENT_SCOPE)
endmacro()


#
# zlib
#
set(ZLIB_PREFIX "zlib-1.2.8")
superset(ZLIB_DIR "${CMAKE_CURRENT_BINARY_DIR}/${ZLIB_PREFIX}")
if (WIN32)
    superset(ZLIB_LIBRARY "${ZLIB_DIR}/${CMAKE_BUILD_TYPE}/zlibstatic.lib")
else ()
    superset(ZLIB_LIBRARY "${ZLIB_DIR}/libz.a")
endif ()

add_custom_command(
	OUTPUT ${ZLIB_DIR}
	COMMAND ${CMAKE_COMMAND} -E remove_directory "${ZLIB_DIR}"
	COMMAND ${CMAKE_COMMAND} -E tar xjf "${THIS_REDIST_DIR}/${ZLIB_PREFIX}.tar.bz2"
	COMMENT "Unpacking zlib package")

if (NOT WIN32)
    set (ASM_MAKE_OPTION LOC=-DASMV OBJA=match.o)
    if (THIS_ARCH MATCHES "^.*86$")
        set(ZLIB_ARCH "")
        set(ASM_FILE_COPY cp contrib/asm686/match.S match.S)
    elseif (THIS_ARCH MATCHES "^x86_64$")
        set(ZLIB_ARCH "--64")
        set(ASM_FILE_COPY cp contrib/amd64/amd64-match.S match.S)
    else ()
        set (ZLIB_ARCH "")
        set (ASM_FILE_COPY "")
        set (ASM_MAKE_OPTION "")
    endif ()

    add_custom_command(
        OUTPUT ${ZLIB_LIBRARY}
        COMMAND CC=${CMAKE_C_COMPILER} ${ZLIB_DIR}/configure --prefix="${ZLIB_DIR}" --static ${ZLIB_ARCH} >zlib.config.log
        COMMAND ${ASM_FILE_COPY}
        COMMAND $(MAKE) ${ASM_MAKE_OPTION} >zlib.build.log 2>zlib.build.error.log
        WORKING_DIRECTORY ${ZLIB_DIR}
        DEPENDS ${ZLIB_DIR}
        COMMENT "Building zlib package")
else ()
    # Sometimes the platform (x64,win32...) is specified as part of the target name
    # this captures the cases where it is specified separately:
    set (ZLIB_PLATFORM "")
    if (CMAKE_GENERATOR_PLATFORM)
        set (ZLIB_PLATFORM -A "${CMAKE_GENERATOR_PLATFORM}")
    endif ()

    add_custom_command(
        OUTPUT ${ZLIB_LIBRARY}
        COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}" ${ZLIB_PLATFORM} -DCMAKE_C_COMPILER="${CMAKE_C_COMPILER}" ${ZLIB_DIR} >zlib.config.log
        COMMAND ${CMAKE_MAKE_PROGRAM} "${ZLIB_DIR}/zlib.sln" >zlib.build.log
        WORKING_DIRECTORY ${ZLIB_DIR}
        DEPENDS ${ZLIB_DIR}
        COMMENT "Building zlib package")
endif ()


set(THIS_ZLIB "${THIS_PROJECT_NAME}_zlib")
add_custom_target(${THIS_ZLIB} DEPENDS "${ZLIB_LIBRARY}")


#
# htslib
#
set(HTSLIB_PREFIX "htslib-1.2.1-204-g8197cfd")
superset(HTSLIB_DIR "${CMAKE_CURRENT_BINARY_DIR}/${HTSLIB_PREFIX}")
superset(HTSLIB_LIBRARY "${HTSLIB_DIR}/libhts.a")

add_custom_command(
    OUTPUT ${HTSLIB_DIR}
    COMMAND ${CMAKE_COMMAND} -E remove_directory "${HTSLIB_DIR}"
    COMMAND ${CMAKE_COMMAND} -E tar xjf "${THIS_REDIST_DIR}/${HTSLIB_PREFIX}.tar.bz2"
    DEPENDS ${THIS_ZLIB}
    COMMENT "Unpacking htslib library")

set (HTSLIB_FINAL_TASK ${HTSLIB_DIR})

if (NOT WIN32)
    # note that htslib ./configure CFLAGS="custom" will overwrite the standard "-g -O2 -Wall" CFLAGS, so
    # we need to restore at least the -O2 for reasonable library performance:
    add_custom_command(
        OUTPUT ${HTSLIB_LIBRARY}
        COMMAND ./configure CC="${CMAKE_C_COMPILER}" CFLAGS='-O2 -I"${ZLIB_DIR}"' LIBS="${ZLIB_LIBRARY}" >htslib.config.log
        COMMAND $(MAKE) lib-static bgzip htsfile tabix >htslib.build.log 2>htslib.build.error.log
        WORKING_DIRECTORY ${HTSLIB_DIR}
        DEPENDS ${HTSLIB_DIR}
        COMMENT "Building htslib library")

    set (HTSLIB_FINAL_TASK ${HTSLIB_LIBRARY})
endif ()

set(THIS_HTSLIB "${THIS_PROJECT_NAME}_htslib")
add_custom_target(${THIS_HTSLIB} DEPENDS "${HTSLIB_FINAL_TASK}")


if (NOT WIN32)
    install(PROGRAMS "${HTSLIB_DIR}/bgzip" DESTINATION "${THIS_LIBEXECDIR}")
    install(PROGRAMS "${HTSLIB_DIR}/htsfile" DESTINATION "${THIS_LIBEXECDIR}")
    install(PROGRAMS "${HTSLIB_DIR}/tabix" DESTINATION "${THIS_LIBEXECDIR}")
endif ()


#
# pyflow
#

set(PYFLOW_PREFIX "pyflow-1.1.10")
set(PYFLOW_DIR "${CMAKE_CURRENT_BINARY_DIR}/${PYFLOW_PREFIX}")
set(PYFLOW_SCRIPT "${PYFLOW_DIR}/src/pyflow.py")

set (PYFLOW_DEPENDS "${PYFLOW_SCRIPT}")
add_custom_command(
    OUTPUT ${PYFLOW_SCRIPT}
    COMMAND ${CMAKE_COMMAND} -E remove_directory "${PYFLOW_DIR}"
    COMMAND ${CMAKE_COMMAND} -E tar xjf "${THIS_REDIST_DIR}/${PYFLOW_PREFIX}.tar.bz2"
    COMMAND ${CMAKE_COMMAND} -E remove -f "${PYFLOW_DIR}/src/__init__.py"
    COMMENT "Unpacking pyflow")

if (PYTHONINTERP_FOUND)
    set (PYFLOW_DEPENDS "${PYFLOW_SCRIPT}c")
    add_custom_command(
        OUTPUT ${PYFLOW_SCRIPT}c
        COMMAND ${PYTHON_EXECUTABLE} -m compileall -q "${PYFLOW_DIR}/src"
        DEPENDS ${PYFLOW_SCRIPT}
        COMMENT "Building pyflow")
endif()

install_python_lib_dir("${PYFLOW_DIR}/src" "${THIS_PYTHON_LIBDIR}/pyflow")

set (THIS_PYFLOW "${THIS_PROJECT_NAME}_pyflow")
add_custom_target(${THIS_PYFLOW} DEPENDS "${PYFLOW_DEPENDS}")


# tie results back to parent:
#
add_dependencies(${THIS_OPT} ${THIS_ZLIB} ${THIS_HTSLIB} ${THIS_PYFLOW})


#
# hoedown
#
set(HOEDOWN_PREFIX "hoedown-1.0.2_with_allext")
set(HOEDOWN_DIR "${CMAKE_CURRENT_BINARY_DIR}/${HOEDOWN_PREFIX}")
superset(HOEDOWN_PROG "${HOEDOWN_DIR}/hoedown_allext")

if (NOT WIN32)
    add_custom_command(
        OUTPUT ${HOEDOWN_PROG}
        COMMAND ${CMAKE_COMMAND} -E remove_directory "${HOEDOWN_DIR}"
        COMMAND ${CMAKE_COMMAND} -E tar xjf "${THIS_REDIST_DIR}/${HOEDOWN_PREFIX}.tar.bz2"
        COMMAND $(MAKE) -C "${HOEDOWN_DIR}"
        COMMENT "Building hoedown markdown parser")

    superset(THIS_MARKDOWN "${THIS_PROJECT_NAME}_markdown")
    add_custom_target(${THIS_MARKDOWN} DEPENDS "${HOEDOWN_PROG}")
endif ()
