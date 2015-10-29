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
## Configuration file for the c++/bin subdirectory
##
## author Come Raczy
##
################################################################################

include(${THIS_CXX_EXECUTABLE_CMAKE})

file (GLOB THIS_PROGRAM_SOURCE_LIST [a-zA-Z0-9]*.cpp)

##
## Generic rule for all the other programs
##
foreach(THIS_PROGRAM_SOURCE ${THIS_PROGRAM_SOURCE_LIST})
    get_filename_component(THIS_PROGRAM ${THIS_PROGRAM_SOURCE} NAME_WE)
    set(THIS_APPLICATION_LIB ${THIS_PROJECT_NAME}_${THIS_PROGRAM})
    add_executable        (${THIS_PROGRAM} ${THIS_PROGRAM_SOURCE})
    target_link_libraries (${THIS_PROGRAM}  ${THIS_APPLICATION_LIB} ${THIS_AVAILABLE_LIBRARIES}
                           ${HTSLIB_LIBRARY} ${CMAKE_THREAD_LIBS_INIT} ${Boost_LIBRARIES}
                           ${THIS_ADDITIONAL_LIB})
    install(TARGETS ${THIS_PROGRAM} RUNTIME DESTINATION ${THIS_LIBEXECDIR})
endforeach()

