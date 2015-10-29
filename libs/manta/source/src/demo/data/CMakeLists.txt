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
## file CMakeLists.txt
##
################################################################################

set(FA_FILE "Homo_sapiens_assembly19.COST16011_region.fa")
set(FA_ARCHIVE_FILE ${CMAKE_CURRENT_SOURCE_DIR}/${FA_FILE}.tar.bz2)
execute_process(
    COMMAND ${CMAKE_COMMAND} -E tar xjf ${FA_ARCHIVE_FILE}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )

file (GLOB DATA_LIST *)
list (REMOVE_ITEM DATA_LIST ${FA_ARCHIVE_FILE})
list (REMOVE_ITEM DATA_LIST ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt)
list (APPEND DATA_LIST ${CMAKE_CURRENT_BINARY_DIR}/${FA_FILE})

install(FILES ${DATA_LIST} DESTINATION ${THIS_MANTADEMODIR}/data)

