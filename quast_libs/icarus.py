#!/usr/bin/python -O

############################################################################
# Copyright (c) 2015-2022 Saint Petersburg State University
# Copyright (c) 2011-2015 Saint Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################

from __future__ import with_statement


from quast_libs.icarus_builder import prepare_alignment_data_for_one_ref, save_alignment_data_for_one_ref, save_contig_size_html, \
    get_assemblies_data, get_contigs_data
from quast_libs.icarus_parser import parse_contigs_fpath, parse_features_data, parse_cov_fpath, parse_genes_data
from quast_libs.icarus_parser import parse_aligner_contig_report
from quast_libs.icarus_utils import make_output_dir, group_references, format_cov_data, format_long_numbers, get_info_by_chr, \
    get_assemblies, check_misassembled_blocks

try:
   from collections import OrderedDict
except ImportError:
   from quast_libs.site_packages.ordered_dict import OrderedDict

import os
import re
from collections import defaultdict
from quast_libs import qconfig, qutils, fastaparser, genome_analyzer
from quast_libs.ca_utils.misc import ref_labels_by_chromosomes
import quast_libs.html_saver.html_saver as html_saver

from quast_libs import reporting
from quast_libs.log import get_logger
logger = get_logger(qconfig.LOGGER_DEFAULT_NAME)


def do(contigs_fpaths, contig_report_fpath_pattern, output_dirpath, ref_fpath,
       cov_fpath=None, physical_cov_fpath=None, gc_fpath=None,
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

    if ref_fpath:
        for name, seq in fastaparser.read_fasta(ref_fpath):
            chr_name = name.split()[0]
            chr_names.append(chr_name)
            chr_len = len(seq)
            total_genome_size += chr_len
            reference_chromosomes[chr_name] = chr_len
        virtual_genome_shift = 100
        cumulative_ref_lengths = [0]
        if ref_labels_by_chromosomes:
            contig_names_by_refs = ref_labels_by_chromosomes
        elif sum(reference_chromosomes.values()) > qconfig.MAX_SIZE_FOR_COMB_PLOT:
            contig_names_by_refs = dict()
            if len(chr_names) > qconfig.ICARUS_MAX_CHROMOSOMES:
                summary_len = 0
                num_parts = 1
                html_name = qconfig.alignment_viewer_part_name + str(num_parts)
                for chr_name, chr_len in reference_chromosomes.items():
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
            aligned_blocks, misassembled_id_to_structure, contigs, ambiguity_alignments = parse_aligner_contig_report(report_fpath,
                                                                                          list(reference_chromosomes.keys()), cumulative_ref_lengths)
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

    if ref_fpath:
        features_data = parse_features_data(features, cumulative_ref_lengths, chr_names)
    if contigs_fpaths and qconfig.gene_finding:
        parse_genes_data(contigs_by_assemblies, genes_by_labels)
    if reference_chromosomes and lists_of_aligned_blocks:
        assemblies = get_assemblies(contigs_fpaths, lists_of_aligned_blocks, virtual_genome_size, find_similar)
    if (assemblies or contigs_by_assemblies) and qconfig.create_icarus_html:
        icarus_html_fpath = js_data_gen(assemblies, contigs_fpaths, reference_chromosomes,
                    output_dirpath, structures_by_labels, contig_names_by_refs=contig_names_by_refs, ref_fpath=ref_fpath, stdout_pattern=stdout_pattern,
                    ambiguity_alignments_by_labels=ambiguity_alignments_by_labels, contigs_by_assemblies=contigs_by_assemblies,
                    features_data=features_data,
                    gc_fpath=gc_fpath, cov_fpath=cov_fpath, physical_cov_fpath=physical_cov_fpath, json_output_dir=json_output_dir)
    else:
        icarus_html_fpath = None

    return icarus_html_fpath


def natural_sort(string_):
    return [int(s) if s.isdigit() else s for s in re.split(r'(\d+)', string_)]


def js_data_gen(assemblies, contigs_fpaths, chromosomes_length, output_dirpath, structures_by_labels,
                contigs_by_assemblies, ambiguity_alignments_by_labels=None, contig_names_by_refs=None, ref_fpath=None,
                stdout_pattern=None, features_data=None, gc_fpath=None, cov_fpath=None, physical_cov_fpath=None, json_output_dir=None):
    chr_names = []
    if chromosomes_length and assemblies:
        chr_to_aligned_blocks = OrderedDict()
        chr_names = list(chromosomes_length.keys())
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

    cov_data, max_depth = parse_cov_fpath(cov_fpath, chr_names, chr_full_names, contig_names_by_refs)
    physical_cov_data, physical_max_depth = parse_cov_fpath(physical_cov_fpath, chr_names, chr_full_names, contig_names_by_refs)
    gc_data, max_gc = parse_cov_fpath(gc_fpath, chr_names, chr_full_names, contig_names_by_refs)

    chr_sizes = {}
    num_contigs = {}
    aligned_bases = genome_analyzer.get_ref_aligned_lengths()
    nx_marks = [reporting.Fields.N50, reporting.Fields.Nx, reporting.Fields.NG50, reporting.Fields.NGx]

    assemblies_data, assemblies_contig_size_data, assemblies_n50 = get_assemblies_data(contigs_fpaths, output_all_files_dir_path, stdout_pattern, nx_marks)

    ref_contigs_dict = {}
    chr_lengths_dict = {}

    ref_data = 'var references_by_id = {};\n'
    chr_names_by_id = dict((chrom, str(i)) for i, chrom in enumerate(chr_names))
    for chrom, i in chr_names_by_id.items():
        ref_data += 'references_by_id["' + str(i) + '"] = "' + chrom + '";\n'
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

        cov_data_str = format_cov_data(chr, cov_data, 'coverage_data', max_depth, 'reads_max_depth') if cov_data else None
        physical_cov_data_str = format_cov_data(chr, physical_cov_data, 'physical_coverage_data', physical_max_depth, 'physical_max_depth') \
            if physical_cov_data else None
        gc_data_str = format_cov_data(chr, gc_data, 'gc_data', 100, 'max_gc') if gc_data else None

        alignment_viewer_fpath, ref_data_str, contigs_structure_str, additional_assemblies_data, ms_selectors, num_misassemblies[chr], aligned_assemblies[chr] = \
            prepare_alignment_data_for_one_ref(chr, chr_full_names, chr_names_by_id, ref_contigs, data_str, chr_to_aligned_blocks, structures_by_labels,
                                               contigs_by_assemblies, ambiguity_alignments_by_labels=ambiguity_alignments_by_labels,
                                               cov_data_str=cov_data_str, physical_cov_data_str=physical_cov_data_str, gc_data_str=gc_data_str,
                                               contig_names_by_refs=contig_names_by_refs, output_dir_path=output_all_files_dir_path)
        ref_name = qutils.name_from_fpath(ref_fpath)
        save_alignment_data_for_one_ref(chr, ref_contigs, ref_name, json_output_dir, alignment_viewer_fpath, ref_data_str, ms_selectors,
                                        ref_data=ref_data, features_data=features_data, assemblies_data=assemblies_data,
                                        contigs_structure_str=contigs_structure_str, additional_assemblies_data=additional_assemblies_data)

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
        for chr in sorted(chr_full_names, key=natural_sort):
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

    return main_menu_fpath