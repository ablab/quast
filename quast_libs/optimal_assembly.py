############################################################################
# Copyright (c) 2015-2022 Saint Petersburg State University
# Copyright (c) 2011-2015 Saint Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################

from __future__ import with_statement
import os
import shlex
import shutil
from collections import defaultdict
from os.path import join, basename, dirname, exists, isdir

from quast_libs import fastaparser, qconfig, qutils, reads_analyzer
from quast_libs.ca_utils.misc import minimap_fpath
from quast_libs.log import get_logger
from quast_libs.qutils import splitext_for_fasta_file, is_non_empty_file, download_external_tool, \
    add_suffix, get_dir_for_download
from quast_libs.ra_utils.misc import sort_bam, bam_to_bed, bedtools_fpath, sambamba_view, calculate_read_len
from quast_libs.reads_analyzer import calculate_insert_size

logger = get_logger(qconfig.LOGGER_DEFAULT_NAME)
mp_polished_suffix = 'mp_polished'
long_reads_polished_suffix = 'long_reads_polished'
MIN_OVERLAP = 10
MIN_CONTIG_LEN = 100
REPEAT_CONF_INTERVAL = 100

def parse_bed(bed_fpath):
    regions = defaultdict(list)
    if bed_fpath and exists(bed_fpath):
        with open(bed_fpath) as f:
            for line in f:
                fs = line.split('\t')
                chrom, start, end = fs[0], fs[1], fs[2]
                regions[chrom].append((int(start), int(end)))
    return regions


def remove_repeat_regions(ref_fpath, repeats_fpath, uncovered_fpath):
    repeats_regions = parse_bed(repeats_fpath)
    uncovered_regions = parse_bed(uncovered_fpath)
    unique_regions = defaultdict(list)
    for name, seq in fastaparser.read_fasta(ref_fpath):
        if name in repeats_regions:
            cur_contig_start = 0
            for start, end in repeats_regions[name]:
                if start > cur_contig_start:
                    unique_regions[name].append([cur_contig_start, start])
                else:
                    unique_regions[name].append([cur_contig_start, cur_contig_start])
                    unique_regions[name].append([start, start])
                cur_contig_start = end + 1
            if cur_contig_start < len(seq):
                unique_regions[name].append([cur_contig_start, len(seq)])
        else:
            unique_regions[name].append([0, len(seq)])
    unique_covered_regions = defaultdict(list)
    for name, regions in unique_regions.items():
        if name in uncovered_regions:
            cur_contig_idx = 0
            cur_contig_start, cur_contig_end = unique_regions[name][cur_contig_idx]
            for uncov_start, uncov_end in uncovered_regions[name]:
                while cur_contig_end < uncov_start:
                    unique_covered_regions[name].append([cur_contig_start, cur_contig_end])
                    cur_contig_idx += 1
                    if cur_contig_idx >= len(unique_regions[name]):
                        break
                    cur_contig_start, cur_contig_end = unique_regions[name][cur_contig_idx]
                if uncov_end < cur_contig_start:
                    continue
                if uncov_start <= cur_contig_start and uncov_end >= cur_contig_end:
                    cur_contig_idx += 1
                    if cur_contig_idx >= len(unique_regions[name]):
                        break
                    cur_contig_start, cur_contig_end = unique_regions[name][cur_contig_idx]
                elif cur_contig_start <= uncov_start <= cur_contig_end or cur_contig_start <= uncov_end <= cur_contig_end:
                    if uncov_start > cur_contig_start:
                        unique_covered_regions[name].append([cur_contig_start, uncov_start])
                    if uncov_end < cur_contig_end:
                        cur_contig_start = uncov_end
                    else:
                        cur_contig_idx += 1
                        if cur_contig_idx >= len(unique_regions[name]):
                            break
                        cur_contig_start, cur_contig_end = unique_regions[name][cur_contig_idx]
                else:
                    unique_covered_regions[name].append([cur_contig_start, cur_contig_end])
            for contig in unique_regions[name][cur_contig_idx:]:
                unique_covered_regions[name].append(contig)
        else:
            unique_covered_regions[name] = unique_regions[name]
    return unique_covered_regions



def get_joiners(ref_name, sam_fpath, bam_fpath, output_dirpath, err_fpath, using_reads):
    bam_filtered_fpath = add_suffix(bam_fpath, 'filtered')
    if not is_non_empty_file(bam_filtered_fpath):
        filter_rule = 'not unmapped and not supplementary and not secondary_alignment'
        sambamba_view(bam_fpath, bam_filtered_fpath, qconfig.max_threads, err_fpath, logger, filter_rule=filter_rule)
    bam_sorted_fpath = add_suffix(bam_filtered_fpath, 'sorted')
    if not is_non_empty_file(bam_sorted_fpath):
        sort_bam(bam_filtered_fpath, bam_sorted_fpath, err_fpath, logger, sort_rule='-n')
    bed_fpath = bam_to_bed(output_dirpath, using_reads, bam_sorted_fpath, err_fpath, logger, bedpe=using_reads == 'mp')
    intervals = defaultdict(list)
    if using_reads == 'mp':
        _, min_is, max_is = calculate_insert_size(sam_fpath, output_dirpath, ref_name, reads_suffix='mp')
    with open(bed_fpath) as bed:
        for l in bed:
            fs = l.split()
            if using_reads == 'mp' and max_is:
                interval_len = int(fs[2]) - int(fs[1])
                if min_is <= abs(interval_len) <= max_is:
                    intervals[fs[0]].append((int(fs[1]), int(fs[2])))
            else:
                intervals[fs[0]].append((int(fs[1]), int(fs[2])))
    return intervals


def get_regions_pairing(regions, joiners, mp_len=0):
    '''
    INPUT:
      -- list unique_covered_regions sorted by start coordinate
      -- list of joiners (long_reads or mate_pairs) sorted by start coordinate
      -- optional: average length of mate pair read (0 for long reads)
    OUTPUT:  pairs of regions IDs which define future scaffolds
    '''

    if len(regions) < 2:
        return []

    def __forward_pass(joiners, joiner_to_region_pair):
        # forward pass, joiners are sorted by their start coordinates
        current = 0  # current region idx
        for j in joiners:
            # if the joiner starts after the current region we need to switch current to the next region
            while regions[current][1] - MIN_OVERLAP < j[0]:
                if current + 1 == len(regions) - 1:  # the next region is the last, so can't be extended to the right
                    return
                current += 1
            # if the joiner ends before the next region it can't extend the current
            if j[1] < regions[current + 1][0] + MIN_OVERLAP:
                continue
            # additional check for mate-pairs: the read should overlap with current region
            if mp_len and j[0] + mp_len < regions[current][0] + MIN_OVERLAP:
                continue
            joiner_to_region_pair[j] = [current]  # saving the scaffold's start region

    def __reverse_pass(reversed_joiners, joiner_to_region_pair):
        # reverse pass, joiners are sorted by their end coordinates
        current = len(regions) - 1  # starting from the last region
        for j in reversed_joiners:
            # if the joiner ends before the current region we need to switch current to the previous region
            while j[1] < regions[current][0] + MIN_OVERLAP:
                if current - 1 == 0:  # the next region is the first, so can't be extended to the left
                    return
                current -= 1
            # if the long read starts after the next region it can't extend the current
            if regions[current - 1][1] - MIN_OVERLAP < j[0]:
                continue
            # additional check for mate-pairs: the read should overlap with current region
            if mp_len and j[1] - mp_len > regions[current][1] - MIN_OVERLAP:
                del joiner_to_region_pair[j]  # there is no end region for this mate-pair, so removing the start region
                continue
            joiner_to_region_pair[j].append(current)  # saving the scaffold's end region

    joiner_to_region_pair = dict()  # joiner to (Start_reg, End_reg) pairs correspondence
    __forward_pass(joiners, joiner_to_region_pair)
    __reverse_pass(sorted(joiner_to_region_pair.keys(), key=lambda x: x[1], reverse=True), joiner_to_region_pair)
    from collections import Counter
    simple_pairs = []
    min_reads = qconfig.upperbound_min_connections
    if not min_reads:
        min_reads = qconfig.MIN_CONNECT_MP if mp_len else qconfig.MIN_CONNECT_LR
    for pair in joiner_to_region_pair.values():
        if len(pair) < 2:
            continue
        for p in range(pair[0], pair[1]):
            simple_pairs.append((p, p + 1))
    read_counts = Counter(simple_pairs)
    uniq_pairs = set(simple_pairs)
    filtered = [p for p in uniq_pairs if read_counts[p] >= min_reads]
    return sorted(filtered)


def scaffolding(regions, region_pairing):
    '''
    INPUT:
      -- list of unique_covered_regions
      -- pairs of region IDs in format [(s1, e1), (s2, e2), ...], sorted with default tuple sorting
    OUTPUT: reference coordinates to extract as scaffolds
    '''

    if not region_pairing:  # no scaffolding, so output existing regions "as is"
        ref_coords_to_output = regions
    else:
        ref_coords_to_output = regions[:region_pairing[0][0]]  # output regions before the first scaffold "as is"
        scaf_start_reg = region_pairing[0][0]
        scaf_end_reg = region_pairing[0][1]
        last_scaf = -1
        for rp in region_pairing[1:]:
            if scaf_start_reg > last_scaf + 1:
                for skipped_reg in range(last_scaf + 1, scaf_start_reg):
                    ref_coords_to_output.append((regions[skipped_reg][0], regions[skipped_reg][1]))
            last_scaf = scaf_end_reg
            if rp[0] <= scaf_end_reg:  # start of the next scaffold is within the current scaffold
                scaf_end_reg = max(scaf_end_reg, rp[1])
            else:  # dump the previous scaffold and switch to extension of the current one
                ref_coords_to_output.append((regions[scaf_start_reg][0], regions[scaf_end_reg][1]))
                scaf_start_reg = rp[0]
                scaf_end_reg = rp[1]
        ref_coords_to_output.append((regions[scaf_start_reg][0], regions[scaf_end_reg][1]))  # dump the last scaffold
        ref_coords_to_output += regions[scaf_end_reg + 1:]  # output regions after the last scaffold "as is"
    return ref_coords_to_output


def trim_ns(seq):
    seq_start = 0
    seq_end = len(seq)
    while seq_start < seq_end and seq[seq_start] == 'N':
        seq_start += 1
    while seq_end > seq_start and seq[seq_end - 1] == 'N':
        seq_end -= 1
    return seq[seq_start: seq_end]


def get_fasta_entries_from_coords(result_fasta, ref_entry, scaffolds, repeats_regions, uncovered_regions=None):
    '''
    INPUT:
      -- ref_entry from fasta reader; example: ('chr1', 'AACCGTNACGT')
      -- scaffolds: coords of final scaffolds; example: [(100, 500), (1000, 5000)], sorted by start coordinate, w/o overlaps
      -- list of not_covered_regions (for mate-pair scaffolding only); example: [(501, 999)], sorted, w/o overlaps
    OUTPUT:
      -- fasta entries; names are based on ref_entry with suffixes, seqs may include Ns (in case of mate-pair scaffolding)
    '''
    if not scaffolds:
        return [('','')]

    ref_len = len(ref_entry[1])
    assert (scaffolds[-1][1] < ref_len + 1), "InternalError: Last scaffold is behind the reference end!"

    def __get_next_region(regions, idx):
        idx += 1
        new_start = (ref_len + 1) if idx == len(regions) else regions[idx][0]
        return idx, new_start

    suffix = '_scaffold'
    uncovered_idx = 0
    uncovered_start = (ref_len + 1) if not uncovered_regions else uncovered_regions[uncovered_idx][0]
    repeats_idx = 0
    repeats_start = (ref_len + 1) if not repeats_regions else repeats_regions[repeats_idx][0]
    prev_end = 0
    for idx, (scf_start, scf_end) in enumerate(scaffolds):
        name = ref_entry[0] + suffix + str(idx)
        subseqs = []
        # shift current uncovered region if needed
        while scf_start > uncovered_start:  # may happen only uncovered_regions is not None
            uncovered_idx, uncovered_start = __get_next_region(uncovered_regions, uncovered_idx)
        while scf_start > repeats_start:
            if repeats_start >= prev_end:
                repeat_name = ref_entry[0] + '_repeat' + str(repeats_idx)
                repeat_seq = trim_ns(ref_entry[1][repeats_regions[repeats_idx][0]: repeats_regions[repeats_idx][1] + 1])
                if len(repeat_seq) >= MIN_CONTIG_LEN:
                    result_fasta.append((repeat_name, repeat_seq))
            repeats_idx, repeats_start = __get_next_region(repeats_regions, repeats_idx)
        current_start = scf_start
        current_end = min(scf_end, uncovered_start)
        if current_end > current_start:
            subseqs.append(ref_entry[1][current_start: current_end])
        while current_end < scf_end:
            uncovered_end = uncovered_regions[uncovered_idx][1]  # it should be always before scf_end (scaffolds always end with a covered region)
            subseqs.append("N" * (uncovered_end - uncovered_start + 1))  # adding uncovered fragment
            uncovered_idx, uncovered_start = __get_next_region(uncovered_regions, uncovered_idx)
            current_start = uncovered_end + 1
            current_end = min(scf_end, uncovered_start)
            subseqs.append(ref_entry[1][current_start: current_end])
        if subseqs:
            entry_seq = trim_ns("".join(subseqs))
            if len(entry_seq) >= MIN_CONTIG_LEN:
                result_fasta.append((name, entry_seq))
        prev_end = current_end
    for repeat in repeats_regions[repeats_idx:]:
        if repeat[0] >= prev_end:
            repeat_name = ref_entry[0] + '_repeat' + str(repeats_idx)
            repeat_seq = trim_ns(ref_entry[1][repeat[0]: repeat[1] + 1])
            if len(repeat_seq) >= MIN_CONTIG_LEN:
                result_fasta.append((repeat_name, repeat_seq))
            repeats_idx += 1


def check_repeats_instances(coords_fpath, repeats_fpath, use_long_reads=False):
    query_instances = defaultdict(list)
    with open(coords_fpath) as f:
        for line in f:
            fs = line.split('\t')
            contig, align_start, align_end, strand, ref_name, ref_start = \
                fs[0], fs[2], fs[3], fs[4], fs[5], fs[7]
            align_start, align_end, ref_start = map(int, (align_start, align_end, ref_start))
            align_start += 1
            ref_start += 1
            matched_bases, bases_in_mapping = map(int, (fs[9], fs[10]))
            optimal_insert_size = qconfig.optimal_assembly_insert_size
            if optimal_insert_size == 'auto' or not optimal_insert_size:
                optimal_insert_size = qconfig.optimal_assembly_default_IS
            if matched_bases > optimal_insert_size:
                query_instances[contig].append((align_start, align_end))
    repeats_regions = defaultdict(list)
    filtered_repeats_fpath = add_suffix(repeats_fpath, 'filtered')
    with open(filtered_repeats_fpath, 'w') as out_f:
        with open(repeats_fpath) as f:
            for line in f:
                fs = line.split()
                query_id = '%s:%s-%s' % (fs[0], fs[1], fs[2])
                if query_id in query_instances and len(query_instances[query_id]) > 1:
                    mapped_repeats = sorted(list(set(query_instances[query_id][1:])))
                    merged_intervals = []
                    i_start, i_end = mapped_repeats[0]
                    merged_interval = (i_start, i_end)
                    for s, e in mapped_repeats[1:]:
                        if s <= merged_interval[1]:
                            merged_interval = (merged_interval[0], max(merged_interval[1], e))
                        else:
                            merged_intervals.append(merged_interval)
                            merged_interval = (s, e)
                    merged_intervals.append(merged_interval)
                    aligned_bases = sum([end - start + 1 for start, end in merged_intervals])
                    if aligned_bases >= (int(fs[2]) - int(fs[1])) * 0.9:
                        if use_long_reads and len(mapped_repeats) > 1:
                            solid_repeats = []
                            full_repeat_pos = int(fs[1])
                            mapped_repeats.sort(key=lambda x: (x[1], x[1] - x[0]), reverse=True)
                            cur_repeat_start, cur_repeat_end = mapped_repeats[0]
                            for repeat_start, repeat_end in mapped_repeats[1:]:
                                if (cur_repeat_start >= repeat_start - REPEAT_CONF_INTERVAL and cur_repeat_end <= repeat_end + REPEAT_CONF_INTERVAL) or \
                                        (repeat_start >= cur_repeat_start - REPEAT_CONF_INTERVAL and repeat_end <= cur_repeat_end + REPEAT_CONF_INTERVAL):
                                    cur_repeat_start, cur_repeat_end = min(repeat_start, cur_repeat_start), max(repeat_end, cur_repeat_end)
                                else:
                                    solid_repeats.append((cur_repeat_start, cur_repeat_end))
                                    cur_repeat_start, cur_repeat_end = repeat_start, repeat_end
                            solid_repeats.append((cur_repeat_start, cur_repeat_end))
                            for repeat in solid_repeats:
                                out_f.write('\t'.join((fs[0], str(repeat[0] + full_repeat_pos), str(repeat[1] + full_repeat_pos))) + '\n')
                                repeats_regions[fs[0]].append((repeat[0] + full_repeat_pos, repeat[1] + full_repeat_pos))
                        else:
                            out_f.write(line)
                            repeats_regions[fs[0]].append((int(fs[1]), int(fs[2])))
    sorted_repeats_fpath = add_suffix(repeats_fpath, 'sorted')
    qutils.call_subprocess(['sort', '-k1,1', '-k2,2n', filtered_repeats_fpath],
                           stdout=open(sorted_repeats_fpath, 'w'), logger=logger)
    return sorted_repeats_fpath, repeats_regions


def get_unique_covered_regions(ref_fpath, tmp_dir, log_fpath, binary_fpath, insert_size, uncovered_fpath, use_long_reads=False):
    red_genome_dir = os.path.join(tmp_dir, 'tmp_red')
    if isdir(red_genome_dir):
        shutil.rmtree(red_genome_dir)
    os.makedirs(red_genome_dir)

    ref_name = qutils.name_from_fpath(ref_fpath)
    ref_symlink = os.path.join(red_genome_dir, ref_name + '.fa')  ## Red recognizes only *.fa files
    if os.path.islink(ref_symlink):
        os.remove(ref_symlink)
    os.symlink(ref_fpath, ref_symlink)

    logger.info('  ' + 'Running repeat masking tool...')
    repeats_fpath = os.path.join(tmp_dir, ref_name + '.rpt')
    if is_non_empty_file(repeats_fpath):
        return_code = 0
        logger.info('  ' + 'Using existing file ' + repeats_fpath + '...')
    else:
        return_code = qutils.call_subprocess([binary_fpath, '-gnm', red_genome_dir, '-rpt', tmp_dir, '-frm', '2', '-min', '5'],
                                             stdout=open(log_fpath, 'w'), stderr=open(log_fpath, 'w'), indent='    ')
    if return_code == 0 and repeats_fpath and exists(repeats_fpath):
        long_repeats_fpath = os.path.join(tmp_dir, qutils.name_from_fpath(ref_fpath) + '.long.rpt')
        with open(long_repeats_fpath, 'w') as out:
            with open(repeats_fpath) as in_f:
                for line in in_f:
                    l = line.split('\t')
                    repeat_len = int(l[2]) - int(l[1])
                    if repeat_len >= insert_size:
                        out.write(line[1:])

        repeats_fasta_fpath = os.path.join(tmp_dir, qutils.name_from_fpath(ref_fpath) + '.fasta')
        coords_fpath = os.path.join(tmp_dir, qutils.name_from_fpath(ref_fpath) + '.rpt.coords.txt')
        if not is_non_empty_file(coords_fpath):
            fasta_index_fpath = ref_fpath + '.fai'
            if exists(fasta_index_fpath):
                os.remove(fasta_index_fpath)
            qutils.call_subprocess([bedtools_fpath('bedtools'), 'getfasta', '-fi', ref_fpath, '-bed',
                                    long_repeats_fpath, '-fo', repeats_fasta_fpath],
                                    stderr=open(log_fpath, 'w'), indent='    ')
            cmdline = [minimap_fpath(), '-c', '-x', 'asm10', '-N', '50', '--mask-level', '1', '--no-long-join', '-r', '100',
                       '-t', str(qconfig.max_threads), '-z', '200', ref_fpath, repeats_fasta_fpath]
            qutils.call_subprocess(cmdline, stdout=open(coords_fpath, 'w'), stderr=open(log_fpath, 'a'))
        filtered_repeats_fpath, repeats_regions = check_repeats_instances(coords_fpath, long_repeats_fpath, use_long_reads)
        unique_covered_regions = remove_repeat_regions(ref_fpath, filtered_repeats_fpath, uncovered_fpath)
        return unique_covered_regions, repeats_regions
    return None, None


def check_prepared_optimal_assembly(insert_size, result_fpath, ref_prepared_optimal_assembly):
    if os.path.isfile(result_fpath) or os.path.isfile(ref_prepared_optimal_assembly):
        already_done_fpath = result_fpath if os.path.isfile(result_fpath) else ref_prepared_optimal_assembly
        logger.notice('  Will reuse already generated Upper Bound Assembly with insert size %d (%s)' %
                      (insert_size, already_done_fpath))
        return already_done_fpath


def do(ref_fpath, original_ref_fpath, output_dirpath):
    logger.print_timestamp()
    logger.main_info("Generating Upper Bound Assembly...")

    if not reads_analyzer.compile_reads_analyzer_tools(logger):
        logger.warning('  Sorry, can\'t create Upper Bound Assembly '
                       '(failed to compile necessary third-party read processing tools [bwa, bedtools, minimap2]), skipping...')
        return None

    if qconfig.platform_name == 'linux_32':
        logger.warning('  Sorry, can\'t create Upper Bound Assembly on this platform '
                       '(only linux64 and macOS are supported), skipping...')
        return None

    red_dirpath = get_dir_for_download('red', 'Red', ['Red'], logger)
    binary_fpath = download_external_tool('Red', red_dirpath, 'red', platform_specific=True, is_executable=True)
    if not binary_fpath or not os.path.isfile(binary_fpath):
        logger.warning('  Sorry, can\'t create Upper Bound Assembly '
                       '(failed to install/download third-party repeat finding tool [Red]), skipping...')
        return None

    logger.main_info("  Insert size, which will be used for Upper Bound Assembly: " + str(qconfig.optimal_assembly_insert_size))
    insert_size = qconfig.optimal_assembly_insert_size
    if insert_size == 'auto' or not insert_size:
        insert_size = qconfig.optimal_assembly_default_IS

    ref_basename, fasta_ext = splitext_for_fasta_file(os.path.basename(ref_fpath))
    result_basename = '%s.%s.is%d.fasta' % (ref_basename, qconfig.optimal_assembly_basename, insert_size)
    long_reads = qconfig.pacbio_reads or qconfig.nanopore_reads
    if long_reads:
        result_basename = add_suffix(result_basename, long_reads_polished_suffix)
    elif qconfig.mate_pairs:
        result_basename = add_suffix(result_basename, mp_polished_suffix)
    result_fpath = os.path.join(output_dirpath, result_basename)

    original_ref_basename, fasta_ext = splitext_for_fasta_file(os.path.basename(original_ref_fpath))
    prepared_optimal_assembly_basename = '%s.%s.is%d.fasta' % (original_ref_basename, qconfig.optimal_assembly_basename, insert_size)
    if long_reads:
        prepared_optimal_assembly_basename = add_suffix(prepared_optimal_assembly_basename, long_reads_polished_suffix)
    elif qconfig.mate_pairs:
        prepared_optimal_assembly_basename = add_suffix(prepared_optimal_assembly_basename, mp_polished_suffix)
    ref_prepared_optimal_assembly = os.path.join(os.path.dirname(original_ref_fpath), prepared_optimal_assembly_basename)
    already_done_fpath = check_prepared_optimal_assembly(insert_size, result_fpath, ref_prepared_optimal_assembly)
    if already_done_fpath:
        return already_done_fpath

    uncovered_fpath = None
    reads_analyzer_dir = join(dirname(output_dirpath), qconfig.reads_stats_dirname)
    if qconfig.reads_fpaths or qconfig.reference_sam or qconfig.reference_bam:
        sam_fpath, bam_fpath, uncovered_fpath = reads_analyzer.align_reference(ref_fpath, reads_analyzer_dir,
                                                                               using_reads='all',
                                                                               calculate_coverage=True)

    if qconfig.optimal_assembly_insert_size != 'auto' and qconfig.optimal_assembly_insert_size != insert_size:
        calculated_insert_size = qconfig.optimal_assembly_insert_size
        result_fpath = result_fpath.replace('is' + str(insert_size), 'is' + str(calculated_insert_size))
        prepared_optimal_assembly_basename = prepared_optimal_assembly_basename.replace('is' + str(insert_size), 'is' + str(calculated_insert_size))
        insert_size = calculated_insert_size
        ref_prepared_optimal_assembly = os.path.join(os.path.dirname(original_ref_fpath), prepared_optimal_assembly_basename)
        already_done_fpath = check_prepared_optimal_assembly(insert_size, result_fpath, ref_prepared_optimal_assembly)
        if already_done_fpath:
            return already_done_fpath

    log_fpath = os.path.join(output_dirpath, 'upper_bound_assembly.log')
    tmp_dir = os.path.join(output_dirpath, 'tmp')
    if os.path.isdir(tmp_dir):
        shutil.rmtree(tmp_dir)
    os.makedirs(tmp_dir)

    unique_covered_regions, repeats_regions = get_unique_covered_regions(ref_fpath, tmp_dir, log_fpath, binary_fpath, insert_size, uncovered_fpath, use_long_reads=long_reads)
    if unique_covered_regions is None:
        logger.error('  Failed to create Upper Bound Assembly, see log for details: ' + log_fpath)
        return None

    reference = list(fastaparser.read_fasta(ref_fpath))
    result_fasta = []

    if long_reads or qconfig.mate_pairs:
        if long_reads:
            join_reads = 'pacbio' if qconfig.pacbio_reads else 'nanopore'
        else:
            join_reads = 'mp'
        sam_fpath, bam_fpath, _ = reads_analyzer.align_reference(ref_fpath, reads_analyzer_dir, using_reads=join_reads)
        joiners = get_joiners(qutils.name_from_fpath(ref_fpath), sam_fpath, bam_fpath, tmp_dir, log_fpath, join_reads)
        uncovered_regions = parse_bed(uncovered_fpath) if join_reads == 'mp' else defaultdict(list)
        mp_len = calculate_read_len(sam_fpath) if join_reads == 'mp' else None
        for chrom, seq in reference:
            region_pairing = get_regions_pairing(unique_covered_regions[chrom], joiners[chrom], mp_len)
            ref_coords_to_output = scaffolding(unique_covered_regions[chrom], region_pairing)
            get_fasta_entries_from_coords(result_fasta, (chrom, seq), ref_coords_to_output, repeats_regions[chrom], uncovered_regions[chrom])
    else:
        for chrom, seq in reference:
            for idx, region in enumerate(unique_covered_regions[chrom]):
                if region[1] - region[0] >= MIN_CONTIG_LEN:
                    result_fasta.append((chrom + '_' + str(idx), seq[region[0]: region[1]]))

    fastaparser.write_fasta(result_fpath, result_fasta)
    logger.info('  ' + 'Theoretical Upper Bound Assembly is saved to ' + result_fpath)
    logger.notice('(on reusing *this* Upper Bound Assembly in the *future* evaluations on *the same* dataset)\n'
                  '\tThe next time, you can simply provide this file as an additional assembly (you could also rename it to UpperBound.fasta for the clarity). '
                  'In this case, you do not need to specify --upper-bound-assembly and provide files with reads (--pe1/pe2, etc).\n'
                  '\t\tOR\n'
                  '\tYou can copy ' + result_fpath + ' to ' + ref_prepared_optimal_assembly + '. '
                  'The next time you evaluate assemblies with --upper-bound-assembly option and against the same reference (' + original_ref_fpath + ') and '
                  'the same reads (or if you specify the insert size of the paired-end reads explicitly with --est-insert-size ' + str(insert_size) + '), '
                  'QUAST will reuse this Upper Bound Assembly.\n')

    if not qconfig.debug:
        shutil.rmtree(tmp_dir)

    logger.main_info('Done.')
    return result_fpath
