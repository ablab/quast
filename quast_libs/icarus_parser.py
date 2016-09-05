#!/usr/bin/python -O

############################################################################
# Copyright (c) 2015-2016 Saint Petersburg State University
# Copyright (c) 2011-2015 Saint Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################

from __future__ import with_statement

from collections import defaultdict

from quast_libs import fastaparser, reporting, qconfig, qutils
from quast_libs.icarus_utils import Alignment, get_html_name, format_long_numbers, Contig, parse_misassembly_info


def parse_nucmer_contig_report(report_fpath, ref_names, cumulative_ref_lengths):
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
                cur_shift = cumulative_ref_lengths[ref_names.index(ref_name)]
                start = unshifted_start + cur_shift
                end = unshifted_end + cur_shift

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


def add_contig(cum_length, contig, not_used_nx, assemblies_n50, assembly, contigs, contig_size_lines, num, structures_by_labels,
               only_nx=False, has_aligned_contigs=True):
    end_contig = cum_length + contig.size
    marks = []
    align = None
    for nx in not_used_nx:
        if assemblies_n50[assembly][nx] == contig.size and \
                (num + 1 >= len(contigs) or contigs[num + 1].size != contig.size):
            marks.append(nx)
    marks = ', '.join(marks)
    genes = ['{start: ' + str(gene.start) + ', end: ' + str(gene.end) + '}' for gene in contig.genes]
    if marks:
        not_used_nx = [nx for nx in not_used_nx if nx not in marks]
    marks = ', marks: "' + marks + '"' if marks else ''
    if not only_nx or marks:
        structure = []
        if structures_by_labels and assembly in structures_by_labels:
            assembly_structure = structures_by_labels[assembly]
            for el in assembly_structure[contig.name]:
                if isinstance(el, Alignment):
                    structure.append('{contig: "' + contig.name + '",corr_start: ' + str(el.start) + ',corr_end: ' +
                                    str(el.end) + ',start:' + str(el.unshifted_start) + ',end:' + str(el.unshifted_end) +
                                    ',start_in_contig:' + str(el.start_in_contig) + ',end_in_contig:' +
                                    str(el.end_in_contig) + ',size: ' + str(contig.size) + ',IDY:' + el.idy + ',chr: "' + el.ref_name + '"},')
                elif type(el) == str:
                    ms_description, ms_type = parse_misassembly_info(el)
                    structure.append('{contig_type: "M", mstype: "' + ms_type + '", msg: "' + ms_description + '"},')
        if has_aligned_contigs and not contig.contig_type:
            contig.contig_type = 'unaligned'
        align = '{name: "' + contig.name + '",size: ' + str(contig.size) + marks + ',contig_type: "' + contig.contig_type + \
                '",structure: [' + ''.join(structure) + ']' + (', genes: [' + ','.join(genes) + ']' if qconfig.gene_finding else '') + '},'
    return end_contig, contig_size_lines, align, not_used_nx


def parse_cov_fpath(cov_fpath, chr_names, chr_full_names, contig_names_by_refs):
    if not cov_fpath:
        return None, None, None
    cov_data = defaultdict(list)
    #not_covered = defaultdict(list)
    max_depth = defaultdict(int)
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
                     chr_full_names):
    additional_data = []
    additional_data.append('var links_to_chromosomes;')
    one_html = len(chr_full_names) == 1
    if not one_html:
        additional_data.append('links_to_chromosomes = {};')
        for ref_name in ref_names:
            chr_name = ref_name
            if contig_names_by_refs and ref_name in contig_names_by_refs:
                chr_name = contig_names_by_refs[ref_name]
            chr_name = get_html_name(chr_name, chr_full_names)
            additional_data.append('links_to_chromosomes["' + ref_name + '"] = "' + chr_name + '";')
    contigs_sizes_str = ['var contig_data = {};']
    contigs_sizes_str.append('var chromosome;')
    contigs_sizes_lines = []
    total_len = 0
    min_contig_size = qconfig.min_contig
    too_many_contigs = False
    has_aligned_contigs = structures_by_labels.values()
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
                                                                assembly, contigs, contigs_sizes_lines, i, structures_by_labels,
                                                                             has_aligned_contigs=has_aligned_contigs)
            contigs_sizes_str.append(align)
        if len(contigs) > qconfig.max_contigs_num_for_size_viewer:
            assembly_len = cum_length
            remained_len = sum(alignment.size for alignment in contigs[last_contig_num:])
            cum_length += remained_len
            remained_genes = ['{start: ' + str(gene.start) + ', end: ' + str(gene.end) + '}' for contig in contigs[last_contig_num:] for gene in contig.genes]
            remained_contigs_name = str(len(contigs) - last_contig_num) + ' hidden contigs shorter than ' + str(contig_threshold) + \
                                    ' bp (total length: ' + format_long_numbers(remained_len) + ' bp)'
            contigs_sizes_str.append(('{name: "' + remained_contigs_name + '", size: ' + str(remained_len) +
                                     ', contig_type:"small_contigs", genes:[' + ','.join(remained_genes) + ']},'))
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
                cur_shift = cumulative_ref_lengths[ref_id]
                name = region.name if region.name else ''
                corr_start = region.start + cur_shift
                corr_end = region.end + cur_shift
                features_data += '{name: "' + name + '", start: ' + str(region.start) + ', end: ' + str(region.end) + \
                                 ',corr_start: ' + str(corr_start) + ',corr_end: ' + str(corr_end) + ', id_: "' + region.id + \
                                 '",kind: "' + feature_container.kind + '", chr:' + str(ref_id) + '},'
            containers_kind.append(feature_container.kind)
            features_data += '],'
        features_data = features_data[:-1] + '];\n'
    return features_data


def parse_genes_data(contigs_by_assemblies, genes_by_labels):
    if not genes_by_labels:
        return
    for label, genes in genes_by_labels.iteritems():
        if not genes:
            continue
        if qconfig.glimmer:
            contigs = dict((contig.name[:qutils.MAX_CONTIG_NAME_GLIMMER], contig) for contig in contigs_by_assemblies[label])
        else:
            contigs = dict((contig.name, contig) for contig in contigs_by_assemblies[label])
        for gene in genes:
            contig = contigs[gene.contig]
            contig.genes.append(gene)
