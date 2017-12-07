############################################################################
# Copyright (c) 2015-2017 Saint Petersburg State University
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
from quast_libs.log import get_logger
from quast_libs.qutils import splitext_for_fasta_file, is_non_empty_file, download_external_tool, \
    add_suffix, get_dir_for_download, get_blast_fpath
from quast_libs.ra_utils.misc import sort_bam, bam_to_bed, bedtools_fpath, sambamba_view, calculate_read_len, \
    minimap_fpath
from quast_libs.reads_analyzer import calculate_insert_size

logger = get_logger(qconfig.LOGGER_DEFAULT_NAME)
mp_polished_suffix = 'mp_polished'
long_reads_polished_suffix = 'long_reads_polished'
MIN_OVERLAP = 10

def parse_uncovered_fpath(uncovered_fpath, fasta_fpath, return_covered_regions=True):
    regions = defaultdict(list)
    prev_start = defaultdict(int)
    if uncovered_fpath and exists(uncovered_fpath):
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


def merge_bed(repeats_fpath, uncovered_fpath, insert_size, output_dirpath, err_path):
    combined_bed_fpath = join(output_dirpath, 'skipped_regions.bed')
    with open(combined_bed_fpath, 'w') as out:
        if exists(repeats_fpath):
            with open(repeats_fpath) as in_f:
                for line in in_f:
                    l = line.split('\t')
                    repeat_len = int(l[2]) - int(l[1])
                    if repeat_len >= insert_size:
                        out.write(line)
        if exists(uncovered_fpath):
            with open(uncovered_fpath) as in_f:
                for line in in_f:
                    out.write(line)

    sorted_bed_fpath = add_suffix(combined_bed_fpath, 'sorted')
    qutils.call_subprocess(['sort', '-k1,1', '-k2,2n', combined_bed_fpath],
                           stdout=open(sorted_bed_fpath, 'w'), stderr=open(err_path, 'a'), logger=logger)
    merged_bed_fpath = add_suffix(combined_bed_fpath, 'merged')
    qutils.call_subprocess([bedtools_fpath('bedtools'), 'merge', '-i', sorted_bed_fpath],
                           stdout=open(merged_bed_fpath, 'w'), stderr=open(err_path, 'a'), logger=logger)
    return merged_bed_fpath


def remove_repeat_regions(ref_fpath, repeats_fpath, insert_size, tmp_dir, uncovered_fpath, err_fpath):
    merged_fpath = merge_bed(repeats_fpath, uncovered_fpath, insert_size, tmp_dir, err_fpath)
    regions_to_remove = parse_uncovered_fpath(merged_fpath, ref_fpath, return_covered_regions=False)
    unique_regions = defaultdict(list)
    for name, seq in fastaparser.read_fasta(ref_fpath):
        if name in regions_to_remove:
            cur_contig_start = 0
            for start, end in regions_to_remove[name]:
                if start > cur_contig_start:
                    unique_regions[name].append([cur_contig_start, start])
                cur_contig_start = end + 1
            if cur_contig_start < len(seq):
                unique_regions[name].append([cur_contig_start, len(seq)])
        else:
            unique_regions[name].append([0, len(seq)])
    return unique_regions


def get_joiners(ref_name, sam_fpath, bam_fpath, output_dirpath, err_fpath, using_reads):
    bam_filtered_fpath = add_suffix(bam_fpath, 'filtered')
    if not is_non_empty_file(bam_filtered_fpath):
        filter_rule = 'not unmapped and not supplementary and not secondary_alignment'
        sambamba_view(bam_fpath, bam_filtered_fpath, qconfig.max_threads, err_fpath, logger, filter_rule=filter_rule)
    bam_sorted_fpath = add_suffix(bam_fpath, 'sorted')
    if not is_non_empty_file(bam_sorted_fpath):
        sort_bam(bam_filtered_fpath, bam_sorted_fpath, err_fpath, logger, sort_rule='-n')
    bed_fpath = bam_to_bed(output_dirpath, using_reads, bam_sorted_fpath, err_fpath, logger, bedpe=using_reads == 'mp')
    intervals = defaultdict(list)
    if using_reads == 'mp':
        insert_size, std_dev = calculate_insert_size(sam_fpath, output_dirpath, ref_name, reads_suffix='mp')
        min_is = insert_size - std_dev
        max_is = insert_size + std_dev
    with open(bed_fpath) as bed:
        for l in bed:
            fs = l.split()
            if using_reads == 'mp' and insert_size:
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
    return sorted(set(tuple(pair) for pair in joiner_to_region_pair.values()))


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
        for rp in region_pairing[1:]:
            if rp[0] <= scaf_end_reg:  # start of the next scaffold is within the current scaffold
                scaf_end_reg = max(scaf_end_reg, rp[1])
            else:  # dump the previous scaffold and switch to extension of the current one
                ref_coords_to_output.append((regions[scaf_start_reg][0], regions[scaf_end_reg][1]))
                scaf_start_reg = rp[0]
                scaf_end_reg = rp[1]
        ref_coords_to_output.append((regions[scaf_start_reg][0], regions[scaf_end_reg][1]))  # dump the last scaffold
        ref_coords_to_output += regions[scaf_end_reg + 1:]  # output regions after the last scaffold "as is"
    return ref_coords_to_output


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
                result_fasta.append((repeat_name, ref_entry[1][repeats_regions[repeats_idx][0]: repeats_regions[repeats_idx][1] + 1]))
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
            result_fasta.append((name, "".join(subseqs)))
        prev_end = current_end
    for repeat in repeats_regions[repeats_idx:]:
        if repeat[0] >= prev_end:
            repeat_name = ref_entry[0] + '_repeat' + str(repeats_idx)
            result_fasta.append((repeat_name, ref_entry[1][repeat[0]: repeat[1] + 1]))
            repeats_idx += 1


def check_repeats_instances(coords_fpath, repeats_fpath):
    query_instances = dict()
    with open(coords_fpath) as f:
        for line in f:
            fs = line.split('\t')
            contig, align_start, align_end, strand, ref_name, ref_start = \
                fs[0], fs[2], fs[3], fs[4], fs[5], fs[7]
            align_start, align_end, ref_start = map(int, (align_start, align_end, ref_start))
            align_start += 1
            ref_start += 1
            matched_bases, bases_in_mapping = map(int, (fs[9], fs[10]))
            score = matched_bases
            if contig in query_instances:
                if score >= max(query_instances[contig]) * 0.8:
                    query_instances[contig].append(score)
            else:
                query_instances[contig] = [score]
    repeats_regions = defaultdict(list)
    filtered_repeats_fpath = add_suffix(repeats_fpath, 'filtered')
    with open(filtered_repeats_fpath, 'w') as out_f:
        with open(repeats_fpath) as f:
            for line in f:
                fs = line.split()
                query_id = '%s:%s-%s' % (fs[0], fs[1], fs[2])
                if query_id in query_instances and len(query_instances[query_id]) > 1:
                    out_f.write(line)
                    repeats_regions[fs[0]].append((int(fs[1]), int(fs[2])))
    return filtered_repeats_fpath, repeats_regions


def get_unique_covered_regions(ref_fpath, tmp_dir, log_fpath, binary_fpath, insert_size, uncovered_fpath):
    red_genome_dir = os.path.join(tmp_dir, 'tmp_red')
    if isdir(red_genome_dir):
        shutil.rmtree(red_genome_dir)
    os.makedirs(red_genome_dir)
    ref_symlink = os.path.join(red_genome_dir, basename(ref_fpath))
    if os.path.islink(ref_symlink):
        os.remove(ref_symlink)
    os.symlink(ref_fpath, ref_symlink)

    logger.info('  ' + 'Running repeat masking tool...')
    repeats_fpath = os.path.join(tmp_dir, qutils.name_from_fpath(ref_fpath) + '.rpt')
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
            cmdline = [minimap_fpath(), '-c', '-B4', '-O4,24', '-N', '50', '--mask-level', '0.9',
                       '-t', str(qconfig.max_threads), '-z', '200', ref_fpath, repeats_fasta_fpath]
            return_code = qutils.call_subprocess(cmdline, stdout=open(coords_fpath, 'w'),
                                                 stderr=open(log_fpath, 'a'))
        filtered_repeats_fpath, repeats_regions = check_repeats_instances(coords_fpath, long_repeats_fpath)
        unique_covered_regions = remove_repeat_regions(ref_fpath, filtered_repeats_fpath, insert_size, tmp_dir, uncovered_fpath, log_fpath)
        return unique_covered_regions, repeats_regions
    return None, None


def do(ref_fpath, original_ref_fpath, output_dirpath):
    logger.print_timestamp()
    logger.main_info("Simulating Optimal Assembly...")

    uncovered_fpath = None
    reads_analyzer_dir = join(dirname(output_dirpath), qconfig.reads_stats_dirname)
    if qconfig.reads_fpaths or qconfig.reference_sam or qconfig.reference_bam:
        sam_fpath, bam_fpath, uncovered_fpath = reads_analyzer.align_reference(ref_fpath, reads_analyzer_dir, using_reads='all', calculate_coverage=True)
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
    ref_prepared_optimal_assembly = os.path.join(os.path.dirname(original_ref_fpath), prepared_optimal_assembly_basename)

    if os.path.isfile(result_fpath) or os.path.isfile(ref_prepared_optimal_assembly):
        already_done_fpath = result_fpath if os.path.isfile(result_fpath) else ref_prepared_optimal_assembly
        logger.notice('  Will reuse already generated Optimal Assembly with insert size %d (%s)' %
                      (insert_size, already_done_fpath))
        return already_done_fpath

    if qconfig.platform_name == 'linux_32':
        logger.warning('  Sorry, can\'t create Optimal Assembly on this platform, skipping...')
        return None

    red_dirpath = get_dir_for_download('red', 'Red', ['Red'], logger)
    binary_fpath = download_external_tool('Red', red_dirpath, 'red', platform_specific=True, is_executable=True)
    if not binary_fpath or not os.path.isfile(binary_fpath):
        logger.warning('  Sorry, can\'t create Optimal Assembly, skipping...')
        return None

    log_fpath = os.path.join(output_dirpath, 'optimal_assembly.log')
    tmp_dir = os.path.join(output_dirpath, 'tmp')
    if os.path.isdir(tmp_dir):
        shutil.rmtree(tmp_dir)
    os.makedirs(tmp_dir)

    unique_covered_regions, repeats_regions = get_unique_covered_regions(ref_fpath, tmp_dir, log_fpath, binary_fpath, insert_size, uncovered_fpath)
    if unique_covered_regions is None:
        logger.error('  Failed to create Optimal Assembly, see log for details: ' + log_fpath)
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
        uncovered_regions = parse_uncovered_fpath(uncovered_fpath, ref_fpath, return_covered_regions=False) if join_reads == 'mp' else defaultdict(list)
        mp_len = calculate_read_len(sam_fpath) if join_reads == 'mp' else None
        for chrom, seq in reference:
            region_pairing = get_regions_pairing(unique_covered_regions[chrom], joiners[chrom], mp_len)
            ref_coords_to_output = scaffolding(unique_covered_regions[chrom], region_pairing)
            get_fasta_entries_from_coords(result_fasta, (chrom, seq), ref_coords_to_output, repeats_regions[chrom], uncovered_regions[chrom])
    else:
        for chrom, seq in reference:
            for idx, region in enumerate(unique_covered_regions[chrom]):
                result_fasta.append((chrom + '_' + str(idx), seq[region[0]: region[1]]))

    fastaparser.write_fasta(result_fpath, result_fasta)
    logger.info('  ' + 'Theoretically optimal Assembly saved to ' + result_fpath)
    logger.notice('You can copy it to ' + ref_prepared_optimal_assembly +
                  ' and QUAST will reuse it in further runs against the same reference (' + original_ref_fpath + ')')

    if not qconfig.debug:
        shutil.rmtree(tmp_dir)

    logger.main_info('Done.')
    return result_fpath
