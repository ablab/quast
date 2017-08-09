############################################################################
# Copyright (c) 2015-2017 Saint Petersburg State University
# Copyright (c) 2011-2015 Saint Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################

from __future__ import with_statement
import os
from collections import defaultdict
from os.path import join, basename, dirname, exists
import shutil
from distutils import dir_util

from quast_libs import fastaparser, qconfig, qutils, reads_analyzer
from quast_libs.log import get_logger
from quast_libs.qutils import splitext_for_fasta_file, is_non_empty_file, split_by_ns, download_external_tool, add_suffix
from quast_libs.ra_utils.misc import bwa_fpath, sambamba_fpath, sort_bam, get_correct_names_for_chroms, bam_to_bed, \
    bwa_index
from quast_libs.reads_analyzer import get_coverage

logger = get_logger(qconfig.LOGGER_DEFAULT_NAME)
mp_polished_suffix = 'mp_polished'
single_polished_suffix = 'single_polished'


def parse_uncovered_fpath(uncovered_fpath, fasta_fpath, return_covered_regions=True):
    regions = defaultdict(list)
    prev_start = defaultdict(int)
    if exists(uncovered_fpath):
        with open(uncovered_fpath) as f:
            for line in f:
                chrom, start, end = line.split('\t')
                if return_covered_regions:
                    if prev_start[chrom] != int(start):
                        regions[chrom].append((prev_start[chrom], int(start)))
                    prev_start[chrom] = int(end)
                else:
                    regions[chrom].append((int(start), int(end)))
    if return_covered_regions:
        for name, seq in fastaparser.read_fasta(fasta_fpath):
            if name in regions:
                if prev_start[name] != len(seq):
                    regions[name].append((prev_start[name], len(seq)))
            else:
                regions[name].append((0, len(seq)))
    return regions


def preprocess_reference(ref_fpath, tmp_dir, uncovered_fpath):
    uncovered_regions = parse_uncovered_fpath(uncovered_fpath, ref_fpath, return_covered_regions=False)
    splitted_fasta = []
    for name, seq in fastaparser.read_fasta(ref_fpath):
        if name in uncovered_regions:
            cur_contig_start = 0
            total_contigs = 0
            for start, end in uncovered_regions[name]:
                total_contigs = split_by_ns(seq[cur_contig_start: start], name, splitted_fasta, total_contigs=total_contigs)
                cur_contig_start = end
            split_by_ns(seq[cur_contig_start:], name, splitted_fasta, total_contigs=total_contigs)
        else:
            split_by_ns(seq, name, splitted_fasta)
    processed_ref_fpath = join(tmp_dir, basename(ref_fpath))
    fastaparser.write_fasta(processed_ref_fpath, splitted_fasta)
    return processed_ref_fpath


def prepare_config_spades(fpath, kmer, ref_fpath, tmp_dir):
    subst_dict = dict()
    subst_dict["K"] = str(kmer)
    subst_dict["dataset"] = os.path.abspath(ref_fpath)
    subst_dict["output_base"] = os.path.abspath(tmp_dir)
    subst_dict["tmp_dir"] = subst_dict["output_base"]
    subst_dict["max_threads"] = str(qconfig.max_threads)

    with open(fpath) as config:
        template_content = config.readlines()
    with open(fpath, 'w') as config:
        for line in template_content:
            if len(line.split()) > 1 and line.split()[0] in subst_dict:
                config.write("%s  %s\n" % (line.split()[0], subst_dict[line.split()[0]]))
            else:
                config.write(line)


def align_ideal_assembly(ref_fpath, assembly_fpath, output_dir, log_fpath, err_fpath):
    sam_fpath = join(output_dir, basename(assembly_fpath) + '.sam')
    bam_fpath = sam_fpath.replace('.sam', '.bam')
    bam_mapped_fpath = add_suffix(bam_fpath, 'mapped')
    bam_sorted_fpath = add_suffix(bam_fpath, 'sorted')
    if not is_non_empty_file(bam_fpath):
        bwa_index(ref_fpath, err_fpath, logger)
        qutils.call_subprocess([bwa_fpath('bwa'), 'mem', '-t', str(qconfig.max_threads), ref_fpath, assembly_fpath],
                               stdout=open(sam_fpath, 'w'), stderr=open(err_fpath, 'a'), logger=logger)
        qutils.call_subprocess([sambamba_fpath('sambamba'), 'view', '-t', str(qconfig.max_threads), '-h', '-f', 'bam',
                                '-S', sam_fpath], stdout=open(bam_fpath, 'w'), stderr=open(err_fpath, 'a'), logger=logger)
    if not is_non_empty_file(bam_sorted_fpath):
        qutils.call_subprocess([sambamba_fpath('sambamba'), 'view', '-t', str(qconfig.max_threads), '-h', '-f', 'bam',
                                '-F', 'not unmapped', bam_fpath],
                               stdout=open(bam_mapped_fpath, 'w'), stderr=open(err_fpath, 'a'), logger=logger)
        sort_bam(bam_mapped_fpath, bam_sorted_fpath, err_fpath, logger)
    cov_fpath = join(output_dir, basename(assembly_fpath) + '.cov')
    uncovered_fpath = add_suffix(cov_fpath, 'uncovered')
    ref_name = qutils.name_from_fpath(ref_fpath)
    correct_chr_names = get_correct_names_for_chroms(output_dir, ref_fpath, sam_fpath, err_fpath, assembly_fpath, logger)
    get_coverage(output_dir, ref_fpath, ref_name, bam_fpath, bam_sorted_fpath, log_fpath, err_fpath,
                 correct_chr_names, cov_fpath, uncovered_fpath=uncovered_fpath, create_cov_files=False)
    return uncovered_fpath


def find_overlaps(intervals1, intervals2, overlap=0):
    interval_idx = 0
    for i, (start, end) in enumerate(intervals1):
        for j, (start2, end2) in enumerate(intervals2[interval_idx: ]):
            if start2 > end:
                break
            if end2 < start:
                continue
            if min(end, end2) - max(start, start2) >= overlap + 1:
                start = min(start, start2)
                end = max(end, end2)
        interval_idx += j
        intervals1[i] = (start, end)
    return merge_overlaps(intervals1)


def merge_overlaps(intervals):
    saved = list(intervals[0])
    for start, end in sorted(intervals):
        if start <= saved[1] + 1:
            saved[1] = max(saved[1], end)
        else:
            yield tuple(saved)
            saved[0] = start
            saved[1] = end
    yield tuple(saved)


def connect_with_matepairs(bam_fpath, output_dirpath, err_fpath):
    bam_filtered_fpath = add_suffix(bam_fpath, 'filtered')
    qutils.call_subprocess([sambamba_fpath('sambamba'), 'view', '-t', str(qconfig.max_threads), '-h', '-f', 'bam',
                            '-F', 'proper_pair and not supplementary and not duplicate', bam_fpath],
                           stdout=open(bam_filtered_fpath, 'w'), stderr=open(err_fpath, 'a'), logger=logger)
    ## sort by read names
    bam_filtered_sorted_fpath = add_suffix(bam_filtered_fpath, 'sorted')
    sort_bam(bam_filtered_fpath, bam_filtered_sorted_fpath, err_fpath, logger, sort_rule='-n')
    bed_fpath = bam_to_bed(output_dirpath, 'matepairs', bam_filtered_sorted_fpath, err_fpath, logger, bedpe=True, only_intervals=True)
    matepair_regions = defaultdict(list)
    with open(bed_fpath) as bed:
        for l in bed:
            fs = l.split()
            matepair_regions[fs[0]].append((int(fs[1]), int(fs[2])))
    return matepair_regions


def is_overlapped(region1, region2, sorted_mp_intervals):
    gap_start, gap_end = region1[1], region2[0]
    for i, (interval_start, interval_end) in enumerate(sorted_mp_intervals):
        if interval_start <= gap_start and interval_end >= gap_end:
            sorted_mp_intervals[:] = sorted_mp_intervals[i: ]
            return True
    sorted_mp_intervals[:] = sorted_mp_intervals[i: ]
    return False


def fill_gaps_mate_pair(bam_fpath, ref_fpath, assembly_fpath, assembly_covered_regions, output_dir, uncovered_fpath, err_fpath):
    matepair_reads_covered_regions = parse_uncovered_fpath(uncovered_fpath, ref_fpath, return_covered_regions=True)
    final_fasta = []
    matepair_regions = connect_with_matepairs(bam_fpath, output_dir, err_fpath)
    final_assembly_fpath = add_suffix(assembly_fpath, mp_polished_suffix)
    for name, seq in fastaparser.read_fasta(ref_fpath):
        covered_regions = list(find_overlaps(assembly_covered_regions[name], matepair_reads_covered_regions[name], overlap=50))
        total_contigs = 0
        if name not in matepair_regions or len(covered_regions) == 1:
            for region in covered_regions:
                final_fasta.append((name.split()[0] + "_" + str(total_contigs + 1), seq[region[0]: region[1]]))
                total_contigs += 1
        else:
            frags_to_merge = [covered_regions.pop(0)]
            sorted_mp_intervals = sorted(matepair_regions[name])
            while covered_regions:
                region2 = covered_regions.pop(0)
                if is_overlapped(frags_to_merge[-1], region2, sorted_mp_intervals):
                    frags_to_merge.append(region2)
                else:
                    merged_seq = merge_fragments_with_ns(seq, frags_to_merge)
                    final_fasta.append((name.split()[0] + "_" + str(total_contigs + 1), merged_seq))
                    total_contigs += 1
                    frags_to_merge = [region2]
            if frags_to_merge:
                merged_seq = merge_fragments_with_ns(seq, frags_to_merge)
                final_fasta.append((name.split()[0] + "_" + str(total_contigs + 1), merged_seq))
                total_contigs += 1
    fastaparser.write_fasta(final_assembly_fpath, final_fasta)
    return final_assembly_fpath


def merge_fragments_with_ns(seq, merged_frags):
    merged_seq = []
    for i in range(1, len(merged_frags)):
        merged_seq.append(seq[merged_frags[i - 1][0]: merged_frags[i - 1][1] + 1])
        merged_seq.append('N' * (merged_frags[i][0] - merged_frags[i - 1][1]))
    merged_seq.append(seq[merged_frags[-1][0]: merged_frags[-1][1]])
    return ''.join(merged_seq)


def fill_gaps_single(ref_fpath, assembly_fpath, assembly_covered_regions, uncovered_fpath):
    single_reads_covered_regions = parse_uncovered_fpath(uncovered_fpath, ref_fpath, return_covered_regions=True)
    final_assembly_fpath = add_suffix(assembly_fpath, single_polished_suffix)
    final_fasta = []
    for name, seq in fastaparser.read_fasta(ref_fpath):
        covered_regions = find_overlaps(assembly_covered_regions[name], single_reads_covered_regions[name], overlap=50)
        for i, region in enumerate(covered_regions):
            start, end = region
            final_fasta.append((name.split()[0] + "_" + str(i + 1), seq[start: end]))
    fastaparser.write_fasta(final_assembly_fpath, final_fasta)
    return final_assembly_fpath


def polish_assembly(ref_fpath, spades_output_fpath, output_dir, tmp_dir):
    log_fpath = join(tmp_dir, 'ideal_assembly.log')
    err_fpath = join(tmp_dir, 'ideal_assembly.err')
    assembly_uncovered_fpath = align_ideal_assembly(ref_fpath, spades_output_fpath, tmp_dir, log_fpath, err_fpath)
    assembly_covered_regions = parse_uncovered_fpath(assembly_uncovered_fpath, ref_fpath, return_covered_regions=True)
    if qconfig.unpaired_reads:
        bam_fpath, uncovered_fpath = reads_analyzer.align_reference(ref_fpath, join(dirname(output_dir), qconfig.reads_stats_dirname),
                                                                    using_reads='single')
        spades_output_fpath = fill_gaps_single(ref_fpath, spades_output_fpath, assembly_covered_regions, uncovered_fpath)
    if qconfig.mate_pairs:
        if qconfig.unpaired_reads:
            assembly_uncovered_fpath = align_ideal_assembly(ref_fpath, spades_output_fpath, tmp_dir, log_fpath, err_fpath)
            assembly_covered_regions = parse_uncovered_fpath(assembly_uncovered_fpath, ref_fpath, return_covered_regions=True)
        bam_fpath, uncovered_fpath = reads_analyzer.align_reference(ref_fpath, join(dirname(output_dir), qconfig.reads_stats_dirname),
                                                                    using_reads='mate_pair')
        spades_output_fpath = fill_gaps_mate_pair(bam_fpath, ref_fpath, spades_output_fpath, assembly_covered_regions,
                                                  tmp_dir, uncovered_fpath, err_fpath)
    return spades_output_fpath


def do(ref_fpath, original_ref_fpath, output_dirpath):
    logger.print_timestamp()
    logger.main_info("Simulating Ideal Assembly...")

    uncovered_fpath = None
    if qconfig.paired_reads or qconfig.reference_sam or qconfig.reference_sam:
        sam_fpath, uncovered_fpath = reads_analyzer.align_reference(ref_fpath, join(dirname(output_dirpath),
                                                                    qconfig.reads_stats_dirname), using_reads='paired_end')
    insert_size = qconfig.ideal_assembly_insert_size
    if insert_size == 'auto' or not insert_size:
        insert_size = qconfig.ideal_assembly_default_IS
    if insert_size % 2 == 0:
        insert_size += 1
        logger.notice('  Current implementation cannot work with even insert sizes, '
                      'will use the closest odd value (%d)' % insert_size)

    ref_basename, fasta_ext = splitext_for_fasta_file(os.path.basename(ref_fpath))
    result_basename = '%s.%s.is%d.fasta' % (ref_basename, qconfig.ideal_assembly_basename, insert_size)
    if qconfig.paired_reads and qconfig.unpaired_reads:
        result_basename = add_suffix(result_basename, single_polished_suffix)
    if qconfig.paired_reads and qconfig.mate_pairs:
        result_basename = add_suffix(result_basename, mp_polished_suffix)
    result_fpath = os.path.join(output_dirpath, result_basename)

    original_ref_basename, fasta_ext = splitext_for_fasta_file(os.path.basename(original_ref_fpath))
    prepared_ideal_assembly_basename = '%s.%s.is%d.fasta' % (original_ref_basename, qconfig.ideal_assembly_basename, insert_size)
    ref_prepared_ideal_assembly = os.path.join(os.path.dirname(original_ref_fpath), prepared_ideal_assembly_basename)

    if os.path.isfile(result_fpath) or os.path.isfile(ref_prepared_ideal_assembly):
        already_done_fpath = result_fpath if os.path.isfile(result_fpath) else ref_prepared_ideal_assembly
        logger.notice('  Will reuse already generated Ideal Assembly with insert size %d (%s)' %
                      (insert_size, already_done_fpath))
        return already_done_fpath

    if qconfig.platform_name == 'linux_32':
        logger.warning('  Sorry, can\'t create Ideal Assembly on this platform, skipping...')
        return None

    base_aux_dir = os.path.join(qconfig.LIBS_LOCATION, 'ideal_assembly')
    configs_dir = os.path.join(base_aux_dir, 'configs')
    binary_fpath = download_external_tool('spades', os.path.join(base_aux_dir, 'bin'), 'spades', platform_specific=True)
    if not os.path.isfile(binary_fpath):
        logger.warning('  Sorry, can\'t create Ideal Assembly, skipping...')
        return None

    log_fpath = os.path.join(output_dirpath, 'spades.log')

    tmp_dir = os.path.join(output_dirpath, 'tmp')
    if os.path.isdir(tmp_dir):
        shutil.rmtree(tmp_dir)
    os.makedirs(tmp_dir)

    processed_ref_fpath = preprocess_reference(ref_fpath, tmp_dir, uncovered_fpath)

    dst_configs = os.path.join(tmp_dir, 'configs')
    main_config = os.path.join(dst_configs, 'config.info')
    dir_util._path_created = {}  # see http://stackoverflow.com/questions/9160227/dir-util-copy-tree-fails-after-shutil-rmtree
    dir_util.copy_tree(configs_dir, dst_configs, preserve_times=False)

    prepare_config_spades(main_config, insert_size, processed_ref_fpath, tmp_dir)

    log_file = open(log_fpath, 'w')
    spades_output_fpath = os.path.join(tmp_dir, 'K%d' % insert_size, 'ideal_assembly.fasta')
    logger.info('  ' + 'Running SPAdes with K=' + str(insert_size) + '...')
    return_code = qutils.call_subprocess(
        [binary_fpath, main_config], stdout=log_file, stderr=log_file, indent='    ')
    if return_code != 0 or not os.path.isfile(spades_output_fpath):
        logger.error('  Failed to create Ideal Assembly, see log for details: ' + log_fpath)
        return None

    if qconfig.mate_pairs or qconfig.unpaired_reads:
        spades_output_fpath = polish_assembly(ref_fpath, spades_output_fpath, output_dirpath, tmp_dir)

    shutil.move(spades_output_fpath, result_fpath)
    logger.info('  ' + 'Ideal Assembly saved to ' + result_fpath)
    logger.notice('You can copy it to ' + ref_prepared_ideal_assembly +
                  ' and QUAST will reuse it in further runs against the same reference (' + original_ref_fpath + ')')

    if not qconfig.debug:
        shutil.rmtree(tmp_dir)

    logger.main_info('Done.')
    return result_fpath
