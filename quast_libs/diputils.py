import os
import subprocess

from quast_libs.fastaparser import read_fasta, get_chr_lengths_from_fastafile
from quast_libs import qconfig


ploid_aligned = {}
dip_genome_by_chr = {}
length_of_haplotypes = {}
homologous_chroms = {}

def execute(execute_that):
    PIPE = subprocess.PIPE
    p = subprocess.Popen(execute_that, shell=True, stdin=PIPE, stdout=PIPE, stderr=subprocess.STDOUT, close_fds=True)
    p.communicate()

def run_mash(fasta_fpath):
    tool_dirpath = os.path.join(qconfig.LIBS_LOCATION, 'mash/mash')
    run_mash = f'{tool_dirpath} dist -i {fasta_fpath} {fasta_fpath} > tmp_mash_res.txt'
    execute(run_mash)

    with open('tmp_mash_res.txt') as inf:
        for line in inf:
            line = line.strip('\n').split('\t')
            if line[0] == line[1]:
                continue
            if float(line[3]) < 0.05: # p-value
                if line[0] not in homologous_chroms.keys():
                    homologous_chroms[line[0]] = []
                homologous_chroms[line[0]].append(line[1])

    delete_tmp_file = 'rm -rf tmp_mash_res.txt'
    execute(delete_tmp_file)

def get_max_n_haplotypes(homologous_chroms):
    n_max_haplotypes = 0
    for key, val in homologous_chroms.items():
        if len(val) + 1 > n_max_haplotypes:
            n_max_haplotypes = len(val) + 1
    return n_max_haplotypes

def fill_dip_dict_by_chromosomes():
    check_added_chroms = []
    counter_haplotypes = 1

    for idx in range(get_max_n_haplotypes(homologous_chroms)):
        dip_genome_by_chr[f'haplotype_{idx+1}'] = []

    homologous_chroms_sorted = dict(sorted(homologous_chroms.items()))
    for chrom in homologous_chroms_sorted.keys():
        if chrom not in check_added_chroms:
            dip_genome_by_chr[f'haplotype_{counter_haplotypes}'].append(chrom)
            check_added_chroms.append(chrom)
            counter_haplotypes += 1
            for other_chr in homologous_chroms_sorted[chrom]:
                if other_chr not in check_added_chroms:
                    dip_genome_by_chr[f'haplotype_{counter_haplotypes}'].append(other_chr)
                    check_added_chroms.append(other_chr)
                    counter_haplotypes += 1
                else:
                    continue
        counter_haplotypes = 1
    return dict(sorted(dip_genome_by_chr.items()))

def get_haplotypes_len(fpath):
    chr_len_d = get_chr_lengths_from_fastafile(fpath)
    for key, val in dip_genome_by_chr.items():
        for chrom in val:
            length_of_haplotypes[key] = length_of_haplotypes.get(key, 0) + chr_len_d[chrom]
    return dict(sorted(length_of_haplotypes.items()))

def compare_aligns(align1, align2):
    return align2 in homologous_chroms[align1]
