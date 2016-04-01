#!/usr/bin/python -O

############################################################################
# Copyright (c) 2015 Saint Petersburg State University
# Copyright (c) 2011-2015 Saint Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################


from __future__ import with_statement

from collections import defaultdict

from libs import qconfig, qutils, fastaparser, genome_analyzer
import os
import math
import libs.html_saver.html_saver as html_saver
from shutil import copyfile

from libs import reporting
from libs.log import get_logger
logger = get_logger(qconfig.LOGGER_DEFAULT_NAME)

MAX_REF_NAME = 20
NAME_FOR_ONE_PLOT = 'Main plot'

misassemblies_types = ['relocation', 'translocation', 'inversion', 'local']
if qconfig.is_combined_ref:
    misassemblies_types = ['relocation', 'translocation', 'inversion', 'interspecies translocation', 'local']

from libs import plotter  # Do not remove this line! It would lead to a warning in matplotlib.
if not plotter.matplotlib_error:
    import matplotlib
    import matplotlib.patches
    import matplotlib.pyplot
    import matplotlib.lines


def get_similar_threshold(total):
    if total <= 2:
        return 1
    return total // 2

def format_long_numbers(number):
    return ''.join(reversed([x + (' ' if i and not i % 3 else '') for i, x in enumerate(reversed(str(number)))]))

class Settings:
    def __init__(self, virtual_genome_size, max_cov_pos, max_cov, assemblies_num):
        self.max_pos = virtual_genome_size
        self.max_cov_pos = max_cov_pos
        self.max_cov = max_cov

        #colors
        self.color_misassembled = ["#e41a1c", "#b82525"]
        self.color_misassembled_similar = ["#ff7500", "#e09110"]
        self.color_correct = ["#4daf4a", "#40cf40"]
        self.color_correct_similar = ["#377eb8", "#576e88"]

        #width
        self.assembly_width = 1700.0
        self.last_margin = 20.0

        #scales
        self.scale = self.assembly_width / self.max_pos
        self.plot_x_scale = self.assembly_width / (self.max_cov_pos + 1)

        #coverage plot
        self.plot_height = 130.0
        self.plot_margin = 40.0
        self.dot_length = self.plot_x_scale

        self.max_log_cov = math.ceil(math.log10(self.max_cov))
        self.plot_y_scale = self.plot_height / self.max_log_cov

        ticNum = 5
        rawTicStep = self.max_pos / ticNum
        ticStepLog = math.pow(10, math.floor(math.log10(rawTicStep)))
        xStep = math.floor(rawTicStep / ticStepLog)
        if xStep >= 7:
            xStep = 10
        elif xStep >= 3:
            xStep = 5
        else:
            xStep = 1

        self.xTics = xStep * ticStepLog
        self.genomeAnnotation = "Genome, "

        ticStepLog = int(math.pow(10, math.floor(math.log10(self.xTics))))
        if xStep == 5:
            ticStepLog *= 10

        if ticStepLog == 1:
            self.genomeAnnotation += "bp"
        elif ticStepLog < 1000:
            self.genomeAnnotation += "x" + str(ticStepLog) + " bp"
        elif ticStepLog == 1000:
            self.genomeAnnotation += "kb"
        elif ticStepLog < 1000000:
            self.genomeAnnotation += "x" + str(ticStepLog / 1000) + " kb"
        elif ticStepLog == 1000000:
            self.genomeAnnotation += "Mb"
        elif ticStepLog < 1000000000:
            self.genomeAnnotation += "x" + str(ticStepLog / 1000000) + " Mb"
        elif ticStepLog == 1000000000:
            self.genomeAnnotation += "Gb"

        if self.max_log_cov <= 3.0:
            self.yTics = 1.0
        else:
            self.yTics = 2.0
            self.max_log_cov -= int(self.max_log_cov) % 2
            self.plot_y_scale = self.plot_height / self.max_log_cov

        self.genomeLength = self.max_pos
        self.genomeAnnotationScale = math.pow(10, math.ceil(math.log10(self.xTics)))

        self.zeroCovStep = -0.2
        self.dotWeight = 0.7

        self.zeroCoverageColor = "blue"
        self.coverageColor = "red"

        #dashed lines
        self.dashLines = True
        self.dashLineWeight = 0.2
        self.ticLength = 6
        self.axisWeight = 0.5

        #assembly display parameters
        self.assemblyStep = 70
        self.similarStep = 7
        self.goodStep = 7
        self.oddStep = [0, 4]
        self.contigHeight = 30
        self.simHeight = 6
        self.xOffset = 85.0

        #names parameters
        self.nameAnnotationXStep = 15
        self.nameAnnotationYStep = 15
        self.xticsStep = 6
        self.yticsStep = self.ticLength + 5
        self.xLabelStep = self.xticsStep + 25
        self.yLabelStep = self.yticsStep + 95

        self.totalWidth = self.xOffset + self.assembly_width
        self.totalHeight = self.plot_height + self.plot_margin + assemblies_num * self.assemblyStep + self.last_margin

        self.totalWidthInches = 14
        self.totalHeightInches = self.totalWidthInches * self.totalHeight / self.totalWidth

        self.contigEdgeDelta = 0.05
        self.minSimilarContig = 10000

        self.minConnectedBlock = 2000
        self.maxBlockGap = 10000

        self.drawArcs = False
        self.analyzeSimilar = False


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

    def length(self):
        return self.end - self.start

    def annotation(self):
        return self.name + "\n" + str(self.start) + "-" + str(self.end)

    def center(self):
        return (self.end + self.start) / 2

    def compare_inexact(self, alignment, settings):
        contig_len = abs(self.end - self.start)
        return abs(alignment.start - self.start) <= (settings.contigEdgeDelta * contig_len) and \
               abs(alignment.end - self.end) <= (settings.contigEdgeDelta * contig_len)


class Arc:
    def __init__(self, c1, c2):
        self.c1 = c1
        self.c2 = c2


class Contig:
    def __init__(self, name, size=None):
        self.name = name
        self.size = size
        self.alignments = []
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


    def find(self, alignment, settings):
        if alignment.length() < settings.minSimilarContig:
            return -1

        i = 0
        while i < len(self.alignments) and not alignment.compare_inexact(self.alignments[i], settings):
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

    def find_similar(self, settings):
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

                    block_id = self.assemblies[j].find(block, settings)
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


class Visualizer:
    def __init__(self, assemblies, covHist, settings, sorted_ref_names, sorted_ref_lengths, virtual_genome_shift):
        self.assemblies = assemblies
        self.covHist = covHist
        self.settings = settings
        self.sorted_ref_names = sorted_ref_names
        self.sorted_ref_lengths = sorted_ref_lengths
        self.virtual_genome_shift = virtual_genome_shift

        self.figure = matplotlib.pyplot.figure(figsize=(settings.totalWidthInches, settings.totalHeightInches))
        self.subplot = self.figure.add_subplot(111)
        self.extent = self.subplot.get_window_extent().transformed(self.figure.dpi_scale_trans.inverted())

    def __del__(self):
        pass

    def show(self):
        self.subplot.axis("equal")
        self.subplot.axis("off")
        matplotlib.pyplot.show()

    def save(self, fileName):
        self.subplot.axis("equal")
        self.subplot.axis("off")
        finalFileName = fileName + ".svg"
        self.figure.savefig(finalFileName, bbox_inches=self.extent) #, format='svg')
        return finalFileName


    def plot_genome_axis(self, offset):
        cur_offset = 0
        for id, ref_length in enumerate(self.sorted_ref_lengths):
            self.subplot.add_line(
                matplotlib.lines.Line2D((offset[0] + cur_offset * self.settings.scale, offset[0] +
                                        (cur_offset + ref_length) * self.settings.scale),
                    (offset[1], offset[1]), c="black", lw=self.settings.axisWeight))

            i = 0.0
            while i < ref_length - self.settings.xTics / 5.0:
                x = offset[0] + cur_offset * self.settings.scale + self.settings.scale * float(i)

                if self.settings.dashLines: # TODO: multi-chromosomes
                    self.subplot.add_line(
                        matplotlib.lines.Line2D((x, x), (offset[1] + self.settings.plot_height, self.settings.last_margin),
                                                c="grey", ls=':', lw=self.settings.dashLineWeight))

                self.subplot.add_line(
                    matplotlib.lines.Line2D((x, x), (offset[1], offset[1] - self.settings.ticLength), c="black",
                                            lw=self.settings.axisWeight))

                self.subplot.annotate(str(round(float(i) / self.settings.genomeAnnotationScale, 1)),
                                      (x + self.settings.xticsStep, offset[1] - self.settings.xticsStep), fontsize=8,
                                      horizontalalignment='left', verticalalignment='top')

                i += self.settings.xTics

            x = offset[0] + (cur_offset + ref_length) * self.settings.scale
            if self.settings.dashLines:
                self.subplot.add_line(matplotlib.lines.Line2D((x, x), (offset[1] + self.settings.plot_height , self.settings.last_margin), c="grey", ls=':', lw=self.settings.dashLineWeight))
            self.subplot.add_line(matplotlib.lines.Line2D((x, x), (offset[1], offset[1] - self.settings.ticLength), c="black", lw=self.settings.axisWeight))

            self.subplot.annotate(str(round(float(ref_length) / self.settings.genomeAnnotationScale, 2)), (x + self.settings.xticsStep, offset[1] - self.settings.xticsStep), fontsize=8, horizontalalignment='left', verticalalignment='top')

            ref_name = self.sorted_ref_names[id]
            if len(ref_name) > MAX_REF_NAME:
                ref_name = "chr_%d (.." % id + ref_name[-(MAX_REF_NAME - 10):] + ")"
            self.subplot.annotate(ref_name, (offset[0] + self.settings.scale * (cur_offset + ref_length / 2.0), offset[1] + 1.5 * self.settings.xLabelStep),
                fontsize=11, horizontalalignment='center', verticalalignment='top')
            cur_offset += ref_length + self.virtual_genome_shift

        self.subplot.annotate(self.settings.genomeAnnotation, (offset[0] + self.settings.assembly_width / 2.0, offset[1] - self.settings.xLabelStep), fontsize=11, horizontalalignment='center', verticalalignment='top')


    def plot_coverage(self, covHist, offset):
        self.subplot.add_line(matplotlib.lines.Line2D((offset[0], offset[0]), (offset[1], offset[1] + self.settings.plot_height), c="black", lw=self.settings.axisWeight))

        cov = 0.0
        while (cov <= self.settings.max_log_cov):
            y = offset[1] + cov * self.settings.plot_y_scale
            self.subplot.add_line(matplotlib.lines.Line2D((offset[0] - self.settings.ticLength, offset[0]), (y, y), c="black", lw=self.settings.axisWeight))
            self.subplot.annotate(str(int(round(math.pow(10, cov)))), (offset[0] - self.settings.yticsStep, y), fontsize=8, horizontalalignment='right', verticalalignment='center')
            cov += self.settings.yTics

        self.subplot.annotate("Coverage", (offset[0] - self.settings.yLabelStep, offset[1] + self.settings.plot_y_scale * self.settings.max_log_cov / 2.0), fontsize=11, horizontalalignment='center', verticalalignment='center', rotation = "vertical")

        for pos in covHist:
            x = offset[0] + pos * self.settings.plot_x_scale
            if covHist[pos] != 0:
                y = offset[1] + math.log10(covHist[pos]) * self.settings.plot_y_scale
                color = self.settings.coverageColor
            else:
                y = offset[1] + self.settings.zeroCovStep * self.settings.plot_y_scale
                color = self.settings.zeroCoverageColor
            self.subplot.add_line(matplotlib.lines.Line2D((x, x + self.settings.dot_length), (y, y), c=color, lw=self.settings.dotWeight))


    def plot_assembly(self, assembly, offset):
        for name in assembly.contigs_by_ids:
            for arc in assembly.contigs_by_ids[name].arcs:
                x = offset[0] + (arc.c1 + arc.c2) * self.settings.scale / 2
                y = offset[1]
                width = abs(arc.c1 - arc.c2) * self.settings.scale
                height = 0.1 * width
                if height < 20:
                    height = 20
                if height > 90:
                    height = 90

                self.subplot.add_patch(
                    matplotlib.patches.Arc((x, y), width, height, angle=180.0, theta1=0.0, theta2=180.0,
                                           ec="black", color="black", lw=0.2))

        for block in assembly.alignments:
            x = offset[0] + block.start * self.settings.scale
            y = offset[1] + block.vPositionDelta
            height = self.settings.contigHeight
            width = block.length() * self.settings.scale

            self.subplot.add_patch(
                matplotlib.patches.Rectangle((x, y), width, height,
                                             ec="black", color=block.color, fill=True, lw=0.0))


    def visualize(self):
        self.subplot.add_patch(
            matplotlib.patches.Rectangle((-20, 0),
                                         self.settings.totalWidth + 20 + self.settings.last_margin,
                                         self.settings.totalHeight + 0,
                                         color="white", fill=True, lw=0))

        self.plot_genome_axis((self.settings.xOffset, self.settings.totalHeight - self.settings.plot_height))

        if self.covHist is not None:
            self.plot_coverage(self.covHist,
                               (self.settings.xOffset, self.settings.totalHeight - self.settings.plot_height))

        if self.assemblies is not None:
            offset = self.settings.plot_height + self.settings.plot_margin + self.settings.assemblyStep

            for assembly in self.assemblies.assemblies:
                self.subplot.annotate(
                    assembly.label,
                    (self.settings.xOffset - self.settings.nameAnnotationXStep,
                     self.settings.totalHeight - offset + self.settings.nameAnnotationYStep),
                    fontsize=12, horizontalalignment='right', verticalalignment='bottom')

                self.plot_assembly(assembly, (self.settings.xOffset, self.settings.totalHeight - offset))
                offset += self.settings.assemblyStep


# def read_coverage(cov_fpath):
#     hist = {}
#     max_pos = 0
#     max_cov = 0
#
#     with open(cov_fpath) as cov_file:
#         for line in cov_file:
#             pos = line.strip().split(' ')
#             hist[int(pos[0])] = int(pos[1])
#             if max_pos < int(pos[0]):
#                 max_pos = int(pos[0])
#             if max_cov < int(pos[1]):
#                 max_cov = int(pos[1])
#
#     return hist, max_pos, max_cov


def draw_alignment_plot(contigs_fpaths, virtual_genome_size, sorted_ref_names, sorted_ref_lengths, virtual_genome_shift,
                        output_dirpath, lists_of_aligned_blocks, arcs=False, similar=False,
                        coverage_hist=None):

    output_fpath = os.path.join(output_dirpath, 'alignment')

    min_visualizer_length = 0

    assemblies = Assemblies(
        contigs_fpaths,
        lists_of_aligned_blocks,
        virtual_genome_size,
        min_visualizer_length)

    asm_number = len(assemblies.assemblies)

    if virtual_genome_size == 0:
        virtual_genome_size = assemblies.find_max_pos()

    max_pos, max_cov = 10, 10
    if coverage_hist:
        max_pos = max(coverage_hist.keys())
        max_cov = max(coverage_hist.values())

    settings = Settings(virtual_genome_size, max_pos, max_cov, asm_number)

    if arcs and assemblies is not None:
        settings.assemblyStep += 40
        assemblies.draw_arcs(settings)

    if similar and assemblies is not None:
        assemblies.find_similar(settings)

    assemblies.apply_colors(settings)

    if qconfig.draw_svg and not plotter.matplotlib_error:
        v = Visualizer(assemblies, coverage_hist, settings, sorted_ref_names, sorted_ref_lengths, virtual_genome_shift)
        v.visualize()
        plot_fpath = v.save(output_fpath)
    else:
        plot_fpath = None
    return plot_fpath, assemblies


def make_output_dir(output_dir_path):
    if not os.path.exists(output_dir_path):
        os.makedirs(output_dir_path)


def do(contigs_fpaths, contig_report_fpath_pattern, output_dirpath,
       ref_fpath, cov_fpath=None, arcs=False, stdout_pattern=None, similar=False, features=None, coverage_hist=None):
    make_output_dir(output_dirpath)

    lists_of_aligned_blocks = []
    contigs_by_assemblies = {}
    structures_by_labels = {}

    total_genome_size = 0
    reference_chromosomes = dict()
    chr_names = []
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
    for length in sorted(reference_chromosomes.values(), reverse=True):
        cumulative_ref_lengths.append(cumulative_ref_lengths[-1] + virtual_genome_shift + length)
    virtual_genome_size = cumulative_ref_lengths[-1] - virtual_genome_shift

    for contigs_fpath in contigs_fpaths:
        report_fpath = contig_report_fpath_pattern % qutils.label_from_fpath_for_fname(contigs_fpath)
        aligned_blocks, misassembled_id_to_structure, contigs = parse_nucmer_contig_report(report_fpath, sorted_ref_names, cumulative_ref_lengths)
        if aligned_blocks is None:
            return None
        label = qconfig.assembly_labels_by_fpath[contigs_fpath]
        for block in aligned_blocks:
            block.label = label
        lists_of_aligned_blocks.append(aligned_blocks)
        contigs_by_assemblies[label] = contigs
        structures_by_labels[label] = misassembled_id_to_structure

    plot_fpath, assemblies = draw_alignment_plot(
        contigs_fpaths, virtual_genome_size, sorted_ref_names, sorted_ref_lengths, virtual_genome_shift, output_dirpath,
        lists_of_aligned_blocks, arcs, similar, coverage_hist)
    if assemblies and qconfig.create_icarus_html:
        icarus_html_fpath = js_data_gen(assemblies, contigs_fpaths, contig_report_fpath_pattern, reference_chromosomes,
                    output_dirpath, structures_by_labels, stdout_pattern=stdout_pattern, contigs_by_assemblies=contigs_by_assemblies,
                    features=features, cov_fpath=cov_fpath)
    else:
        icarus_html_fpath = None

    return icarus_html_fpath, plot_fpath

def parse_nucmer_contig_report(report_fpath, sorted_ref_names, cumulative_ref_lengths):
    aligned_blocks = []
    contigs = []
    misassembled_contigs_ids = set()

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
        for i, line in enumerate(report_file):
            split_line = line.strip().split('\t')
            if i == 0:
                start_col = split_line.index('S1')
                end_col = split_line.index('E1')
                start_in_contig_col = split_line.index('S2')
                end_in_contig_col = split_line.index('E2')
                ref_col = split_line.index('Reference')
                contig_col = split_line.index('Contig')
                idy_col = split_line.index('IDY')
            elif split_line and split_line[0] == 'CONTIG':
                _, name, size = split_line
                contig = Contig(name=name, size=int(size))
                contigs.append(contig)
            elif split_line and len(split_line) < 5:
                misassembled_id_to_structure[contig_id].append(line.strip())
                misassembled_contigs_ids.add(contig_id)
            elif split_line and len(split_line) > 5:
                unshifted_start, unshifted_end, start_in_contig, end_in_contig, ref_name, contig_id, idy = split_line[start_col], \
                            split_line[end_col], split_line[start_in_contig_col], split_line[end_in_contig_col], split_line[ref_col], \
                            split_line[contig_col], split_line[idy_col]
                unshifted_start, unshifted_end, start_in_contig, end_in_contig = int(unshifted_start), int(unshifted_end),\
                                                                                 int(start_in_contig), int(end_in_contig)
                cur_shift = cumulative_ref_lengths[sorted_ref_names.index(ref_name)]
                start = unshifted_start + cur_shift
                end = unshifted_end + cur_shift

                is_rc = ((start - end) * (start_in_contig - end_in_contig)) < 0
                position_in_ref = unshifted_start
                block = Alignment(
                    name=contig_id, start=start, end=end, unshifted_start=unshifted_start, unshifted_end=unshifted_end, is_rc=is_rc,
                    idy=idy, start_in_contig=start_in_contig, end_in_contig=end_in_contig, position_in_ref=position_in_ref, ref_name=ref_name)
                misassembled_id_to_structure[contig_id].append(block)

                aligned_blocks.append(block)

    for block in aligned_blocks:
        if block.name in misassembled_contigs_ids:
            block.misassembled = True

    return aligned_blocks, misassembled_id_to_structure, contigs


def js_data_gen(assemblies, contigs_fpaths, contig_report_fpath_pattern, chromosomes_length,
                output_dirpath, structures_by_labels, contigs_by_assemblies, stdout_pattern=None, features=None, cov_fpath=None):
    chr_to_aligned_blocks = defaultdict(dict)
    chr_names = chromosomes_length.keys()
    for assembly in assemblies.assemblies:
        chr_to_aligned_blocks[assembly.label] = defaultdict(list)
        for align in assembly.alignments:
            chr_to_aligned_blocks[assembly.label][align.ref_name].append(align)

    summary_fname = qconfig.alignment_summary_fname
    summary_path = os.path.join(output_dirpath, summary_fname)
    main_menu_link = '<a href="../{summary_fname}" style="color: white">Main menu</a>'.format(**locals())
    output_all_files_dir_path = os.path.join(output_dirpath, qconfig.alignment_plots_dirname)
    if not os.path.exists(output_all_files_dir_path):
        os.mkdir(output_all_files_dir_path)
    import contigs_analyzer
    if contigs_analyzer.ref_labels_by_chromosomes:
        contig_names_by_refs = contigs_analyzer.ref_labels_by_chromosomes
        chr_full_names = list(set([contig_names_by_refs[contig] for contig in chr_names]))
    elif sum(chromosomes_length.values()) < qconfig.MAX_SIZE_FOR_COMB_PLOT and len(chr_names) > 1:
        chr_full_names = [NAME_FOR_ONE_PLOT]
    else:
        chr_full_names = chr_names

    if cov_fpath:
        cov_data = dict()
        not_covered = dict()
        cur_len = dict()
        with open(cov_fpath, 'r') as coverage:
            name = chr_names[0]
            contig_to_chr = {}
            for chr in chr_full_names:
                cov_data.setdefault(chr, [])
                not_covered.setdefault(chr, [])
                cur_len.setdefault(chr, 0)
                if contigs_analyzer.ref_labels_by_chromosomes:
                    contigs = [contig for contig in chr_names if contig_names_by_refs[contig] == chr]
                elif chr == NAME_FOR_ONE_PLOT:
                    contigs = chr_names
                else:
                    contigs = [chr]
                for contig in contigs:
                    contig_to_chr[contig] = chr
            for index, line in enumerate(coverage):
                c = list(line.split())
                name = contig_to_chr[qutils.correct_name(c[0])]
                cur_len[name] += int(c[2])
                if index % 100 == 0 and index > 0:
                    cov_data[name].append(cur_len[name]/100)
                    cur_len[name] = 0
                if c[2] == '0':
                    not_covered[name].append(c[1])
    chr_sizes = {}
    num_contigs = {}
    aligned_bases = genome_analyzer.get_ref_aligned_lengths()
    aligned_bases_by_chr = {}
    num_misassemblies = {}
    aligned_assemblies = {}
    assemblies_n50 = defaultdict(dict)
    nx_marks = [reporting.Fields.N50, reporting.Fields.N75, reporting.Fields.NG50,reporting.Fields.NG75]

    assemblies_data = ''
    assemblies_data += 'var assemblies_links = {};\n'
    assemblies_data += 'var assemblies_len = {};\n'
    assemblies_data += 'var assemblies_contigs = {};\n'
    assemblies_data += 'var assemblies_misassemblies = {};\n'
    assemblies_data += 'var assemblies_n50 = {};\n'
    assemblies_contig_size_data = ''
    for contigs_fpath in contigs_fpaths:
        label = qconfig.assembly_labels_by_fpath[contigs_fpath]
        contig_stdout_fpath = stdout_pattern % qutils.label_from_fpath_for_fname(contigs_fpath) + '.stdout'
        report = reporting.get(contigs_fpath)
        l = report.get_field(reporting.Fields.TOTALLEN)
        contigs = report.get_field(reporting.Fields.CONTIGS)
        n50 = report.get_field(reporting.Fields.N50)
        assemblies_data += 'assemblies_links["{label}"] = "{contig_stdout_fpath}";\n'.format(**locals())
        assemblies_contig_size_data += 'assemblies_len["{label}"] = {l};\n'.format(**locals())
        assemblies_contig_size_data += 'assemblies_contigs["{label}"] = {contigs};\n'.format(**locals())
        assemblies_contig_size_data += 'assemblies_n50["{label}"] = "{n50}";\n'.format(**locals())
        for nx in nx_marks:
            assemblies_n50[label][nx] = report.get_field(nx)

    ref_ids = {}
    ref_contigs_dict = {}
    chr_lengths_dict = {}

    ref_data = 'var references_id = {};\n'
    for i, chr in enumerate(chr_full_names):
        ref_ids[chr] = i
        ref_data += 'references_id[{i}] = "{chr}";\n'.format(**locals())
        if contigs_analyzer.ref_labels_by_chromosomes:
            ref_contigs = [contig for contig in chr_names if contig_names_by_refs[contig] == chr]
        elif chr == NAME_FOR_ONE_PLOT:
            ref_contigs = chr_names
        else:
            ref_contigs = [chr]
        ref_contigs_dict[chr] = ref_contigs
        chr_lengths_dict[chr] = [0] + [chromosomes_length[contig] for contig in ref_contigs]

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
                if not chr or chr not in ref_ids:
                    continue
                ref_id = ref_ids[chr]
                name = region.name if region.name else ''
                features_data += '{{name: "{name}", start: {region.start}, end: {region.end}, id_: "{region.id}", ' \
                                 'kind: "{feature_container.kind}", chr:{ref_id}}},'.format(**locals())
            containers_kind.append(feature_container.kind)
            features_data += '],'
        features_data = features_data[:-1] + '];\n'

    for i, chr in enumerate(chr_full_names):
        short_chr = chr[:30]
        if len(chr_full_names) == 1:
            short_chr = qconfig.one_alignment_viewer_name
        num_misassemblies[chr] = 0
        aligned_bases_by_chr[chr] = []
        aligned_assemblies[chr] = []
        data_str = []
        additional_assemblies_data = ''
        if contigs_analyzer.ref_labels_by_chromosomes:
            data_str.append('var links_to_chromosomes = {};')
            links_to_chromosomes = []
            used_chromosomes = []

        ref_contigs = ref_contigs_dict[chr]
        chr_lengths = chr_lengths_dict[chr]
        chr_size = sum([chromosomes_length[contig] for contig in ref_contigs])
        chr_sizes[chr] = chr_size
        num_contigs[chr] = len(ref_contigs)
        for ref_contig in ref_contigs:
            aligned_bases_by_chr[chr].extend(aligned_bases[ref_contig])
        data_str.append('var chromosomes_len = {};')
        for ref_contig in ref_contigs:
            l = chromosomes_length[ref_contig]
            data_str.append('chromosomes_len["{ref_contig}"] = {l};'.format(**locals()))

        # adding assembly data
        data_str.append('var contig_data = {};')
        data_str.append('contig_data["{chr}"] = {{}};'.format(**locals()))
        assemblies_len = defaultdict(int)
        assemblies_contigs = defaultdict(set)
        ms_types = dict()
        for assembly in chr_to_aligned_blocks.keys():
            data_str.append('contig_data["{chr}"]["{assembly}"] = [ '.format(**locals()))
            prev_len = 0
            ms_types[assembly] = defaultdict(int)
            for num_contig, ref_contig in enumerate(ref_contigs):
                if num_contig > 0:
                    prev_len += chr_lengths[num_contig]
                if ref_contig in chr_to_aligned_blocks[assembly]:
                    prev_end = None
                    for alignment in sorted(chr_to_aligned_blocks[assembly][ref_contig], key=lambda x: x.start):
                        assemblies_len[assembly] += abs(alignment.end_in_contig - alignment.start_in_contig) + 1
                        assemblies_contigs[assembly].add(alignment.name)
                        contig_structure = structures_by_labels[alignment.label]
                        misassembled_ends = []
                        if alignment.misassembled:
                            num_misassemblies[chr] += 1
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
                                    misassembled_ends.append('L')
                            else: misassembled_ends.append('')
                            if num_alignment + 1 < len(contig_structure[alignment.name]) and \
                                            type(contig_structure[alignment.name][num_alignment + 1]) == str:
                                misassembly_type = contig_structure[alignment.name][num_alignment + 1].split(',')[0].strip()
                                if not 'fake' in misassembly_type:
                                    if 'local' in misassembly_type:
                                        misassembly_type = 'local'
                                    alignment.misassemblies += ';' + misassembly_type
                                    ms_types[assembly][misassembly_type] += 1
                                    misassembled_ends.append('R')
                            else: misassembled_ends.append('')
                        else:
                            alignment.misassembled = False
                            alignment.misassemblies = ''

                        corr_start = prev_len + alignment.unshifted_start
                        corr_end = prev_len + alignment.unshifted_end
                        if misassembled_ends: misassembled_ends = ';'.join(misassembled_ends)
                        else: misassembled_ends = ''
                        data_str.append('{{name: "{alignment.name}", corr_start: {corr_start}, corr_end: {corr_end},' \
                                    'start: {alignment.unshifted_start}, end: {alignment.unshifted_end}, ' \
                                    'similar: "{alignment.similar}", misassemblies: "{alignment.misassemblies}", ' \
                                    'mis_ends: "{misassembled_ends}"'.format(**locals()))

                        if alignment.name != 'FICTIVE':
                            if len(aligned_assemblies[chr]) < len(contigs_fpaths) and alignment.label not in aligned_assemblies[chr]:
                                aligned_assemblies[chr].append(alignment.label)
                            data_str.append(', structure: [ ')
                            for el in contig_structure[alignment.name]:
                                if isinstance(el, Alignment):
                                    if el.ref_name in ref_contigs:
                                        num_chr = ref_contigs.index(el.ref_name)
                                        corr_len = sum(chr_lengths[:num_chr+1])
                                    else:
                                        corr_len = -int(el.end)
                                        if contigs_analyzer.ref_labels_by_chromosomes and el.ref_name not in used_chromosomes:
                                            used_chromosomes.append(el.ref_name)
                                            new_chr = contig_names_by_refs[el.ref_name]
                                            links_to_chromosomes.append('links_to_chromosomes["{el.ref_name}"] = "{new_chr}";'.format(**locals()))
                                    corr_el_start = corr_len + int(el.start)
                                    corr_el_end = corr_len + int(el.end)
                                    data_str.append('{{type: "A", corr_start: {corr_el_start}, corr_end: {corr_el_end}, start: {el.start}, '
                                                    'end: {el.end}, start_in_contig: {el.start_in_contig}, end_in_contig: {el.end_in_contig}, '
                                                    'IDY: {el.idy}, chr: "{el.ref_name}"}},'.format(**locals()))
                                elif type(el) == str:
                                    data_str.append('{{type: "M", mstype: "{el}"}},'.format(**locals()))
                            data_str[-1] = data_str[-1][:-1] + ']},'

                        else: data_str[-1] += '},'
            data_str[-1] = data_str[-1][:-1] + '];'
            assembly_len = assemblies_len[assembly]
            assembly_contigs = len(assemblies_contigs[assembly])
            local_misassemblies = ms_types[assembly]['local'] / 2
            ext_misassemblies = sum(ms_types[assembly].values()) / 2 - local_misassemblies
            additional_assemblies_data += 'assemblies_len["{assembly}"] = {assembly_len};\n'.format(**locals())
            additional_assemblies_data += 'assemblies_contigs["{assembly}"] = {assembly_contigs};\n'.format(**locals())
            additional_assemblies_data += 'assemblies_misassemblies["{assembly}"] = "{ext_misassemblies}' \
                               '+{local_misassemblies}";\n'.format(**locals())

        if contigs_analyzer.ref_labels_by_chromosomes:
            data_str.append(''.join(links_to_chromosomes))
        if cov_fpath:
            # adding coverage data
            data_str.append('var coverage_data = {};')
            if cov_data[chr]:
                data_str.append('coverage_data["{chr}"] = [ '.format(**locals()))
                for e in cov_data[chr]:
                    data_str.append('{e},'.format(**locals()))
                data_str[-1] = data_str[-1][:-1] + '];'

            data_str.append('var not_covered = {};')
            data_str.append('not_covered["{chr}"] = [ '.format(**locals()))
            if len(not_covered[chr]) > 0:
                for e in not_covered[chr]:
                    data_str.append('{e},'.format(**locals()))
                data_str[-1] = data_str[-1][:-1]
            data_str[-1] += '];'
        data_str = '\n'.join(data_str)

        with open(html_saver.get_real_path('_chr_templ.html'), 'r') as template:
            with open(os.path.join(output_all_files_dir_path, '{short_chr}.html'.format(**locals())), 'w') as result:
                for line in template:
                    if line.find('<!--- data: ---->') != -1:
                        result.write(data_str)
                        result.write(ref_data)
                        result.write(features_data)
                        result.write(assemblies_data)
                        result.write(additional_assemblies_data)
                        chromosome = '","'.join(ref_contigs)
                        result.write('var CHROMOSOME = "{chr}";\n'.format(**locals()))
                        result.write('var chrContigs = ["{chromosome}"];\n'.format(**locals()))
                    elif line.find('<!--- misassemblies selector: ---->') != -1:
                        result.write('<div align="center" style="margin-top: 7px;">')
                        result.write('Misassembly type to show:')
                        for ms_type in misassemblies_types:
                            ms_count = sum(ms_types[assembly][ms_type] / 2 for assembly in chr_to_aligned_blocks.keys())
                            is_checked = 'checked="checked"' if ms_count > 0 else ''
                            if ms_type == 'local':
                                result.write('&nbsp&nbsp&nbsp&nbsp&nbsp&nbsp&nbsp&nbsp<label><input type="checkbox" id="{ms_type}" '
                                             'name="misassemblies_select" {is_checked}/> <i>{ms_type} ({ms_count})</i></label>&nbsp&nbsp'.format(**locals()))
                            else:
                                result.write('<label><input type="checkbox" id="{ms_type}" name="misassemblies_select" '
                                     '{is_checked}/> {ms_type} ({ms_count})</label>&nbsp&nbsp'.format(**locals()))
                        result.write('</div>')
                    elif line.find('<!--- css: ---->') != -1:
                        result.write(open(html_saver.get_real_path(os.path.join('static', 'contig_alignment_plot.css'))).read())
                        result.write(open(html_saver.get_real_path(os.path.join('static', 'common.css'))).read())
                        result.write(open(html_saver.get_real_path(os.path.join('static', 'bootstrap', 'bootstrap.css'))).read())
                    elif line.find('<!--- scripts: ---->') != -1:
                        result.write(open(html_saver.get_real_path(os.path.join('static', 'd3.js'))).read())
                        result.write(open(html_saver.get_real_path(os.path.join('static', 'scripts',
                                                                                'contig_alignment_plot_script.js'))).read())
                    elif line.find('<!--- title: ---->') != -1:
                        result.write('Icarus')
                    elif line.find('<!--- subtitle: ---->') != -1:
                        result.write('Contig alignment viewer')
                    elif line.find('<!--- reference: ---->') != -1:
                        chr_name = chr.replace('_', ' ')
                        if len(chr_name) > 90:
                            chr_name = chr_name[:90] + '...'
                        result.write(chr_name)
                    elif line.find('<!--- menu: ---->') != -1:
                        result.write(main_menu_link)
                    else:
                        result.write(line)

    contigs_sizes_str = ['var contig_data = {};']
    contigs_sizes_str.append('var CHROMOSOME;')
    min_contig_size = qconfig.min_contig
    contigs_sizes_str.append('var minContigSize = {min_contig_size};'.format(**locals()))
    contigs_sizes_lines = []
    total_len = 0
    for assembly in contigs_by_assemblies:
        contigs_sizes_str.append('contig_data["{assembly}"] = [ '.format(**locals()))
        cum_length = 0
        contigs = sorted(contigs_by_assemblies[assembly], key=lambda x: x.size, reverse=True)
        not_used_nx = [nx for nx in nx_marks]
        for i, alignment in enumerate(contigs):
            end_contig = cum_length + alignment.size
            marks = []
            for nx in not_used_nx:
                if assemblies_n50[assembly][nx] == alignment.size and \
                        (i + 1 >= len(contigs) or contigs[i+1].size != alignment.size):
                    marks.append(nx)
            marks = ', '.join(marks)
            if marks:
                contigs_sizes_lines.append('{{assembly: "{assembly}", corr_end: {end_contig}, label: "{marks}"}}'.format(**locals()))
                not_used_nx = [nx for nx in not_used_nx if nx not in marks]
            marks = ', marks: "' + marks + '"' if marks else ''
            contigs_sizes_str.append('{name: "' + alignment.name + '", size: ' + str(alignment.size) + marks + '},')
            cum_length = end_contig
        total_len = max(total_len, cum_length)
        contigs_sizes_str[-1] = contigs_sizes_str[-1][:-1] + '];\n\n'
    contigs_sizes_str = '\n'.join(contigs_sizes_str)
    contigs_sizes_str += 'var contigLines = [' + ','.join(contigs_sizes_lines) + '];\n\n'
    contigs_sizes_str += 'var contigs_total_len = {total_len};\n'.format(**locals())

    with open(html_saver.get_real_path('_chr_templ.html'), 'r') as template:
        with open(os.path.join(output_all_files_dir_path, qconfig.contig_size_viewer_fname), 'w') as result:
            for line in template:
                if line.find('<!--- data: ---->') != -1:
                    result.write(assemblies_data)
                    result.write(contigs_sizes_str)
                    result.write(assemblies_contig_size_data)
                elif line.find('<!--- Contig size threshold: ---->') != -1:
                    result.write('<div align="center" style="margin-top: 7px;">Hide contigs < '
                                 '<input class="textBox" id="input_contig_threshold" type="text" size="5" /> bp </div>')
                elif line.find('<!--- css: ---->') != -1:
                    result.write(open(html_saver.get_real_path(os.path.join('static', 'contig_alignment_plot.css'))).read())
                    result.write(open(html_saver.get_real_path(os.path.join('static', 'common.css'))).read())
                    result.write(open(html_saver.get_real_path(os.path.join('static', 'bootstrap', 'bootstrap.css'))).read())
                elif line.find('<!--- scripts: ---->') != -1:
                    result.write(open(html_saver.get_real_path(os.path.join('static', 'd3.js'))).read())
                    result.write(open(html_saver.get_real_path(os.path.join('static', 'scripts',
                                                                            'contig_alignment_plot_script.js'))).read())
                elif line.find('<!--- menu: ---->') != -1:
                    result.write(main_menu_link)
                elif line.find('<!--- title: ---->') != -1:
                    result.write('Icarus')
                elif line.find('<!--- subtitle: ---->') != -1:
                    result.write('Contig size viewer')
                else:
                    result.write(line)

    icarus_links = defaultdict(list)
    if len(chr_full_names) > 1:
        chr_link = qconfig.alignment_summary_fname
        #icarus_def = 'Icarus: interactive contig assessment viewer'
        icarus_links["links"].append(chr_link)
        icarus_links["links_names"].append('Icarus main menu')

    with open(html_saver.get_real_path(qconfig.alignment_summary_template_fname), 'r') as template:
        with open(summary_path, 'w') as result:
            num_aligned_assemblies = [len(aligned_assemblies[chr]) for chr in chr_full_names]
            is_unaligned_asm_exists = len(set(num_aligned_assemblies)) > 1
            for line in template:
                if line.find('<!--- css: ---->') != -1:
                    result.write(open(html_saver.get_real_path(os.path.join('static', 'contig_alignment_plot.css'))).read())
                    result.write(open(html_saver.get_real_path(os.path.join('static', 'common.css'))).read())
                    result.write(open(html_saver.get_real_path(os.path.join('static', 'bootstrap', 'bootstrap.css'))).read())
                elif line.find('<!--- scripts: ---->') != -1:
                    result.write(open(html_saver.get_real_path(os.path.join('static', 'jquery-1.8.2.js'))).read())
                    result.write(open(html_saver.get_real_path(os.path.join('static', 'bootstrap', 'bootstrap.min.js'))).read())
                elif line.find('<!--- assemblies: ---->') != -1:
                    labels = [qconfig.assembly_labels_by_fpath[contigs_fpath] for contigs_fpath in contigs_fpaths]
                    result.write('Assemblies: ' + ', '.join(labels))
                elif line.find('<!--- th_assemblies: ---->') != -1:
                    if is_unaligned_asm_exists:
                        result.write('<th># assemblies</th>')
                elif line.find('<!--- references: ---->') != -1:
                    for chr in sorted(chr_full_names):
                        result.write('<tr>')
                        short_chr = chr[:30]
                        if len(chr_full_names) == 1:
                            html_name = qconfig.one_alignment_viewer_name
                            contig_alignment_name = qconfig.contig_alignment_viewer_name
                            chr_link = os.path.join(qconfig.alignment_plots_dirname, '{html_name}.html'.format(**locals()))
                            icarus_links["links"].append(chr_link)
                            icarus_links["links_names"].append(contig_alignment_name)
                        else:
                            chr_link = os.path.join(qconfig.alignment_plots_dirname, '{short_chr}.html'.format(**locals()))
                        chr_name = chr.replace('_', ' ')
                        tooltip = ''
                        if len(chr_name) > 50:
                            short_name = chr[:50]
                            tooltip = 'data-toggle="tooltip" title="{chr_name}">'
                            chr_name = '{short_name}...'.format(**locals())
                        aligned_lengths = [aligned_len for aligned_len in aligned_bases_by_chr[chr] if aligned_len is not None]
                        chr_genome = sum(aligned_lengths) * 100.0 / (chr_sizes[chr] * len(contigs_fpaths))
                        chr_size = chr_sizes[chr]
                        result.write('<td><a href="{chr_link}">{chr_name}</a></td>'.format(**locals()))
                        result.write('<td>%s</td>' % num_contigs[chr])
                        result.write('<td>%s</td>' % format_long_numbers(chr_size))
                        if is_unaligned_asm_exists:
                            result.write('<td>%s</td>' % len(aligned_assemblies[chr]))
                        result.write('<td>%.3f</td>' % chr_genome)
                        result.write('<td>%s</td>' % num_misassemblies[chr])
                        result.write('</tr>')
                elif line.find('<!--- links: ---->') != -1:
                    contig_size_name = qconfig.contig_size_viewer_name
                    contig_size_browser_fname = os.path.join(qconfig.alignment_plots_dirname, qconfig.contig_size_viewer_fname)
                    icarus_links["links"].append(contig_size_browser_fname)
                    icarus_links["links_names"].append(contig_size_name)
                    contig_size_browser_link = '<tr><td><a href="{contig_size_browser_fname}">{contig_size_name}</a></td></tr>'.format(**locals())
                    result.write(contig_size_browser_link)
                    result.write('<tr><td><a href="%s">QUAST report</a></td></tr>' % html_saver.report_fname)
                    result.write('</span>')
                else:
                    result.write(line)

    html_saver.save_icarus_links(output_dirpath, icarus_links)
    return summary_path