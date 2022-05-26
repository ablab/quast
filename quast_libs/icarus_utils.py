#!/usr/bin/python -O

############################################################################
# Copyright (c) 2015-2022 Saint Petersburg State University
# Copyright (c) 2011-2015 Saint Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################

from __future__ import with_statement

import os

from quast_libs import qconfig, qutils
from quast_libs.html_saver.html_saver import trim_ref_name


class Alignment:
    def __init__(self, name, start, end, unshifted_start=None, unshifted_end=None, is_rc=None,
                 start_in_contig=None, end_in_contig=None, position_in_ref=None, ref_name=None, idy=None, is_best_set=None):
        self.name = name
        self.start = start
        self.end = end
        self.unshifted_start = unshifted_start
        self.unshifted_end = unshifted_end
        self.is_rc = is_rc
        self.idy = idy
        self.start_in_contig = start_in_contig
        self.end_in_contig = end_in_contig
        self.position_in_ref = position_in_ref
        self.ref_name = ref_name
        self.is_best_set = is_best_set

        self.order = 0
        self.similar = False
        self.misassembled = False
        self.misassemblies = ''
        self.color = "#000000"
        self.vPositionDelta = 0
        self.ambiguous = False

    def length(self):
        return self.end - self.start

    def annotation(self):
        return self.name + "\n" + str(self.start) + "-" + str(self.end)

    def center(self):
        return (self.end + self.start) // 2

    def compare_inexact(self, alignment):
        if alignment.ref_name != self.ref_name:
            return False
        contig_len = abs(self.end - self.start)
        return abs(alignment.start - self.start) <= (qconfig.contig_len_delta * contig_len) and \
               abs(alignment.end - self.end) <= (qconfig.contig_len_delta * contig_len)

    def __hash__(self):
        return hash((self.name, self.start, self.end, self.start_in_contig, self.end_in_contig))


class Arc:
    def __init__(self, c1, c2):
        self.c1 = c1
        self.c2 = c2


class Assembly:
    def __init__(self, contigs_fpath, aligned_blocks, min_visualizer_length=0):
        self.fpath = contigs_fpath
        self.label = qconfig.assembly_labels_by_fpath[contigs_fpath]
        self.alignments = []
        self.misassembled_contig_ids = []
        self.contigs_by_ids = {}

        i = 0
        for block in aligned_blocks:
            if block.end - block.start < min_visualizer_length:
                continue

            block.order = i % 2
            i += 1

            c_id = block.name
            if c_id not in self.contigs_by_ids:
                self.contigs_by_ids[c_id] = Contig(c_id)

            if block.misassembled:
                self.misassembled_contig_ids.append(c_id)

            self.alignments.append(block)
            self.contigs_by_ids[c_id].alignments.append(len(self.alignments) - 1)

    def find(self, alignment):
        if alignment.length() < qconfig.min_similar_contig_size:
            return -1

        i = 0
        while i < len(self.alignments) and not alignment.compare_inexact(self.alignments[i]):
            i += 1

        if i == len(self.alignments):
            return -1
        else:
            return i

    def apply_color(self, settings):
        for block in self.alignments:
            block.vPositionDelta += settings.oddStep[block.order]
            if block.misassembled:
                if not block.similar:
                    block.color = settings.color_misassembled[block.order]
                else:
                    block.color = settings.color_misassembled_similar[block.order]
            else:
                block.vPositionDelta += settings.goodStep
                if not block.similar:
                    block.color = settings.color_correct[block.order]
                else:
                    block.color = settings.color_correct_similar[block.order]

    def draw_arcs(self, settings):
    #		print (self.misassembled)
    #		print (self.contigs.keys())
        for c_id in self.misassembled_contig_ids:
            if c_id not in self.contigs_by_ids:
                continue

            contig = self.contigs_by_ids[c_id]
            sortedBlocks = sorted(contig.alignments, key=lambda x: min(self.alignments[x].start_in_contig, self.alignments[x].end_in_contig))

            joinedAlignments = []
            currentStart = 0
            currentCStart = 0

            i = 0
            while i < len(sortedBlocks):
                block = sortedBlocks[i]

                currentStart = self.alignments[block].start
                currentCStart = min(self.alignments[block].start_in_contig, self.alignments[block].end_in_contig)

                while i < len(sortedBlocks) - 1 and abs(self.alignments[sortedBlocks[i]].end - self.alignments[
                    sortedBlocks[i + 1]].start) < settings.maxBlockGap and self.alignments[sortedBlocks[i]].is_rc == \
                        self.alignments[sortedBlocks[i + 1]].is_rc:
                    i += 1

                if self.alignments[sortedBlocks[i]].end - currentStart < settings.minConnectedBlock:
                    i += 1
                    continue

                joinedAlignments.append(
                    Alignment("", currentStart, self.alignments[sortedBlocks[i]].end, currentStart, self.alignments[sortedBlocks[i]].end,
                              self.alignments[sortedBlocks[i]].is_rc, currentCStart, self.alignments[sortedBlocks[i]].ref_name))
                i += 1

            i = 0
            while i < len(joinedAlignments) - 1:
                contig.arcs.append(Arc(joinedAlignments[i].center(), joinedAlignments[i + 1].center()))
                i += 1


class Assemblies:
    def __init__(self, contigs_fpaths,
                 lists_of_aligned_blocks,
                 max_pos, min_visualized_length=0):
        self.assemblies = []
        self.max_pos = max_pos

        for i, c_fpath in enumerate(contigs_fpaths):
            self.assemblies.append(
                Assembly(c_fpath, lists_of_aligned_blocks[i],
                         min_visualized_length))

    def find_similar(self):
        for i in range(0, len(self.assemblies)):
            # print("processing assembly " + str(i))
            order = 0
            for block_num in range(0, len(self.assemblies[i].alignments)):
                block = self.assemblies[i].alignments[block_num]
                if not block.is_best_set:
                    continue
                if block.similar:
                    order = (block.order + 1) % 2
                    continue

                total = 0
                sim_block_ids_within_asm = [-1 for jj in range(0, len(self.assemblies))]
                sim_block_ids_within_asm[i] = block_num

                for j in range(0, len(self.assemblies)):
                    if i == j:
                        continue

                    block_id = self.assemblies[j].find(block)
                    if block_id != -1 \
                        and block.misassembled == \
                            self.assemblies[j].alignments[block_id].misassembled:
                        sim_block_ids_within_asm[j] = block_id
                        total += 1

                if total < get_similar_threshold(len(self.assemblies)):
                    continue

                for j in range(0, len(self.assemblies)):
                    block_id = sim_block_ids_within_asm[j]
                    if block_id == -1:
                        continue
                    self.assemblies[j].alignments[block_id].similar = True
                    self.assemblies[j].alignments[block_id].order = order

                order = (order + 1) % 2

    def draw_arcs(self, settings):
        for a in self.assemblies:
            a.draw_arcs(settings)

    def apply_colors(self, settings):
        for a in self.assemblies:
            a.apply_color(settings)

    def find_max_pos(self):
        max_pos = 0
        for asm in self.assemblies:
            asm_max_pos = asm.alignments[len(asm.alignments) - 1].end
            if max_pos < asm_max_pos:
                max_pos = asm_max_pos
        self.max_pos = max_pos
        return max_pos


class Contig:
    def __init__(self, name, size=None, contig_type=''):
        self.name = name
        self.size = size
        self.alignments = []
        self.contig_type = contig_type
        self.arcs = []
        self.genes = []


def get_similar_threshold(total):
    if total <= 2:
        return 1
    return total // 2

def format_long_numbers(number):
    return ''.join(reversed([x + (' ' if i and not i % 3 else '') for i, x in enumerate(reversed(str(number)))]))


def get_assemblies(contigs_fpaths, lists_of_aligned_blocks, virtual_genome_size=None, find_similar=True):
    min_visualizer_length = 0

    assemblies = Assemblies(
        contigs_fpaths,
        lists_of_aligned_blocks,
        virtual_genome_size,
        min_visualizer_length)

    if find_similar and assemblies is not None:
        assemblies.find_similar()

    return assemblies


def make_output_dir(output_dir_path):
    if not os.path.exists(output_dir_path):
        os.makedirs(output_dir_path)


def format_cov_data(chr, cov_data, cov_data_name, max_depth, max_depth_name):
    data = []
    data.append('var ' + cov_data_name + ' = {};')
    data.append('var ' + max_depth_name + ' = {};')
    if cov_data[chr]:
        chr_max_depth = max_depth[chr] if isinstance(max_depth, dict) else max_depth
        data.append(max_depth_name + '["' + chr + '"] = ' + str(chr_max_depth) + ';')
        data.append(cov_data_name + '["' + chr + '"] = [ ')
        line = ''
        for i, e in enumerate(cov_data[chr]):
            if i % 100 == 0:
                data.append(line)
                line = ''
            line += str(e) + ','
        data.append(line)
        data[-1] = data[-1][:-1] + '];'
    return data


def get_html_name(chr, chr_full_names):
    if len(chr_full_names) == 1:
        return qconfig.one_alignment_viewer_name
    return trim_ref_name(chr)


def get_info_by_chr(chr, aligned_bases_by_chr, chr_sizes, contigs_fpaths, contig_names_by_refs, one_chromosome=False):
    if one_chromosome:
        html_name = qconfig.one_alignment_viewer_name
        chr_link = os.path.join(qconfig.icarus_dirname, html_name + '.html')
    else:
        chr_link = os.path.join(qconfig.icarus_dirname, trim_ref_name(chr) + '.html')
    chr_name = chr.replace('_', ' ')
    tooltip = ''
    if len(chr_name) > 70:
        short_name = chr[:70]
        tooltip = chr_name
        chr_name = short_name + '...'
    aligned_lengths = [aligned_len for aligned_len in aligned_bases_by_chr[chr] if aligned_len is not None]
    chr_genome = sum(aligned_lengths) * 100.0 / (chr_sizes[chr] * len(contigs_fpaths))
    chr_size = chr_sizes[chr]
    return chr_link, chr_name, chr_genome, chr_size, tooltip


def group_references(chr_names, contig_names_by_refs, chromosomes_length, ref_fpath):
    if contig_names_by_refs:
        added_refs = set()
        chr_full_names = [added_refs.add(ref) or ref for ref in [contig_names_by_refs[contig] for contig in chr_names]
                          if ref not in added_refs]
    elif sum(chromosomes_length.values()) < qconfig.MAX_SIZE_FOR_COMB_PLOT and len(chr_names) > 1:
        chr_full_names = [qutils.name_from_fpath(ref_fpath)]
    else:
        contig_names_by_refs = dict()
        chr_full_names = chr_names
        for i in range(len(chr_names)):
            contig_names_by_refs[chr_names[i]] = chr_full_names[i]
    return chr_full_names, contig_names_by_refs


def check_misassembled_blocks(aligned_blocks, misassembled_id_to_structure, filter_local=False):
    for alignment in aligned_blocks:
        if not alignment.is_best_set:  # alignment is not in the best set
            continue
        contig_structure = misassembled_id_to_structure[alignment.name]
        for num_alignment, el in enumerate(contig_structure):
            if isinstance(el, Alignment):
                if el.start == alignment.start and el.end == alignment.end:
                    break
        misassembled = False
        if type(contig_structure[num_alignment - 1]) == str:
            misassembly_type = contig_structure[num_alignment - 1].split(',')[0].strip()
            if is_misassembly_real(misassembly_type, filter_local):
                misassembled = True
        if num_alignment + 1 < len(contig_structure) and \
                        type(contig_structure[num_alignment + 1]) == str:
            misassembly_type = contig_structure[num_alignment + 1].split(',')[0].strip()
            if is_misassembly_real(misassembly_type, filter_local):
                misassembled = True
        alignment.misassembled = misassembled
    return aligned_blocks


def parse_misassembly_info(misassembly):
    ms_description = misassembly
    ms_type = 'real'
    if qconfig.large_genome and ('local' in ms_description or 'fake' in ms_description or 'indel' in ms_description):
        ms_type = 'skip'
    elif not is_misassembly_real(ms_description):
        if 'unknown' in ms_description:
            return 'unknown', 'unknown'
        if 'fake' in ms_description:
            ms_type = 'fake'
        elif 'indel' in ms_description:
            ms_type = 'skip'
        ms_description = ms_description.split(':')[1]
    return ms_description, ms_type


def get_misassembly_for_alignment(contig_structure, alignment):
    misassembled_ends = ''
    misassemblies = []
    for num_alignment, el in enumerate(contig_structure):
        if isinstance(el, Alignment):
            if el.start == alignment.start and el.end == alignment.end and el.start_in_contig == alignment.start_in_contig:
                break
    is_misassembly = False
    if type(contig_structure[num_alignment - 1]) == str:
        misassembly_type = contig_structure[num_alignment - 1].split(',')[0].strip()
        is_misassembly = is_misassembly_real(misassembly_type)
        if is_misassembly:
            if 'local' in misassembly_type:
                misassembly_type = 'local'
            misassemblies.append(misassembly_type)
            if alignment.start_in_contig < alignment.end_in_contig:
                misassembled_ends += 'L'
            else:
                misassembled_ends += 'R'
    if not is_misassembly:
        misassemblies.append('')
    is_misassembly = False
    if num_alignment + 1 < len(contig_structure) and \
                    type(contig_structure[num_alignment + 1]) == str:
        misassembly_type = contig_structure[num_alignment + 1].split(',')[0].strip()
        is_misassembly = is_misassembly_real(misassembly_type)
        if is_misassembly:
            if 'local' in misassembly_type:
                misassembly_type = 'local'
            misassemblies.append(misassembly_type)
            if alignment.start_in_contig < alignment.end_in_contig:
                misassembled_ends += ';' + 'R'
            else:
                misassembled_ends += ';' + 'L'
    if not is_misassembly:
        misassemblies.append('')
    return misassemblies, misassembled_ends


def is_misassembly_real(misassembly_type, filter_local=False):
    if filter_local and 'local' in misassembly_type:
        return False
    return 'fake' not in misassembly_type and 'indel' not in misassembly_type