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

# \author Chris Saunders

# update file with various build-time properties
# requires CONFIG_FILE SOURCE_FILE DEST_FILE
#
# expected CONFIG_FILE format is:
# "key1\tvalue1\n"
# "key2\tvalue2\n" ...
#

file (READ ${CONFIG_FILE} CONFIG_LINES)
STRING(REPLACE "\n" ";" CONFIG_LINES "${CONFIG_LINES}")
foreach (CONFIG_LINE ${CONFIG_LINES})
    STRING(REPLACE "\t" ";" CONFIG_LIST "${CONFIG_LINE}")
    list (GET CONFIG_LIST 0 PAIR0)
    list (GET CONFIG_LIST 1 PAIR1)
    set(${PAIR0} "${PAIR1}")
endforeach ()
configure_file(${SOURCE_FILE} ${DEST_FILE} @ONLY)
