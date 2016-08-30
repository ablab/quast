#!/bin/bash

VERSION=`cat VERSION.txt`
NOW=$(date +"%d.%m.%Y %H:%M")

ARCHIVE_NAME=quast-$VERSION.tar.gz
QUAST_FOLDER=quast-$VERSION

mkdir -p release/$QUAST_FOLDER
cp -r quast_libs      release/$QUAST_FOLDER
cp -r test_data       release/$QUAST_FOLDER
cp *.py               release/$QUAST_FOLDER
cp *.html             release/$QUAST_FOLDER
cp *.txt              release/$QUAST_FOLDER
cp quast_ref.bib      release/$QUAST_FOLDER
cp install.sh         release/$QUAST_FOLDER
cp install_full.sh    release/$QUAST_FOLDER
echo Build $NOW    >> release/$QUAST_FOLDER/VERSION.txt

sh clean.sh release/$QUAST_FOLDER

rm -f   release/$QUAST_FOLDER/.gitignore
rm -fr  release/$QUAST_FOLDER/quast_results
rm -fr  release/$QUAST_FOLDER/quast_test_output
rm -fr  release/$QUAST_FOLDER/quast_libs/site_packages/*/test
rm -fr  release/$QUAST_FOLDER/quast_libs/site_packages/*/tests
rm -fr  release/$QUAST_FOLDER/quast_libs/blast/16S_RNA_blastdb

cd release
tar -pczf $ARCHIVE_NAME $QUAST_FOLDER
cd ..
mv release/$ARCHIVE_NAME .
rm -fr release
echo "Archive created: $ARCHIVE_NAME"
