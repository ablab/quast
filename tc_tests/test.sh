#!/bin/bash

python_interpreter=python2.5

data_dir=data
results_dir=$data_dir/results

if [ -e "$results_dir" ]
then
	rm -rf $results_dir
	mkdir $results_dir
fi

set -x

                                                $python_interpreter quast.py $contigs_1K_1 $contigs_1K_2 -o $results_dir/c1k1_c1k2

#if [ ! $? -eq 0 ]; then exit 1; fi; echo; echo; $python_interpreter quast.py $contigs_1K_1 $contigs_1K_2 -o $results_dir/c1k1_c1k2_R_m                -R $reference_1K -m
#if [ ! $? -eq 0 ]; then exit 1; fi; echo; echo; $python_interpreter quast.py $contigs_1K_1 $contigs_1K_2 -o $results_dir/c1k1_c1k2_R_m_gene-finding   -R $reference_1K -m --gene-finding

if [ ! $? -eq 0 ]; then exit 1; fi; echo; echo; $python_interpreter quast.py $contigs_1K_1 $contigs_1K_2 -o $results_dir/c1k1_c1k2_R 		 -R $reference_1K
if [ ! $? -eq 0 ]; then exit 1; fi; echo; echo; $python_interpreter quast.py $contigs_1K_1 $contigs_1K_2 -o $results_dir/c1k1_c1k2_G 		 -G $genes_1K
if [ ! $? -eq 0 ]; then exit 1; fi; echo; echo; $python_interpreter quast.py $contigs_1K_1 $contigs_1K_2 -o $results_dir/c1k1_c1k2_Ggff 	 -G $genes_gff_1K
if [ ! $? -eq 0 ]; then exit 1; fi; echo; echo; $python_interpreter quast.py $contigs_1K_1 $contigs_1K_2 -o $results_dir/c1k1_c1k2_Gncbi	 -G $genes_ncbi_1K
if [ ! $? -eq 0 ]; then exit 1; fi; echo; echo; $python_interpreter quast.py $contigs_1K_1 $contigs_1K_2 -o $results_dir/c1k1_c1k2_O 		 -O $operons_1K
if [ ! $? -eq 0 ]; then exit 1; fi; echo; echo; $python_interpreter quast.py $contigs_1K_1 $contigs_1K_2 -o $results_dir/c1k1_c1k2_R_G_O 	 -R $reference_1K -G $genes_1K -O $operons_1K
if [ ! $? -eq 0 ]; then exit 1; fi; echo; echo; $python_interpreter quast.py $contigs_1K_1 $contigs_1K_2 -o $results_dir/c1k1_c1k2_R_Gnbci_O -R $reference_1K -G $genes_ncbi_1K -O $operons_1K
if [ ! $? -eq 0 ]; then exit 1; fi; echo; echo; $python_interpreter quast.py $contigs_1K_1 $contigs_1K_2 -o $results_dir/c1k1_c1k2_R_Ggff_O  -R $reference_1K -G $genes_gff_1K  -O $operons_1K
if [ ! $? -eq 0 ]; then exit 1; fi; echo; echo; $python_interpreter quast.py $contigs_1K_1 $contigs_1K_2 -o $results_dir/c1k1_c1k2_R_G 		 -R $reference_1K -G $genes_1K
if [ ! $? -eq 0 ]; then exit 1; fi; echo; echo; $python_interpreter quast.py $contigs_1K_1 $contigs_1K_2 -o $results_dir/c1k1_c1k2_R_O 		 -R $reference_1K -G $operons_1K
if [ ! $? -eq 0 ]; then exit 1; fi; echo; echo; $python_interpreter quast.py $contigs_1K_1 $contigs_1K_2 -o $results_dir/c1k1_c1k2_G_O 		 -G $genes_1K     -O $operons_1K

if [ ! $? -eq 0 ]; then exit 1; fi; echo; echo; $python_interpreter quast.py $contigs_1K_1 $contigs_1K_2 -o $results_dir/c1k1_c1k2_R_G_allow-ambiguity_scaffolds_threads_mincluster  -R $reference_1K -G $genes_1K  --ambiguity-usage all --scaffolds --threads 5 --mincluster 50
if [ ! $? -eq 0 ]; then exit 1; fi; echo; echo; $python_interpreter quast.py $contigs_1K_1 $contigs_1K_2 -o $results_dir/c1k1_c1k2_R_G_O_min-contig 		  			             -R $reference_1K -G $genes_1K  --min-contig 200
if [ ! $? -eq 0 ]; then exit 1; fi; echo; echo; $python_interpreter quast.py $contigs_1K_1 $contigs_1K_2 -o $results_dir/c1k1_c1k2_R_G_O_gage 				 		                 -R $reference_1K -G $genes_1K  --gage
if [ ! $? -eq 0 ]; then exit 1; fi; echo; echo; $python_interpreter quast.py $contigs_1K_1 $contigs_1K_2 -o $results_dir/c1k1_c1k2_R_G_O_contig-thresholds 	                         -R $reference_1K -G $genes_1K  --contig-thresholds 200,500
if [ ! $? -eq 0 ]; then exit 1; fi; echo; echo; $python_interpreter quast.py $contigs_1K_1 $contigs_1K_2 -o $results_dir/c1k1_c1k2_R_G_genemark-thresholds_gene-finding              -R $reference_1K               --gene-finding --gene-thresholds 0,300
if [ ! $? -eq 0 ]; then exit 1; fi; echo; echo; $python_interpreter quast.py $contigs_1K_1 $contigs_1K_2 -o $results_dir/c1k1_c1k2_R_G_O_eukaryote  		                         -R $reference_1K -G $genes_1K  --gene-finding --eukaryote
if [ ! $? -eq 0 ]; then exit 1; fi; echo; echo; $python_interpreter quast.py $contigs_1K_1 $contigs_1K_2 -o $results_dir/c1k1_c1k2_R_G_O_j 	 			                             -R $reference_1K -G $genes_1K  -j
if [ ! $? -eq 0 ]; then exit 1; fi; echo; echo; $python_interpreter quast.py $contigs_1K_1 $contigs_1K_2 -o $results_dir/c1k1_c1k2_R_G_O_J-json_report 	                             -R $reference_1K -G $genes_1K  -J $results_dir/c1k1_c1k2_R_G_O_J/json_report

if [ ! $? -eq 0 ]; then exit 1; fi; echo; echo; $python_interpreter quast.py run_test_data/matched_unpaired.fasta                -R run_test_data/not_matched_iter1_unpaired_longest.fasta
if [ ! $? -eq 0 ]; then exit 1; fi; echo; echo; $python_interpreter quast.py run_test_data/matched_unpaired.fasta $contigs_1K_1  -R run_test_data/not_matched_iter1_unpaired_longest.fasta

echo "all tests passed!";
set -x
