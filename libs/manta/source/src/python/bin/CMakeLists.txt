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

set(INSTALL_TO_DIR "${THIS_BINDIR}")

file(RELATIVE_PATH THIS_RELATIVE_PYTHON_LIBDIR "${INSTALL_TO_DIR}" "${THIS_PYTHON_LIBDIR}")

include ("${THIS_MACROS_CMAKE}")

configure_files("${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}" "*.py")
configure_files("${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}" "*.ini")

include("${THIS_GLOBALS_CMAKE}") # required for *_PERMISSIONS values

install_fileglob("${CMAKE_CURRENT_BINARY_DIR}" "${INSTALL_TO_DIR}" "*.py"
                 "${THIS_EXECUTABLE_PERMISSIONS}")
install_fileglob("${CMAKE_CURRENT_BINARY_DIR}" "${INSTALL_TO_DIR}" "*.ini"
                 "${THIS_LIBRARY_PERMISSIONS}")

