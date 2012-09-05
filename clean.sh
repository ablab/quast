#!/bin/bash
make -C libs/MUMmer3.23-osx/ clean
make -C libs/MUMmer3.23-linux/ clean
rm -f *.pyc libs/*.pyc
#rm -rf quast_results/results_* quast_results/latest
rm clean.sh~
