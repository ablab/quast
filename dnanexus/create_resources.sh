#!/bin/bash

# some variables
SRC=`pwd`/..
DST=`pwd`/resources/usr
VERSION=`cat ../VERSION`

if [ -e $DST ]; then
    rm -r $DST    
fi
mkdir -p $DST

cd $SRC
./make_tar.sh >/dev/null
cp quast-$VERSION.tar.gz $DST
cd $DST
tar xfz quast-$VERSION.tar.gz
mv quast-$VERSION bin

# removing unnecessary stuff
rm quast-$VERSION.tar.gz
rm bin/metaquast.py  # not supported yet
rm -r bin/test_data
rm -r bin/libs/MUMmer3.23-osx

# GeneMark can't be used on DNAnexus platform because of License limitations
rm -r bin/libs/genemark
rm -r bin/libs/metagenemark
sed "s/LICENSE_LIMITATIONS_MODE = False/LICENSE_LIMITATIONS_MODE = True/" $SRC/libs/genemark.py > bin/libs/genemark.py

# removing comments from several .js and .css
for f in bin/libs/html_saver/static/*.js 
do
    cpp -P $f > ${f}_tmp 2>/dev/null
    mv ${f}_tmp $f
done

