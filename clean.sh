#!/bin/bash
make -C libs/MUMmer3.23-osx/ clean
make -C libs/MUMmer3.23-linux/ clean
rm -f *.pyc libs/*.pyc show-snps.err
rm -rf results_* latest
rm clean.sh~
