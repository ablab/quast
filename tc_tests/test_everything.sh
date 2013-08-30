#!/bin/bash

python_interpreter=python2.5

sh ../clean.sh

for f in test_*.py; do
    $python_interpreter "$f";
    if [ ! $? -eq 0 ]; then exit 1; fi;
    echo;
    echo;
    done