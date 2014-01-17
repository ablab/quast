#!/usr/bin/python -O

############################################################################
# Copyright (c) 2011-2014 Saint-Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################


from __future__ import with_statement
from libs import qconfig, qutils, fastaparser
import os
import math

from libs.log import get_logger
logger = get_logger(qconfig.LOGGER_DEFAULT_NAME)

MAX_REF_NAME = 20

from libs import plotter  # Do not remove this line! It would lead to a warning in matplotlib.
if not plotter.matplotlib_error:
    import matplotlib
    import matplotlib.patches
    import matplotlib.pyplot
    import matplotlib.lines


def get_similar_threshold(total):
    if total <= 2:
        return 2
    return total / 2 + total % 2


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

        self.contigEdgeDelta = 3000
        self.minSimilarContig = 10000

        self.minConnectedBlock = 2000
        self.maxBlockGap = 10000

        self.drawArcs = False
        self.analyzeSimilar = False


class Alignment:
    def __init__(self, name, start, end, is_rc, position_in_conitg):
        self.name = name
        self.start = start
        self.end = end
        self.is_rc = is_rc
        self.position_in_contig = position_in_conitg

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
        return abs(alignment.start - self.start) <= settings.contigEdgeDelta and \
               abs(alignment.end - self.end) <= settings.contigEdgeDelta


class Arc:
    def __init__(self, c1, c2):
        self.c1 = c1
        self.c2 = c2


class Contig:
    def __init__(self, name):
        self.name = name
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
            sortedBlocks = sorted(contig.alignments, key=lambda x: self.alignments[x].position_in_contig)

            joinedAlignments = []
            currentStart = 0
            currentCStart = 0

            i = 0
            while i < len(sortedBlocks):
                block = sortedBlocks[i]

                currentStart = self.alignments[block].start
                currentCStart = self.alignments[block].position_in_contig

                while i < len(sortedBlocks) - 1 and abs(self.alignments[sortedBlocks[i]].end - self.alignments[
                    sortedBlocks[i + 1]].start) < settings.maxBlockGap and self.alignments[sortedBlocks[i]].is_rc == \
                        self.alignments[sortedBlocks[i + 1]].is_rc:
                    i += 1

                if self.alignments[sortedBlocks[i]].end - currentStart < settings.minConnectedBlock:
                    i += 1
                    continue

                joinedAlignments.append(
                    Alignment("", currentStart, self.alignments[sortedBlocks[i]].end, self.alignments[sortedBlocks[i]].is_rc,
                              currentCStart))
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
    if plotter.matplotlib_error:
        return

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

    v = Visualizer(assemblies, coverage_hist, settings, sorted_ref_names, sorted_ref_lengths, virtual_genome_shift)
    v.visualize()
    return v.save(output_fpath)


def do(contigs_fpaths, contig_report_fpath_pattern, output_dirpath,
       ref_fpath, arcs=False, similar=False, coverage_hist=None):
    lists_of_aligned_blocks = []

    total_genome_size = 0
    reference_chromosomes = dict()
    for name, seq in fastaparser.read_fasta(ref_fpath):
        chr_name = name.split()[0]
        chr_len = len(seq)
        total_genome_size += chr_len
        reference_chromosomes[chr_name] = chr_len
    virtual_genome_shift = int(0.1 * total_genome_size)
    sorted_ref_names = sorted(reference_chromosomes, key=reference_chromosomes.get, reverse=True)
    sorted_ref_lengths = sorted(reference_chromosomes.values(), reverse=True)
    cumulative_ref_lengths = [0]
    for length in sorted(reference_chromosomes.values(), reverse=True):
        cumulative_ref_lengths.append(cumulative_ref_lengths[-1] + virtual_genome_shift + length)
    virtual_genome_size = cumulative_ref_lengths[-1] - virtual_genome_shift

    for contigs_fpath in contigs_fpaths:
        report_fpath = contig_report_fpath_pattern % qutils.name_from_fpath(contigs_fpath)
        aligned_blocks = parse_nucmer_contig_report(report_fpath, sorted_ref_names, cumulative_ref_lengths)
        if aligned_blocks is None:
            return None
        lists_of_aligned_blocks.append(aligned_blocks)

    plot_fpath = draw_alignment_plot(
        contigs_fpaths, virtual_genome_size, sorted_ref_names, sorted_ref_lengths, virtual_genome_shift, output_dirpath,
        lists_of_aligned_blocks, arcs, similar, coverage_hist)
    return plot_fpath


def parse_nucmer_contig_report(report_fpath, sorted_ref_names, cumulative_ref_lengths):
    aligned_blocks = []

    with open(report_fpath) as report_file:
        misassembled_contigs_ids = []

        for line in report_file:
            if line.startswith('Analyzing contigs...'):
                break

        cur_contig_id = ''
        for line in report_file:
            if line.startswith('CONTIG:'):
                cur_contig_id = line.split('CONTIG:')[1].strip()

            if (line.find('Extensive misassembly') != -1) and (cur_contig_id != ''):
                misassembled_contigs_ids.append(cur_contig_id.split()[0])
                cur_contig_id = ''

            if line.startswith('Analyzing coverage...'):
                break

        cur_shift = 0
        for line in report_file:
            split_line = line.strip().split(' ')
            if split_line and split_line[0] == 'Reference':
                ref_name = split_line[1][:-1]
                if ref_name in sorted_ref_names:
                    cur_shift = cumulative_ref_lengths[sorted_ref_names.index(ref_name)]
                else:
                    logger.warning('reference name ' + ref_name + ' not found in file with reference!\nCannot draw contig alignment plot!')
                    return None
            elif split_line and split_line[0] == 'Align':
                start = int(split_line[2]) + cur_shift
                end = int(split_line[3]) + cur_shift
                contig_id = split_line[4]
                start_in_contig = int(split_line[5])
                end_in_contig = int(split_line[6])

                is_rc = ((start - end) * (start_in_contig - end_in_contig)) < 0
                block = Alignment(
                    contig_id, start, end, is_rc,
                    position_in_conitg=min(start_in_contig, end_in_contig))

                if contig_id in misassembled_contigs_ids:
                    block.misassembled = True

                aligned_blocks.append(block)

    return aligned_blocks
