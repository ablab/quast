############################################################################
# Copyright (c) 2015-2022 Saint Petersburg State University
# Copyright (c) 2011-2015 Saint Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################
from __future__ import with_statement
from __future__ import division
import os
import re
from collections import defaultdict

from quast_libs import qconfig

qconfig.check_python_version()
from quast_libs import contigs_analyzer, fastaparser, reporting, plotter_data
from quast_libs import qutils
from quast_libs.qutils import correct_seq, correct_name, get_uniq_name, run_parallel

from quast_libs.log import get_logger
logger = get_logger(qconfig.LOGGER_META_NAME)


class Assembly:
    def __init__(self, fpath, label):
        self.fpath = fpath
        self.label = label
        self.name = os.path.splitext(os.path.basename(self.fpath))[0]


def parallel_partition_contigs(asm, assemblies_by_ref, corrected_dirpath, alignments_fpath_template):
    assembly_label = qutils.label_from_fpath(asm.fpath)
    corr_assembly_label = qutils.label_from_fpath_for_fname(asm.fpath)
    logger.info('  ' + 'processing ' + assembly_label)
    added_ref_asm = []
    not_aligned_fname = corr_assembly_label + '_not_aligned_anywhere.fasta'
    not_aligned_fpath = os.path.join(corrected_dirpath, not_aligned_fname)
    contigs = {}
    aligned_contig_names = set()
    aligned_contigs_for_each_ref = {}
    contigs_seq = fastaparser.read_fasta_one_time(asm.fpath)
    alignments_fpath = alignments_fpath_template % corr_assembly_label
    if os.path.exists(alignments_fpath):
        with open(alignments_fpath) as f:
            for line in f:
                values = line.split()
                if values[0] in contigs_analyzer.ref_labels_by_chromosomes.keys():
                    ref_name = contigs_analyzer.ref_labels_by_chromosomes[values[0]]
                    ref_contigs_names = values[1:]
                    ref_contigs_fpath = os.path.join(
                        corrected_dirpath, corr_assembly_label + '_to_' + ref_name + '.fasta')
                    if ref_name not in aligned_contigs_for_each_ref:
                        aligned_contigs_for_each_ref[ref_name] = []

                    for (cont_name, seq) in contigs_seq:
                        if not cont_name in contigs:
                            contigs[cont_name] = seq

                        if cont_name in ref_contigs_names and cont_name not in aligned_contigs_for_each_ref[ref_name]:
                            # Collecting all aligned contigs names in order to further extract not aligned
                            aligned_contig_names.add(cont_name)
                            aligned_contigs_for_each_ref[ref_name].append(cont_name)
                            fastaparser.write_fasta(ref_contigs_fpath, [(cont_name, seq)], 'a')

                    ref_asm = Assembly(ref_contigs_fpath, assembly_label)
                    if ref_asm.name not in added_ref_asm:
                        if ref_name in assemblies_by_ref:
                            assemblies_by_ref[ref_name].append(ref_asm)
                            added_ref_asm.append(ref_asm.name)
        if qconfig.space_efficient:
            os.remove(alignments_fpath)

    # Extraction not aligned contigs
    all_contigs_names = set(contigs.keys())
    not_aligned_contigs_names = all_contigs_names - aligned_contig_names
    fastaparser.write_fasta(not_aligned_fpath, [(name, contigs[name]) for name in not_aligned_contigs_names])

    not_aligned_asm = Assembly(not_aligned_fpath, asm.label)
    return assemblies_by_ref, not_aligned_asm


def partition_contigs(assemblies, ref_fpaths, corrected_dirpath, alignments_fpath_template, labels):
    # array of assemblies for each reference
    assemblies_by_ref = dict([(qutils.name_from_fpath(ref_fpath), []) for ref_fpath in ref_fpaths])
    n_jobs = min(qconfig.max_threads, len(assemblies))
    parallel_run_args = [(asm, assemblies_by_ref, corrected_dirpath, alignments_fpath_template)
                         for asm in assemblies]
    assemblies_dicts, not_aligned_assemblies = run_parallel(parallel_partition_contigs, parallel_run_args, n_jobs)
    assemblies_by_ref = []
    for ref_fpath in ref_fpaths:
        ref_name = qutils.name_from_fpath(ref_fpath)
        not_sorted_assemblies = set([val for sublist in (assemblies_dicts[i][ref_name] for i in range(len(assemblies_dicts))) for val in sublist])
        sorted_assemblies = []
        for label in labels:  # sort by label
            for assembly in not_sorted_assemblies:
                if assembly.label == label:
                    sorted_assemblies.append(assembly)
                    break
        assemblies_by_ref.append((ref_fpath, sorted_assemblies))
    return assemblies_by_ref, not_aligned_assemblies


def correct_assemblies(contigs_fpaths, output_dirpath, labels):
    corrected_dirpath = os.path.join(output_dirpath, qconfig.corrected_dirname)
    # we need correction but do not need min-contig filtration
    min_contig = qconfig.min_contig
    qconfig.min_contig = 0
    corrected_contigs_fpaths, old_contigs_fpaths = qutils.correct_contigs(contigs_fpaths, corrected_dirpath, labels, reporting=None)
    qconfig.min_contig = min_contig
    assemblies = [Assembly(fpath, qutils.label_from_fpath(fpath)) for fpath in old_contigs_fpaths]
    corrected_labels = [asm.label for asm in assemblies]

    if qconfig.draw_plots or qconfig.html_report:
        corr_fpaths = [asm.fpath for asm in assemblies]
        corr_labels = [asm.label for asm in assemblies]
        plotter_data.save_colors_and_ls(corr_fpaths, labels=corr_labels)
    return assemblies, corrected_labels


def correct_meta_references(ref_fpaths, corrected_dirpath, downloaded_refs=False):
    corrected_ref_fpaths = []

    combined_ref_fpath = os.path.join(corrected_dirpath, qconfig.combined_ref_name)

    chromosomes_by_refs = {}

    def _proceed_seq(seq_name, seq, ref_name, ref_fasta_ext, total_references, ref_fpath):
        seq_fname = ref_name
        seq_fname += ref_fasta_ext

        if total_references > 1:
            corr_seq_fpath = corrected_ref_fpaths[-1]
        else:
            corr_seq_fpath = qutils.unique_corrected_fpath(os.path.join(corrected_dirpath, seq_fname))
            corrected_ref_fpaths.append(corr_seq_fpath)
        corr_seq_name = qutils.name_from_fpath(corr_seq_fpath) + '_' + seq_name
        if not qconfig.no_check:
            corr_seq = correct_seq(seq, ref_fpath)
            if not corr_seq:
                return None, None

        fastaparser.write_fasta(corr_seq_fpath, [(corr_seq_name, seq)], 'a')

        contigs_analyzer.ref_labels_by_chromosomes[corr_seq_name] = qutils.name_from_fpath(corr_seq_fpath)
        chromosomes_by_refs[ref_name].append((corr_seq_name, len(seq)))

        return corr_seq_name, corr_seq_fpath

    ref_fnames = [os.path.basename(ref_fpath) for ref_fpath in ref_fpaths]
    ref_names = []
    for ref_fname in ref_fnames:
        ref_name, ref_fasta_ext = qutils.splitext_for_fasta_file(ref_fname)
        ref_names.append(ref_name)

    excluded_ref_fpaths = []
    ref_names = qutils.process_labels(ref_fpaths)
    for ref_fpath, ref_name in zip(ref_fpaths, ref_names):
        total_references = 0
        ref_fname = os.path.basename(ref_fpath)
        _, ref_fasta_ext = qutils.splitext_for_fasta_file(ref_fname)

        chromosomes_by_refs[ref_name] = []
        used_seq_names = defaultdict(int)

        corr_seq_fpath = None
        for i, (seq_name, seq) in enumerate(fastaparser.read_fasta(ref_fpath)):
            total_references += 1
            seq_name = correct_name(seq_name, qutils.MAX_CONTIG_NAME - len(ref_name) - 1)
            uniq_seq_name = get_uniq_name(seq_name, used_seq_names)
            used_seq_names[seq_name] += 1
            corr_seq_name, corr_seq_fpath = _proceed_seq(uniq_seq_name, seq, ref_name, ref_fasta_ext, total_references, ref_fpath)
            if not corr_seq_name:
                break
        if corr_seq_fpath:
            logger.main_info('  ' + ref_fpath + ' ==> ' + qutils.name_from_fpath(corr_seq_fpath) + '')
            fastaparser.write_fasta(combined_ref_fpath, fastaparser.read_fasta(corr_seq_fpath), 'a')
        elif downloaded_refs:
            logger.warning('Skipping ' + ref_fpath + ' because it'
                           ' is empty or contains incorrect sequences (header-only or with non-ACGTN characters)!')
            # cleaning
            for corr_seq_name, _ in chromosomes_by_refs[ref_name]:
                del contigs_analyzer.ref_labels_by_chromosomes[corr_seq_name]
            del chromosomes_by_refs[ref_name]
            corrected_ref_fpaths.pop()
            excluded_ref_fpaths.append(ref_fpath)
        else:
            logger.error('Reference file ' + ref_fpath +
                         ' is empty or contains incorrect sequences (header-only or with non-ACGTN characters)!',
                         exit_with_code=1)
    for excluded in excluded_ref_fpaths:
        ref_fpaths.remove(excluded)

    if len(chromosomes_by_refs) > 0:
        logger.main_info('  All references were combined in ' + qconfig.combined_ref_name)
    else:
        logger.warning('All references were skipped!')

    return corrected_ref_fpaths, combined_ref_fpath, chromosomes_by_refs, ref_fpaths


def get_downloaded_refs_with_alignments(genome_info_fpath, ref_fpaths, chromosomes_by_refs):
    refs_len = {}
    with open(genome_info_fpath, 'r') as report_file:
        report_file.readline()
        for line in report_file:
            if line == '\n' or not line:
                break
            lengths = re.findall(r'length: (\d+)', line)
            if lengths and len(lengths) == 2:
                line = line.split()
                refs_len[line[0]] = (lengths[0], lengths[1])

    corr_refs = []
    for ref_fpath in ref_fpaths:
        ref_fname = os.path.basename(ref_fpath)
        ref, ref_fasta_ext = qutils.splitext_for_fasta_file(ref_fname)
        aligned_len = 0
        all_len = 0
        for chromosome in chromosomes_by_refs[ref]:
            if chromosome[0] in refs_len:
                aligned_len += int(refs_len[chromosome[0]][1])
                all_len += int(refs_len[chromosome[0]][0])
        if not aligned_len:
            continue
        if aligned_len > all_len * qconfig.downloaded_ref_min_aligned_rate:
            corr_refs.append(ref_fpath)
    return corr_refs


def calculate_ave_read_support(combined_output_dirpath, assemblies):
    unique_contigs_fpath = os.path.join(combined_output_dirpath, qconfig.detailed_contigs_reports_dirname,
                                        qconfig.unique_contigs_fname_pattern)
    for assembly in assemblies:
        aligned_contigs_by_ref = dict()
        assembly_label = qutils.label_from_fpath(assembly.fpath)
        corr_assembly_label = qutils.label_from_fpath_for_fname(assembly.fpath)
        with open(unique_contigs_fpath % corr_assembly_label) as in_f:
            for line in in_f:
                ref_name, contig_len, contig_cov = line.strip().split('\t')
                aligned_contigs_by_ref.setdefault(ref_name, []).append((float(contig_len), float(contig_cov)))
        for ref_name, contigs in aligned_contigs_by_ref.items():
            ref_cov = sum(contig_cov * aligned_len for (aligned_len, contig_cov) in contigs) / \
                      sum(aligned_len for (aligned_len, contig_cov) in contigs)
            corr_assembly_label = qutils.label_from_fpath_for_fname(assembly.fpath)
            ref_contigs_fpath = os.path.join(
                        os.path.dirname(assembly.fpath), corr_assembly_label + '_to_' + ref_name + '.fasta')
            qconfig.assembly_labels_by_fpath[ref_contigs_fpath] = assembly_label
            report = reporting.get(ref_contigs_fpath, ref_name=ref_name)
            report.add_field(reporting.Fields.AVE_READ_SUPPORT, '%.2f' % ref_cov)

