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
## author Roman Petrovski
##
################################################################################


foreach (THIS_DIR ${THIS_TEST_DIRS})
    message (STATUS "Testing access to ${THIS_DIR}...")
    set (TEST_DIR "${THIS_DIR}/test")
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E make_directory "${TEST_DIR}"
        RESULT_VARIABLE TMP_RESULT )
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E remove_directory "${TEST_DIR}")

    if (TMP_RESULT)
        message (STATUS "ERROR: Directory is not writeable: ${THIS_DIR}")
        message (STATUS "If you don't have administrator access to the "
                         "target installation location, please use --prefix "
                         "command-line option during configuration. "
                         "Please see 'configure --help' for all installer "
                         "command-line options.")
        message (FATAL_ERROR "ERROR: installation cannot continue")
    else ()
        message (STATUS "Directory is writeable: ${THIS_DIR}")
    endif ()
endforeach ()
