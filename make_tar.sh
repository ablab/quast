#!/bin/bash

VERSION=`cat VERSION`
NOW=$(date +"%d.%m.%Y_%H:%M")
ARCHIVE_NAME=quast-$VERSION.tar.gz

mkdir release
mkdir release/quast
cp -r libs            release/quast
cp -r test_data       release/quast
cp quast.py           release/quast
cp manual.html        release/quast
cp VERSION            release/quast
echo Build $NOW    >> release/quast/VERSION
sed "s/RELEASE_MODE=False/RELEASE_MODE=True/" quast.py > release/quast/quast.py

make -C release/quast/libs/MUMmer3.23-osx/   clean >/dev/null 2>/dev/null
make -C release/quast/libs/MUMmer3.23-linux/ clean >/dev/null 2>/dev/null
rm -f   release/quast/libs/*.pyc
rm -rf  release/quast/results_*
rm -rf	release/quast/libs/mauve
rm -rf	release/quast/libs/genemark_suite_linux_64

cd release
tar -pczf $ARCHIVE_NAME quast
cd ..
mv release/$ARCHIVE_NAME .
rm -rf release
echo "Archive created: $ARCHIVE_NAME"