#!/usr/bin/python -O

############################################################################
# Copyright (c) 2015-2016 Saint Petersburg State University
# Copyright (c) 2011-2015 Saint Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################


from __future__ import with_statement

from collections import defaultdict, OrderedDict

from libs import qconfig, qutils, fastaparser, genome_analyzer, contigs_analyzer
import os
import math
import libs.html_saver.html_saver as html_saver
import libs.html_saver.json_saver as json_saver
from shutil import copyfile

from libs import reporting
from libs.log import get_logger
logger = get_logger(qconfig.LOGGER_DEFAULT_NAME)

MAX_REF_NAME = 20
summary_fname = qconfig.icarus_html_fname
main_menu_link = '<a href="../{summary_fname}" style="color: white; text-decoration: none;">Main menu</a>'.format(**locals())

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
        self.ambiguous = False

    def length(self):
        return self.end - self.start

    def annotation(self):
        return self.name + "\n" + str(self.start) + "-" + str(self.end)

    def center(self):
        return (self.end + self.start) / 2

    def compare_inexact(self, alignment, settings):
        if alignment.ref_name != self.ref_name:
            return False
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
       ref_fpath, cov_fpath=None, arcs=False, stdout_pattern=None, similar=False, features=None, coverage_hist=None,
       json_output_dir=None):
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
        plot_fpath, assemblies = draw_alignment_plot(
            contigs_fpaths, virtual_genome_size, sorted_ref_names, sorted_ref_lengths, virtual_genome_shift, output_dirpath,
            lists_of_aligned_blocks, arcs, similar, coverage_hist)
    if (assemblies or contigs_by_assemblies) and qconfig.create_icarus_html:
        icarus_html_fpath = js_data_gen(assemblies, contigs_fpaths, contig_report_fpath_pattern, reference_chromosomes,
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
                _, name, size = split_line
                contig = Contig(name=name, size=int(size))
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


def add_contig(cum_length, contig, not_used_nx, assemblies_n50, assembly, contigs, contig_size_lines, num, only_nx=False):
    end_contig = cum_length + contig.size
    marks = []
    align = None
    for nx in not_used_nx:
        if assemblies_n50[assembly][nx] == contig.size and \
                (num + 1 >= len(contigs) or contigs[num + 1].size != contig.size):
            marks.append(nx)
    marks = ', '.join(marks)
    if marks:
        contig_size_lines.append('{{assembly: "{assembly}", corr_end: {end_contig}, label: "{marks}", size: {contig.size}}}'.format(**locals()))
        not_used_nx = [nx for nx in not_used_nx if nx not in marks]
    marks = ', marks: "' + marks + '"' if marks else ''
    if not only_nx or marks:
        align = '{name: "' + contig.name + '", size: ' + str(contig.size) + marks + '},'
    return end_contig, contig_size_lines, align, not_used_nx


def parse_cov_fpath(cov_fpath, chr_names, chr_full_names):
    if not cov_fpath:
        return None, None, None
    cov_data = defaultdict(list)
    #not_covered = defaultdict(list)
    cur_len = defaultdict(int)
    max_depth = defaultdict(int)
    cov_factor = 10
    with open(cov_fpath, 'r') as coverage:
        contig_to_chr = {}
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
            c = list(line.split())
            name = contig_to_chr[qutils.correct_name(c[0])]
            cur_len[name] += int(c[2])
            if (index + 1) % cov_factor == 0 and index > 0:
                cur_depth = cur_len[name] / cov_factor
                max_depth[chr] = max(cur_depth, max_depth[chr])
                cov_data[name].append(cur_depth)
                cur_len[name] = 0
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
            assemblies_data += 'assemblies_links["{label}"] = "{contig_stdout_fpath}";\n'.format(**locals())
        assemblies_contig_size_data += 'assemblies_len["{label}"] = {l};\n'.format(**locals())
        assemblies_contig_size_data += 'assemblies_contigs["{label}"] = {contigs};\n'.format(**locals())
        assemblies_contig_size_data += 'assemblies_n50["{label}"] = "{n50}";\n'.format(**locals())
        for nx in nx_marks:
            assemblies_n50[label][nx] = report.get_field(nx)
    return assemblies_data, assemblies_contig_size_data, assemblies_n50


def get_contigs_data(contigs_by_assemblies, nx_marks, assemblies_n50):
    contigs_sizes_str = ['var contig_data = {};']
    contigs_sizes_str.append('var CHROMOSOME;')
    contigs_sizes_lines = []
    total_len = 0
    min_contig_size = qconfig.min_contig
    too_many_contigs = False
    for assembly in contigs_by_assemblies:
        contigs_sizes_str.append('contig_data["{assembly}"] = [ '.format(**locals()))
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
                                                                assembly, contigs, contigs_sizes_lines, i)
            contigs_sizes_str.append(align)
        if len(contigs) > qconfig.max_contigs_num_for_size_viewer:
            assembly_len = cum_length
            remained_len = sum(alignment.size for alignment in contigs[last_contig_num:])
            cum_length += remained_len
            remained_contigs_name = str(len(contigs) - last_contig_num) + ' hidden contigs shorter than ' + str(contig_threshold) + \
                                    ' bp (total length: ' + format_long_numbers(remained_len) + ' bp)'
            contigs_sizes_str.append(('{{name: "' + remained_contigs_name + '", size: ' + str(remained_len) +
                                     ', type:"small_contigs"}},').format(**locals()))
        if not_used_nx and last_contig_num < len(contigs):
            for i, alignment in enumerate(contigs[last_contig_num:]):
                if not not_used_nx:
                    break
                assembly_len, contigs_sizes_lines, align, not_used_nx = add_contig(assembly_len, alignment, not_used_nx, assemblies_n50,
                                                                    assembly, contigs, contigs_sizes_lines, last_contig_num + i, only_nx=True)
        total_len = max(total_len, cum_length)
        contigs_sizes_str[-1] = contigs_sizes_str[-1][:-1] + '];\n\n'
    contigs_sizes_str = '\n'.join(contigs_sizes_str)
    contigs_sizes_str += 'var contigLines = [' + ','.join(contigs_sizes_lines) + '];\n\n'
    contigs_sizes_str += 'var contigs_total_len = {total_len};\n'.format(**locals())
    contigs_sizes_str += 'var minContigSize = {min_contig_size};'.format(**locals())
    return contigs_sizes_str, too_many_contigs


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
                features_data += '{{name: "{name}", start: {region.start}, end: {region.end}, id_: "{region.id}", ' \
                                 'kind: "{feature_container.kind}", chr:{ref_id}}},'.format(**locals())
            containers_kind.append(feature_container.kind)
            features_data += '],'
        features_data = features_data[:-1] + '];\n'
    return features_data


def save_alignment_data_for_one_ref(chr, chr_full_names, ref_contigs, chr_lengths, data_str, chr_to_aligned_blocks,
                                    structures_by_labels, output_dir_path=None, ref_data=None, features_data=None,
                                    assemblies_data=None, cov_data=None, not_covered=None, max_depth=None):
    short_chr = chr[:30]
    if len(chr_full_names) == 1:
        short_chr = qconfig.one_alignment_viewer_name

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
    data_str.append('var oneHtml = "{is_one_html}";'.format(**locals()))
    # adding assembly data
    data_str.append('var contig_data = {};')
    data_str.append('contig_data["{chr}"] = {{}};'.format(**locals()))
    assemblies_len = defaultdict(int)
    assemblies_contigs = defaultdict(set)
    ms_types = dict()
    for assembly in chr_to_aligned_blocks.keys():
        data_str.append('contig_data["{chr}"]["{assembly}"] = [ '.format(**locals()))
        ms_types[assembly] = defaultdict(int)
        for num_contig, ref_contig in enumerate(ref_contigs):
            if ref_contig in chr_to_aligned_blocks[assembly]:
                prev_end = None
                for alignment in sorted(chr_to_aligned_blocks[assembly][ref_contig], key=lambda x: x.start):
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
                            corr_el_start = el.start
                            corr_el_end = el.end
                            data_str.append('{type: "A",corr_start: ' + str(corr_el_start) + ',corr_end: ' +
                                            str(corr_el_end) + ',start:' + str(el.start) + ',end:' + str(el.end) +
                                            ',start_in_contig:' + str(el.start_in_contig) + ',end_in_contig:' +
                                            str(el.end_in_contig) + ',IDY:' + el.idy + ',chr: "' + el.ref_name + '"},')
                        elif type(el) == str:
                            data_str.append('{{type: "M", mstype: "{el}"}},'.format(**locals()))
                    data_str[-1] = data_str[-1][:-1] + ']},'

        data_str[-1] = data_str[-1][:-1] + '];'
        assembly_len = assemblies_len[assembly]
        assembly_contigs = len(assemblies_contigs[assembly])
        local_misassemblies = ms_types[assembly]['local'] / 2
        ext_misassemblies = (sum(ms_types[assembly].values()) - ms_types[assembly]['interspecies translocation']) / 2 - \
                            local_misassemblies + ms_types[assembly]['interspecies translocation']
        additional_assemblies_data += 'assemblies_len["{assembly}"] = {assembly_len};\n'.format(**locals())
        additional_assemblies_data += 'assemblies_contigs["{assembly}"] = {assembly_contigs};\n'.format(**locals())
        additional_assemblies_data += 'assemblies_misassemblies["{assembly}"] = "{ext_misassemblies}' \
                           '+{local_misassemblies}";\n'.format(**locals())

    if contigs_analyzer.ref_labels_by_chromosomes:
        data_str.append(''.join(links_to_chromosomes))
    if cov_data:
        # adding coverage data
        data_str.append('var coverage_data = {};')
        data_str.append('var max_depth = {};')
        if cov_data[chr]:
            chr_max_depth = max_depth[chr]
            data_str.append('max_depth["{chr}"] = {chr_max_depth};'.format(**locals()))
            data_str.append('coverage_data["{chr}"] = [ '.format(**locals()))
            for e in cov_data[chr]:
                data_str.append('{e},'.format(**locals()))
            data_str[-1] = data_str[-1][:-1] + '];'

        data_str.append('var not_covered = {};')
        data_str.append('not_covered["{chr}"] = [ '.format(**locals()))
        # if len(not_covered[chr]) > 0:
        #     for e in not_covered[chr]:
        #         data_str.append('{e},'.format(**locals()))
        #     data_str[-1] = data_str[-1][:-1]
        data_str[-1] += '];'
    data_str = '\n'.join(data_str)

    misassemblies_types = ['relocation', 'translocation', 'inversion', 'interspecies translocation', 'local']
    if not qconfig.is_combined_ref:
        misassemblies_types.remove('interspecies translocation')
    with open(html_saver.get_real_path('_chr_templ.html'), 'r') as template:
        with open(os.path.join(output_dir_path, '{short_chr}.html'.format(**locals())), 'w') as result:
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
                    ms_counts_by_type = OrderedDict()
                    for ms_type in misassemblies_types:
                        factor = 1 if ms_type == 'interspecies translocation' else 2
                        ms_counts_by_type[ms_type] = sum(ms_types[assembly][ms_type] / factor for assembly in chr_to_aligned_blocks.keys())
                    total_ms_count = sum(ms_counts_by_type.values()) - ms_counts_by_type['local']
                    result.write('Show misassemblies: '.format(**locals()))
                    for ms_type, ms_count in ms_counts_by_type.items():
                        is_checked = 'checked="checked"'  #if ms_count > 0 else ''
                        ms_name = ms_type
                        if ms_type != 'local':
                            if ms_count != 1:
                                ms_name += 's'
                            result.write('<label><input type="checkbox" id="{ms_type}" name="misassemblies_select" '
                                 '{is_checked}/>{ms_name} ({ms_count})</label>'.format(**locals()))
                        else:
                            result.write('<label><input type="checkbox" id="{ms_type}" name="misassemblies_select" '
                                 '{is_checked}/>{ms_name} ({ms_count})</label>'.format(**locals()))
                elif line.find('<!--- css: ---->') != -1:
                    result.write(html_saver.css_html(os.path.join('static', qconfig.icarus_css_name)))
                    result.write(html_saver.css_html(os.path.join('static', 'common.css')))
                    result.write(html_saver.css_html(os.path.join('static', 'bootstrap', 'bootstrap.css')))
                elif line.find('<!--- scripts: ---->') != -1:
                    result.write(html_saver.js_html(os.path.join('static', 'd3.js')))
                    result.write(html_saver.js_html(os.path.join('static', 'scripts', qconfig.icarus_script_name)))
                elif line.find('<!--- reference: ---->') != -1:
                    chr_name = chr.replace('_', ' ')
                    # if len(chr_name) > 120:
                    #     chr_name = chr_name[:90] + '...'
                    result.write('<div class="reftitle">')
                    result.write('<b>Contig alignment viewer.</b> Contigs aligned to "' + chr_name + '"')
                    result.write('</div>')
                elif line.find('<!--- menu: ---->') != -1:
                    result.write(main_menu_link)
                else:
                    result.write(line)
    return num_misassemblies, aligned_assemblies


def get_info_by_chr(chr, aligned_bases_by_chr, chr_sizes, contigs_fpaths, one_chromosome=False):
    short_chr = chr[:30]
    if one_chromosome:
        html_name = qconfig.one_alignment_viewer_name
        chr_link = os.path.join(qconfig.icarus_dirname, '{html_name}.html'.format(**locals()))
    else:
        chr_link = os.path.join(qconfig.icarus_dirname, '{short_chr}.html'.format(**locals()))
    chr_name = chr.replace('_', ' ')
    tooltip = ''
    if len(chr_name) > 50:
        short_name = chr[:50]
        tooltip = 'data-toggle="tooltip" title="{chr_name}">'
        chr_name = '{short_name}...'.format(**locals())
    aligned_lengths = [aligned_len for aligned_len in aligned_bases_by_chr[chr] if aligned_len is not None]
    chr_genome = sum(aligned_lengths) * 100.0 / (chr_sizes[chr] * len(contigs_fpaths))
    chr_size = chr_sizes[chr]
    return chr_link, chr_name, chr_genome, chr_size


def js_data_gen(assemblies, contigs_fpaths, contig_report_fpath_pattern, chromosomes_length, output_dirpath, structures_by_labels,
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

    summary_path = os.path.join(output_dirpath, summary_fname)
    output_all_files_dir_path = os.path.join(output_dirpath, qconfig.icarus_dirname)
    if not os.path.exists(output_all_files_dir_path):
        os.mkdir(output_all_files_dir_path)
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
        ref_data += 'references_id["{chr}"] = {i};\n'.format(**locals())
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
            data_str.append('chromosomes_len["{ref_contig}"] = {l};'.format(**locals()))
            aligned_bases_by_chr[chr].extend(aligned_bases[ref_contig])
        num_misassemblies[chr], aligned_assemblies[chr] = save_alignment_data_for_one_ref(chr, chr_full_names, ref_contigs, chr_lengths,
                                                          data_str, chr_to_aligned_blocks, structures_by_labels,
                                                          ref_data=ref_data, features_data=features_data, assemblies_data=assemblies_data,
                                                          cov_data=cov_data, not_covered=not_covered, max_depth=max_depth, output_dir_path=output_all_files_dir_path)

    contigs_sizes_str, too_many_contigs = get_contigs_data(contigs_by_assemblies, nx_marks, assemblies_n50)
    with open(html_saver.get_real_path('_chr_templ.html'), 'r') as template:
        with open(os.path.join(output_all_files_dir_path, qconfig.contig_size_viewer_fname), 'w') as result:
            for line in template:
                if line.find('<!--- data: ---->') != -1:
                    result.write(assemblies_data)
                    result.write(contigs_sizes_str)
                    result.write(assemblies_contig_size_data)
                elif line.find('<!--- Contig size threshold: ---->') != -1:
                    result.write('Fade contigs shorter than <input class="textBox" '
                                 'id="input_contig_threshold" type="text" size="5" /> bp </span>')
                elif line.find('<!--- css: ---->') != -1:
                    result.write(html_saver.css_html(os.path.join('static', qconfig.icarus_css_name)))
                    result.write(html_saver.css_html(os.path.join('static', 'common.css')))
                    result.write(html_saver.css_html(os.path.join('static', 'bootstrap', 'bootstrap.css')))
                elif line.find('<!--- scripts: ---->') != -1:
                    result.write(html_saver.js_html(os.path.join('static', 'd3.js')))
                    result.write(html_saver.js_html(os.path.join('static', 'scripts', qconfig.icarus_script_name)))
                elif line.find('<!--- menu: ---->') != -1:
                    result.write(main_menu_link)
                elif line.find('<!--- reference: ---->') != -1:
                    result.write('<div class="reftitle">')
                    result.write('<b>Contig size viewer. </b>')
                    if too_many_contigs:
                        result.write('For better performance, only largest %s contigs of each assembly were loaded' %
                                 str(qconfig.max_contigs_num_for_size_viewer))
                    result.write('</div>')
                else:
                    result.write(line)

    icarus_links = defaultdict(list)
    if len(chr_full_names) > 1:
        chr_link = qconfig.icarus_html_fname
        icarus_links["links"].append(chr_link)
        icarus_links["links_names"].append(qconfig.icarus_link)

    with open(html_saver.get_real_path(qconfig.icarus_menu_template_fname), 'r') as template:
        with open(summary_path, 'w') as result:
            num_aligned_assemblies = [len(aligned_assemblies[chr]) for chr in chr_full_names]
            is_unaligned_asm_exists = len(set(num_aligned_assemblies)) > 1
            for line in template:
                if line.find('<!--- css: ---->') != -1:
                    result.write(html_saver.css_html(os.path.join('static', qconfig.icarus_css_name)))
                    result.write(html_saver.css_html(os.path.join('static', 'common.css')))
                    result.write(html_saver.css_html(os.path.join('static', 'bootstrap', 'bootstrap.css')))
                elif line.find('<!--- scripts: ---->') != -1:
                    result.write(html_saver.js_html(os.path.join('static', 'd3.js')))
                    result.write(html_saver.js_html(os.path.join('static', 'scripts', qconfig.icarus_script_name)))
                elif line.find('<!--- assemblies: ---->') != -1:
                    labels = [qconfig.assembly_labels_by_fpath[contigs_fpath] for contigs_fpath in contigs_fpaths]
                    result.write('<b>Assemblies: </b>' + ', '.join(labels))
                elif line.find('<!--- div_references: ---->') != -1:
                    if chr_full_names and len(chr_full_names) > 1:
                        result.write('<div>')
                    else:
                        if chr_full_names:
                            chr = chr_full_names[0]
                            chr_link, chr_name, chr_genome, chr_size = get_info_by_chr(chr, aligned_bases_by_chr, chr_sizes,
                                                                                       contigs_fpaths, one_chromosome=True)
                            viewer_name = qconfig.contig_alignment_viewer_name
                            viewer_link = '<a href="{chr_link}">{viewer_name}</a>'.format(**locals())
                            viewer_info = viewer_link + \
                                  '<div class="reference_details">' \
                                      '<p>Aligned to sequences from  ' + os.path.basename(ref_fpath) + '</p>' \
                                      '<p>Fragments: ' + str(num_contigs[chr]) + ', length: ' + format_long_numbers(chr_size) + \
                                        ('bp, mean genome fraction: %.3f' % chr_genome) + '%, misassembled blocks: ' + str(num_misassemblies[chr]) + '</p>' + \
                                  '</div>'
                            icarus_links["links"].append(chr_link)
                            icarus_links["links_names"].append(qconfig.icarus_link)
                            result.write('<div class="contig_alignment_viewer_panel">')
                            result.write(viewer_info)
                            result.write('</div>')
                        result.write('<div style="display:none;">')
                elif line.find('<!--- th_assemblies: ---->') != -1:
                    if is_unaligned_asm_exists:
                        result.write('<th># assemblies</th>')
                elif line.find('<!--- references: ---->') != -1 and len(chr_full_names) > 1:
                    for chr in sorted(chr_full_names):
                        chr_link, chr_name, chr_genome, chr_size = get_info_by_chr(chr, aligned_bases_by_chr, chr_sizes, contigs_fpaths)
                        result.write('<!--- reference:%s ---->' % chr)
                        result.write('<tr>')
                        result.write('<td><a href="{chr_link}">{chr_name}</a></td>'.format(**locals()))
                        result.write('<td>%s</td>' % num_contigs[chr])
                        result.write('<td>%s</td>' % format_long_numbers(chr_size))
                        if is_unaligned_asm_exists:
                            result.write('<td>%s</td>' % len(aligned_assemblies[chr]))
                        result.write('<td>%.3f</td>' % chr_genome)
                        result.write('<td>%s</td>' % num_misassemblies[chr])
                        result.write('</tr>\n')
                elif line.find('<!--- links: ---->') != -1:
                    contig_size_name = qconfig.contig_size_viewer_name
                    contig_size_browser_fname = os.path.join(qconfig.icarus_dirname, qconfig.contig_size_viewer_fname)
                    if not chr_names:
                        icarus_links["links"].append(contig_size_browser_fname)
                        icarus_links["links_names"].append(qconfig.icarus_link)
                    contig_size_browser_link = '<tr><td><a href="{contig_size_browser_fname}">{contig_size_name}</a></td></tr>'.format(**locals())
                    result.write(contig_size_browser_link)
                    result.write('<tr><td><a href="%s">QUAST report</a></td></tr>' % html_saver.report_fname)
                    result.write('</span>')
                else:
                    result.write(line)

    html_saver.save_icarus_links(output_dirpath, icarus_links)
    if json_output_dir:
        json_saver.save_icarus_links(json_output_dir, icarus_links)

    return summary_path