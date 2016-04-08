#!/bin/bash

VERSION=`cat VERSION.txt`
NOW=$(date +"%d.%m.%Y %H:%M")

ARCHIVE_NAME=quast-$VERSION.tar.gz
QUAST_FOLDER=quast-$VERSION
SITE_PACKAGES=libs/site_packages

mkdir -p release/$QUAST_FOLDER
cp -r libs            release/$QUAST_FOLDER
cp -r test_data       release/$QUAST_FOLDER
cp quast.py           release/$QUAST_FOLDER
cp metaquast.py       release/$QUAST_FOLDER
cp manual.html        release/$QUAST_FOLDER
cp VERSION.txt        release/$QUAST_FOLDER
cp LICENSE.txt        release/$QUAST_FOLDER
cp CHANGES.txt        release/$QUAST_FOLDER
cp README.txt         release/$QUAST_FOLDER
cp INSTALL.txt        release/$QUAST_FOLDER
cp install.sh         release/$QUAST_FOLDER
cp install_full.sh    release/$QUAST_FOLDER
echo Build $NOW    >> release/$QUAST_FOLDER/VERSION.txt

sh clean.sh release/$QUAST_FOLDER

rm -f   release/$QUAST_FOLDER/libs/.gitignore
rm -fr  release/$QUAST_FOLDER/quast_results
rm -fr  release/$QUAST_FOLDER/quast_test_output
rm -fr  release/$QUAST_FOLDER/$SITE_PACKAGES/*/test
rm -fr  release/$QUAST_FOLDER/$SITE_PACKAGES/*/tests
rm -fr  release/$QUAST_FOLDER/libs/blast/16S_RNA_blastdb

cd release
tar -pczf $ARCHIVE_NAME $QUAST_FOLDER
cd ..
mv release/$ARCHIVE_NAME .
rm -fr release
echo "Archive created: $ARCHIVE_NAME"
