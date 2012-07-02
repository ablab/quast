#!/bin/bash
mkdir release/quast
cp -r libs            release/quast
cp release/quality.py release/quast
cp release/README     release/quast
cp quast.py           release/quast
cp manual.html        release/quast

make -C release/quast/libs/MUMmer3.23-osx/ clean
make -C release/quast/libs/MUMmer3.23-linux/ clean
rm -f   release/quast/libs/*.pyc
rm -rf  release/quast/results_* 
rm -rf	release/quast/libs/mauve
rm -rf	release/quast/libs/genemark_suite_linux_64

cd release
tar -pczf quast.tar.gz quast
cd ..
mv release/quast.tar.gz .
rm -rf release/quast
