############################################################################
# Copyright (c) 2015-2017 Saint Petersburg State University
# Copyright (c) 2011-2015 Saint Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################

from __future__ import with_statement

import os
from collections import defaultdict
from os.path import join, exists

from quast_libs import qutils, qconfig
from quast_libs.fastaparser import get_chr_lengths_from_fastafile
from quast_libs.icarus_utils import get_assemblies, check_misassembled_blocks, Alignment
from quast_libs.qutils import get_path_to_program, is_non_empty_file

circos_png_fname = 'circos.png'
TRACK_WIDTH = 0.04
TRACK_INTERVAL = 0.02


def create_ideogram(chr_lengths, output_dir):
    num_chromosomes = 0
    max_len = 0
    karyotype_fpath = join(output_dir, 'reference.karyotype.txt')
    with open(karyotype_fpath, 'w') as out_f:
        for name, seq_len in chr_lengths.items():
            out_f.write('\t'.join(['chr', '-', name, name, '0', str(seq_len), 'blue']) + '\n')
            max_len = max(max_len, seq_len)
            num_chromosomes += 1

    ideogram_fpath = join(output_dir, 'ideogram.conf')
    with open(ideogram_fpath, 'w') as out_f:
        out_f.write('<ideogram>\n')
        out_f.write('<spacing>\n')
        if qconfig.prokaryote and num_chromosomes == 1:
            out_f.write('default = 0r\n')  # circular chromosome
        elif num_chromosomes <= 30:
            out_f.write('default = 0.005r\n')
        elif num_chromosomes <= 100:
            out_f.write('default = 0.001r\n')
        else:
            out_f.write('default = 0.0005r\n')
        out_f.write('break = 0.005r\n')
        out_f.write('</spacing>\n')
        out_f.write('thickness = 40p\n')
        out_f.write('stroke_thickness = 2\n')
        out_f.write('stroke_color = black\n')
        out_f.write('fill = yes\n')
        out_f.write('fill_color = blue\n')
        out_f.write('radius = 0.85r\n')
        out_f.write('show_label = no\n')
        out_f.write('label_font = default\n')
        out_f.write('label_radius = dims(ideogram,radius) + 0.05r\n')
        out_f.write('label_size = 36\n')
        out_f.write('label_parallel = yes\n')
        out_f.write('band_stroke_thickness = 2\n')
        out_f.write('show_bands = yes\n')
        out_f.write('fill_bands = yes\n')
        out_f.write('</ideogram>')
    return max_len, karyotype_fpath, ideogram_fpath


def create_ticks_conf(chrom_units, output_dir):
    ticks_fpath = join(output_dir, 'ticks.conf')
    with open(ticks_fpath, 'w') as out_f:
        out_f.write('show_ticks = yes\n')
        out_f.write('show_tick_labels = yes\n')
        out_f.write('show_grid = no\n')
        out_f.write('<ticks>\n')
        out_f.write('skip_first_label = yes\n')
        out_f.write('skip_last_label = no\n')
        out_f.write('radius = dims(ideogram,radius_outer)\n')
        out_f.write('tick_separation = 2p\n')
        out_f.write('min_label_distance_to_edge = 0p\n')
        out_f.write('label_separation = 5p\n')
        out_f.write('label_offset = 5p\n')
        out_f.write('label_size = 12p\n')
        out_f.write('thickness = 3p\n')
        if chrom_units * 10 >= 10 ** 6:
            label_multiplier = 1.0 / (chrom_units * 10)
            suffix = 'Mbp'
        else:
            label_multiplier = 1.0 / chrom_units
            suffix = 'Kbp'
        out_f.write('label_multiplier = ' + str(label_multiplier) + '\n')
        out_f.write('<tick>\n')
        out_f.write('spacing = 1u\n')
        out_f.write('color = dgrey\n')
        out_f.write('size = 12p\n')
        out_f.write('show_label = no\n')
        out_f.write('format = %s\n')
        out_f.write('</tick>\n')
        out_f.write('<tick>\n')
        out_f.write('spacing = 5u\n')
        out_f.write('color = black\n')
        out_f.write('size = 18p\n')
        out_f.write('show_label = yes\n')
        out_f.write('label_size = 24p\n')
        out_f.write('format = %s\n')
        out_f.write('</tick>\n')
        out_f.write('<tick>\n')
        out_f.write('spacing = 10u\n')
        out_f.write('color = black\n')
        out_f.write('size = 24p\n')
        out_f.write('show_label = yes\n')
        out_f.write('label_size = 32p\n')
        out_f.write('suffix = " %s"\n' % suffix)
        out_f.write('format = %s\n')
        out_f.write('</tick>\n')
        out_f.write('</ticks>')
    return ticks_fpath


def parse_nucmer_contig_report(report_fpath):
    aligned_blocks = []
    misassembled_id_to_structure = defaultdict(list)

    with open(report_fpath) as report_file:
        contig_id = None

        start_col = None
        end_col = None
        ref_col = None
        contig_col = None
        ambig_col = None
        best_col = None
        for i, line in enumerate(report_file):
            split_line = line.replace('\n', '').split('\t')
            if i == 0:
                start_col = split_line.index('S1')
                end_col = split_line.index('E1')
                ref_col = split_line.index('Reference')
                contig_col = split_line.index('Contig')
                idy_col = split_line.index('IDY')
                ambig_col = split_line.index('Ambiguous')
                best_col = split_line.index('Best_group')
            elif split_line and split_line[0] == 'CONTIG':
                continue
            elif split_line and len(split_line) < 5:
                misassembled_id_to_structure[contig_id].append(line.strip())
            elif split_line and len(split_line) > 5:
                start, end, ref_name, contig_id, ambiguity, is_best = int(split_line[start_col]), int(split_line[end_col]), \
                                                           split_line[ref_col], split_line[contig_col], \
                                                           split_line[ambig_col], split_line[best_col]
                block = Alignment(name=contig_id, start=start, end=end, ref_name=ref_name, is_best_set=is_best == 'True')
                block.ambiguous = ambiguity
                if block.is_best_set:
                    aligned_blocks.append(block)
                    misassembled_id_to_structure[contig_id].append(block)

    return aligned_blocks, misassembled_id_to_structure


def parse_alignments(contigs_fpaths, contig_report_fpath_pattern):
    lists_of_aligned_blocks = []
    for contigs_fpath in contigs_fpaths:
        if contig_report_fpath_pattern:
            report_fpath = contig_report_fpath_pattern % qutils.label_from_fpath_for_fname(contigs_fpath)
            aligned_blocks, misassembled_id_to_structure = parse_nucmer_contig_report(report_fpath)
            if aligned_blocks is None:
                continue

            aligned_blocks = check_misassembled_blocks(aligned_blocks, misassembled_id_to_structure)
            lists_of_aligned_blocks.append(aligned_blocks)

    if lists_of_aligned_blocks:
        return get_assemblies(contigs_fpaths, lists_of_aligned_blocks).assemblies


def create_alignment_plots(assembly, output_dir):
    conf_fpath = join(output_dir, assembly.label + '.conf')
    with open(conf_fpath, 'w') as out_f:
        for align in assembly.alignments:
            color = 'green'
            if align.misassembled:
                color = 'red'
            elif align.ambiguous:
                color = 'purple'
            out_f.write('\t'.join([align.ref_name, str(align.start), str(align.end), 'color=' + color]) + '\n')
    return conf_fpath


def create_gc_plot(gc_fpath, output_dir):
    gc_circos_fpath = join(output_dir, 'gc.txt')
    chrom = None
    coord = 0
    n = qconfig.GC_window_size_large if qconfig.large_genome else qconfig.GC_window_size  # non-overlapping windows
    min_gc = 100
    max_gc = 0
    with open(gc_fpath) as in_f:
        with open(gc_circos_fpath, 'w') as out_f:
            for line in in_f:
                if line.startswith('#'):
                    chrom = line.split()[0][1:]
                    coord = 0
                else:
                    gc = float(line.split()[-1])
                    out_f.write('\t'.join([chrom, str(coord), str(coord + n), str(gc)]) + '\n')
                    coord += n
                    min_gc = min(min_gc, gc)
                    max_gc = max(max_gc, gc)
    min_gc /= 1.1
    max_gc *= 1.1
    return min_gc, max_gc, gc_circos_fpath


def create_genes_plot(features_containers, output_dir):
    feature_fpaths = []
    for feature_container in features_containers:
        feature_fpath = join(output_dir, feature_container.kind + '.txt')
        if len(feature_container.region_list) == 0:
            continue
        with open(feature_fpath, 'w') as out_f:
            for region in feature_container.region_list:
                chrom = region.chromosome if region.chromosome and region.chromosome in feature_container.chr_names_dict \
                    else region.seqname
                chrom = feature_container.chr_names_dict[chrom] if chrom in feature_container.chr_names_dict else None
                if not chrom:
                    continue
                out_f.write('\t'.join([chrom, str(region.start), str(region.end)]) + '\n')
        feature_fpaths.append(feature_fpath)
    return feature_fpaths


def create_genome_file(chr_lengths, output_dir):
    genome_fpath = join(output_dir, 'genome.txt')
    with open(genome_fpath, 'w') as out_f:
        for name, seq_len in chr_lengths.items():
            out_f.write('\t'.join([name, '0', str(seq_len)]) + '\n')
    return genome_fpath


def create_conf(ref_fpath, assemblies, output_dir, icarus_gc_fpath, features_containers):
    chr_lengths = get_chr_lengths_from_fastafile(ref_fpath)
    max_len, karyotype_fpath, ideogram_fpath = create_ideogram(chr_lengths, output_dir)
    if max_len >= 10 ** 6:
        chrom_units = 10 ** 5
    elif max_len >= 10 ** 5:
        chrom_units = 10 ** 4
    else:
        chrom_units = 1000
    ticks_fpath = create_ticks_conf(chrom_units, output_dir)
    min_gc, max_gc, gc_fpath = create_gc_plot(icarus_gc_fpath, output_dir)
    feature_fpaths = create_genes_plot(features_containers, output_dir)
    genome_fpath = create_genome_file(chr_lengths, output_dir)
    conf_fpath = join(output_dir, 'circos.conf')
    radius = 0.96
    plot_idx = 0
    track_intervals = [TRACK_INTERVAL] * len(assemblies)
    if feature_fpaths:
        track_intervals[-1] = TRACK_INTERVAL * 2
        track_intervals += [TRACK_INTERVAL] * len(feature_fpaths)
    track_intervals[-1] = TRACK_INTERVAL * 3
    with open(conf_fpath, 'w') as out_f:
        out_f.write('<<include etc/colors_fonts_patterns.conf>>\n')
        out_f.write('<<include %s>>\n' % ideogram_fpath)
        out_f.write('<<include %s>>\n' % ticks_fpath)
        out_f.write('karyotype = %s\n' % karyotype_fpath)
        out_f.write('chromosomes_units = %d\n' % chrom_units)
        out_f.write('chromosomes_display_default = yes\n')
        out_f.write('track_width = ' + str(TRACK_WIDTH) + '\n')
        for i in range(len(track_intervals)):
            out_f.write('track%d_pos = %f\n' % (i, radius))
            radius -= TRACK_WIDTH
            radius -= track_intervals[i]
        out_f.write('track%d_pos = %f\n' % (len(track_intervals), radius))
        out_f.write('<image>\n')
        out_f.write('dir = %s\n' % output_dir)
        out_f.write('file = %s\n' % circos_png_fname)
        out_f.write('png = yes\n')
        out_f.write('svg = no\n')
        out_f.write('radius = 1500p\n')
        out_f.write('angle_offset = -90\n')
        out_f.write('auto_alpha_colors = yes\n')
        out_f.write('auto_alpha_steps = 5\n')
        out_f.write('background = white\n')
        out_f.write('</image>\n')
        out_f.write('<highlights>\n')
        out_f.write('<highlight>\n')
        out_f.write('file = %s\n' % genome_fpath)
        out_f.write('r0 = eval(sprintf("%.3fr",conf(track0_pos)))\n')
        out_f.write('r1 = eval(sprintf("%.3fr",conf(track' + str(len(assemblies) - 1) + '_pos) - conf(track_width))) - 0.005r\n')
        out_f.write('fill_color = 255,255,240\n')
        out_f.write('</highlight>\n')
        out_f.write('</highlights>\n')
        out_f.write('<<include etc/housekeeping.conf>>\n')
        out_f.write('<plots>\n')
        out_f.write('layers_overflow = collapse\n')
        for assembly in assemblies:
            alignments_conf = create_alignment_plots(assembly, output_dir)
            out_f.write('<plot>\n')
            out_f.write('type = tile\n')
            out_f.write('thickness = 40\n')
            out_f.write('layers = 1\n')
            out_f.write('file = %s\n' % alignments_conf)
            out_f.write('r0 = eval(sprintf("%.3fr",conf(track' + str(plot_idx) + '_pos) - conf(track_width)))\n')
            out_f.write('r1 = eval(sprintf("%.3fr",conf(track' + str(plot_idx) + '_pos)))\n')
            out_f.write('</plot>\n')
            plot_idx += 1
        for feature_fpath in feature_fpaths:
            # genes plot
            out_f.write('<plot>\n')
            out_f.write('type = tile\n')
            out_f.write('thickness = 20\n')
            out_f.write('layers = 2\n')
            out_f.write('file = %s\n' % feature_fpath)
            out_f.write('color = vvdorange\n')
            out_f.write('r0 = eval(sprintf("%.3fr",conf(track' + str(plot_idx) + '_pos) - conf(track_width)))\n')
            out_f.write('r1 = eval(sprintf("%.3fr",conf(track' + str(plot_idx) + '_pos)))\n')
            out_f.write('</plot>\n')
            plot_idx += 1
        # GC plot
        out_f.write('<plot>\n')
        out_f.write('type = histogram\n')
        out_f.write('thickness = 3\n')
        out_f.write('file = %s\n' % gc_fpath)
        out_f.write('color = dgrey\n')
        out_f.write('min = %d\n' % min_gc)
        out_f.write('max = %d\n' % max_gc)
        out_f.write('r0 = eval(sprintf("%.3fr",conf(track' + str(plot_idx) + '_pos) - conf(track_width)))\n')
        out_f.write('r1 = eval(sprintf("%.3fr",conf(track' + str(plot_idx) + '_pos)))\n')
        out_f.write('<axes>\n')
        out_f.write('<axis>\n')
        out_f.write('spacing = 0.2r\n')
        out_f.write('thickness = 1r\n')
        out_f.write('color = lyellow\n')
        out_f.write('</axis>\n')
        out_f.write('</axes>\n')
        out_f.write('</plot>\n')
        out_f.write('</plots>\n')

    return conf_fpath


def do(ref_fpath, contigs_fpaths, contig_report_fpath_pattern, icarus_gc_fpath, features_containers, output_dir, logger):
    if not exists(output_dir):
        os.makedirs(output_dir)
    assemblies = parse_alignments(contigs_fpaths, contig_report_fpath_pattern)
    conf_fpath = create_conf(ref_fpath, assemblies, output_dir, icarus_gc_fpath, features_containers)
    circos_exec = get_path_to_program('circos')
    if not circos_exec:
        logger.warning('Circos is not installed!\n'
                       'If you want to create Circos plots, install Circos as described at http://circos.ca/tutorials/lessons/configuration/distribution_and_installation '
                       'and run the following command:\n'
                       'circos -conf ' + conf_fpath)
        return None

    cmdline = [circos_exec, '-conf', conf_fpath]
    log_fpath = join(output_dir, 'circos.log')
    err_fpath = join(output_dir, 'circos.err')
    circos_png_fpath = join(output_dir, circos_png_fname)
    return_code = qutils.call_subprocess(cmdline, stdout=open(log_fpath, 'w'), stderr=open(err_fpath, 'w'))
    if return_code == 0 and is_non_empty_file(circos_png_fpath):
        return circos_png_fpath
    else:
        logger.warning('  Circos diagram was not created. See ' + err_fpath + ' for details')