#!/bin/bash

if [ $# -gt 0 ]; then
    python_interpreter=$1
else
    python_interpreter=python2.6
fi

sh ../clean.sh

for f in test_*.py; do
    $python_interpreter "$f";
    if [ ! $? -eq 0 ]; then exit 1; fi;
    echo;
    echo;
    done
