#! /bin/sh

echo "/* This script-generated file contains the PostScript prologues" > output
echo " * in a form suitable to inclusion in a C source code */" >> output
echo "" >> output

for i in `ls -1 *.ps | LC_ALL=C sort`; do
	echo $i > temp
	name=`sed -e 's/\.ps/_ps/g' -e 's/-/_/g' temp`
	rm temp
	echo "static const char *prologue_$name[] = {" >> output
	sed -e 's/"/\\"/g' -e 's/^/\"/g' -e 's/\t/\\t/g' -e 's/$/\\n\",/g' $i >> output
	echo "NULL" >> output
	echo "};" >> output
	echo "" >> output
done

cat output

rm output

