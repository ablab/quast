if [ $1 ]
then QUAST_FOLDER=$1
else QUAST_FOLDER=.
fi

make -C $QUAST_FOLDER/libs/MUMmer3.23-osx/   clean >/dev/null 2>/dev/null
make -C $QUAST_FOLDER/libs/MUMmer3.23-linux/ clean >/dev/null 2>/dev/null
rm -f   $QUAST_FOLDER/libs/MUMmer3.23-osx/make.*
rm -f   $QUAST_FOLDER/libs/MUMmer3.23-linux/make.*

make -C $QUAST_FOLDER/libs/glimmer/src/ clean >/dev/null 2>/dev/null
rm -f   $QUAST_FOLDER/libs/glimmer/src/make.*

rm -f   $QUAST_FOLDER/libs/gage/*.class

rm -f   $QUAST_FOLDER/*.pyc
rm -f   $QUAST_FOLDER/libs/*.pyc
rm -f   $QUAST_FOLDER/libs/html_saver/*.pyc
rm -f   $QUAST_FOLDER/libs/site_packages/*/*.pyc

make -C $QUAST_FOLDER/libs/samtools clean >/dev/null 2>/dev/null
make -C $QUAST_FOLDER/libs/bowtie2 clean >/dev/null 2>/dev/null

rm -rf  $QUAST_FOLDER/libs/manta/build
rm -rf  $QUAST_FOLDER/libs/blast
