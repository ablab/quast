#!/bin/sh
MACHINE=`uname`
PROC=`uname -p`
SCRIPT_PATH=$0
SCRIPT_PATH=`dirname $SCRIPT_PATH`
JAVA_PATH=$SCRIPT_PATH:.
MUMMER_PATH=$SCRIPT_PATH/../MUMmer3.23-osx

if [ `uname -s` = "Linux" ]; then
    MUMMER_PATH=$SCRIPT_PATH/../MUMmer3.23-linux;
fi

REF=$1
CONTIGS=$2
OUTPUT_FOLDER=$3
MIN_CONTIG=$4
MIN_CONTIG=`expr $MIN_CONTIG - 1`

if [ ! -e $OUTPUT_FOLDER ]; then
    mkdir $OUTPUT_FOLDER
fi

CONTIG_FILE=$OUTPUT_FOLDER/$(basename $CONTIGS)

CUR_DIR=`pwd`
cd $SCRIPT_PATH
if [ ! -e GetFastaStats.class -o ! -e SizeFasta.class -o ! -e Utils.class ]; then
    javac GetFastaStats.java
    javac SizeFasta.java
    javac Utils.java
    if [ ! -e GetFastaStats.class -o ! -e SizeFasta.class -o ! -e Utils.class ]; then
        echo "Error occurred during compilation of java classes ($SCRIPT_PATH/*.java)! Try to compile them manually!" >&2
        exit 1       
    fi
fi
cd $MUMMER_PATH
if [ ! -e nucmer -o ! -e delta-filter -o ! -e dnadiff ]; then    
    echo "Compiling MUMmer"
    make >/dev/null 2>/dev/null        
    if [ ! -e nucmer -o ! -e delta-filter -o ! -e dnadiff ]; then    
        echo "Error occurred during MUMmer compilation ($MUMMER_PATH)! Try to compile it manually!" >&2
        exit 2
    fi
fi
cd $CUR_DIR

GENOMESIZE=`java -cp $JAVA_PATH SizeFasta $REF |awk '{SUM+=$NF; print SUM}'|tail -n 1`

echo "Contig Stats"
java -cp $JAVA_PATH GetFastaStats -o -min $MIN_CONTIG -genomeSize $GENOMESIZE $CONTIGS 2>/dev/null

$MUMMER_PATH/nucmer --maxmatch -p $CONTIG_FILE -l 30 -banded -D 5 $REF $CONTIGS
$MUMMER_PATH/delta-filter -o 95 -i 95 $CONTIG_FILE.delta > $CONTIG_FILE.fdelta
$MUMMER_PATH/dnadiff -d $CONTIG_FILE.fdelta -p $CONTIG_FILE

$SCRIPT_PATH/getMummerStats.sh $CONTIGS $SCRIPT_PATH $CONTIG_FILE $MIN_CONTIG
cat $CONTIG_FILE.1coords |awk '{print NR" "$5}' > $CONTIG_FILE.matches.lens

echo ""
echo "Corrected Contig Stats"
java -cp $JAVA_PATH:. GetFastaStats -o -min $MIN_CONTIG -genomeSize $GENOMESIZE $CONTIG_FILE.matches.lens 2> /dev/null

#if [ $DEBUG -eq 0 ]; then
#    rm -rf $OUTPUT_FOLDER
#fi
