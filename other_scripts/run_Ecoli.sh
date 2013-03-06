#!/bin/sh

# get dir where script is stored
cur_dir="$( cd "$( dirname "$0" )" && pwd )"
genes="$( cd "$( dirname "$0" )" && cd ../../../data/input/E.coli/genes && pwd )"
#BASE="$( cd "$( dirname "$0" )" && cd ../../.. && pwd )"

# running quality if genes exists
if [ -e "$genes/genes.txt" ]
then
    python $cur_dir/quast.py -R $genes/MG1655-K12.fasta -G $genes/genes.txt -O $genes/operons.txt "$@"
else
    python $cur_dir/quast.py "$@"
fi
