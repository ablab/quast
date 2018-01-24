#!/usr/bin/python -O

############################################################################
# Copyright (c) 2015-2017 Saint Petersburg State University
# Copyright (c) 2011-2015 Saint Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################

from __future__ import with_statement

from collections import defaultdict

from quast_libs import fastaparser, qconfig, qutils
from quast_libs.icarus_utils import Alignment, Contig


def parse_aligner_contig_report(report_fpath, ref_names, cumulative_ref_lengths):
    aligned_blocks = []
    contigs = []

    with open(report_fpath) as report_file:
        misassembled_id_to_structure = defaultdict(list)
        ambiguity_alignments = defaultdict(list)
        contig_id = None

        start_col = None
        end_col = None
        start_in_contig_col = None
        end_in_contig_col = None
        ref_col = None
        contig_col = None
        idy_col = None
        ambig_col = None
        best_col = None
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
                best_col = split_line.index('Best_group')
            elif split_line and split_line[0] == 'CONTIG':
                _, name, size, contig_type = split_line
                contig = Contig(name=name, size=int(size), contig_type=contig_type)
                contigs.append(contig)
            elif split_line and len(split_line) < 5:
                misassembled_id_to_structure[contig_id].append(line.strip())
            elif split_line and len(split_line) > 5:
                unshifted_start, unshifted_end, start_in_contig, end_in_contig, ref_name, contig_id, idy, ambiguity, is_best = \
                    split_line[start_col], split_line[end_col], split_line[start_in_contig_col], split_line[end_in_contig_col], \
                    split_line[ref_col], split_line[contig_col], split_line[idy_col], split_line[ambig_col], split_line[best_col]
                unshifted_start, unshifted_end, start_in_contig, end_in_contig = int(unshifted_start), int(unshifted_end),\
                                                                                 int(start_in_contig), int(end_in_contig)
                cur_shift = cumulative_ref_lengths[ref_names.index(ref_name)] or 1
                start = unshifted_start + cur_shift - 1
                end = unshifted_end + cur_shift - 1

                is_rc = ((start - end) * (start_in_contig - end_in_contig)) < 0
                position_in_ref = unshifted_start
                block = Alignment(
                    name=contig_id, start=start, end=end, unshifted_start=unshifted_start, unshifted_end=unshifted_end,
                    is_rc=is_rc, start_in_contig=start_in_contig, end_in_contig=end_in_contig, position_in_ref=position_in_ref, ref_name=ref_name,
                    idy=idy, is_best_set=is_best == 'True')
                block.ambiguous = ambiguity
                if block.is_best_set:
                    misassembled_id_to_structure[contig_id].append(block)
                else:
                    ambiguity_alignments[contig_id].append(block)

                aligned_blocks.append(block)

    return aligned_blocks, misassembled_id_to_structure, contigs, ambiguity_alignments


def parse_contigs_fpath(contigs_fpath):
    contigs = []
    for name, seq in fastaparser.read_fasta(contigs_fpath):
        contig = Contig(name=name, size=len(seq))
        contigs.append(contig)
    return contigs


def parse_cov_fpath(cov_fpath, chr_names, chr_full_names, contig_names_by_refs):
    if not cov_fpath:
        return None, None
    cov_data = defaultdict(list)
    max_depth = defaultdict(int)
    chr_contigs = []
    with open(cov_fpath, 'r') as coverage:
        contig_to_chr = dict()
        index_to_chr = dict()
        for chr in chr_full_names:
            if contig_names_by_refs:
                contigs = [contig for contig in chr_names if contig_names_by_refs[contig] == chr]
            elif len(chr_full_names) == 1:
                contigs = chr_names
            else:
                contigs = [chr]
            for contig in contigs:
                contig_to_chr[contig] = chr
            chr_contigs.extend(contigs)
        chr_name = None
        for index, line in enumerate(coverage):
            fs = line.split()
            if line.startswith('#'):
                chr_name = fs[0][1:]
                index_to_chr[fs[1]] = chr_name
            elif chr_name in chr_contigs:
                chrom = contig_to_chr[index_to_chr[fs[0]]]
                depth = int(float(fs[1]))
                max_depth[chrom] = max(depth, max_depth[chrom])
                cov_data[chrom].append(depth)
    return cov_data, max_depth


def parse_features_data(features, cumulative_ref_lengths, ref_names):
    features_data = 'var features_data = [];\n'
    if features:
        features_data += 'features_data = [ '
        containers_kind = []
        for feature_container in features:
            if len(feature_container.region_list) == 0:
                continue
            features_data += '[ '
            for region in feature_container.region_list:
                chrom = region.chromosome if region.chromosome and region.chromosome in feature_container.chr_names_dict \
                    else region.seqname
                chrom = feature_container.chr_names_dict[chrom] if chrom in feature_container.chr_names_dict else None
                if not chrom or chrom not in ref_names:
                    continue
                ref_id = ref_names.index(chrom)
                cur_shift = cumulative_ref_lengths[ref_id]
                corr_start = region.start + cur_shift
                corr_end = region.end + cur_shift
                features_data += '{name: "' + str(region.name) + '", start: ' + str(region.start) + ', end: ' + str(region.end) + \
                                 ',corr_start: ' + str(corr_start) + ',corr_end: ' + str(corr_end) + ', id_: "' + str(region.id) + \
                                 '",kind: "' + str(feature_container.kind) + '", chr:' + str(ref_id) + '},'
            containers_kind.append(feature_container.kind)
            features_data += '],'
        features_data = features_data[:-1] + '];\n'
    return features_data


def parse_map_data(map_fpaths, cumulative_ref_lengths):
    map_blocks = []
    map_sites = []
    map_coords = []
    if map_fpaths:
        for map_fpath in map_fpaths:
            blocks, sites, maps = parse_electronic_map(map_fpath, cumulative_ref_lengths)
            map_blocks.append(blocks)
            map_sites.append(sites)
            map_coords.append(maps)
    map_data = format_electronic_map(map_blocks, map_sites, map_coords)
    return map_data


def format_electronic_map(map_blocks, map_sites, map_coords):
    map_data = 'var map_blocks = [];\n'
    map_data += 'var map_sites = [];\n'
    if map_blocks:
        map_data += 'map_blocks = [ '
        for coords, blocks in zip(map_coords, map_blocks):
            map_data = map_data + '[ '
            for block in blocks:
                start, end, map_id, strand = block
                map_start, map_end = coords[map_id]
                map_data += '{id: "' + str(map_id) + '", start: ' + str(start) + ', end: ' + str(end) + \
                            ', map_start: ' + str(map_start) + ', map_end: ' + str(map_end) + \
                            ', strand: "' + strand + '"},'
            map_data = map_data[:-1] + '],'
        map_data = map_data[:-1] + '];\n'
    if map_sites:
        map_data += 'map_sites = [ '
        for map, sites_data in enumerate(map_sites):
            map_data = map_data + '[ '
            for pos, sites in sites_data.items():
                site_class = 'multi' if len(sites) > 1 else 'single'
                site_ids, map_ids = [x[0] for x in sites], [x[1] for x in sites]
                map_data += '{id: "' + str(';'.join(site_ids)) + '", pos: ' + str(pos) + ', map_id:"' + str(';'.join(map_ids)) +\
                            '", class: "' + site_class + '"},'
            map_data = map_data[:-1] + '],'
        map_data = map_data[:-1] + '];\n'
    return map_data


def parse_electronic_map(map_fpath, cumulative_ref_lengths):
    blocks = []
    sites = defaultdict(list)
    maps_start = defaultdict(int)
    maps_end = defaultdict(int)
    with open(map_fpath) as in_f:
        map_id = None
        strand = None
        header = None
        map_sites = []
        for line in in_f:
            if line.startswith('//'):
                continue
            fs = line.split()
            if not header:
                header = fs
                continue
            fwd_pos, rv_pos = int(fs[2]), int(fs[3])
            if map_id is not None and fs[0] != map_id:
                if map_sites:
                    map_start, map_end = (map_sites[0][0], map_sites[-1][0]) if strand == '+' else (map_sites[-1][0], map_sites[0][0])
                    maps_start[map_id] = map_start
                    maps_end[map_id] = map_end
                    if map_start < map_end:
                        blocks.append((map_start, map_end, map_id, strand))
                    else:
                        blocks.append((0, map_end, map_id, strand))
                        blocks.append((map_start, cumulative_ref_lengths[-1], map_id, strand))
                strand = None
                map_sites = []
            if not fwd_pos and not rv_pos:
                continue
            if fwd_pos and not rv_pos:
                pos = fwd_pos
                strand = '+'
            elif rv_pos and not fwd_pos:
                pos = rv_pos
                strand = '-'
            elif strand == '+' and fwd_pos:
                pos = fwd_pos
            else:
                pos = rv_pos
            site_id = fs[1]
            map_sites.append((pos, fs[0], site_id))
            sites[pos].append((site_id, fs[0]))
            map_id = fs[0]
        map_start, map_end = (map_sites[0][0], map_sites[-1][0]) if strand == '+' else (map_sites[-1][0], map_sites[0][0])
        maps_start[map_id] = map_start
        maps_end[map_id] = map_end
        if map_start < map_end:
            blocks.append((map_start, map_end, map_id, strand))
        else:
            blocks.append((0, map_end, map_id, strand))
            blocks.append((map_start, cumulative_ref_lengths[-1], map_id, strand))
    maps = dict()
    for map_id in maps_start.keys():
        maps[map_id] = (maps_start[map_id], maps_end[map_id])
    return blocks, sites, maps


def parse_genes_data(contigs_by_assemblies, genes_by_labels):
    if not genes_by_labels:
        return
    for label, genes in genes_by_labels.items():
        if not genes:
            continue
        if qconfig.glimmer:
            contigs = dict((contig.name[:qutils.MAX_CONTIG_NAME_GLIMMER], contig) for contig in contigs_by_assemblies[label])
        else:
            contigs = dict((contig.name, contig) for contig in contigs_by_assemblies[label])
        for gene in genes:
            contig = contigs[gene.contig]
            contig.genes.append(gene)
