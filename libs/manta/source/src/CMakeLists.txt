#
# Manta - Structural Variant and Indel Caller
# Copyright (c) 2013-2016 Illumina, Inc.
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
## project src cmake
##
################################################################################


add_subdirectory (cmake/preInstall)

# force unit tests at compile time, and set them to be more verbose than
# cmake's default "test" target:
set(THIS_UNITTESTS "${THIS_PROJECT_NAME}_unittests")
set(utest_cmd ${CMAKE_CTEST_COMMAND} --force-new-ctest-process --output-on-failure)
add_custom_target(${THIS_UNITTESTS}
    ALL
    COMMAND ${utest_cmd}
    )

add_subdirectory (c++)
add_subdirectory (python)
add_subdirectory (demo)
add_subdirectory (cmake/postInstall)
