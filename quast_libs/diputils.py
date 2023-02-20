from collections import OrderedDict
import re

from quast_libs.fastaparser import read_fasta, get_chr_lengths_from_fastafile

class DipQuastAnalyzer:
    ploid_aligned = {}
    def __init__(self):
        self._dip_genome_by_chr = OrderedDict()
        self._length_of_haplotypes = OrderedDict()

    def fill_dip_dict_by_chromosomes(self, fasta_fpath):
        for name, _ in read_fasta(fasta_fpath):
            haplotype = re.findall(r'(haplotype\d+)', name)[0]
            if haplotype not in self._dip_genome_by_chr.keys():
                self._dip_genome_by_chr[haplotype] = []
            self._dip_genome_by_chr[haplotype].append(name)

        return self._dip_genome_by_chr

    def get_haplotypes_len(self, fpath):
        chr_len_d = get_chr_lengths_from_fastafile(fpath)
        for key, val in self._dip_genome_by_chr.items():
            for chrom in val:
                self._length_of_haplotypes[key] = self._length_of_haplotypes.get(key, 0) + chr_len_d[chrom]

        return self._length_of_haplotypes













