#!/usr/bin/python -O

############################################################################
# Copyright (c) 2015-2016 Saint Petersburg State University
# Copyright (c) 2011-2015 Saint Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################

from __future__ import with_statement

from collections import defaultdict

from quast_libs.icarus_parser import parse_contigs_fpath, parse_features_data, parse_cov_fpath, get_assemblies_data, \
    get_contigs_data, parse_genes_data
from quast_libs.icarus_parser import parse_nucmer_contig_report
from quast_libs.icarus_utils import make_output_dir, group_references, format_cov_data, format_long_numbers, get_info_by_chr, \
    get_html_name, get_assemblies, check_misassembled_blocks, Alignment, parse_misassembly_info, get_misassembly_for_alignment

try:
   from collections import OrderedDict
except ImportError:
   from site_packages.ordered_dict import OrderedDict

import os
from quast_libs import qconfig, qutils, fastaparser, genome_analyzer
from quast_libs.ca_utils.misc import ref_labels_by_chromosomes
import quast_libs.html_saver.html_saver as html_saver
import quast_libs.html_saver.json_saver as json_saver
from quast_libs.svg_alignment_plotter import draw_alignment_plot

from quast_libs import reporting
from quast_libs.log import get_logger
logger = get_logger(qconfig.LOGGER_DEFAULT_NAME)


def do(contigs_fpaths, contig_report_fpath_pattern, output_dirpath, ref_fpath, cov_fpath=None,  physical_cov_fpath=None,
       stdout_pattern=None, find_similar=True, features=None, json_output_dir=None, genes_by_labels=None):
    make_output_dir(output_dirpath)

    lists_of_aligned_blocks = []
    contigs_by_assemblies = OrderedDict()
    structures_by_labels = {}
    ambiguity_alignments_by_labels = {}

    total_genome_size = 0
    reference_chromosomes = OrderedDict()
    contig_names_by_refs = None
    assemblies = None
    chr_names = []
    features_data = None

    plot_fpath = None
    max_small_chromosomes = 10

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
        if ref_labels_by_chromosomes:
            contig_names_by_refs = ref_labels_by_chromosomes
        elif sum(reference_chromosomes.values()) > qconfig.MAX_SIZE_FOR_COMB_PLOT:
            contig_names_by_refs = dict()
            if len(chr_names) > max_small_chromosomes:
                summary_len = 0
                num_parts = 1
                html_name = qconfig.alignment_viewer_part_name + str(num_parts)
                for chr_name, chr_len in reference_chromosomes.iteritems():
                    summary_len += chr_len
                    contig_names_by_refs[chr_name] = html_name
                    if summary_len >= qconfig.MAX_SIZE_FOR_COMB_PLOT:
                        summary_len = 0
                        num_parts += 1
                        html_name = qconfig.alignment_viewer_part_name + str(num_parts)
            else:
                for chr_name in chr_names:
                    contig_names_by_refs[chr_name] = chr_name

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
            aligned_blocks, misassembled_id_to_structure, contigs, ambiguity_alignments = parse_nucmer_contig_report(report_fpath,
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
            if qconfig.ambiguity_usage == 'all':
                ambiguity_alignments_by_labels[label] = ambiguity_alignments
        contigs_by_assemblies[label] = contigs

    if contigs_fpaths and ref_fpath and features:
        features_data = parse_features_data(features, cumulative_ref_lengths, chr_names)
    if contigs_fpaths and qconfig.gene_finding:
        parse_genes_data(contigs_by_assemblies, genes_by_labels)
    if reference_chromosomes and lists_of_aligned_blocks:
        assemblies = get_assemblies(contigs_fpaths, virtual_genome_size, lists_of_aligned_blocks, find_similar)
        if qconfig.draw_svg:
            plot_fpath = draw_alignment_plot(assemblies, virtual_genome_size, output_dirpath, sorted_ref_names, sorted_ref_lengths, virtual_genome_shift)
    if (assemblies or contigs_by_assemblies) and qconfig.create_icarus_html:
        icarus_html_fpath = js_data_gen(assemblies, contigs_fpaths, reference_chromosomes,
                    output_dirpath, structures_by_labels, contig_names_by_refs=contig_names_by_refs, ref_fpath=ref_fpath, stdout_pattern=stdout_pattern,
                    ambiguity_alignments_by_labels=ambiguity_alignments_by_labels, contigs_by_assemblies=contigs_by_assemblies,
                    features_data=features_data, cov_fpath=cov_fpath, physical_cov_fpath=physical_cov_fpath, json_output_dir=json_output_dir)
    else:
        icarus_html_fpath = None

    return icarus_html_fpath, plot_fpath


def add_contig_structure_data(data_str, alignment_name, structure, ref_contigs, chr_full_names, contig_names_by_refs,
                              used_chromosomes, links_to_chromosomes):
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
            data_str.append('{contig: "' + alignment_name + '",corr_start: ' + str(corr_el_start) + ',corr_end: ' +
                            str(corr_el_end) + ',start:' + str(el.unshifted_start) + ',end:' + str(el.unshifted_end) +
                            ',start_in_contig:' + str(el.start_in_contig) + ',end_in_contig:' +
                            str(el.end_in_contig) + ',IDY:' + el.idy + ',chr: "' + el.ref_name + '"},')
        elif type(el) == str:
            ms_description, ms_type = parse_misassembly_info(el)
            data_str.append('{contig_type: "M", mstype: "' + ms_type + '", msg: "' + ms_description + '"},')
    return data_str


def prepare_alignment_data_for_one_ref(chr, chr_full_names, ref_contigs, data_str, chr_to_aligned_blocks,
                                       structures_by_labels, contigs_by_assemblies, ambiguity_alignments_by_labels=None,
                                       contig_names_by_refs=None, output_dir_path=None, cov_data_str=None, physical_cov_data_str=None):
    html_name = get_html_name(chr, chr_full_names)
    alignment_viewer_fpath = os.path.join(output_dir_path, html_name + '.html')

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
                for alignment in alignments:
                    if prev_end > alignment.start:
                        for prev_align in prev_alignments:
                            if alignment.name != prev_align.name and prev_align.end - alignment.start > 100:
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
                    assemblies_len[assembly] += abs(alignment.end_in_contig - alignment.start_in_contig) + 1
                    assemblies_contigs[assembly].add(alignment.name)
                    contig_structure = structures_by_labels[alignment.label]
                    if alignment.misassembled:
                        num_misassemblies += 1
                        misassemblies, misassembled_ends = get_misassembly_for_alignment(contig_structure[alignment.name], alignment)
                        for misassembly in misassemblies:
                            if misassembly:
                                ms_types[assembly][misassembly] += 1
                        alignment.misassemblies = ';'.join(misassemblies)
                    else:
                        misassembled_ends = ''

                    genes = []
                    start_in_contig, end_in_contig = min(alignment.start_in_contig, alignment.end_in_contig), \
                                                     max(alignment.start_in_contig, alignment.end_in_contig)
                    for gene in contigs[alignment.name].genes:
                        if start_in_contig < gene.start < end_in_contig or start_in_contig < gene.end < end_in_contig:
                            corr_start = max(alignment.start, alignment.start + (gene.start - start_in_contig))
                            corr_end = min(alignment.end, alignment.end + (gene.end - end_in_contig))
                            gene_info = '{start: ' + str(gene.start) + ', end: ' + str(gene.end) + ', corr_start: ' + \
                                        str(corr_start) + ', corr_end: ' + str(corr_end) + '}'
                            genes.append(gene_info)
                    data_str.append('{name: "' + alignment.name + '",corr_start:' + str(alignment.start) +
                                    ', corr_end: ' + str(alignment.end) + ',start:' + str(alignment.unshifted_start) +
                                    ',end:' + str(alignment.unshifted_end) + ',similar:"' + ('True' if alignment.similar else 'False') +
                                    '", misassemblies:"' + alignment.misassemblies + '",mis_ends:"' + misassembled_ends + '"')
                    if alignment.ambiguous:
                        data_str[-1] += ', ambiguous: "True"'
                    if alignment.is_best_set:
                        data_str[-1] += ', is_best: "True"'

                    aligned_assemblies.add(alignment.label)
                    if overlapped_contigs[alignment]:
                        data_str.append(',overlaps:[ ')
                        data_str.append(','.join(overlapped_contigs[alignment]))
                        data_str.append(']')
                    if qconfig.gene_finding:
                        data_str.append(', genes: [' + ','.join(genes) + ']')
                    data_str.append(', structure: [ ')
                    data_str = add_contig_structure_data(data_str, alignment.name, contig_structure[alignment.name], ref_contigs,
                                                         chr_full_names, contig_names_by_refs, used_chromosomes, links_to_chromosomes)
                    data_str.append('], ')
                    if ambiguity_alignments_by_labels and qconfig.ambiguity_usage == 'all':
                        data_str.append('ambiguous_alignments: [ ')
                        data_str = add_contig_structure_data(data_str, alignment.name, ambiguity_alignments_by_labels[alignment.label][alignment.name],
                                                             ref_contigs, chr_full_names, contig_names_by_refs,
                                                             used_chromosomes, links_to_chromosomes)
                        data_str[-1] = data_str[-1][:-1] + '],'
                    data_str[-1] = data_str[-1][:-1] + '},'

        data_str[-1] = data_str[-1][:-1] + '];'
        assembly_len = assemblies_len[assembly]
        assembly_contigs = len(assemblies_contigs[assembly])
        local_misassemblies = ms_types[assembly]['local'] / 2
        ext_misassemblies = (sum(ms_types[assembly].values()) - ms_types[assembly]['interspecies translocation']) / 2 - \
                            local_misassemblies + ms_types[assembly]['interspecies translocation']
        additional_assemblies_data += 'assemblies_len["' + assembly + '"] = ' + str(assembly_len) + ';\n'
        additional_assemblies_data += 'assemblies_contigs["' + assembly + '"] = ' + str(assembly_contigs) + ';\n'
        additional_assemblies_data += 'assemblies_misassemblies["' + assembly + '"] = "' + str(ext_misassemblies) + '+' + \
                                      str(local_misassemblies) + '";\n'

    if contig_names_by_refs:
        data_str.append(''.join(links_to_chromosomes))
    if cov_data_str:
        # adding coverage data
        data_str.extend(cov_data_str)
    if physical_cov_data_str:
        data_str.extend(physical_cov_data_str)

    data_str = '\n'.join(data_str)

    misassemblies_types = ['relocation', 'translocation', 'inversion', 'interspecies translocation', 'local']
    if not qconfig.is_combined_ref:
        misassemblies_types.remove('interspecies translocation')

    ms_counts_by_type = OrderedDict()
    for ms_type in misassemblies_types:
        factor = 1 if ms_type == 'interspecies translocation' else 2
        ms_counts_by_type[ms_type] = sum(ms_types[assembly][ms_type] / factor for assembly in chr_to_aligned_blocks.keys())
    total_ms_count = sum(ms_counts_by_type.values()) - ms_counts_by_type['local']
    ms_selectors = []
    for ms_type, ms_count in ms_counts_by_type.items():
        ms_name = ms_type
        if ms_count != 1 and ms_type != 'local':
            ms_name += 's'
        ms_selectors.append((ms_type, ms_name, str(ms_count)))

    return alignment_viewer_fpath, data_str, additional_assemblies_data, ms_selectors, num_misassemblies, aligned_assemblies


def save_alignment_data_for_one_ref(chr_name, ref_contigs, ref_name, json_output_dir, alignment_viewer_fpath, data_str, ms_selectors,
                                    ref_data='', features_data='', assemblies_data='', additional_assemblies_data=''):
    alignment_viewer_template_fpath = html_saver.get_real_path(qconfig.icarus_viewers_template_fname)
    data_dict = dict()
    chr_data = 'chromosome = "' + chr_name + '";\n'
    chromosomes = '","'.join(ref_contigs)
    chr_data += 'var chrContigs = ["' + chromosomes + '"];\n'
    if qconfig.alignment_viewer_part_name in chr_name:
        chr_name = ref_name
        chr_name += ' (' + str(len(ref_contigs)) + (' entries)' if len(ref_contigs) > 1 else ' entry)')
    chr_name = chr_name.replace('_', ' ')
    all_data = ref_data + assemblies_data + additional_assemblies_data + chr_data + features_data + data_str
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
    contig_size_viewer_fpath = os.path.join(output_all_files_dir_path, qconfig.contig_size_viewer_fname)
    data_dict = dict()
    data_dict['title'] = 'Contig size viewer'
    data_dict['size_viewer'] = True
    if too_many_contigs:
        data_dict['num_contigs_warning'] = str(qconfig.max_contigs_num_for_size_viewer)
        html_saver.save_icarus_data(json_output_dir, data_dict['num_contigs_warning'], 'num_contigs_warning')

    data_dict['data'] = '<script type="text/javascript">' + all_data + '</script>'
    html_saver.save_icarus_data(json_output_dir, data_dict['data'], 'data_sizes')
    html_saver.save_icarus_html(contig_size_template_fpath, contig_size_viewer_fpath, data_dict)


def js_data_gen(assemblies, contigs_fpaths, chromosomes_length, output_dirpath, structures_by_labels,
                contigs_by_assemblies, ambiguity_alignments_by_labels=None, contig_names_by_refs=None, ref_fpath=None,
                stdout_pattern=None, features_data=None, cov_fpath=None, physical_cov_fpath=None, json_output_dir=None):
    chr_names = []
    if chromosomes_length and assemblies:
        chr_to_aligned_blocks = OrderedDict()
        chr_names = chromosomes_length.keys()
        for assembly in assemblies.assemblies:
            chr_to_aligned_blocks[assembly.label] = defaultdict(list)
            similar_correct = 0
            similar_misassembled = 0

            for align in assembly.alignments:
                chr_to_aligned_blocks[assembly.label][align.ref_name].append(align)
                if align.similar:
                    if align.misassembled:
                        similar_misassembled += 1
                    else:
                        similar_correct += 1
            report = reporting.get(assembly.fpath)
            report.add_field(reporting.Fields.SIMILAR_CONTIGS, similar_correct)
            report.add_field(reporting.Fields.SIMILAR_MIS_BLOCKS, similar_misassembled)

    main_menu_fpath = os.path.join(output_dirpath, qconfig.icarus_html_fname)
    output_all_files_dir_path = os.path.join(output_dirpath, qconfig.icarus_dirname)
    if not os.path.exists(output_all_files_dir_path):
        os.mkdir(output_all_files_dir_path)

    chr_full_names, contig_names_by_refs = group_references(chr_names, contig_names_by_refs, chromosomes_length, ref_fpath)

    cov_data, not_covered, max_depth = parse_cov_fpath(cov_fpath, chr_names, chr_full_names, contig_names_by_refs)
    physical_cov_data, not_covered, physical_max_depth = parse_cov_fpath(physical_cov_fpath, chr_names, chr_full_names, contig_names_by_refs)

    chr_sizes = {}
    num_contigs = {}
    aligned_bases = genome_analyzer.get_ref_aligned_lengths()
    nx_marks = [reporting.Fields.N50, reporting.Fields.N75, reporting.Fields.NG50, reporting.Fields.NG75]

    assemblies_data, assemblies_contig_size_data, assemblies_n50 = get_assemblies_data(contigs_fpaths, stdout_pattern, nx_marks)

    ref_contigs_dict = {}
    chr_lengths_dict = {}

    ref_data = 'var references_by_id = {};\n'
    for i, chr in enumerate(chr_names):
        ref_data += 'references_by_id[' + str(i) + '] = "' + str(chr) + '";\n'
    for i, chr in enumerate(chr_full_names):
        if contig_names_by_refs:
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

        cov_data = format_cov_data(cov_data, max_depth, chr, 'coverage_data', 'reads_max_depth') if cov_data else None
        physical_cov_data = format_cov_data(physical_cov_data, physical_max_depth, chr, 'physical_coverage_data', 'physical_max_depth') \
            if physical_cov_data else None

        alignment_viewer_fpath, ref_data_str, additional_assemblies_data, ms_selectors, num_misassemblies[chr], aligned_assemblies[chr] = \
            prepare_alignment_data_for_one_ref(chr, chr_full_names, ref_contigs, data_str, chr_to_aligned_blocks, structures_by_labels,
                                               contigs_by_assemblies, ambiguity_alignments_by_labels=ambiguity_alignments_by_labels,
                                               cov_data_str=cov_data, physical_cov_data_str=physical_cov_data,
                                               contig_names_by_refs=contig_names_by_refs, output_dir_path=output_all_files_dir_path)
        ref_name = qutils.name_from_fpath(ref_fpath)
        save_alignment_data_for_one_ref(chr, ref_contigs, ref_name, json_output_dir, alignment_viewer_fpath, ref_data_str, ms_selectors,
                                        ref_data=ref_data, features_data=features_data, assemblies_data=assemblies_data,
                                        additional_assemblies_data=additional_assemblies_data)

    contigs_sizes_str, too_many_contigs = get_contigs_data(contigs_by_assemblies, nx_marks, assemblies_n50, structures_by_labels,
                                                           contig_names_by_refs, chr_names, chr_full_names)
    all_data = assemblies_data + assemblies_contig_size_data + contigs_sizes_str
    save_contig_size_html(output_all_files_dir_path, json_output_dir, too_many_contigs, all_data)

    icarus_links = defaultdict(list)
    if len(chr_full_names) > 1:
        chr_link = qconfig.icarus_html_fname
        icarus_links["links"].append(chr_link)
        icarus_links["links_names"].append(qconfig.icarus_link)

    main_menu_template_fpath = html_saver.get_real_path(qconfig.icarus_menu_template_fname)
    main_data_dict = dict()

    labels = [qconfig.assembly_labels_by_fpath[contigs_fpath] for contigs_fpath in contigs_fpaths]
    main_data_dict['assemblies'] = labels
    html_saver.save_icarus_data(json_output_dir, ', '.join(labels), 'assemblies')

    contig_size_browser_fpath = os.path.join(qconfig.icarus_dirname, qconfig.contig_size_viewer_fname)
    main_data_dict['contig_size_html'] = contig_size_browser_fpath
    html_saver.save_icarus_data(json_output_dir, contig_size_browser_fpath, 'contig_size_html')
    if not chr_names:
        icarus_links["links"].append(contig_size_browser_fpath)
        icarus_links["links_names"].append(qconfig.icarus_link)

    if chr_full_names and (len(chr_full_names) > 1 or qconfig.is_combined_ref):
        main_data_dict['table_references'] = {'references': []}
        num_aligned_assemblies = [len(aligned_assemblies[chr]) for chr in chr_full_names]
        is_unaligned_asm_exists = len(set(num_aligned_assemblies)) > 1
        if is_unaligned_asm_exists:
            main_data_dict['table_references']['th_assemblies'] = True
        for chr in sorted(chr_full_names):
            chr_link, chr_name, chr_genome, chr_size, tooltip = get_info_by_chr(chr, aligned_bases_by_chr, chr_sizes, contigs_fpaths,
                                                                                contig_names_by_refs, one_chromosome=len(chr_full_names) == 1)
            reference_dict = dict()
            reference_dict['chr_link'] = chr_link
            reference_dict['tooltip'] = tooltip
            reference_dict['chr_name'] = os.path.basename(chr_name)
            reference_dict['num_contigs'] = str(num_contigs[chr])
            reference_dict['chr_size'] = format_long_numbers(chr_size)
            if is_unaligned_asm_exists:
                reference_dict['num_assemblies'] = str(len(aligned_assemblies[chr]))
            reference_dict['chr_gf'] = '%.3f' % chr_genome
            reference_dict['num_misassemblies'] = str(num_misassemblies[chr])
            main_data_dict['table_references']['references'].append(reference_dict)
        html_saver.save_icarus_data(json_output_dir, main_data_dict['table_references'], 'table_references', as_text=False)
    else:
        if chr_full_names:
            chr = chr_full_names[0]
            chr_link, chr_name, chr_genome, chr_size, tooltip = get_info_by_chr(chr, aligned_bases_by_chr, chr_sizes, contigs_fpaths,
                                                                                contig_names_by_refs, one_chromosome=True)
            main_data_dict['one_reference'] = dict()
            main_data_dict['one_reference']['alignment_link'] = chr_link
            main_data_dict['one_reference']['ref_fpath'] = os.path.basename(ref_fpath)
            main_data_dict['one_reference']['ref_fragments'] = str(num_contigs[chr])
            main_data_dict['one_reference']['ref_size'] = format_long_numbers(chr_size)
            main_data_dict['one_reference']['ref_gf'] = '%.3f' % chr_genome
            main_data_dict['one_reference']['num_misassemblies'] = str(num_misassemblies[chr])
            icarus_links["links"].append(chr_link)
            icarus_links["links_names"].append(qconfig.icarus_link)
            html_saver.save_icarus_data(json_output_dir, main_data_dict['one_reference'], 'menu_reference', as_text=False)
    html_saver.save_icarus_html(main_menu_template_fpath, main_menu_fpath, main_data_dict)

    html_saver.save_icarus_links(output_dirpath, icarus_links)
    if json_output_dir:
        json_saver.save_icarus_links(json_output_dir, icarus_links)

    return main_menu_fpath