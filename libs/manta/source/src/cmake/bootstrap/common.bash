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
## Definition of functions and variables common to all bootstrap scripts.
##
## author Come Raczy
##
################################################################################

# common log definition for bash installation scripts:
ilog() {
	echo -e $@ >&2
}


common_create_source () {
    if [[ ! -e $SOURCE_TARBALL ]] ; then
        ilog $SCRIPT: source tarball $SOURCE_TARBALL not found
        exit 1
    fi
    ilog Decompressing $SOURCE_TARBALL
    mkdir -p ${BUILD_DIR}
    tar -C ${BUILD_DIR} -${TARBALL_COMPRESSION}xf $SOURCE_TARBALL

    if [[ ! -d $SOURCE_DIR ]] ; then
        ilog $SOURCE_DIR does not exist
        exit 1
    fi
}
