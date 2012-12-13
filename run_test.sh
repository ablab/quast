#!/bin/bash

python_interpreter=python2.5

data_dir=run_test_data
results_dir=$data_dir/results

if [ -e "$results_dir" ]
then
	rm -rf $results_dir
	mkdir $results_dir
fi

contigs_1K_1=$data_dir/SPAdes_contigs_1.fasta
contigs_1K_2=$data_dir/SPAdes_contigs_2.fasta
reference_1K=$data_dir/reference_1K.fa.gz
genes_1K=$data_dir/genes_1K.txt
genes_gff_1K=$data_dir/genes_1K.gff
genes_ncbi_1K=$data_dir/genes_1K.ncbi
operons_1K=$data_dir/operons_1K.txt

set -x
#if [ ! -d "testenv" ]; then
#    easy_install pip
#    pip install virtualenv
#    virtualenv --python=$python_interpreter testenv
#fi

                                              $python_interpreter quast.py $contigs_1K_1 $contigs_1K_2 -o $results_dir/c1k1_c1k2
if [ $? -eq 1 ]; then exit 1; fi; echo; echo; $python_interpreter quast.py $contigs_1K_1 $contigs_1K_2 -o $results_dir/c1k1_c1k2_R 		     -R $reference_1K
if [ $? -eq 1 ]; then exit 1; fi; echo; echo; $python_interpreter quast.py $contigs_1K_1 $contigs_1K_2 -o $results_dir/c1k1_c1k2_G 		     -G $genes_1K
if [ $? -eq 1 ]; then exit 1; fi; echo; echo; $python_interpreter quast.py $contigs_1K_1 $contigs_1K_2 -o $results_dir/c1k1_c1k2_Ggff 	     -G $genes_gff_1K
if [ $? -eq 1 ]; then exit 1; fi; echo; echo; $python_interpreter quast.py $contigs_1K_1 $contigs_1K_2 -o $results_dir/c1k1_c1k2_Gncbi	     -G $genes_ncbi_1K
if [ $? -eq 1 ]; then exit 1; fi; echo; echo; $python_interpreter quast.py $contigs_1K_1 $contigs_1K_2 -o $results_dir/c1k1_c1k2_O 		     -O $operons_1K
if [ $? -eq 1 ]; then exit 1; fi; echo; echo; $python_interpreter quast.py $contigs_1K_1 $contigs_1K_2 -o $results_dir/c1k1_c1k2_R_G 		 -R $reference_1K -G $genes_1K
if [ $? -eq 1 ]; then exit 1; fi; echo; echo; $python_interpreter quast.py $contigs_1K_1 $contigs_1K_2 -o $results_dir/c1k1_c1k2_R_O 		 -R $reference_1K -G $operons_1K
if [ $? -eq 1 ]; then exit 1; fi; echo; echo; $python_interpreter quast.py $contigs_1K_1 $contigs_1K_2 -o $results_dir/c1k1_c1k2_G_O 		 -G $genes_1K     -O $operons_1K
if [ $? -eq 1 ]; then exit 1; fi; echo; echo; $python_interpreter quast.py $contigs_1K_1 $contigs_1K_2 -o $results_dir/c1k1_c1k2_R_G_O 	     -R $reference_1K -G $genes_1K      -O $operons_1K
if [ $? -eq 1 ]; then exit 1; fi; echo; echo; $python_interpreter quast.py $contigs_1K_1 $contigs_1K_2 -o $results_dir/c1k1_c1k2_R_Gnbci_O   -R $reference_1K -G $genes_ncbi_1K -O $operons_1K
if [ $? -eq 1 ]; then exit 1; fi; echo; echo; $python_interpreter quast.py $contigs_1K_1 $contigs_1K_2 -o $results_dir/c1k1_c1k2_R_Ggff_O 	 -R $reference_1K -G $genes_gff_1K  -O $operons_1K
if [ $? -eq 1 ]; then exit 1; fi; echo; echo; $python_interpreter quast.py $contigs_1K_1 $contigs_1K_2 -o $results_dir/c1k1_c1k2_R_G_O_min-contig 			 --min-contig 200 				-R $reference_1K -G $genes_ncbi_1K -O $operons_1K
if [ $? -eq 1 ]; then exit 1; fi; echo; echo; $python_interpreter quast.py $contigs_1K_1 $contigs_1K_2 -o $results_dir/c1k1_c1k2_R_G_O_gage 				 --gage 						-R $reference_1K -G $genes_ncbi_1K -O $operons_1K
if [ $? -eq 1 ]; then exit 1; fi; echo; echo; $python_interpreter quast.py $contigs_1K_1 $contigs_1K_2 -o $results_dir/c1k1_c1k2_R_G_O_contig-thresholds 	 --contig-thresholds 200,500 	-R $reference_1K -G $genes_ncbi_1K -O $operons_1K
if [ $? -eq 1 ]; then exit 1; fi; echo; echo; $python_interpreter quast.py $contigs_1K_1 $contigs_1K_2 -o $results_dir/c1k1_c1k2_R_G_O_genemark-thresholds   --genemark-thresholds 0,300 	-R $reference_1K
if [ $? -eq 1 ]; then exit 1; fi; echo; echo; $python_interpreter quast.py $contigs_1K_1 $contigs_1K_2 -o $results_dir/c1k1_c1k2_R_G_O_eukaryote  		 --eukaryote 				-R $reference_1K -G $genes_ncbi_1K -O $operons_1K
if [ $? -eq 1 ]; then exit 1; fi; echo; echo; $python_interpreter quast.py $contigs_1K_1 $contigs_1K_2 -o $results_dir/c1k1_c1k2_R_G_O_j 	 			-R $reference_1K -G $genes_1K -O $operons_1K -j
if [ $? -eq 1 ]; then exit 1; fi; echo; echo; $python_interpreter quast.py $contigs_1K_1 $contigs_1K_2 -o $results_dir/c1k1_c1k2_R_G_O_J-json_report 	-R $reference_1K -G $genes_1K -O $operons_1K -J $results_dir/c1k1_c1k2_R_G_O_J/json_report
if [ $? -eq 1 ]; then exit 1; fi; echo; echo; $python_interpreter quast.py run_test_data/matched_unpaired.fasta -R run_test_data/not_matched_iter1_unpaired_longest.fasta
if [ $? -eq 1 ]; then exit 1; fi; echo; echo; $python_interpreter quast.py run_test_data/matched_unpaired.fasta $contigs_1K_1 -R run_test_data/not_matched_iter1_unpaired_longest.fasta
if [ $? -eq 1 ]; then exit 1; fi;

set -x
