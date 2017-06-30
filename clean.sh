#!/usr/bin/env bash

if [ $1 ]
then QUAST_FOLDER=$1
else QUAST_FOLDER=.
fi

make -C $QUAST_FOLDER/quast_libs/MUMmer/ clean >/dev/null 2>/dev/null
rm -f   $QUAST_FOLDER/quast_libs/MUMmer/make.*

make -C $QUAST_FOLDER/quast_libs/glimmer/src/ clean >/dev/null 2>/dev/null
rm -f   $QUAST_FOLDER/quast_libs/glimmer/src/make.*

rm -f   $QUAST_FOLDER/*.pyc
rm -f   $QUAST_FOLDER/quast_libs/*.pyc
rm -f   $QUAST_FOLDER/quast_libs/html_saver/*.pyc
rm -f   $QUAST_FOLDER/quast_libs/site_packages/*/*.pyc

make -C $QUAST_FOLDER/quast_libs/bwa clean >/dev/null 2>/dev/null

rm -rf  $QUAST_FOLDER/quast_libs/gridss*
rm -rf  $QUAST_FOLDER/quast_libs/blast
