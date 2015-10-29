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


message (STATUS "Verifying target directories access")

include ("${THIS_GLOBALS_CMAKE}")
install(
    CODE "set(THIS_TEST_DIRS \"\${THIS_FULL_DATADIR}\"
                               \"\${THIS_FULL_BINDIR}\" \"\${THIS_FULL_LIBDIR}\"
                               \"\${THIS_FULL_LIBEXECDIR}\")"
    SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/checkTargetPathWritable.cmake"
)
