#!/bin/bash

CPUS=$(grep -c bogomips /proc/cpuinfo)
CURL="curl"
GUNZIP="gzip -c"

RFAM="Rfam.seed"
RFAMURL="ftp://ftp.ebi.ac.uk/pub/databases/Rfam/CURRENT/${RFAM}.gz"
if [ ! -r "$RFAM" ]; then
  echo "Downloading: $RFAM"
  $CURL "$RFAMURL" | $GUNZIP -c > "$RFAM"
else
  echo "Using existing file: $RFAM"
fi

# 23S only as 16S is in RFAM
SILVA="SILVA_128_LSURef_tax_silva_full_align_trunc.fasta"
SILVAURL="http://www.arb-silva.de/fileadmin/silva_databases/current/Exports/${SILVA}.gz"
if [ ! -r "$SILVA" ]; then
  echo "Downloading: $SILVA"
  $CURL "$SILVAURL" | $GUNZIP -c > "$SILVA"
else
  echo "Using existing file: $SILVA"
fi

# this will write three files: LSU.Kingdom.aln
echo "Fixing and splitting SILVA data"
./fix-SILVA.pl --seed --type LSU "$SILVA"

# Prepare RFAM for fetches
echo "Indexing $RFAM"
rm -f "$RFAM.ssi"
esl-afetch --index "$RFAM"

echo "Fetching models..."

# Bact
echo "Bac"
esl-afetch "$RFAM" RF00001 > 5S.bac.aln
esl-reformat -r stockholm LSU.Bacteria.aln > 23S.bac.aln
esl-afetch "$RFAM" RF00177 > 16S.bac.aln

# Arch
echo "Arc"
esl-afetch "$RFAM" RF00001 > 5S.arc.aln
esl-afetch "$RFAM" RF00002 > 5_8S.arc.aln
esl-reformat -r stockholm LSU.Archaea.aln > 23S.arc.aln
esl-afetch "$RFAM" RF01959 > 16S.arc.aln

# Euk
echo "Euk"
esl-afetch "$RFAM" RF00001 > 5S.euk.aln
esl-afetch "$RFAM" RF00002 > 5_8S.euk.aln
esl-reformat -r stockholm LSU.Eukaryota.aln > 28S.euk.aln
esl-afetch "$RFAM" RF01960 > 18S.euk.aln

# Mito
FILE="12S.mito.aln"
if [ ! -r "$FILE" ]; then
  echo "Missing included $FILE file."
  exit 1
fi
FILE="16S.mito.aln"
if [ ! -r "$FILE" ]; then
  echo "Missing included $FILE file."
  exit 1
fi


for K in arc bac euk mito ; do
  for T in 5S 5_8S 12S 16S 23S 28S ; do 
    ID="$T.$K"
    if [ -r "$ID.aln" ]; then
      echo "*** $ID ***"
      hmmbuild --cpu $CPUS --rna -n "${T}_rRNA" $T.$K.hmm $T.$K.aln
    fi
  done
  cat *.$K.hmm > $K.hmm
  #rm -f *.$K.hmm
  #hmmpress -f $K.hmm    
done

echo "Databases ready, copy them to the barrnap db/ folder:"
ls -1 {arc,bac,euk,mito}.hmm

exit


# FOR THE FUTURE! --accurate mode using cmscan

for ID in $(cat MODELS) ; do

  echo "Extracting: $ID.aln"
  esl-afetch "$RFAM" $ID > $ID.aln

  echo "Building: $ID.hmm"
  rm -f $ID.hmm.h?? $ID.hmm
  hmmbuild --hand --rna $ID.hmm $ID.aln
  hmmpress $ID.hmm

  echo "Building: $ID.cm"
  rm -f $ID.cm.i?? $ID.cm
  cmbuild --hand -F $ID.cm $ID.aln
  cmcalibrate --cpu $CPUS $ID.cm
  cmpress $ID.cm

done

echo "Done."
