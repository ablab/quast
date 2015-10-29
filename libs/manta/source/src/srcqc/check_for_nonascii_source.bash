#!/usr/bin/env bash
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

set -o nounset
set -o pipefail

reltoabs() {
    (cd $1; pwd -P)
}

scriptdir=$(reltoabs $(dirname $0))
srcdir=$(reltoabs $scriptdir/..)


if [ $# != 0 ]; then
    cat <<EOF

usage: $0

check for non-ascii characters in all source files. If found the filename, line number and a visual highlight within each line will be displayed

EOF
    exit 2
fi


find_cmake_source() {
    base_dir=$1
    find $base_dir -type f \
        -name "*.cmake" -or \
        -name "CMakeLists.txt"
}

find_cxx_source() {
    base_dir=$1
    find $base_dir -type f \
        -name "*.cpp" -or \
        -name "*.c" -or \
        -name "*.hh" -or \
        -name "*.h"
}

find_script_source() {
    base_dir=$1
    find $base_dir -type f \
        -name "*.bash" -or \
        -name "*.sh" -or \
        -name "configure" -or \
        -name "*.py"
}

get_source() {
    for f in $srcdir/*; do
        dir=$(basename $f)
        if [ $dir == "submodule" ]; then continue; fi
        find_cmake_source $f
        find_cxx_source $f
        find_script_source $f
    done
}

is_error=false
for f in $(get_source); do
    #echo "checking: $f"

    # not portable to OS X:
    #grep --color='auto' -n -H -P "[\x80-\xFF]" $f

    # note literal space and tab character in match pattern:
    #
    LC_ALL=C grep --color='auto' -n -H "[^ -~	]" $f
    if [ $? != 1 ]; then
        is_error=true
    fi
done

if $is_error; then
    echo "ERROR: source contains non-ascii or non-printing characters" 1>&2
    exit 1
fi
