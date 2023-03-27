from collections import OrderedDict
import os
import re
import subprocess

from quast_libs.fastaparser import read_fasta, get_chr_lengths_from_fastafile
from quast_libs import qconfig


ploid_aligned = {}
dip_genome_by_chr = {}
length_of_haplotypes = {}
homologous_chroms = {}

def fill_dip_dict_by_chromosomes(fasta_fpath):
    # dip_genome_by_chr = {}
    for name, _ in read_fasta(fasta_fpath):
        haplotype = re.findall(r'(haplotype\d+)', name)[0]
        if haplotype not in dip_genome_by_chr.keys():
            dip_genome_by_chr[haplotype] = []
        dip_genome_by_chr[haplotype].append(name)
    return dict(sorted(dip_genome_by_chr.items()))

def get_haplotypes_len(fpath):
    # length_of_haplotypes = {}
    chr_len_d = get_chr_lengths_from_fastafile(fpath)
    for key, val in dip_genome_by_chr.items():
        for chrom in val:
            length_of_haplotypes[key] = length_of_haplotypes.get(key, 0) + chr_len_d[chrom]
    return dict(sorted(length_of_haplotypes.items()))

def compare_aligns(align1, align2):
    return align2 in homologous_chroms[align1]

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













