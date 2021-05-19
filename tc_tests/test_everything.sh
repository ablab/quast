#!/bin/bash

if [ $# -gt 0 ]; then
    python_interpreter=$1
else
    python_interpreter=python
fi

# based on https://stackoverflow.com/questions/59895/how-can-i-get-the-source-directory-of-a-bash-script-from-within-the-script-itsel
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"

CWD=`pwd`
cd $SCRIPT_DIR
for f in test_*.py; do
    $python_interpreter "$f";
    if [ ! $? -eq 0 ]; then exit 1; fi;
    echo;
    echo;
done
cd $CWD
