from quast_libs.fastaparser import read_fasta

class DipQuastAnalyzer:
    def __init__(self):
        self.dip_genome_by_chr = {}
        self.dip_genome_by_chr_len = {}
        self.genome_size_by_haplotypes = {}
        self.__remember_haplotypes = []
    def fill_dip_dict_by_chromosomes(self, fasta_fpath):
        for name, seq in read_fasta(fasta_fpath):
            chr_name, haplotype = name.strip('\n').split('_')
            chr_len = len(seq)
            if haplotype not in self.dip_genome_by_chr_len.keys():
                self.dip_genome_by_chr_len[haplotype] = {}
                self.dip_genome_by_chr[haplotype] = {}
                self.__remember_haplotypes.append(haplotype)
            self.dip_genome_by_chr_len[haplotype][chr_name] = chr_len
            self.dip_genome_by_chr[haplotype][chr_name] = seq

        for haplotype_n in self.__remember_haplotypes:
            self.genome_size_by_haplotypes[haplotype_n] = sum(self.dip_genome_by_chr_len[haplotype_n].values())

        return self.dip_genome_by_chr, self.genome_size_by_haplotypes







