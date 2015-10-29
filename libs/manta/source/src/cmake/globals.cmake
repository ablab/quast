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
## CMake configuration file to identify the configuration of the system
##
## author Roman Petrovski
##
################################################################################

set(THIS_EXECUTABLE_PERMISSIONS OWNER_WRITE OWNER_READ GROUP_READ WORLD_READ OWNER_EXECUTE GROUP_EXECUTE WORLD_EXECUTE)
set(THIS_LIBRARY_PERMISSIONS OWNER_WRITE OWNER_READ GROUP_READ WORLD_READ)

install(CODE "

    # With package generator, the location where files are placed is not the location where they will be run.
    # _FULL_ variables are guaranteed valid only at runtime
    set (CPACK_GENERATOR \"${CPACK_GENERATOR}\")
    set (FULL_PREFIX \"\")
    if (NOT CPACK_GENERATOR)
        set (FULL_PREFIX \"$ENV{DESTDIR}\")
    endif()

    get_filename_component(THIS_FULL_DATADIR        \"\${FULL_PREFIX}${THIS_DATADIR}\" ABSOLUTE)
    get_filename_component(THIS_FULL_BINDIR         \"\${FULL_PREFIX}${THIS_BINDIR}\" ABSOLUTE)
    get_filename_component(THIS_FULL_LIBDIR         \"\${FULL_PREFIX}${THIS_LIBDIR}\" ABSOLUTE)
    get_filename_component(THIS_FULL_LIBEXECDIR     \"\${FULL_PREFIX}${THIS_LIBEXECDIR}\" ABSOLUTE)
    get_filename_component(THIS_FULL_PYTHON_LIBDIR  \"\${FULL_PREFIX}${THIS_PYTHON_LIBDIR}\" ABSOLUTE)
    ")

