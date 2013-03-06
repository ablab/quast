#!/bin/sh

# get dir where script is stored
cur_dir="$( cd "$( dirname "$0" )" && pwd )"
genes="$( cd "$( dirname "$0" )" && cd ../../../data/input/S.aureus/genes && pwd )"

# running quality if genes exists
if [ -e "$genes/bacteria_genes.txt" ]
then
    python $cur_dir/quast.py -R $genes/USA300_FPR3757.fasta -G $genes/bacteria_genes.txt "$@"
else
    python $cur_dir/quast.py "$@"
fi
