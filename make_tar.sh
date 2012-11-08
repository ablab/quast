#!/bin/bash

VERSION=`cat VERSION`
NOW=$(date +"%d.%m.%Y_%H:%M")
ARCHIVE_NAME=quast-$VERSION.tar.gz
QUAST_FOLDER=quast-$VERSION

mkdir release
mkdir release/$QUAST_FOLDER
cp -r libs            release/$QUAST_FOLDER
cp -r test_data       release/$QUAST_FOLDER
cp quast.py           release/$QUAST_FOLDER
cp manual.html        release/$QUAST_FOLDER
cp VERSION            release/$QUAST_FOLDER
cp LICENSE            release/$QUAST_FOLDER
cp CHANGES            release/$QUAST_FOLDER
echo Build $NOW    >> release/$QUAST_FOLDER/VERSION
sed "s/RELEASE_MODE=False/RELEASE_MODE=True/" quast.py > release/$QUAST_FOLDER/quast.py

make -C release/$QUAST_FOLDER/libs/MUMmer3.23-osx/   clean >/dev/null 2>/dev/null
make -C release/$QUAST_FOLDER/libs/MUMmer3.23-linux/ clean >/dev/null 2>/dev/null
rm -f   release/$QUAST_FOLDER/libs/MUMmer3.23-linux/make.*
rm -f   release/$QUAST_FOLDER/libs/MUMmer3.23-osx/make.*
rm -f   release/$QUAST_FOLDER/libs/gage/*.class
rm -f   release/$QUAST_FOLDER/libs/*.pyc
rm -f   release/$QUAST_FOLDER/libs/html_saver/*.pyc
rm -rf  release/$QUAST_FOLDER/quast_results
rm -rf	release/$QUAST_FOLDER/libs/mauve
rm -f   release/$QUAST_FOLDER/libs/.gitignore

cd release
tar -pczf $ARCHIVE_NAME $QUAST_FOLDER
cd ..
mv release/$ARCHIVE_NAME .
rm -rf release
echo "Archive created: $ARCHIVE_NAME"
