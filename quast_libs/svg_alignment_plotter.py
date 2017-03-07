#!/usr/bin/python -O

############################################################################
# Copyright (c) 2015-2017 Saint Petersburg State University
# Copyright (c) 2011-2015 Saint Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################


from __future__ import with_statement

import math
import os

from quast_libs import plotter  # Do not remove this line! It would lead to a warning in matplotlib.
if plotter.can_draw_plots:
    import matplotlib
    import matplotlib.patches
    import matplotlib.pyplot
    import matplotlib.lines

MAX_REF_NAME = 20


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


def draw_alignment_plot(assemblies, virtual_genome_size, output_dirpath, sorted_ref_names, sorted_ref_lengths, virtual_genome_shift,
                        coverage_hist=None, arcs=None):
    if not plotter.can_draw_plots:
        return None

    output_fpath = os.path.join(output_dirpath, 'alignment')
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

    assemblies.apply_colors(settings)

    v = Visualizer(assemblies, coverage_hist, settings, sorted_ref_names, sorted_ref_lengths, virtual_genome_shift)
    v.visualize()
    plot_fpath = v.save(output_fpath)
    return plot_fpath