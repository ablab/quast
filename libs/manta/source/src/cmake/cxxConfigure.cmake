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
## CMake configuration file for c++ executables
##
## author Come Raczy
##
################################################################################


INCLUDE(CheckFunctionExists)

find_path(HAVE_INTTYPES_H  inttypes.h)
find_path(HAVE_MEMORY_H    memory.h)
find_path(HAVE_STDINT_H    stdint.h)
find_path(HAVE_STDLIB_H    stdlib.h)
find_path(HAVE_STRING_H    string.h)
find_path(HAVE_STRINGS_H   strings.h)
find_path(HAVE_UNISTD_H    unistd.h)

set (CMAKE_REQUIRED_LIBRARIES m)
check_function_exists(floorf HAVE_FLOORF)
check_function_exists(round  HAVE_ROUND)
check_function_exists(roundf HAVE_ROUNDF)
check_function_exists(powf HAVE_POWF)

include ("${THIS_MACROS_CMAKE}")

# Support for static linking
# Note that this implies that all libraries must be found with the
# exact file name (libXXX.a or libXXX.so)
#if    (THIS_FORCE_STATIC_LINK)
#    message(STATUS "All libraries will be statically linked")
#    set(CMAKE_SHARED_LIBRARY_LINK_C_FLAGS "-static")
#    set(CMAKE_SHARED_LIBRARY_LINK_CXX_FLAGS "-static")
    # ensure that even if cmake decides to allow for dynamic libs resolution,
    # this gets overriden into static...
#    set(CMAKE_EXE_LINK_DYNAMIC_CXX_FLAGS ${CMAKE_EXE_LINK_STATIC_CXX_FLAGS})
#    set(THIS_LIBRARY_PREFIX ${CMAKE_STATIC_LIBRARY_PREFIX})
#    set(THIS_LIBRARY_SUFFIX ${CMAKE_STATIC_LIBRARY_SUFFIX})
    # set(CMAKE_EXE_LINKER_FLAGS "-static-libgcc -static-libstdc++")
#else  ()
#    set(THIS_LIBRARY_PREFIX "")
#    set(THIS_LIBRARY_SUFFIX "")
#endif ()

# point to local opt/ copy of zlib:
include_directories(${ZLIB_DIR})
set  (THIS_ADDITIONAL_LIB ${THIS_ADDITIONAL_LIB} ${ZLIB_LIBRARY})

# required support for librt to allow boost chrono
if (UNIX AND NOT APPLE)
    set  (THIS_ADDITIONAL_LIB ${THIS_ADDITIONAL_LIB} rt)
endif ()

if (WIN32)
    set  (THIS_ADDITIONAL_LIB ${THIS_ADDITIONAL_LIB} ws2_32)
endif ()


# htslib 1.x forces pthreads in link:
find_package( Threads )

# setup ccache if found in path
if (NOT WIN32)
    find_program(CCACHE_PATH ccache)
    set (IS_CCACHE TRUE)
    if (CCACHE_PATH STREQUAL "CCACHE_PATH-NOTFOUND")
        set (IS_CCACHE FALSE)
    endif()
endif ()

set (IS_CLANG ((CMAKE_C_COMPILER_ID STREQUAL "Clang") OR (CMAKE_C_COMPILER_ID STREQUAL "AppleClang")))
set (IS_CLANGXX ((CMAKE_CXX_COMPILER_ID STREQUAL "Clang") OR (CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")))


if (${IS_CCACHE})
    message (STATUS "Found ccache: ${CCACHE_PATH}")
    SET_PROPERTY(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ${CCACHE_PATH})
    SET_PROPERTY(GLOBAL PROPERTY RULE_LAUNCH_LINK ${CCACHE_PATH})

    # special logic to get clang and ccache working together (suggestion from http://petereisentraut.blogspot.com/2011/09/ccache-and-clang-part-2.html):
    set(ENV{CCACHE_CPP2} "yes")
    if (${IS_CLANGXX})
        append_args (CMAKE_CXX_FLAGS "-Qunused-arguments")
    endif()
    if (${IS_CLANG})
        append_args (CMAKE_C_FLAGS "-Qunused-arguments")
    endif()

else()
    message (STATUS "No ccache found")
endif()


# Force static linking
set(CMAKE_SHARED_LIBRARY_LINK_C_FLAGS "")

function(get_compiler_name_version compiler_name compiler_version)
    execute_process(COMMAND ${compiler_name} -dumpversion OUTPUT_VARIABLE this_version)
    STRING(REGEX REPLACE "(\r?\n)+$" "" this_version "${this_version}")
    set(${compiler_version} ${this_version} PARENT_SCOPE)
endfunction()

function(get_compiler_version compiler_version)
    get_compiler_name_version(${CMAKE_CXX_COMPILER} this_version)
    set(${compiler_version} ${this_version} PARENT_SCOPE)
endfunction()

# clang doesn't make finding the version easy for us,
# and apple makes it even harder...
macro(get_clang_version compiler_version)
#    execute_process(COMMAND bash -c "${CMAKE_CXX_COMPILER} -v 2>&1 | awk '{printf $3; exit}'" OUTPUT_VARIABLE ${compiler_version})
    execute_process(COMMAND bash -c "echo | ${CMAKE_CXX_COMPILER} -dM -E - | awk '/__clang_version__/ {printf $3; exit}' | tr -d '\"'" OUTPUT_VARIABLE ${compiler_version})
    if (APPLE)
        # translate apple clang version numbers back to root llvm value (better way to do this?)
        if (${compiler_version} VERSION_LESS "3.1")
            set (${compiler_version} "0.0")
        elseif (${compiler_version} VERSION_LESS "4.2")
            set (${compiler_version} "3.1")
        elseif (${compiler_version} VERSION_LESS "5.0")
            set (${compiler_version} "3.2")
        elseif (${compiler_version} VERSION_LESS "5.1")
            set (${compiler_version} "3.3")
        elseif (${compiler_version} VERSION_LESS "6.0")
            set (${compiler_version} "3.4")
        elseif (${compiler_version} VERSION_LESS "6.1")
            set (${compiler_version} "3.5")
        else ()
            set (${compiler_version} "3.6")
        endif ()
    endif ()
endmacro()

macro(test_min_compiler compiler_version min_compiler_version compiler_label)
    if (${compiler_version} VERSION_LESS ${min_compiler_version})
        message (FATAL_ERROR "Unsupported version for ${compiler_label}: ${compiler_version}: "
                             "only versions >= ${min_compiler_version} are supported")
    endif ()
endmacro()


set (min_gxx_version "4.7")
set (min_clang_version "3.1")
set (min_intel_version "14.0")
set (min_msvc_version "1800") # cl.exe 18, as shipped in Visual Studio 12 2013

set (CXX_COMPILER_NAME "${CMAKE_CXX_COMPILER_ID}")
set (COMPILER_VERSION "UNKNOWN")

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    get_compiler_version(COMPILER_VERSION)
    set (CXX_COMPILER_NAME "g++")
    test_min_compiler(${COMPILER_VERSION} "${min_gxx_version}" "${CXX_COMPILER_NAME}")
elseif (${IS_CLANGXX})
    get_clang_version(COMPILER_VERSION)
    set (CXX_COMPILER_NAME "clang++")
    test_min_compiler(${COMPILER_VERSION} "${min_clang_version}" "${CXX_COMPILER_NAME}")
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "Intel")
    get_compiler_version(COMPILER_VERSION)
    set (CXX_COMPILER_NAME "icpc")
    test_min_compiler(${COMPILER_VERSION} "${min_intel_version}" "${CXX_COMPILER_NAME}")
elseif (MSVC)
    set (COMPILER_VERSION ${MSVC_VERSION})
    set (CXX_COMPILER_NAME "msvc")
    test_min_compiler(${COMPILER_VERSION} "${min_msvc_version}" "${CXX_COMPILER_NAME}")
endif ()

message (STATUS "Using compiler: ${CXX_COMPILER_NAME} version ${COMPILER_VERSION}")

if (CMAKE_CXX_COMPILER_ID STREQUAL "Intel")
    # for intel we also need to test the minimum version of g++ currently
    # in the path (because this is the stdc++ library that intel will use):
    get_compiler_name_version("g++" gxx_compiler_version)
    test_min_compiler(${gxx_compiler_version} "${min_gxx_version}" "g++ libstdc++ (library used by icpc)")
    message (STATUS "Using libstdc++: gnu version ${gxx_compiler_version}")
endif ()


#
# set compile flags
#


##
## set static linking of standard libraries for binary redistribution:
##
set (IS_STANDARD_STATIC FALSE)
if     (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    if (NOT (${COMPILER_VERSION} VERSION_LESS "4.5"))
        set (IS_STANDARD_STATIC TRUE)
    endif ()
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "Intel")
    set (IS_STANDARD_STATIC TRUE)
endif ()

if (${IS_STANDARD_STATIC})
    append_args (CMAKE_EXE_LINKER_FLAGS "-static-libgcc -static-libstdc++")
endif ()


##
## set bug workarounds:
##

# determine version of libstdc++ library
if     (CMAKE_CXX_COMPILER_ID STREQUAL "INTEL")
    set(STDCXX_VERSION ${gxx_compiler_version})
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(STDCXX_VERSION ${COMPILER_VERSION})
else ()
    set(STDCXX_VERSION FALSE)
endif ()

if     (STDCXX_VERSION)
    if (((${STDCXX_VERSION} VERSION_EQUAL "4.7") OR (${STDCXX_VERSION} VERSION_EQUAL "4.7.3")) OR
        ((${STDCXX_VERSION} VERSION_EQUAL "4.8") OR (${STDCXX_VERSION} VERSION_EQUAL "4.8.2")))
        # workaround for: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=58800
        add_definitions( -DBROKEN_NTH_ELEMENT )
    endif ()
endif ()


##
## set warning flags:
##
set (GNU_COMPAT_COMPILER ( (CMAKE_CXX_COMPILER_ID STREQUAL "GNU") OR (${IS_CLANGXX}) OR (CMAKE_CXX_COMPILER_ID STREQUAL "Intel")))
if (${GNU_COMPAT_COMPILER})
    append_args(CXX_WARN_FLAGS "-Wall -Wextra -Wshadow -Wunused -Wpointer-arith -Winit-self -pedantic -Wunused-parameter")
    append_args(CXX_WARN_FLAGS "-Wundef -Wno-unknown-pragmas")
    append_args(CXX_WARN_FLAGS "-Wdeprecated")

    if ((NOT CMAKE_CXX_COMPILER_ID STREQUAL "Intel") OR (NOT ${COMPILER_VERSION} VERSION_LESS "14.0"))
        append_args(CXX_WARN_FLAGS "-Wdisabled-optimization")
        append_args(CXX_WARN_FLAGS "-Wno-missing-braces")
    endif ()

    if (NOT CMAKE_CXX_COMPILER_ID STREQUAL "Intel")
        append_args(CXX_WARN_FLAGS "-Wempty-body")
        append_args(CXX_WARN_FLAGS "-Wredundant-decls")
    endif ()

    if (NOT ${CMAKE_BUILD_TYPE} STREQUAL "Debug")
        append_args(CXX_WARN_FLAGS "-Wuninitialized")
    endif ()
elseif (MSVC)
    append_args(CXX_WARN_FLAGS "/W3 /wd4305 /wd4244 /wd4068")
    # suppress warnings for size_t to {unsigned,int, etc...} narrowing (most occur in 64 bit build):
    append_args(CXX_WARN_FLAGS "/wd4267")
    add_definitions(/D_CRT_SECURE_NO_WARNINGS)
endif ()

if     (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    if (NOT (${COMPILER_VERSION} VERSION_LESS "4.2"))
        append_args(CXX_WARN_FLAGS "-Wlogical-op")
    endif ()

    if ((${COMPILER_VERSION} VERSION_LESS "4.8") AND (NOT (${COMPILER_VERSION} VERSION_LESS "4.7")))
        # switching off warning about unused function because otherwise compilation will fail with g++ 4.7.3 in Ubuntu,
        # don't know which patch levels are affected, so marking out all gcc 4.7.X
        append_args(CXX_WARN_FLAGS "-Wno-unused-function")
    endif ()

    if (NOT (${COMPILER_VERSION} VERSION_LESS "5.1"))
        # these mostly only make sense with flto:
        append_args(CXX_WARN_FLAGS "-Wodr")
        #append_args(CXX_WARN_FLAGS "-Wsuggest-final-types -Wsuggest-final-methods")
    endif ()

elseif (${IS_CLANGXX})
    # set to true to uncover new clang warnings after llvm update:
    set (IS_WARN_EVERYTHING FALSE)

    if (${IS_WARN_EVERYTHING})
        append_args(CXX_WARN_FLAGS "-Weverything")
    endif ()

    append_args(CXX_WARN_FLAGS "-Wmissing-prototypes -Wunused-exception-parameter -Wbool-conversion")
    append_args(CXX_WARN_FLAGS "-Wsizeof-array-argument -Wstring-conversion")
    append_args(CXX_WARN_FLAGS "-Wheader-hygiene -Wmismatched-tags")

    if (${IS_WARN_EVERYTHING})
        append_args(CXX_WARN_FLAGS "-Wno-sign-conversion -Wno-weak-vtables -Wno-conversion -Wno-cast-align -Wno-padded")
        append_args(CXX_WARN_FLAGS "-Wno-switch-enum -Wno-missing-noreturn -Wno-covered-switch-default")
        append_args(CXX_WARN_FLAGS "-Wno-unreachable-code -Wno-global-constructors -Wno-exit-time-destructors")
        append_args(CXX_WARN_FLAGS "-Wno-c++98-compat -Wno-old-style-cast -Wno-unused-member-function")
        append_args(CXX_WARN_FLAGS "-Wno-documentation -Wno-float-equal")
    endif ()

    if (NOT (${COMPILER_VERSION} VERSION_LESS "3.2"))
        append_args(CXX_WARN_FLAGS "-Wimplicit-fallthrough -Wloop-analysis -Wextra-semi")
        append_args(CXX_WARN_FLAGS "-Wmissing-variable-declarations -Wunused-private-field")
    endif ()

    if (NOT (${COMPILER_VERSION} VERSION_LESS "3.3"))
        append_args(CXX_WARN_FLAGS "-Woverloaded-shift-op-parentheses")

        if (${IS_WARN_EVERYTHING})
            append_args(CXX_WARN_FLAGS "-Wno-documentation-unknown-command")
        endif ()
    endif ()

    if (NOT (${COMPILER_VERSION} VERSION_LESS "3.4"))
        append_args(CXX_WARN_FLAGS "-Wheader-guard -Wlogical-not-parentheses")
    endif ()

    if (NOT (${COMPILER_VERSION} VERSION_LESS "3.6"))
        append_args(CXX_WARN_FLAGS "-Wunreachable-code-return -Wkeyword-macro -Winconsistent-missing-override")

        if (${IS_WARN_EVERYTHING})
            append_args(CXX_WARN_FLAGS "-Wno-reserved-id-macro")
        endif ()
    endif ()

    if (NOT (${COMPILER_VERSION} VERSION_LESS "3.7"))
        if (${IS_WARN_EVERYTHING})
            append_args(CXX_WARN_FLAGS "-Wno-c++98-compat-pedantic")
        endif ()
    endif ()

elseif (CMAKE_CXX_COMPILER_ID STREQUAL "Intel")
    # suppress errors in boost headers:
    append_args(CXX_WARN_FLAGS "-diag-disable 177,193,869,1599,3280")

    append_args(CXX_WARN_FLAGS "-Wunused-variable -Wpointer-arith")

    #append_args(CXX_WARN_FLAGS "-Wmissing-prototypes -Wmissing-declarations -Wunused-variable -Wpointer-arith -Wuninitialized")
endif()

append_args (CMAKE_CXX_FLAGS "${CXX_WARN_FLAGS}")


if (${GNU_COMPAT_COMPILER})
    if ((NOT CMAKE_CXX_COMPILER_ID STREQUAL "Intel") OR (${COMPILER_VERSION} VERSION_LESS "15.0"))
        append_args (CMAKE_CXX_FLAGS "-std=c++0x")
    else ()
        append_args (CMAKE_CXX_FLAGS "-std=c++11")
    endif ()
    set (CMAKE_CXX_FLAGS_DEBUG "-O0 -g")

    # The NDEBUG macro is intentionally removed from release. One discussion on this is:
    # http://www.drdobbs.com/an-exception-or-a-bug/184401686
    set (CMAKE_CXX_FLAGS_RELEASE "-O3 -fomit-frame-pointer")
    set (CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O2 -g")
    set (CMAKE_CXX_FLAGS_ASAN "-O1 -g -fsanitize=address -fno-omit-frame-pointer -fno-optimize-sibling-calls")
    #set (CMAKE_CXX_FLAGS_PROFILE "-O0 -g -pg -fprofile-arcs -ftest-coverage")

    # this doesn't seem to impact performance, taking out for now:
    #if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    #    set (CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -flto")
    #endif ()
endif()

# if ASan build type is requested, check that the compiler supports it:
if (CMAKE_BUILD_TYPE STREQUAL "ASan")
    set (IS_ASAN_SUPPORTED false)
    if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        if (NOT (${COMPILER_VERSION} VERSION_LESS "4.8"))
            set (IS_ASAN_SUPPORTED true)
        endif ()
    elseif (${IS_CLANGXX})
        if (NOT (${COMPILER_VERSION} VERSION_LESS "3.1"))
            set (IS_ASAN_SUPPORTED true)
        endif ()
    endif ()

    if (NOT ${IS_ASAN_SUPPORTED})
        message(FATAL_ERROR "Address sanitizer build type requested, but this is not supported by compiler.")
    endif ()
endif ()

#
# take advantage of analyze on VS
#
if (MSVC)
    if (IS_MSVC_ANALYZE)
        append_args (CMAKE_CXX_FLAGS "/analyze")
    endif ()
endif ()


if (${GNU_COMPAT_COMPILER})

  if (${DEVELOPER_MODE})
    # some compiler versions will produce warnings with no reasonable workaround,
    # turn Werror off in this case
    #
    # a very common example are warnings from boost generated despite this library
    # being identified as a system header
    #
    set(IS_WERROR true)
    if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        if (${COMPILER_VERSION} VERSION_LESS "4.2")
            set(IS_WERROR false)
        endif ()
    endif ()

    if(${IS_WERROR})
        message (STATUS "Building in developer mode: treating compiler warnings as errors")
        append_args (CMAKE_CXX_FLAGS "-Werror")
    endif ()
  endif ()

  if (CMAKE_SYSTEM_PROCESSOR MATCHES "^i[67]86$")
    ##
    ## Use scalar floating point instructions from the SSE instruction set.
    ## Note: Pentium3 SSE supports only single precision arithmetics
    ##
    append_args(CMAKE_CXX_FLAGS "-msse -mfpmath=sse")
  elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "^i[345]86$")
    ##
    ## Prevent using 80bit registers (more consistent rounding)
    ##
    append_args (CMAKE_CXX_FLAGS "-ffloat-store")
  endif ()

endif()

# cmake configure-time c++ configuration:
set(THIS_CXX_CONFIG_H_DIR ${CMAKE_CURRENT_BINARY_DIR}/lib)
set (CONFIG_DEST_FILE ${THIS_CXX_CONFIG_H_DIR}/common/config.h)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/lib/common/config.h.in ${CONFIG_DEST_FILE} @ONLY)

# build-time c++ configuration:
# note: (csaunders) tried to do this as add_custom_command every which way, can't get cmake to figure out
#       dependency chain in this case
set (CXX_BUILDTIME_CONFIG_BASENAME "configBuildTimeInfo.h")
set (CXX_BUILDTIME_CONFIG_SOURCE_FILE ${CMAKE_CURRENT_SOURCE_DIR}/lib/common/${CXX_BUILDTIME_CONFIG_BASENAME}.in)
set (CXX_BUILDTIME_CONFIG_DEST_FILE ${THIS_CXX_CONFIG_H_DIR}/common/${CXX_BUILDTIME_CONFIG_BASENAME})
set (CXX_BUILDTIME_CONFIG_TARGET "${THIS_PROJECT_NAME}_cxx_buildtime_config")
add_custom_target(${CXX_BUILDTIME_CONFIG_TARGET}
    DEPENDS ${THIS_BUILDTIME_CONFIG_TARGET}
    COMMAND ${CMAKE_COMMAND}
    -D CONFIG_FILE=${THIS_BUILDTIME_CONFIG_FILE}
    -D SOURCE_FILE=${CXX_BUILDTIME_CONFIG_SOURCE_FILE}
    -D DEST_FILE=${CXX_BUILDTIME_CONFIG_DEST_FILE}
    -P ${THIS_MODULE_DIR}/buildTimeConfigure.cmake)

# special config hack for windows
if (WIN32)
    set (UNSTD_DEST_FILE ${THIS_CXX_CONFIG_H_DIR}/unistd.h)
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/lib/blt_util/compat_unistd.h ${UNSTD_DEST_FILE} COPYONLY)
endif ()

#
# include dirs:
#
set (THIS_CXX_BEFORE_SYSTEM_INCLUDES "${Boost_INCLUDE_DIRS}" "${HTSLIB_DIR}")
set (THIS_CXX_ALL_INCLUDES "${CMAKE_SOURCE_DIR}/c++/lib")
