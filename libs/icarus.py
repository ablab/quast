#!/usr/bin/python -O

############################################################################
# Copyright (c) 2015-2016 Saint Petersburg State University
# Copyright (c) 2011-2015 Saint Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################


from __future__ import with_statement

from collections import defaultdict
try:
   from collections import OrderedDict
except ImportError:
   from site_packages.ordered_dict import OrderedDict

from libs import qconfig, qutils, fastaparser, genome_analyzer, contigs_analyzer
import os
import libs.html_saver.html_saver as html_saver
import libs.html_saver.json_saver as json_saver
from libs.svg_alignment_plotter import draw_alignment_plot

from libs import reporting
from libs.log import get_logger
logger = get_logger(qconfig.LOGGER_DEFAULT_NAME)

MAX_REF_NAME = 20
summary_fname = qconfig.icarus_html_fname
contigEdgeDelta = 0.05
minSimilarContig = 10000

def get_similar_threshold(total):
    if total <= 2:
        return 1
    return total // 2

def format_long_numbers(number):
    return ''.join(reversed([x + (' ' if i and not i % 3 else '') for i, x in enumerate(reversed(str(number)))]))


class Alignment:
    def __init__(self, name, start, end, unshifted_start, unshifted_end, is_rc, idy, start_in_contig,end_in_contig, position_in_ref, ref_name):
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

        self.order = 0
        self.similar = False
        self.misassembled = False
        self.color = "#000000"
        self.vPositionDelta = 0
        self.ambiguous = False

    def length(self):
        return self.end - self.start

    def annotation(self):
        return self.name + "\n" + str(self.start) + "-" + str(self.end)

    def center(self):
        return (self.end + self.start) / 2

    def compare_inexact(self, alignment):
        if alignment.ref_name != self.ref_name:
            return False
        contig_len = abs(self.end - self.start)
        return abs(alignment.start - self.start) <= (contigEdgeDelta * contig_len) and \
               abs(alignment.end - self.end) <= (contigEdgeDelta * contig_len)

    def __hash__(self):
        return hash((self.name, self.start, self.end, self.start_in_contig, self.end_in_contig))


class Arc:
    def __init__(self, c1, c2):
        self.c1 = c1
        self.c2 = c2


class Contig:
    def __init__(self, name, size=None, contig_type=''):
        self.name = name
        self.size = size
        self.alignments = []
        self.contig_type = contig_type
        self.arcs = []


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
        if alignment.length() < minSimilarContig:
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


def get_assemblies(contigs_fpaths, virtual_genome_size, lists_of_aligned_blocks, find_similar=True):
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


def do(contigs_fpaths, contig_report_fpath_pattern, output_dirpath, ref_fpath, cov_fpath=None, stdout_pattern=None,
       find_similar=True, features=None, json_output_dir=None):
    make_output_dir(output_dirpath)

    lists_of_aligned_blocks = []
    contigs_by_assemblies = OrderedDict()
    structures_by_labels = {}

    total_genome_size = 0
    reference_chromosomes = OrderedDict()
    assemblies = None
    chr_names = []
    features_data = None

    plot_fpath = None

    if ref_fpath:
        for name, seq in fastaparser.read_fasta(ref_fpath):
            chr_name = name.split()[0]
            chr_names.append(chr_name)
            chr_len = len(seq)
            total_genome_size += chr_len
            reference_chromosomes[chr_name] = chr_len
        virtual_genome_shift = 100
        sorted_ref_names = sorted(reference_chromosomes, key=reference_chromosomes.get, reverse=True)
        sorted_ref_lengths = sorted(reference_chromosomes.values(), reverse=True)
        cumulative_ref_lengths = [0]
        contig_names_by_refs = None
        if contigs_analyzer.ref_labels_by_chromosomes:
            contig_names_by_refs = contigs_analyzer.ref_labels_by_chromosomes

        for i, chr in enumerate(chr_names):
            chr_length = reference_chromosomes[chr]
            len_to_append = cumulative_ref_lengths[-1] + chr_length
            if contig_names_by_refs:
                if i < len(chr_names) - 1 and contig_names_by_refs[chr] != contig_names_by_refs[chr_names[i + 1]]:
                    len_to_append = 0
            cumulative_ref_lengths.append(len_to_append)
        virtual_genome_size = sum(reference_chromosomes.values()) + virtual_genome_shift * (len(reference_chromosomes.values()) - 1)

    for contigs_fpath in contigs_fpaths:
        label = qconfig.assembly_labels_by_fpath[contigs_fpath]
        if not contig_report_fpath_pattern:
            contigs = parse_contigs_fpath(contigs_fpath)
        else:
            report_fpath = contig_report_fpath_pattern % qutils.label_from_fpath_for_fname(contigs_fpath)
            aligned_blocks, misassembled_id_to_structure, contigs = parse_nucmer_contig_report(report_fpath,
                                                                        reference_chromosomes.keys(), cumulative_ref_lengths)
            if not contigs:
                contigs = parse_contigs_fpath(contigs_fpath)
            if aligned_blocks is None:
                return None
            for block in aligned_blocks:
                block.label = label
            aligned_blocks = check_misassembled_blocks(aligned_blocks, misassembled_id_to_structure)
            lists_of_aligned_blocks.append(aligned_blocks)
            structures_by_labels[label] = misassembled_id_to_structure
        contigs_by_assemblies[label] = contigs

    if contigs_fpaths and ref_fpath and features:
        features_data = parse_features_data(features, cumulative_ref_lengths, reference_chromosomes.keys())
    if reference_chromosomes and lists_of_aligned_blocks:
        assemblies = get_assemblies(contigs_fpaths, virtual_genome_size, lists_of_aligned_blocks, find_similar)
        if qconfig.draw_svg:
            plot_fpath = draw_alignment_plot(assemblies, virtual_genome_size, output_dirpath, sorted_ref_names, sorted_ref_lengths, virtual_genome_shift)
    if (assemblies or contigs_by_assemblies) and qconfig.create_icarus_html:
        icarus_html_fpath = js_data_gen(assemblies, contigs_fpaths, reference_chromosomes,
                    output_dirpath, structures_by_labels, ref_fpath=ref_fpath, stdout_pattern=stdout_pattern,
                    contigs_by_assemblies=contigs_by_assemblies, features_data=features_data, cov_fpath=cov_fpath,
                    json_output_dir=json_output_dir)
    else:
        icarus_html_fpath = None

    return icarus_html_fpath, plot_fpath


def check_misassembled_blocks(aligned_blocks, misassembled_id_to_structure):
    for alignment in aligned_blocks:
        contig_structure = misassembled_id_to_structure[alignment.name]
        for num_alignment, el in enumerate(contig_structure):
            if isinstance(el, Alignment):
                if el.start == alignment.start and el.end == alignment.end:
                    break
        misassembled = False
        if type(contig_structure[num_alignment - 1]) == str:
            misassembly_type = contig_structure[num_alignment - 1].split(',')[0].strip()
            if not 'fake' in misassembly_type:
                misassembled = True
        if num_alignment + 1 < len(contig_structure) and \
                        type(contig_structure[num_alignment + 1]) == str:
            misassembly_type = contig_structure[num_alignment + 1].split(',')[0].strip()
            if not 'fake' in misassembly_type:
                misassembled = True
        alignment.misassembled = misassembled
    return aligned_blocks


def parse_nucmer_contig_report(report_fpath, ref_names, cumulative_ref_lengths):
    aligned_blocks = []
    contigs = []

    with open(report_fpath) as report_file:
        misassembled_id_to_structure = defaultdict(list)
        contig_id = None

        start_col = None
        end_col = None
        start_in_contig_col = None
        end_in_contig_col = None
        ref_col = None
        contig_col = None
        idy_col = None
        ambig_col = None
        for i, line in enumerate(report_file):
            split_line = line.replace('\n', '').split('\t')
            if i == 0:
                start_col = split_line.index('S1')
                end_col = split_line.index('E1')
                start_in_contig_col = split_line.index('S2')
                end_in_contig_col = split_line.index('E2')
                ref_col = split_line.index('Reference')
                contig_col = split_line.index('Contig')
                idy_col = split_line.index('IDY')
                ambig_col = split_line.index('Ambiguous')
            elif split_line and split_line[0] == 'CONTIG':
                _, name, size, contig_type = split_line
                contig = Contig(name=name, size=int(size), contig_type=contig_type)
                contigs.append(contig)
            elif split_line and len(split_line) < 5:
                misassembled_id_to_structure[contig_id].append(line.strip())
            elif split_line and len(split_line) > 5:
                unshifted_start, unshifted_end, start_in_contig, end_in_contig, ref_name, contig_id, idy, ambiguity = \
                    split_line[start_col], split_line[end_col], split_line[start_in_contig_col], split_line[end_in_contig_col], \
                    split_line[ref_col], split_line[contig_col], split_line[idy_col], split_line[ambig_col]
                unshifted_start, unshifted_end, start_in_contig, end_in_contig = int(unshifted_start), int(unshifted_end),\
                                                                                 int(start_in_contig), int(end_in_contig)
                cur_shift = cumulative_ref_lengths[ref_names.index(ref_name)]
                start = unshifted_start + cur_shift
                end = unshifted_end + cur_shift

                is_rc = ((start - end) * (start_in_contig - end_in_contig)) < 0
                position_in_ref = unshifted_start
                block = Alignment(
                    name=contig_id, start=start, end=end, unshifted_start=unshifted_start, unshifted_end=unshifted_end, is_rc=is_rc,
                    idy=idy, start_in_contig=start_in_contig, end_in_contig=end_in_contig, position_in_ref=position_in_ref, ref_name=ref_name)
                block.ambiguous = ambiguity
                misassembled_id_to_structure[contig_id].append(block)

                aligned_blocks.append(block)

    return aligned_blocks, misassembled_id_to_structure, contigs


def parse_contigs_fpath(contigs_fpath):
    contigs = []
    for name, seq in fastaparser.read_fasta(contigs_fpath):
        contig = Contig(name=name, size=len(seq))
        contigs.append(contig)
    return contigs


def add_contig(cum_length, contig, not_used_nx, assemblies_n50, assembly, contigs, contig_size_lines, num, structures_by_labels,
               only_nx=False):
    end_contig = cum_length + contig.size
    marks = []
    align = None
    for nx in not_used_nx:
        if assemblies_n50[assembly][nx] == contig.size and \
                (num + 1 >= len(contigs) or contigs[num + 1].size != contig.size):
            marks.append(nx)
    marks = ', '.join(marks)
    if marks:
        contig_size_lines.append('{assembly: "' + assembly + '", corr_end: ' + str(end_contig) + ', label: "' + marks +
                                 '", size: ' + str(contig.size) + '}')
        not_used_nx = [nx for nx in not_used_nx if nx not in marks]
    marks = ', marks: "' + marks + '"' if marks else ''
    if not only_nx or marks:
        structure = []
        if structures_by_labels and assembly in structures_by_labels:
            assembly_structure = structures_by_labels[assembly]
            prev_pos = 0
            for el in assembly_structure[contig.name]:
                if isinstance(el, Alignment):
                    alignment_start = min(el.start_in_contig, el.end_in_contig)
                    if alignment_start - 1 != prev_pos:
                        structure.append('{type: "unaligned",contig: "' + contig.name + '",start_in_contig:' + str(prev_pos + 1) +
                                         ',end_in_contig:' + str(alignment_start - 1) + ',size: ' + str(contig.size) + '},')
                    prev_pos = max(el.start_in_contig, el.end_in_contig)
                    structure.append('{type: "A",contig: "' + contig.name + '",corr_start: ' + str(el.start) + ',corr_end: ' +
                                    str(el.end) + ',start:' + str(el.unshifted_start) + ',end:' + str(el.unshifted_end) +
                                    ',start_in_contig:' + str(el.start_in_contig) + ',end_in_contig:' +
                                    str(el.end_in_contig) + ',size: ' + str(contig.size) + ',chr: "' + el.ref_name + '"},')
                elif type(el) == str:
                    structure.append('{type: "M", mstype: "' + el + '"},')
            if prev_pos < contig.size * 0.95:
                structure.append('{type: "unaligned",contig: "' + contig.name + '",start_in_contig:' + str(prev_pos + 1) +
                                 ',end_in_contig:' + str(contig.size - 1) + ',size: ' + str(contig.size) + '},')

        align = '{name: "' + contig.name + '",size: ' + str(contig.size) + marks + ',type: "' + contig.contig_type + \
                '",structure: [' + ''.join(structure) + ']},'
    return end_contig, contig_size_lines, align, not_used_nx


def parse_cov_fpath(cov_fpath, chr_names, chr_full_names):
    if not cov_fpath:
        return None, None, None
    cov_data = defaultdict(list)
    #not_covered = defaultdict(list)
    max_depth = defaultdict(int)
    with open(cov_fpath, 'r') as coverage:
        contig_to_chr = dict()
        index_to_chr = dict()
        for chr in chr_full_names:
            if contigs_analyzer.ref_labels_by_chromosomes:
                contig_names_by_refs = contigs_analyzer.ref_labels_by_chromosomes
                contigs = [contig for contig in chr_names if contig_names_by_refs[contig] == chr]
            elif len(chr_full_names) == 1:
                contigs = chr_names
            else:
                contigs = [chr]
            for contig in contigs:
                contig_to_chr[contig] = chr
        for index, line in enumerate(coverage):
            fs = line.split()
            if line.startswith('#'):
                chr_name = fs[0][1:]
                index_to_chr[fs[1]] = chr_name
            else:
                name = contig_to_chr[index_to_chr[fs[0]]]
                depth = int(fs[1])
                max_depth[chr] = max(depth, max_depth[chr])
                cov_data[name].append(depth)
            # if c[2] == '0':
            #     not_covered[name].append(c[1])
    return cov_data, None, max_depth


def get_assemblies_data(contigs_fpaths, stdout_pattern, nx_marks):
    assemblies_n50 = defaultdict(dict)
    assemblies_data = ''
    assemblies_data += 'var assemblies_links = {};\n'
    assemblies_data += 'var assemblies_len = {};\n'
    assemblies_data += 'var assemblies_contigs = {};\n'
    assemblies_data += 'var assemblies_misassemblies = {};\n'
    assemblies_data += 'var assemblies_n50 = {};\n'
    assemblies_contig_size_data = ''
    for contigs_fpath in contigs_fpaths:
        label = qconfig.assembly_labels_by_fpath[contigs_fpath]
        report = reporting.get(contigs_fpath)
        l = report.get_field(reporting.Fields.TOTALLEN)
        contigs = report.get_field(reporting.Fields.CONTIGS)
        n50 = report.get_field(reporting.Fields.N50)
        if stdout_pattern:
            contig_stdout_fpath = stdout_pattern % qutils.label_from_fpath_for_fname(contigs_fpath) + '.stdout'
            assemblies_data += 'assemblies_links["' + label + '"] = "' + contig_stdout_fpath + '";\n'
        assemblies_contig_size_data += 'assemblies_len["' + label + '"] = ' + str(l) + ';\n'
        assemblies_contig_size_data += 'assemblies_contigs["' + label + '"] = ' + str(contigs) + ';\n'
        assemblies_contig_size_data += 'assemblies_n50["' + label + '"] = "' + str(n50) + '";\n'
        for nx in nx_marks:
            assemblies_n50[label][nx] = report.get_field(nx)
    return assemblies_data, assemblies_contig_size_data, assemblies_n50


def get_contigs_data(contigs_by_assemblies, nx_marks, assemblies_n50, structures_by_labels, contig_names_by_refs, ref_names,
                     one_html=True):
    additional_data = []
    additional_data.append('var links_to_chromosomes;')
    if not one_html:
        additional_data.append('links_to_chromosomes = {};')
        for ref_name in ref_names:
            chr_name = ref_name
            if contig_names_by_refs and ref_name in contig_names_by_refs:
                chr_name = contig_names_by_refs[ref_name]
            additional_data.append('links_to_chromosomes["' + ref_name + '"] = "' + chr_name + '";')
    contigs_sizes_str = ['var contig_data = {};']
    contigs_sizes_str.append('var chromosome;')
    contigs_sizes_lines = []
    total_len = 0
    min_contig_size = qconfig.min_contig
    too_many_contigs = False
    for assembly in contigs_by_assemblies:
        contigs_sizes_str.append('contig_data["' + assembly + '"] = [ ')
        cum_length = 0
        contigs = sorted(contigs_by_assemblies[assembly], key=lambda x: x.size, reverse=True)
        last_contig_num = min(len(contigs), qconfig.max_contigs_num_for_size_viewer)
        contig_threshold = contigs[last_contig_num - 1].size
        not_used_nx = [nx for nx in nx_marks]
        if len(contigs) > qconfig.max_contigs_num_for_size_viewer:
            too_many_contigs = True

        for i, alignment in enumerate(contigs):
            if i >= last_contig_num:
                break
            cum_length, contigs_sizes_lines, align, not_used_nx = add_contig(cum_length, alignment, not_used_nx, assemblies_n50,
                                                                assembly, contigs, contigs_sizes_lines, i, structures_by_labels)
            contigs_sizes_str.append(align)
        if len(contigs) > qconfig.max_contigs_num_for_size_viewer:
            assembly_len = cum_length
            remained_len = sum(alignment.size for alignment in contigs[last_contig_num:])
            cum_length += remained_len
            remained_contigs_name = str(len(contigs) - last_contig_num) + ' hidden contigs shorter than ' + str(contig_threshold) + \
                                    ' bp (total length: ' + format_long_numbers(remained_len) + ' bp)'
            contigs_sizes_str.append(('{name: "' + remained_contigs_name + '", size: ' + str(remained_len) +
                                     ', type:"small_contigs"},'))
        if not_used_nx and last_contig_num < len(contigs):
            for i, alignment in enumerate(contigs[last_contig_num:]):
                if not not_used_nx:
                    break
                assembly_len, contigs_sizes_lines, align, not_used_nx = add_contig(assembly_len, alignment, not_used_nx, assemblies_n50,
                                                                    assembly, contigs, contigs_sizes_lines, last_contig_num + i, structures_by_labels, only_nx=True)
        total_len = max(total_len, cum_length)
        contigs_sizes_str[-1] = contigs_sizes_str[-1][:-1] + '];\n\n'
    contigs_sizes_str = '\n'.join(contigs_sizes_str)
    contigs_sizes_str += 'var contigLines = [' + ','.join(contigs_sizes_lines) + '];\n\n'
    contigs_sizes_str += 'var contigs_total_len = ' + str(total_len) + ';\n'
    contigs_sizes_str += 'var minContigSize = ' + str(min_contig_size) + ';'
    contig_viewer_data = contigs_sizes_str + '\n'.join(additional_data)
    return contig_viewer_data, too_many_contigs


def parse_features_data(features, cumulative_ref_lengths, ref_names):
    features_data = 'var features_data;\n'
    if features:
        features_data += 'features_data = [ '
        containers_kind = []
        for feature_container in features:
            if len(feature_container.region_list) == 0:
                continue
            features_data += '[ '
            for region in feature_container.region_list:
                chr = region.chromosome if region.chromosome and region.chromosome in feature_container.chr_names_dict \
                    else region.seqname
                chr = feature_container.chr_names_dict[chr] if chr in feature_container.chr_names_dict else None
                if not chr or chr not in ref_names:
                    continue
                ref_id = ref_names.index(chr)
                name = region.name if region.name else ''
                region.start += cumulative_ref_lengths[ref_id]
                region.end += cumulative_ref_lengths[ref_id]
                features_data += '{name: "' + name + '", start: ' + str(region.start) + ', end: ' + str(region.end) + \
                                 ', id_: "' + region.id + '",kind: "' + feature_container.kind + '", chr:' + str(ref_id) + '},'
            containers_kind.append(feature_container.kind)
            features_data += '],'
        features_data = features_data[:-1] + '];\n'
    return features_data


def save_alignment_data_for_one_ref(chr, chr_full_names, ref_contigs, chr_lengths, data_str, chr_to_aligned_blocks,
                                    structures_by_labels, output_dir_path=None, json_output_dir=None, ref_data=None, features_data=None,
                                    assemblies_data=None, cov_data=None, not_covered=None, max_depth=None):
    html_name = chr
    if len(chr_full_names) == 1:
        html_name = qconfig.one_alignment_viewer_name

    alignment_viewer_template_fpath = html_saver.get_real_path(qconfig.icarus_viewers_template_fname)
    alignment_viewer_fpath = os.path.join(output_dir_path, html_name + '.html')
    html_saver.init_icarus(alignment_viewer_template_fpath, alignment_viewer_fpath)

    additional_assemblies_data = ''
    data_str.append('var links_to_chromosomes;')
    if contigs_analyzer.ref_labels_by_chromosomes:
        data_str.append('links_to_chromosomes = {};')
        links_to_chromosomes = []
        used_chromosomes = []
    num_misassemblies = 0
    aligned_assemblies = set()
    contig_names_by_refs = contigs_analyzer.ref_labels_by_chromosomes or None

    is_one_html = len(chr_full_names) == 1
    data_str.append('var oneHtml = "' + str(is_one_html) + '";')
    # adding assembly data
    data_str.append('var contig_data = {};')
    data_str.append('contig_data["' + chr + '"] = {};')
    assemblies_len = defaultdict(int)
    assemblies_contigs = defaultdict(set)
    ms_types = dict()
    for assembly in chr_to_aligned_blocks.keys():
        data_str.append('contig_data["' + chr + '"]["' + assembly + '"] = [ ')
        ms_types[assembly] = defaultdict(int)
        for num_contig, ref_contig in enumerate(ref_contigs):
            if ref_contig in chr_to_aligned_blocks[assembly]:
                overlapped_contigs = defaultdict(list)
                alignments = sorted(chr_to_aligned_blocks[assembly][ref_contig], key=lambda x: x.start)
                prev_end = 0
                prev_alignments = []
                for alignment in alignments:
                    if prev_end > alignment.start:
                        for prev_align in prev_alignments:
                            if prev_align.end - alignment.start > 100:
                                overlapped_contigs[prev_align].append('{contig: "' + alignment.name + '",corr_start: ' + str(alignment.start) +
                                    ',corr_end: ' + str(alignment.end) + ',start:' + str(alignment.unshifted_start) + ',end:' + str(alignment.unshifted_end) +
                                    ',start_in_contig:' + str(alignment.start_in_contig) + ',end_in_contig:' +
                                    str(alignment.end_in_contig) + ',chr: "' + alignment.ref_name + '"}')
                                overlapped_contigs[alignment].append('{contig: "' + prev_align.name + '",corr_start: ' + str(prev_align.start) +
                                    ',corr_end: ' + str(prev_align.end) + ',start:' + str(prev_align.unshifted_start) + ',end:' + str(prev_align.unshifted_end) +
                                    ',start_in_contig:' + str(prev_align.start_in_contig) + ',end_in_contig:' +
                                    str(prev_align.end_in_contig) + ',chr: "' + prev_align.ref_name + '"}')
                        prev_alignments.append(alignment)
                    else:
                        prev_alignments = [alignment]
                    prev_end = max(prev_end, alignment.end)

                for alignment in alignments:
                    if alignment.misassembled:
                        num_misassemblies += 1
                    assemblies_len[assembly] += abs(alignment.end_in_contig - alignment.start_in_contig) + 1
                    assemblies_contigs[assembly].add(alignment.name)
                    contig_structure = structures_by_labels[alignment.label]
                    misassembled_ends = []
                    for num_alignment, el in enumerate(contig_structure[alignment.name]):
                        if isinstance(el, Alignment):
                            if el.start == alignment.start and el.end == alignment.end:
                                break
                    alignment.misassemblies = ''
                    if type(contig_structure[alignment.name][num_alignment - 1]) == str:
                        misassembly_type = contig_structure[alignment.name][num_alignment - 1].split(',')[0].strip()
                        if not 'fake' in misassembly_type:
                            if 'local' in misassembly_type:
                                misassembly_type = 'local'
                            alignment.misassemblies += misassembly_type
                            ms_types[assembly][misassembly_type] += 1
                            if alignment.start_in_contig < alignment.end_in_contig:
                                misassembled_ends.append('L')
                            else:
                                misassembled_ends.append('R')
                    else: misassembled_ends.append('')
                    if num_alignment + 1 < len(contig_structure[alignment.name]) and \
                                    type(contig_structure[alignment.name][num_alignment + 1]) == str:
                        misassembly_type = contig_structure[alignment.name][num_alignment + 1].split(',')[0].strip()
                        if not 'fake' in misassembly_type:
                            if 'local' in misassembly_type:
                                misassembly_type = 'local'
                            alignment.misassemblies += ';' + misassembly_type
                            ms_types[assembly][misassembly_type] += 1
                            if alignment.start_in_contig < alignment.end_in_contig:
                                misassembled_ends.append('R')
                            else:
                                misassembled_ends.append('L')
                    else: misassembled_ends.append('')

                    if misassembled_ends: misassembled_ends = ';'.join(misassembled_ends)
                    else: misassembled_ends = ''
                    data_str.append('{name: "' + alignment.name + '",corr_start:' + str(alignment.start) +
                                    ', corr_end: ' + str(alignment.end) + ',start:' + str(alignment.unshifted_start) +
                                    ',end:' + str(alignment.unshifted_end) + ',similar:"' + ('True' if alignment.similar else 'False') +
                                    '", misassemblies:"' + alignment.misassemblies + '",mis_ends:"' + misassembled_ends + '"')
                    if alignment.ambiguous:
                        data_str[-1] += ', ambiguous: "True"'

                    aligned_assemblies.add(alignment.label)
                    if overlapped_contigs[alignment]:
                        data_str.append(',overlaps:[ ')
                        data_str.append(','.join(overlapped_contigs[alignment]))
                        data_str.append(']')
                    data_str.append(', structure: [ ')
                    for el in contig_structure[alignment.name]:
                        if isinstance(el, Alignment):
                            if el.ref_name in ref_contigs:
                                num_chr = ref_contigs.index(el.ref_name)
                                # corr_len = sum(chr_lengths[:num_chr+1])
                            else:
                                # corr_len = -int(el.end)
                                if contigs_analyzer.ref_labels_by_chromosomes and el.ref_name not in used_chromosomes:
                                    used_chromosomes.append(el.ref_name)
                                    new_chr = contig_names_by_refs[el.ref_name]
                                    links_to_chromosomes.append('links_to_chromosomes["' + el.ref_name + '"] = "' + new_chr + '";')
                            corr_el_start = el.start
                            corr_el_end = el.end
                            data_str.append('{type: "A",contig: "' + alignment.name + '",corr_start: ' + str(corr_el_start) + ',corr_end: ' +
                                            str(corr_el_end) + ',start:' + str(el.unshifted_start) + ',end:' + str(el.unshifted_end) +
                                            ',start_in_contig:' + str(el.start_in_contig) + ',end_in_contig:' +
                                            str(el.end_in_contig) + ',IDY:' + el.idy + ',chr: "' + el.ref_name + '"},')
                        elif type(el) == str:
                            data_str.append('{type: "M", mstype: "' + el + '"},')
                    data_str[-1] = data_str[-1][:-1] + ']},'

        data_str[-1] = data_str[-1][:-1] + '];'
        assembly_len = assemblies_len[assembly]
        assembly_contigs = len(assemblies_contigs[assembly])
        local_misassemblies = ms_types[assembly]['local'] / 2
        ext_misassemblies = (sum(ms_types[assembly].values()) - ms_types[assembly]['interspecies translocation']) / 2 - \
                            local_misassemblies + ms_types[assembly]['interspecies translocation']
        additional_assemblies_data += 'assemblies_len["' + assembly + '"] = ' + str(assembly_len) + ';\n'
        additional_assemblies_data += 'assemblies_contigs["' + assembly + '"] = ' + str(assembly_contigs) + ';\n'
        additional_assemblies_data += 'assemblies_misassemblies["' + assembly + '"] = "' + str(ext_misassemblies) + '' \
                           '+' + str(local_misassemblies) + '";\n'

    if contigs_analyzer.ref_labels_by_chromosomes:
        data_str.append(''.join(links_to_chromosomes))
    if cov_data:
        # adding coverage data
        data_str.append('var coverage_data = {};')
        data_str.append('var max_depth = {};')
        if cov_data[chr]:
            chr_max_depth = max_depth[chr]
            data_str.append('max_depth["' + chr + '"] = ' + str(chr_max_depth) + ';')
            data_str.append('coverage_data["' + chr + '"] = [ ')
            for e in cov_data[chr]:
                data_str.append(str(e) + ',')
            data_str[-1] = data_str[-1][:-1] + '];'

        data_str.append('var not_covered = {};')
        data_str.append('not_covered["' + chr + '"] = [ ')
        # if len(not_covered[chr]) > 0:
        #     for e in not_covered[chr]:
        #         data_str.append('' + e + ','.format(**locals()))
        #     data_str[-1] = data_str[-1][:-1]
        data_str[-1] += '];'
    data_str = '\n'.join(data_str)

    misassemblies_types = ['relocation', 'translocation', 'inversion', 'interspecies translocation', 'local']
    if not qconfig.is_combined_ref:
        misassemblies_types.remove('interspecies translocation')
    chr_data = 'chromosome = "' + chr + '";\n'
    chromosome = '","'.join(ref_contigs)
    chr_data += 'var chrContigs = ["' + chromosome + '"];\n'
    chr_name = chr.replace('_', ' ')
    reference_text = '<div class="reftitle"><b>Contig alignment viewer.</b> Contigs aligned to "' + chr_name + '"</div>'
    html_saver.save_icarus_data(json_output_dir, reference_text, 'reference', alignment_viewer_fpath)
    ms_counts_by_type = OrderedDict()
    for ms_type in misassemblies_types:
        factor = 1 if ms_type == 'interspecies translocation' else 2
        ms_counts_by_type[ms_type] = sum(ms_types[assembly][ms_type] / factor for assembly in chr_to_aligned_blocks.keys())
    total_ms_count = sum(ms_counts_by_type.values()) - ms_counts_by_type['local']
    ms_selector_text = 'Show misassemblies: '
    for ms_type, ms_count in ms_counts_by_type.items():
        is_checked = 'checked="checked"'  #if ms_count > 0 else ''
        ms_name = ms_type
        if ms_count != 1 and ms_type != 'local':
            ms_name += 's'
        ms_selector_text += '<label><input type="checkbox" id="' + ms_type + '" name="misassemblies_select" ' + \
                            is_checked + '/>' + ms_name + ' (' + str(ms_count) + ')</label>'
    html_saver.save_icarus_data(json_output_dir, ms_selector_text, 'ms_selector', alignment_viewer_fpath)
    all_data = ref_data + assemblies_data + additional_assemblies_data + chr_data + features_data + data_str
    html_saver.save_icarus_data(json_output_dir, all_data, 'contig_alignments', alignment_viewer_fpath)
    html_saver.clean_html(alignment_viewer_fpath)

    return num_misassemblies, aligned_assemblies


def get_info_by_chr(chr, aligned_bases_by_chr, chr_sizes, contigs_fpaths, one_chromosome=False):
    if one_chromosome:
        html_name = qconfig.one_alignment_viewer_name
        chr_link = os.path.join(qconfig.icarus_dirname, html_name + '.html')
    else:
        chr_link = os.path.join(qconfig.icarus_dirname, chr + '.html')
    chr_name = chr.replace('_', ' ')
    tooltip = ''
    if len(chr_name) > 50:
        short_name = chr[:50]
        tooltip = 'data-toggle="tooltip" title="' + chr_name + '">'
        chr_name = short_name + '...'
    aligned_lengths = [aligned_len for aligned_len in aligned_bases_by_chr[chr] if aligned_len is not None]
    chr_genome = sum(aligned_lengths) * 100.0 / (chr_sizes[chr] * len(contigs_fpaths))
    chr_size = chr_sizes[chr]
    return chr_link, chr_name, chr_genome, chr_size


def js_data_gen(assemblies, contigs_fpaths, chromosomes_length, output_dirpath, structures_by_labels,
                contigs_by_assemblies, ref_fpath=None, stdout_pattern=None, features_data=None, cov_fpath=None, json_output_dir=None):
    chr_full_names = []
    chr_names = []
    if chromosomes_length and assemblies:
        chr_to_aligned_blocks = OrderedDict()
        chr_names = chromosomes_length.keys()
        for assembly in assemblies.assemblies:
            chr_to_aligned_blocks[assembly.label] = defaultdict(list)
            for align in assembly.alignments:
                chr_to_aligned_blocks[assembly.label][align.ref_name].append(align)

    main_menu_fpath = os.path.join(output_dirpath, summary_fname)
    output_all_files_dir_path = os.path.join(output_dirpath, qconfig.icarus_dirname)
    if not os.path.exists(output_all_files_dir_path):
        os.mkdir(output_all_files_dir_path)
    contig_names_by_refs = None
    if contigs_analyzer.ref_labels_by_chromosomes:
        contig_names_by_refs = contigs_analyzer.ref_labels_by_chromosomes
        added_refs = set()
        chr_full_names = [added_refs.add(ref) or ref for ref in [contig_names_by_refs[contig] for contig in chr_names]
                          if ref not in added_refs]
    elif sum(chromosomes_length.values()) < qconfig.MAX_SIZE_FOR_COMB_PLOT and len(chr_names) > 1:
        chr_full_names = [qutils.name_from_fpath(ref_fpath)]
    else:
        chr_full_names = chr_names

    cov_data, not_covered, max_depth = parse_cov_fpath(cov_fpath, chr_names, chr_full_names)

    chr_sizes = {}
    num_contigs = {}
    aligned_bases = genome_analyzer.get_ref_aligned_lengths()
    aligned_bases_by_chr = {}
    num_misassemblies = {}
    nx_marks = [reporting.Fields.N50, reporting.Fields.N75, reporting.Fields.NG50,reporting.Fields.NG75]

    assemblies_data, assemblies_contig_size_data, assemblies_n50 = get_assemblies_data(contigs_fpaths, stdout_pattern, nx_marks)

    ref_contigs_dict = {}
    chr_lengths_dict = {}

    ref_data = 'var references_id = {};\n'
    for i, chr in enumerate(chr_names):
        ref_data += 'references_id["' + chr + '"] = ' + str(i) + ';\n'
    for i, chr in enumerate(chr_full_names):
        if contigs_analyzer.ref_labels_by_chromosomes:
            ref_contigs = [contig for contig in chr_names if contig_names_by_refs[contig] == chr]
        elif len(chr_full_names) == 1:
            ref_contigs = chr_names
        else:
            ref_contigs = [chr]
        ref_contigs_dict[chr] = ref_contigs
        chr_lengths_dict[chr] = [0] + [chromosomes_length[contig] for contig in ref_contigs]

    num_misassemblies = defaultdict(int)
    aligned_bases_by_chr = defaultdict(list)
    aligned_assemblies = defaultdict(set)
    for i, chr in enumerate(chr_full_names):
        ref_contigs = ref_contigs_dict[chr]
        chr_lengths = chr_lengths_dict[chr]
        chr_size = sum([chromosomes_length[contig] for contig in ref_contigs])
        chr_sizes[chr] = chr_size
        num_contigs[chr] = len(ref_contigs)
        data_str = []
        data_str.append('var chromosomes_len = {};')
        for ref_contig in ref_contigs:
            l = chromosomes_length[ref_contig]
            data_str.append('chromosomes_len["' + ref_contig + '"] = ' + str(l) + ';')
            aligned_bases_by_chr[chr].extend(aligned_bases[ref_contig])
        num_misassemblies[chr], aligned_assemblies[chr] = save_alignment_data_for_one_ref(chr, chr_full_names, ref_contigs, chr_lengths,
                                                          data_str, chr_to_aligned_blocks, structures_by_labels, json_output_dir=json_output_dir,
                                                          ref_data=ref_data, features_data=features_data, assemblies_data=assemblies_data,
                                                          cov_data=cov_data, not_covered=not_covered, max_depth=max_depth, output_dir_path=output_all_files_dir_path)

    contigs_sizes_str, too_many_contigs = get_contigs_data(contigs_by_assemblies, nx_marks, assemblies_n50, structures_by_labels,
                                                           contig_names_by_refs, chr_names, one_html=len(chr_full_names) == 1)
    contig_size_template_fpath = html_saver.get_real_path(qconfig.icarus_viewers_template_fname)
    contig_size_viewer_fpath = os.path.join(output_all_files_dir_path, qconfig.contig_size_viewer_fname)
    html_saver.init_icarus(contig_size_template_fpath, contig_size_viewer_fpath)
    warning_contigs = '. For better performance, only largest %s contigs of each assembly were loaded' % str(qconfig.max_contigs_num_for_size_viewer) \
        if too_many_contigs else ''
    reference_text = '<div class="reftitle"><b>Contig size viewer</b>' + warning_contigs + '</div>'
    html_saver.save_icarus_data(json_output_dir, reference_text, 'reference', contig_size_viewer_fpath)
    size_threshold_text = 'Fade contigs shorter than <input class="textBox" id="input_contig_threshold" type="text" size="5" /> bp </span>'
    html_saver.save_icarus_data(json_output_dir, size_threshold_text, 'size_threshold', contig_size_viewer_fpath)
    all_data = assemblies_data + assemblies_contig_size_data + contigs_sizes_str
    html_saver.save_icarus_data(json_output_dir, all_data, 'contig_sizes', contig_size_viewer_fpath)
    html_saver.clean_html(contig_size_viewer_fpath)

    icarus_links = defaultdict(list)
    if len(chr_full_names) > 1:
        chr_link = qconfig.icarus_html_fname
        icarus_links["links"].append(chr_link)
        icarus_links["links_names"].append(qconfig.icarus_link)

    main_menu_template_fpath = html_saver.get_real_path(qconfig.icarus_menu_template_fname)
    html_saver.init_icarus(main_menu_template_fpath, main_menu_fpath)

    labels = [qconfig.assembly_labels_by_fpath[contigs_fpath] for contigs_fpath in contigs_fpaths]
    assemblies = '<b>Assemblies: </b>' + ', '.join(labels)
    html_saver.save_icarus_data(json_output_dir, assemblies, 'assemblies', main_menu_fpath)

    num_aligned_assemblies = [len(aligned_assemblies[chr]) for chr in chr_full_names]
    is_unaligned_asm_exists = len(set(num_aligned_assemblies)) > 1
    if is_unaligned_asm_exists:
        html_saver.save_icarus_data(json_output_dir, '<th># assemblies</th>', 'th_assemblies', main_menu_fpath)

    contig_size_name = qconfig.contig_size_viewer_name
    contig_size_browser_fname = os.path.join(qconfig.icarus_dirname, qconfig.contig_size_viewer_fname)
    if not chr_names:
        icarus_links["links"].append(contig_size_browser_fname)
        icarus_links["links_names"].append(qconfig.icarus_link)
    contig_size_browser_link = '<tr><td><a href="' + contig_size_browser_fname + '">' + contig_size_name + '</a></td></tr>'
    quast_report_link = '<tr><td><a href="%s">QUAST report</a></td></tr>' % html_saver.report_fname
    html_saver.save_icarus_data(json_output_dir, contig_size_browser_link + quast_report_link, 'links', main_menu_fpath)
    div_references = []
    if chr_full_names and len(chr_full_names) > 1:
        reference_table = []
        div_references.append('<div>')
        for chr in sorted(chr_full_names):
            chr_link, chr_name, chr_genome, chr_size = get_info_by_chr(chr, aligned_bases_by_chr, chr_sizes, contigs_fpaths)
            reference_table.append('<tr>')
            reference_table.append('<td><a href="' + chr_link + '">' + chr_name + '</a></td>')
            reference_table.append('<td>%s</td>' % num_contigs[chr])
            reference_table.append('<td>%s</td>' % format_long_numbers(chr_size))
            if is_unaligned_asm_exists:
                reference_table.append('<td>%s</td>' % len(aligned_assemblies[chr]))
            reference_table.append('<td>%.3f</td>' % chr_genome)
            reference_table.append('<td>%s</td>' % num_misassemblies[chr])
            reference_table.append('</tr>')
        html_saver.save_icarus_data(json_output_dir, '\n'.join(reference_table), 'references', main_menu_fpath)
    else:
        if chr_full_names:
            chr = chr_full_names[0]
            chr_link, chr_name, chr_genome, chr_size = get_info_by_chr(chr, aligned_bases_by_chr, chr_sizes,
                                                                       contigs_fpaths, one_chromosome=True)
            viewer_name = qconfig.contig_alignment_viewer_name
            viewer_link = '<a href="' + chr_link + '">' + viewer_name + '</a>'
            viewer_info = viewer_link + \
                  '<div class="reference_details">' \
                      '<p>Aligned to sequences from  ' + os.path.basename(ref_fpath) + '</p>' \
                      '<p>Fragments: ' + str(num_contigs[chr]) + ', length: ' + format_long_numbers(chr_size) + \
                        ('bp, mean genome fraction: %.3f' % chr_genome) + '%, misassembled blocks: ' + str(num_misassemblies[chr]) + '</p>' + \
                  '</div>'
            icarus_links["links"].append(chr_link)
            icarus_links["links_names"].append(qconfig.icarus_link)
            div_references.append('<div class="contig_alignment_viewer_panel">')
            div_references.append(viewer_info)
            div_references.append('</div>')
        div_references.append('<div style="display:none;">')
    html_saver.save_icarus_data(json_output_dir, '\n'.join(div_references), 'div_references', main_menu_fpath)
    html_saver.clean_html(main_menu_fpath)

    html_saver.save_icarus_links(output_dirpath, icarus_links)
    if json_output_dir:
        json_saver.save_icarus_links(json_output_dir, icarus_links)

    return main_menu_fpath