from collections import OrderedDict
import re

from quast_libs.fastaparser import read_fasta, get_chr_lengths_from_fastafile

ploid_aligned = {}
dip_genome_by_chr = {}
length_of_haplotypes = {}

def fill_dip_dict_by_chromosomes(fasta_fpath):
    dip_genome_by_chr = {}
    for name, _ in read_fasta(fasta_fpath):
        haplotype = re.findall(r'(haplotype\d+)', name)[0]
        if haplotype not in dip_genome_by_chr.keys():
            dip_genome_by_chr[haplotype] = []
        dip_genome_by_chr[haplotype].append(name)
    return dict(sorted(dip_genome_by_chr.items()))

def get_haplotypes_len(fpath):
    length_of_haplotypes = {}
    chr_len_d = get_chr_lengths_from_fastafile(fpath)
    for key, val in dip_genome_by_chr.items():
        for chrom in val:
            length_of_haplotypes[key] = length_of_haplotypes.get(key, 0) + chr_len_d[chrom]
    return dict(sorted(length_of_haplotypes.items()))













