import os
import subprocess

from quast_libs.fastaparser import get_chr_lengths_from_fastafile
from quast_libs import qconfig

ploid_aligned = {}
dip_genome_by_chr = {}
length_of_haplotypes = {}
homologous_chroms = {}
l_names_ambiguity_contigs = []


def execute(execute_that):
    PIPE = subprocess.PIPE
    p = subprocess.Popen(execute_that, shell=True, stdin=PIPE, stdout=PIPE, stderr=subprocess.STDOUT, close_fds=True)
    p.communicate()


def run_mash(fasta_fpath):
    tool_dirpath = os.path.join(qconfig.LIBS_LOCATION, 'mash/mash')
    mash_command = f'{tool_dirpath} dist -i {fasta_fpath} {fasta_fpath} > tmp_mash_res.txt'
    execute(mash_command)

    with open('tmp_mash_res.txt') as inf:
        for line in inf:
            line = line.strip('\n').split('\t')
            if line[0] == line[1]:
                continue
            if float(line[3]) < 0.05:  # p-value
                if line[0] not in homologous_chroms.keys():
                    homologous_chroms[line[0]] = []
                homologous_chroms[line[0]].append(line[1])
    os.remove('tmp_mash_res.txt')


def get_max_n_haplotypes():
    n_max_haplotypes = 0
    for key, val in homologous_chroms.items():
        if len(val) + 1 > n_max_haplotypes:
            n_max_haplotypes = len(val) + 1
    return n_max_haplotypes


def fill_dip_dict_by_chromosomes():
    check_added_chroms = []
    counter_haplotypes = 1
    for idx in range(get_max_n_haplotypes()):
        dip_genome_by_chr[f'haplotype_{idx + 1}'] = []

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


def is_homologous_ref(align1, align2):
    return align2 in homologous_chroms[align1]


def genome_coverage_for_unambiguity_contigs(ref_aligns, reference_chromosomes):
    genome_mapping = {}
    for chr_name, chr_len in reference_chromosomes.items():
        genome_mapping[chr_name] = [0] * (chr_len + 1)
    for chr_name, aligns in ref_aligns.items():
        for align in aligns:
            if align.contig not in l_names_ambiguity_contigs:
                if align.s1 < align.e1:
                    for pos in range(align.s1, align.e1 + 1):
                        genome_mapping[align.ref][pos] = 1
                else:
                    for pos in range(align.s1, len(genome_mapping[align.ref])):
                        genome_mapping[align.ref][pos] = 1
                    for pos in range(1, align.e1 + 1):
                        genome_mapping[align.ref][pos] = 1
    return genome_mapping


def find_ambiguity_alignments(ref_aligns):
    ambiguity_contigs = {}
    for key in ref_aligns.keys():
        if key not in ambiguity_contigs.keys():
            ambiguity_contigs[key] = []
        for contig in ref_aligns[key]:
            if contig.contig in l_names_ambiguity_contigs:
                ambiguity_contigs[key].append(contig)
    for key in ambiguity_contigs.keys():
        ambiguity_contigs[key] = sorted(ambiguity_contigs[key], key=lambda ctg: ctg.s1)
    return ambiguity_contigs


def find_contig_alignments_to_haplotypes(ambiguity_contigs_pos):
    alignments_by_contigs = {}
    for chrom in ambiguity_contigs_pos.keys():
        for align in ambiguity_contigs_pos[chrom]:
            if align.contig not in alignments_by_contigs.keys():
                alignments_by_contigs[align.contig] = []
            alignments_by_contigs[align.contig].append(align)
    return alignments_by_contigs


def get_coords_for_non_overlapping_seq(ambiguity_contigs, ctg_under_study):
    l_non_overlapping_pos = []
    coords_to_ignore = []
    coords_of_study_ctg = [[ctg_under_study.s1, ctg_under_study.e1]]  # [[start_coord, stop_coord],...]
    for study_ctg_coord in coords_of_study_ctg:
        for compared_ctg in ambiguity_contigs[ctg_under_study.ref]:
            if compared_ctg.contig != ctg_under_study.contig:
                if study_ctg_coord[0] >= compared_ctg.s1:  # example: start_interest 20 >= start_compared 10
                    if study_ctg_coord[0] <= compared_ctg.e1:
                        if study_ctg_coord[1] > compared_ctg.e1:
                            study_ctg_coord[0] = compared_ctg.e1 + 1
                        elif study_ctg_coord[1] <= compared_ctg.e1:
                            coords_to_ignore.append(study_ctg_coord)
                            break

                elif study_ctg_coord[0] < compared_ctg.s1:  # example: start_interest 20 < start_compared 30
                    if study_ctg_coord[1] >= compared_ctg.s1:
                        if study_ctg_coord[1] <= compared_ctg.e1:
                            study_ctg_coord[1] = compared_ctg.s1 - 1
                        elif study_ctg_coord[1] > compared_ctg.e1:
                            coords_of_study_ctg.append([study_ctg_coord[0], compared_ctg.s1 - 1])
                            coords_of_study_ctg.append([compared_ctg.e1 + 1, study_ctg_coord[1]])
                            coords_to_ignore.append(study_ctg_coord)
                            break

    for coord in coords_of_study_ctg:
        if coord not in coords_to_ignore:
            start_coord, stop_coord = coord
            l_non_overlapping_pos += list(set(range(start_coord, stop_coord + 1)))
    return l_non_overlapping_pos


def leave_best_alignment_for_ambiguity_contigs(ref_aligns, reference_chromosomes, ca_output):
    genome_mapping = genome_coverage_for_unambiguity_contigs(ref_aligns, reference_chromosomes)
    ambiguity_contigs = find_ambiguity_alignments(ref_aligns)
    alignments_by_contigs = find_contig_alignments_to_haplotypes(ambiguity_contigs)  # {ambiguity_contig_name: [alignment_haplotype_1, alignment_haplotype_2, ..., alignment_haplotype_n]}
    ca_output.stdout_f.write('\nNOTICE: to the analysis used just one alignment of ambiguity contigs to the best haplotype\n')
    ca_output.stdout_f.write('Best alignment for ambiguity contigs:\n')

    for contig in alignments_by_contigs.keys():
        l_of_alignment_to_not_the_best_haplotypes = []
        contribution_of_non_overlapping_seq = {}  # {ref_align: [contribution_of_contig_to_genome_mapping, len_align_to_ref]}
        for align in alignments_by_contigs[contig]:
            contribution_of_non_overlapping_seq[align.ref] = [0]
            l_non_overlapping_pos = get_coords_for_non_overlapping_seq(ambiguity_contigs, align)
            for position in l_non_overlapping_pos:
                if genome_mapping[align.ref][position] == 0:
                    contribution_of_non_overlapping_seq[align.ref][0] += 1
            contribution_of_non_overlapping_seq[align.ref].append(align.e1 - align.s1 + 1)

        ref_of_best_alignment_of_ctg, _ = sorted(contribution_of_non_overlapping_seq.items(), key=lambda x: x[1])[::-1][0]
        for align in alignments_by_contigs[contig]:
            if align.ref == ref_of_best_alignment_of_ctg:
                for pos in range(align.s1, align.e1 + 1):
                    genome_mapping[align.ref][pos] = 1
                ca_output.stdout_f.write(
                    f'\tContig {align.contig}: best alignment to {ref_of_best_alignment_of_ctg}. ')
            else:
                l_of_alignment_to_not_the_best_haplotypes.append(align.ref)
                for ambiguity_contig in ambiguity_contigs[align.ref]:
                    if ambiguity_contig.contig == contig:
                        ambiguity_contigs[align.ref].remove(ambiguity_contig)
                        break
                for ambiguity_contig in ref_aligns[align.ref]:
                    if ambiguity_contig.contig == contig:
                        ref_aligns[align.ref].remove(ambiguity_contig)
                        break
        alignment_to_not_the_best_haplotypes = ', '.join(l_of_alignment_to_not_the_best_haplotypes)
        ca_output.stdout_f.write(f'Skipping alignment to {alignment_to_not_the_best_haplotypes}.\n')
