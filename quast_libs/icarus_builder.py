#!/usr/bin/python -O

############################################################################
# Copyright (c) 2015-2022 Saint Petersburg State University
# Copyright (c) 2011-2015 Saint Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################
from os.path import join
from collections import defaultdict
try:
   from collections import OrderedDict
except ImportError:
   from quast_libs.site_packages.ordered_dict import OrderedDict

from quast_libs import qconfig, qutils, reporting
from quast_libs.html_saver import html_saver
from quast_libs.icarus_utils import Alignment, get_html_name, format_long_numbers, get_misassembly_for_alignment, parse_misassembly_info


def get_assemblies_data(contigs_fpaths, icarus_dirpath, stdout_pattern, nx_marks):
    assemblies_n50 = defaultdict(dict)
    assemblies_data = ''
    assemblies_data += 'var assemblies_links = {};\n'
    assemblies_data += 'var assemblies_len = {};\n'
    assemblies_data += 'var assemblies_contigs = {};\n'
    assemblies_data += 'var assemblies_misassemblies = {};\n'
    assemblies_data += 'var assemblies_n50 = {};\n'
    assemblies_contig_size_data = ''
    for contigs_fpath in contigs_fpaths:
        assembly_label = qutils.label_from_fpath(contigs_fpath)
        report = reporting.get(contigs_fpath)
        l = report.get_field(reporting.Fields.TOTALLEN)
        contigs = report.get_field(reporting.Fields.CONTIGS)
        n50 = report.get_field(reporting.Fields.N50)
        if stdout_pattern:
            contig_stdout_fpath = stdout_pattern % qutils.label_from_fpath_for_fname(contigs_fpath) + '.stdout'
            contig_stdout_fpath = qutils.relpath(contig_stdout_fpath, icarus_dirpath)
            assemblies_data += 'assemblies_links["' + assembly_label + '"] = "' + contig_stdout_fpath + '";\n'
        assemblies_contig_size_data += 'assemblies_len["' + assembly_label + '"] = ' + str(l) + ';\n'
        assemblies_contig_size_data += 'assemblies_contigs["' + assembly_label + '"] = ' + str(contigs) + ';\n'
        assemblies_contig_size_data += 'assemblies_n50["' + assembly_label + '"] = "' + str(n50) + '";\n'
        for nx in nx_marks:
            assemblies_n50[assembly_label][nx] = report.get_field(nx)
    return assemblies_data, assemblies_contig_size_data, assemblies_n50


def add_contig_structure_data(data_str, structure, ref_contigs, chr_full_names, contig_names_by_refs,
                              used_chromosomes, links_to_chromosomes, chr_names_by_id):
    for el in structure:
        if isinstance(el, Alignment):
            if el.ref_name in ref_contigs:
                num_chr = ref_contigs.index(el.ref_name)
                # corr_len = sum(chr_lengths[:num_chr+1])
            else:
                # corr_len = -int(el.end)
                if el.ref_name not in used_chromosomes:
                    used_chromosomes.append(el.ref_name)
                    if contig_names_by_refs:
                        other_ref_name = contig_names_by_refs[el.ref_name]
                        links_to_chromosomes.append('links_to_chromosomes["' + el.ref_name + '"] = "' +
                                                get_html_name(other_ref_name, chr_full_names) + '";')
            corr_el_start = el.start
            corr_el_end = el.end
            data_str.append('{corr_start: ' + str(corr_el_start) + ',corr_end: ' +
                            str(corr_el_end) + ',start:' + str(el.unshifted_start) + ',end:' + str(el.unshifted_end) +
                            ',start_in_contig:' + str(el.start_in_contig) + ',end_in_contig:' +
                            str(el.end_in_contig) + ',IDY:' + el.idy + ',chr: "' + chr_names_by_id[el.ref_name] + '"},')
        elif type(el) == str:
            ms_description, ms_type = parse_misassembly_info(el)
            data_str.append('{contig_type: "M", mstype: "' + ms_type + '", msg: "' + ms_description + '"},')
    return data_str


def get_contigs_structure(assemblies_contigs, chr_to_aligned_blocks, contigs_by_assemblies, ref_contigs, chr_full_names,
                          contig_names_by_refs, structures_by_labels, used_chromosomes, links_to_chromosomes, chr_names_by_id):
    contigs_data_str = []
    contigs_data_str.append('var contig_lengths = {};')
    contigs_data_str.append('var contig_structures = {};')
    for assembly in chr_to_aligned_blocks.keys():
        contigs_data_str.append('contig_lengths["' + assembly + '"] = {};')
        contigs_data_str.append('contig_structures["' + assembly + '"] = {};')
        used_contigs = assemblies_contigs[assembly]
        for contig in contigs_by_assemblies[assembly]:
            if contig.name not in used_contigs:
                continue
            contigs_data_str.append('contig_lengths["' + assembly + '"]["' + contig.name + '"] = ' + str(contig.size) + ';')
            data_str = ['contig_structures["' + assembly + '"]["' + contig.name + '"] = [ ']
            contig_structure = structures_by_labels[assembly][contig.name]
            data_str = add_contig_structure_data(data_str, contig_structure, ref_contigs, chr_full_names,
                                                 contig_names_by_refs, used_chromosomes, links_to_chromosomes, chr_names_by_id)
            data_str.append('];')
            contigs_data_str.extend(data_str)
    contigs_data_str = '\n'.join(contigs_data_str)
    return contigs_data_str


def prepare_alignment_data_for_one_ref(chr, chr_full_names, chr_names_by_id, ref_contigs, data_str, chr_to_aligned_blocks,
                                       structures_by_labels, contigs_by_assemblies, ambiguity_alignments_by_labels=None,
                                       contig_names_by_refs=None, output_dir_path=None,
                                       cov_data_str=None, physical_cov_data_str=None, gc_data_str=None):
    html_name = get_html_name(chr, chr_full_names)
    alignment_viewer_fpath = join(output_dir_path, html_name + '.html')

    additional_assemblies_data = ''
    data_str.append('var links_to_chromosomes;')
    links_to_chromosomes = []
    used_chromosomes = []
    if contig_names_by_refs:
        data_str.append('links_to_chromosomes = {};')
    num_misassemblies = 0
    aligned_assemblies = set()

    is_one_html = len(chr_full_names) == 1
    data_str.append('var oneHtml = ' + str(is_one_html).lower() + ';')
    # adding assembly data
    data_str.append('var contig_data = {};')
    data_str.append('contig_data["' + chr + '"] = {};')
    assemblies_len = defaultdict(int)
    assemblies_contigs = defaultdict(set)
    ms_types = dict()
    for assembly in chr_to_aligned_blocks.keys():
        data_str.append('contig_data["' + chr + '"]["' + assembly + '"] = [ ')
        ms_types[assembly] = defaultdict(int)
        contigs = dict((contig.name, contig) for contig in contigs_by_assemblies[assembly])
        for num_contig, ref_contig in enumerate(ref_contigs):
            if ref_contig in chr_to_aligned_blocks[assembly]:
                overlapped_contigs = defaultdict(list)
                alignments = sorted(chr_to_aligned_blocks[assembly][ref_contig], key=lambda x: x.start)
                prev_end = 0
                prev_alignments = []
                if not qconfig.large_genome:
                    for alignment in alignments:
                        if prev_end > alignment.start:
                            for prev_align in prev_alignments:
                                if alignment.name != prev_align.name and prev_align.end - alignment.start > 100:
                                    overlapped_contigs[prev_align].append('{contig:"' + alignment.name + '",corr_start: ' + str(alignment.start) +
                                        ',corr_end: ' + str(alignment.end) + ',start:' + str(alignment.unshifted_start) +
                                        ',end:' + str(alignment.unshifted_end) + ',start_in_contig:' + str(alignment.start_in_contig) +
                                        ',end_in_contig:' + str(alignment.end_in_contig) + ',chr: "' + chr_names_by_id[alignment.ref_name] + '"}')
                                    overlapped_contigs[alignment].append('{contig:"' + prev_align.name + '",corr_start: ' + str(prev_align.start) +
                                        ',corr_end: ' + str(prev_align.end) + ',start:' + str(prev_align.unshifted_start) +
                                        ',end:' + str(prev_align.unshifted_end) + ',start_in_contig:' + str(prev_align.start_in_contig) +
                                        ',end_in_contig:' + str(prev_align.end_in_contig) + ',chr: "' + chr_names_by_id[prev_align.ref_name] + '"}')
                            prev_alignments.append(alignment)
                        else:
                            prev_alignments = [alignment]
                        prev_end = max(prev_end, alignment.end)

                for alignment in alignments:
                    assemblies_len[assembly] += abs(alignment.end_in_contig - alignment.start_in_contig) + 1
                    assemblies_contigs[assembly].add(alignment.name)
                    contig_structure = structures_by_labels[alignment.label]
                    contig_more_unaligned = False
                    if alignment.misassembled:
                        num_misassemblies += 1
                        misassemblies, misassembled_ends = get_misassembly_for_alignment(contig_structure[alignment.name], alignment)
                        if 'unknown' in misassemblies:
                            alignment.misassemblies = 'unknown'
                            misassembled_ends = ''
                            contig_more_unaligned = True
                        else:
                            for misassembly in misassemblies:
                                if misassembly:
                                    ms_types[assembly][misassembly] += 1
                        alignment.misassemblies = ';'.join(misassemblies)
                    else:
                        if contigs[alignment.name].contig_type == 'correct_unaligned':
                            contig_more_unaligned = True
                        misassembled_ends = ''

                    genes = []
                    start_in_contig, end_in_contig = min(alignment.start_in_contig, alignment.end_in_contig), \
                                                     max(alignment.start_in_contig, alignment.end_in_contig)
                    for gene in contigs[alignment.name].genes:
                        if start_in_contig < gene.start < end_in_contig or start_in_contig < gene.end < end_in_contig:
                            corr_start = max(alignment.start, alignment.start + (gene.start - start_in_contig))
                            corr_end = min(alignment.end, alignment.end + (gene.end - end_in_contig))
                            gene_info = '{start:' + str(gene.start) + ',end:' + str(gene.end) + ',corr_start:' + \
                                        str(corr_start) + ',corr_end:' + str(corr_end) + '}'
                            genes.append(gene_info)
                    data_str.append('{name:"' + alignment.name + '",corr_start:' + str(alignment.start) + ',corr_end:' +
                                    str(alignment.end) + ',start:' + str(alignment.unshifted_start) + ',end:' +
                                    str(alignment.unshifted_end) + ',misassemblies:"' + alignment.misassemblies + '",mis_ends:"' + misassembled_ends + '"')
                    if alignment.similar:
                        data_str[-1] += ',similar:"True"'
                    if alignment.ambiguous:
                        data_str[-1] += ',ambiguous:"True"'
                    if alignment.is_best_set:
                        data_str[-1] += ',is_best:"True"'
                    if contig_more_unaligned:
                        data_str[-1] += ',more_unaligned:"True"'

                    aligned_assemblies.add(alignment.label)
                    if overlapped_contigs[alignment]:
                        data_str.append(',overlaps:[ ')
                        data_str.append(','.join(overlapped_contigs[alignment]))
                        data_str.append(']')
                    if qconfig.gene_finding:
                        data_str.append(',genes:[' + ','.join(genes) + ']')
                    if ambiguity_alignments_by_labels and qconfig.ambiguity_usage == 'all':
                        data_str.append(',ambiguous_alignments:[ ')
                        data_str = add_contig_structure_data(data_str, ambiguity_alignments_by_labels[alignment.label][alignment.name],
                                                             ref_contigs, chr_full_names, contig_names_by_refs,
                                                             used_chromosomes, links_to_chromosomes, chr_names_by_id)
                        data_str[-1] = data_str[-1][:-1] + '],'
                    data_str[-1] = data_str[-1] + '},'

        data_str[-1] = data_str[-1][:-1] + '];'
        assembly_len = assemblies_len[assembly]
        assembly_contigs = len(assemblies_contigs[assembly])
        local_misassemblies = ms_types[assembly]['local'] // 2
        ext_misassemblies = (sum(ms_types[assembly].values()) - ms_types[assembly]['interspecies translocation']) // 2 - \
                            local_misassemblies + ms_types[assembly]['interspecies translocation']
        additional_assemblies_data += 'assemblies_len["' + assembly + '"] = ' + str(assembly_len) + ';\n'
        additional_assemblies_data += 'assemblies_contigs["' + assembly + '"] = ' + str(assembly_contigs) + ';\n'
        additional_assemblies_data += 'assemblies_misassemblies["' + assembly + '"] = "' + str(ext_misassemblies) + '+' + \
                                      str(local_misassemblies) + '";\n'

    if cov_data_str:
        # adding coverage data
        data_str.extend(cov_data_str)
    if physical_cov_data_str:
        data_str.extend(physical_cov_data_str)
    if gc_data_str:
        data_str.append('var gc_window_size = ' + (str(qconfig.GC_window_size_large) if
                        qconfig.large_genome else str(qconfig.GC_window_size)) + ';')
        data_str.extend(gc_data_str)

    misassemblies_types = ['relocation', 'translocation', 'inversion', 'interspecies translocation', 'local']
    if not qconfig.is_combined_ref:
        misassemblies_types.remove('interspecies translocation')

    ms_counts_by_type = OrderedDict()
    for ms_type in misassemblies_types:
        factor = 1 if ms_type == 'interspecies translocation' else 2
        ms_counts_by_type[ms_type] = sum(ms_types[assembly][ms_type] // factor for assembly in chr_to_aligned_blocks.keys())
    total_ms_count = sum(ms_counts_by_type.values()) - ms_counts_by_type['local']
    ms_selectors = []
    for ms_type, ms_count in ms_counts_by_type.items():
        ms_name = ms_type
        if ms_count != 1 and ms_type != 'local':
            ms_name += 's'
        ms_selectors.append((ms_type, ms_name, str(ms_count)))

    contigs_structure_str = get_contigs_structure(assemblies_contigs, chr_to_aligned_blocks, contigs_by_assemblies, ref_contigs, chr_full_names,
                                                   contig_names_by_refs, structures_by_labels, used_chromosomes, links_to_chromosomes, chr_names_by_id)

    if contig_names_by_refs:
        data_str.append(''.join(links_to_chromosomes))
    data_str = '\n'.join(data_str)
    return alignment_viewer_fpath, data_str, contigs_structure_str, additional_assemblies_data, ms_selectors, num_misassemblies, aligned_assemblies


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
    genes = ['{start:' + str(gene.start) + ',end:' + str(gene.end) + '}' for gene in contig.genes]
    if marks:
        not_used_nx = [nx for nx in not_used_nx if nx not in marks]
    marks = ', marks: "' + marks + '"' if marks else ''
    if not only_nx or marks:
        structure = []
        if structures_by_labels and assembly in structures_by_labels:
            assembly_structure = structures_by_labels[assembly]
            for el in assembly_structure[contig.name]:
                if isinstance(el, Alignment):
                    structure.append('{contig:"' + contig.name + '",corr_start: ' + str(el.start) + ',corr_end: ' +
                                    str(el.end) + ',start:' + str(el.unshifted_start) + ',end:' + str(el.unshifted_end) +
                                    ',start_in_contig:' + str(el.start_in_contig) + ',end_in_contig:' +
                                    str(el.end_in_contig) + ',size:' + str(contig.size) + ',IDY:' + el.idy + ',chr:"' + el.ref_name + '"},')
                elif type(el) == str:
                    ms_description, ms_type = parse_misassembly_info(el)
                    structure.append('{contig_type: "M", mstype: "' + ms_type + '", msg: "' + ms_description + '"},')
        if has_aligned_contigs and not contig.contig_type:
            contig.contig_type = 'unaligned'
        align = '{name:"' + contig.name + '",size:' + str(contig.size) + marks + ',contig_type: "' + contig.contig_type + \
                '",structure:[' + ''.join(structure) + ']' + (',genes:[' + ','.join(genes) + ']' if qconfig.gene_finding else '') + '},'
    return end_contig, contig_size_lines, align, not_used_nx


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
            remained_genes = ['{start:' + str(gene.start) + ',end:' + str(gene.end) + '}' for contig in contigs[last_contig_num:]
                              for gene in contig.genes]
            remained_contigs_name = str(len(contigs) - last_contig_num) + ' hidden contigs shorter than ' + str(contig_threshold) + \
                                    ' bp (total length: ' + format_long_numbers(remained_len) + ' bp)'
            contigs_sizes_str.append(('{name:"' + remained_contigs_name + '", size:' + str(remained_len) +
                                     ', contig_type:"short_contigs"' + (',genes:[' + ','.join(remained_genes) + ']},'
                                      if qconfig.gene_finding else '},')))
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


def save_alignment_data_for_one_ref(chr_name, ref_contigs, ref_name, json_output_dir, alignment_viewer_fpath, data_str, ms_selectors,
                                    ref_data='', features_data='', assemblies_data='', contigs_structure_str='', additional_assemblies_data=''):
    alignment_viewer_template_fpath = html_saver.get_real_path(qconfig.icarus_viewers_template_fname)
    data_dict = dict()
    chr_data = 'chromosome = "' + chr_name + '";\n'
    chromosomes = '","'.join(ref_contigs)
    chr_data += 'var chrContigs = ["' + chromosomes + '"];\n'
    if qconfig.alignment_viewer_part_name in chr_name:
        chr_name = ref_name
        chr_name += ' (' + str(len(ref_contigs)) + (' entries)' if len(ref_contigs) > 1 else ' entry)')
    chr_name = chr_name.replace('_', ' ')
    all_data = ref_data + assemblies_data + additional_assemblies_data + chr_data + features_data + data_str + contigs_structure_str
    data_dict['title'] = 'Contig alignment viewer'
    data_dict['reference'] = chr_name
    data_dict['data'] = '<script type="text/javascript">' + all_data + '</script>'
    data_dict['misassemblies_checkboxes'] = []
    for (ms_type, ms_name, ms_count) in ms_selectors:
        checkbox = {'ms_type': ms_type, 'ms_name': ms_name, 'ms_count': ms_count}
        data_dict['misassemblies_checkboxes'].append(checkbox)
    html_saver.save_icarus_html(alignment_viewer_template_fpath, alignment_viewer_fpath, data_dict)
    html_saver.save_icarus_data(json_output_dir, chr_name, 'ref_name')
    html_saver.save_icarus_data(json_output_dir, data_dict['data'], 'data_alignments')
    html_saver.save_icarus_data(json_output_dir, data_dict['reference'], 'reference')
    html_saver.save_icarus_data(json_output_dir, data_dict['misassemblies_checkboxes'], 'ms_selector', as_text=False)


def save_contig_size_html(output_all_files_dir_path, json_output_dir, too_many_contigs, all_data):
    contig_size_template_fpath = html_saver.get_real_path(qconfig.icarus_viewers_template_fname)
    contig_size_viewer_fpath = join(output_all_files_dir_path, qconfig.contig_size_viewer_fname)
    data_dict = dict()
    data_dict['title'] = 'Contig size viewer'
    data_dict['size_viewer'] = True
    if too_many_contigs:
        data_dict['num_contigs_warning'] = str(qconfig.max_contigs_num_for_size_viewer)
        html_saver.save_icarus_data(json_output_dir, data_dict['num_contigs_warning'], 'num_contigs_warning')

    data_dict['data'] = '<script type="text/javascript">' + all_data + '</script>'
    html_saver.save_icarus_data(json_output_dir, data_dict['data'], 'data_sizes')
    html_saver.save_icarus_html(contig_size_template_fpath, contig_size_viewer_fpath, data_dict)
