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
## CMake configuration file for common installation macros
##
## authors: Roman Petrovski, Mauricio Varea
##
################################################################################

macro(configure_files srcDir destDir pattern)
    file(GLOB templateFiles RELATIVE ${srcDir} ${srcDir}/${pattern})
    foreach(templateFile ${templateFiles})
        #message(STATUS "Configuring file ${srcDir}/${templateFile}")
        configure_file(${srcDir}/${templateFile} ${destDir}/${templateFile} @ONLY)
    endforeach()
endmacro()


macro(install_fileglob srcDir destDir pattern perm)
    file(GLOB templateFiles ${srcDir}/${pattern})
    install(FILES ${templateFiles} DESTINATION ${destDir} PERMISSIONS ${perm})
endmacro()


#
# Macro to find libraries, with support for static-only search
#
macro(static_find_library name header library)
    if    (NOT ${name}_INCLUDE_DIR)
        find_path(${name}_INCLUDE_DIR ${header}
                  HINTS ENV C_INCLUDE_PATH ENV CPATH ENV CPLUS_INCLUDE_PATH)
    endif ()
    if    (${name}_INCLUDE_DIR AND NOT ${name}_LIBRARY)
        find_library(${name}_LIBRARY
                     NAMES "${LIBRARY_PREFIX}${library}${LIBRARY_SUFFIX}"
                     HINTS ENV LIBRARY_PATH)
    endif ()
    if(${name}_INCLUDE_DIR AND ${name}_LIBRARY)
        set (HAVE_${name} ${${name}_LIBRARY})
        message (STATUS "Found ${name}  header: ${${name}_INCLUDE_DIR}/${header}")
        message (STATUS "Found ${name} library: ${${name}_LIBRARY}")
    endif()
endmacro()



#
# get all sub directories: (refaim@stackoverflow)
#
function(subdirlist result curdir)
    file(GLOB children RELATIVE ${curdir} ${curdir}/*)
    set(dirlist "")
    foreach(child ${children})
        if(IS_DIRECTORY ${curdir}/${child})
            set(dirlist ${dirlist} ${child})
        endif()
    endforeach()
    set(${result} ${dirlist} PARENT_SCOPE)
endfunction()



#
# standard join(list) -> string function
#
# usage:
# join (C_FLAG_LIST " " C_FLAG_STRING)
#
function(join list sep output)
    set(tmp "")
    foreach(val ${${list}})
        if (tmp STREQUAL "")
            set(tmp ${val})
        else()
            set(tmp "${tmp}${sep}${val}")
        endif()
    endforeach()
    set(${output} ${tmp} PARENT_SCOPE)
endfunction()


# usage:
# append(C_WARN_FLAGS " -Wall -Wextra")
#
function(append output)
    set(tmp ${${output}})
    foreach(val ${ARGN})
        set(tmp "${tmp}${val}")
    endforeach()
    set(${output} ${tmp} PARENT_SCOPE)
endfunction()

# Appends strings with spaces added automatically:
#
# usage:
# append_args(C_WARN_FLAGS "-Wall" "-Wextra")
#
function(append_args output)
    set(sep " ")
    set(tmp ${${output}})
    foreach(val ${ARGN})
        if ("${tmp}" STREQUAL "")
            set(tmp "${val}")
        else()
            set(tmp "${tmp}${sep}${val}")
        endif()
    endforeach()
    set(${output} ${tmp} PARENT_SCOPE)
endfunction()


include("${THIS_GLOBALS_CMAKE}") # get THIS_*_PERMISSIONS

#
# handle installation of a directory of python library code
# TODO: generate py->pyc here as well
#
function(install_python_lib_dir fromdir todir)
    install (DIRECTORY "${fromdir}/" DESTINATION "${todir}" FILE_PERMISSIONS ${THIS_LIBRARY_PERMISSIONS} FILES_MATCHING PATTERN "*.py")
    install (DIRECTORY "${fromdir}/" DESTINATION "${todir}" FILE_PERMISSIONS ${THIS_EXECUTABLE_PERMISSIONS} FILES_MATCHING PATTERN "*.pyc")
endfunction()
