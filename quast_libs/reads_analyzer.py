############################################################################
# Copyright (c) 2015-2017 Saint Petersburg State University
# Copyright (c) 2011-2015 Saint Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################

from __future__ import with_statement
import os
import shutil
import shlex
from collections import defaultdict
from math import sqrt
from os.path import isfile, join, basename, abspath, isdir, getsize, dirname

from quast_libs import qconfig, qutils
from quast_libs.ca_utils.misc import ref_labels_by_chromosomes
from quast_libs.fastaparser import create_fai_file
from quast_libs.ra_utils.misc import compile_reads_analyzer_tools, sambamba_fpath, bwa_fpath, bedtools_fpath, \
    bwa_dirpath, lap_fpath, download_gridss, get_gridss_fpath, get_gridss_memory, \
    paired_reads_names_are_equal, sort_bam, bwa_index, reformat_bedpe, get_correct_names_for_chroms, \
    all_read_names_correct, clean_read_names, check_cov_file, bam_to_bed, get_safe_fpath
from quast_libs.qutils import is_non_empty_file, add_suffix, get_chr_len_fpath, run_parallel, \
    get_path_to_program, check_java_version

from quast_libs.log import get_logger
from quast_libs.reporting import save_reads

logger = get_logger(qconfig.LOGGER_DEFAULT_NAME)
ref_sam_fpaths = {}


class Mapping(object):
    MIN_MAP_QUALITY = 20  # for distiguishing "good" reads and "bad" ones

    def __init__(self, fields):
        self.ref, self.start, self.mapq, self.ref_next, self.len = \
            fields[2], int(fields[3]), int(fields[4]), fields[6], len(fields[9])
        self.end = self.start + self.len - 1  # actually not always true because of indels

    @staticmethod
    def parse(line):
        if line.startswith('@'):  # comment
            return None
        if len(line.split('\t')) < 11:  # not valid line
            return None
        mapping = Mapping(line.split('\t'))
        return mapping


class QuastDeletion(object):
    ''' describes situtations: GGGGBBBBBNNNNNNNNNNNNBBBBBBGGGGGG, where
    G -- "good" read (high mapping quality)
    B -- "bad" read (low mapping quality)
    N -- no mapped reads
    size of Ns fragment -- "deletion" (not less than MIN_GAP)
    size of Bs fragment -- confidence interval (not more than MAX_CONFIDENCE_INTERVAL,
        fixing last/first G position otherwise)
    '''

    MAX_CONFIDENCE_INTERVAL = 150
    MIN_GAP = qconfig.extensive_misassembly_threshold - 2 * MAX_CONFIDENCE_INTERVAL

    def __init__(self, ref, prev_good=None, prev_bad=None, next_bad=None, next_good=None, next_bad_end=None):
        self.ref, self.prev_good, self.prev_bad, self.next_bad, self.next_good, self.next_bad_end = \
            ref, prev_good, prev_bad, next_bad, next_good, next_bad_end
        self.id = 'QuastDEL'

    def is_valid(self):
        return self.prev_good is not None and self.prev_bad is not None and \
               self.next_bad is not None and self.next_good is not None and \
               (self.next_bad - self.prev_bad > QuastDeletion.MIN_GAP)

    def set_prev_good(self, mapping):
        self.prev_good = mapping.end
        self.prev_bad = self.prev_good  # prev_bad cannot be earlier than prev_good!
        return self  # to use this function like "deletion = QuastDeletion(ref).set_prev_good(coord)"

    def set_prev_bad(self, mapping=None, position=None):
        self.prev_bad = position if position else mapping.end
        if self.prev_good is None or self.prev_good + QuastDeletion.MAX_CONFIDENCE_INTERVAL < self.prev_bad:
            self.prev_good = max(1, self.prev_bad - QuastDeletion.MAX_CONFIDENCE_INTERVAL)
        return self  # to use this function like "deletion = QuastDeletion(ref).set_prev_bad(coord)"

    def set_next_good(self, mapping=None, position=None):
        self.next_good = position if position else mapping.start
        if self.next_bad is None:
            self.next_bad = self.next_good
        elif self.next_good - QuastDeletion.MAX_CONFIDENCE_INTERVAL > self.next_bad:
            self.next_good = self.next_bad + QuastDeletion.MAX_CONFIDENCE_INTERVAL

    def set_next_bad(self, mapping):
        self.next_bad = mapping.start
        self.next_bad_end = mapping.end
        self.next_good = self.next_bad  # next_good is always None at this moment (deletion is complete otherwise)

    def set_next_bad_end(self, mapping):
        if self.next_bad is None:
            self.next_bad = mapping.start
        self.next_bad_end = mapping.end
        self.next_good = min(mapping.start, self.next_bad + QuastDeletion.MAX_CONFIDENCE_INTERVAL)

    def __str__(self):
        return '\t'.join(map(str, [self.ref, self.prev_good, self.prev_bad,
                          self.ref, self.next_bad, self.next_good,
                          self.id]))


def process_one_ref(cur_ref_fpath, output_dirpath, err_path, max_threads, bam_fpath=None, bed_fpath=None):
    ref_name = qutils.name_from_fpath(cur_ref_fpath)
    if not bam_fpath:
        sam_fpath = join(output_dirpath, ref_name + '.sam')
        bam_fpath = join(output_dirpath, ref_name + '.bam')
        bam_sorted_fpath = join(output_dirpath, ref_name + '.sorted.bam')
    else:
        sam_fpath = bam_fpath.replace('.bam', '.sam')
        bam_sorted_fpath = add_suffix(bam_fpath, 'sorted')
    bed_fpath = bed_fpath or join(output_dirpath, ref_name + '.bed')
    if is_non_empty_file(bed_fpath):
        logger.info('  Using existing BED-file: ' + bed_fpath)
        return bed_fpath

    if not isfile(bam_sorted_fpath):
        qutils.call_subprocess([sambamba_fpath('sambamba'), 'view', '-t', str(max_threads), '-h', '-S', '-f', 'bam',
                                '-F', 'not unmapped', sam_fpath], stdout=open(bam_fpath, 'w'), stderr=open(err_path, 'a'), logger=logger)
        sort_bam(bam_fpath, bam_sorted_fpath, err_path, logger, threads=max_threads)
    if not is_non_empty_file(bam_sorted_fpath + '.bai'):
        qutils.call_subprocess([sambamba_fpath('sambamba'), 'index', bam_sorted_fpath],
                               stderr=open(err_path, 'a'), logger=logger)
    create_fai_file(cur_ref_fpath)
    vcf_output_dirpath = join(output_dirpath, ref_name + '_gridss')
    vcf_fpath = join(vcf_output_dirpath, ref_name + '.vcf')
    if not is_non_empty_file(vcf_fpath):
        if isdir(vcf_output_dirpath):
            shutil.rmtree(vcf_output_dirpath, ignore_errors=True)
        os.makedirs(vcf_output_dirpath)
        max_mem = get_gridss_memory()
        env = os.environ.copy()
        env["PATH"] += os.pathsep + bwa_dirpath
        bwa_index(cur_ref_fpath, err_path, logger)
        qutils.call_subprocess(['java', '-ea', '-Xmx' + str(max_mem) + 'g', '-Dsamjdk.create_index=true', '-Dsamjdk.use_async_io_read_samtools=true',
                                '-Dsamjdk.use_async_io_write_samtools=true', '-Dsamjdk.use_async_io_write_tribble=true',
                                '-cp', get_gridss_fpath(), 'gridss.CallVariants', 'I=' + bam_sorted_fpath, 'O=' + vcf_fpath,
                                'ASSEMBLY=' + join(vcf_output_dirpath, ref_name + '.gridss.bam'), 'REFERENCE_SEQUENCE=' + cur_ref_fpath,
                                'WORKER_THREADS=' + str(max_threads), 'WORKING_DIR=' + vcf_output_dirpath],
                                stderr=open(err_path, 'a'), logger=logger, env=env)
    if is_non_empty_file(vcf_fpath):
        raw_bed_fpath = add_suffix(bed_fpath, 'raw')
        filtered_bed_fpath = add_suffix(bed_fpath, 'filtered')
        qutils.call_subprocess(['java', '-cp', get_gridss_fpath(), 'au.edu.wehi.idsv.VcfBreakendToBedpe',
                                'I=' + vcf_fpath, 'O=' + raw_bed_fpath, 'OF=' + filtered_bed_fpath, 'R=' + cur_ref_fpath,
                                'INCLUDE_HEADER=TRUE'], stderr=open(err_path, 'a'), logger=logger)
        reformat_bedpe(raw_bed_fpath, bed_fpath)
    return bed_fpath


def search_sv_with_gridss(main_ref_fpath, bam_fpath, meta_ref_fpaths, output_dirpath, err_path):
    logger.info('  Searching structural variations with GRIDSS...')
    final_bed_fpath = join(output_dirpath, qutils.name_from_fpath(main_ref_fpath) + '_' + qconfig.sv_bed_fname)
    if isfile(final_bed_fpath):
        logger.info('    Using existing file: ' + final_bed_fpath)
        return final_bed_fpath

    if not get_path_to_program('java') or not check_java_version(1.8):
        logger.warning('Java 1.8 or later is required to run GRIDSS. Please install it and rerun QUAST.')
        return None
    if not get_path_to_program('Rscript'):
        logger.warning('R is required to run GRIDSS. Please install it and rerun QUAST.')
        return None

    if meta_ref_fpaths:
        n_jobs = min(len(meta_ref_fpaths), qconfig.max_threads)
        threads_per_job = max(1, qconfig.max_threads // n_jobs)
        parallel_args = [(cur_ref_fpath, output_dirpath, err_path, threads_per_job) for cur_ref_fpath in meta_ref_fpaths]
        bed_fpaths = run_parallel(process_one_ref, parallel_args, n_jobs, filter_results=True)
        if bed_fpaths:
            qutils.cat_files(bed_fpaths, final_bed_fpath)
    else:
        process_one_ref(main_ref_fpath, output_dirpath, err_path, qconfig.max_threads, bam_fpath=bam_fpath, bed_fpath=final_bed_fpath)
    logger.info('    Saving to: ' + final_bed_fpath)
    return final_bed_fpath


def search_trivial_deletions(temp_output_dir, sam_sorted_fpath, ref_files, ref_labels, seq_lengths, need_ref_splitting):
    deletions = []
    trivial_deletions_fpath = join(temp_output_dir, qconfig.trivial_deletions_fname)
    logger.info('  Looking for trivial deletions (long zero-covered fragments)...')
    need_trivial_deletions = True
    if isfile(trivial_deletions_fpath):
        need_trivial_deletions = False
        logger.info('    Using existing file: ' + trivial_deletions_fpath)
    if need_trivial_deletions or need_ref_splitting:
        with open(sam_sorted_fpath) as sam_file:
            cur_deletion = None
            for line in sam_file:
                mapping = Mapping.parse(line)
                if mapping:
                    if mapping.ref == '*':
                        continue
                    # common case: continue current deletion (potential) on the same reference
                    if cur_deletion and cur_deletion.ref == mapping.ref:
                        if cur_deletion.next_bad is None:  # previous mapping was in region BEFORE 0-covered fragment
                            # just passed 0-covered fragment
                            if mapping.start - cur_deletion.prev_bad > QuastDeletion.MIN_GAP:
                                cur_deletion.set_next_bad(mapping)
                                if mapping.mapq >= Mapping.MIN_MAP_QUALITY:
                                    cur_deletion.set_next_good(mapping)
                                    if cur_deletion.is_valid():
                                        deletions.append(cur_deletion)
                                    cur_deletion = QuastDeletion(mapping.ref).set_prev_good(mapping)
                            # continue region BEFORE 0-covered fragment
                            elif mapping.mapq >= Mapping.MIN_MAP_QUALITY:
                                cur_deletion.set_prev_good(mapping)
                            else:
                                cur_deletion.set_prev_bad(mapping)
                        else:  # previous mapping was in region AFTER 0-covered fragment
                            # just passed another 0-cov fragment between end of cur_deletion BAD region and this mapping
                            if mapping.start - cur_deletion.next_bad_end > QuastDeletion.MIN_GAP:
                                if cur_deletion.is_valid():  # add previous fragment's deletion if needed
                                    deletions.append(cur_deletion)
                                cur_deletion = QuastDeletion(mapping.ref).set_prev_bad(position=cur_deletion.next_bad_end)
                            # continue region AFTER 0-covered fragment (old one or new/another one -- see "if" above)
                            elif mapping.mapq >= Mapping.MIN_MAP_QUALITY:
                                cur_deletion.set_next_good(mapping)
                                if cur_deletion.is_valid():
                                    deletions.append(cur_deletion)
                                cur_deletion = QuastDeletion(mapping.ref).set_prev_good(mapping)
                            else:
                                cur_deletion.set_next_bad_end(mapping)
                    # special case: just started or just switched to the next reference
                    else:
                        if cur_deletion and cur_deletion.ref in seq_lengths:  # switched to the next ref
                            cur_deletion.set_next_good(position=seq_lengths[cur_deletion.ref])
                            if cur_deletion.is_valid():
                                deletions.append(cur_deletion)
                        cur_deletion = QuastDeletion(mapping.ref).set_prev_good(mapping)

                    if need_ref_splitting:
                        cur_ref = ref_labels[mapping.ref]
                        if mapping.ref_next.strip() == '=' or cur_ref == ref_labels[mapping.ref_next]:
                            if ref_files[cur_ref] is not None:
                                ref_files[cur_ref].write(line)
            if cur_deletion and cur_deletion.ref in seq_lengths:  # switched to the next ref
                cur_deletion.set_next_good(position=seq_lengths[cur_deletion.ref])
                if cur_deletion.is_valid():
                    deletions.append(cur_deletion)
    if need_ref_splitting:
        for ref_handler in ref_files.values():
            if ref_handler is not None:
                ref_handler.close()
    if need_trivial_deletions:
        logger.info('  Trivial deletions: %d found' % len(deletions))
        logger.info('    Saving to: ' + trivial_deletions_fpath)
        with open(trivial_deletions_fpath, 'w') as f:
            for deletion in deletions:
                f.write(str(deletion) + '\n')
    return trivial_deletions_fpath


def align_reference(ref_fpath, output_dir):
    required_files = []
    ref_name = qutils.name_from_fpath(ref_fpath)
    cov_fpath = qconfig.cov_fpath or join(output_dir, ref_name + '.cov')
    uncovered_fpath = add_suffix(cov_fpath, 'uncovered')
    insert_size_fpath = join(output_dir, ref_name + '.is.txt')
    if not is_non_empty_file(uncovered_fpath):
        required_files.append(uncovered_fpath)
    if not is_non_empty_file(insert_size_fpath):
        required_files.append(insert_size_fpath)

    temp_output_dir = join(output_dir, 'temp_output')
    if not isdir(temp_output_dir):
        os.makedirs(temp_output_dir)

    log_path = join(output_dir, 'reads_stats.log')
    err_path = join(output_dir, 'reads_stats.err')
    correct_chr_names, sam_fpath, bam_fpath = align_single_file(ref_fpath, temp_output_dir, log_path, err_path,
                                                                qconfig.max_threads, sam_fpath=qconfig.reference_sam,
                                                                bam_fpath=qconfig.reference_bam, required_files=required_files,
                                                                is_reference=True, alignment_only=True)
    qconfig.reference_sam = sam_fpath
    qconfig.reference_bam = bam_fpath
    insert_size = calculate_insert_size(sam_fpath, insert_size_fpath)
    if not insert_size:
        logger.info('  Failed calculating insert size.')
    else:
        qconfig.ideal_assembly_insert_size = insert_size

    if not required_files:
        return uncovered_fpath
    if not sam_fpath:
        logger.info('  Failed detecting uncovered regions.')
        return None

    bam_mapped_fpath = get_safe_fpath(temp_output_dir, add_suffix(bam_fpath, 'mapped'))
    bam_sorted_fpath = get_safe_fpath(temp_output_dir, add_suffix(bam_mapped_fpath, 'sorted'))

    if is_non_empty_file(bam_sorted_fpath):
        logger.info('  Using existing sorted BAM-file: ' + bam_sorted_fpath)
    else:
        qutils.call_subprocess([sambamba_fpath('sambamba'), 'view', '-t', str(qconfig.max_threads), '-h', '-f', 'bam',
                                '-F', 'not unmapped', bam_fpath],
                               stdout=open(bam_mapped_fpath, 'w'), stderr=open(err_path, 'a'), logger=logger)
        sort_bam(bam_mapped_fpath, bam_sorted_fpath, err_path, logger)
    if not is_non_empty_file(uncovered_fpath):
        get_coverage(temp_output_dir, ref_fpath, ref_name, bam_fpath, bam_sorted_fpath, log_path, err_path,
                     cov_fpath, None, correct_chr_names, uncovered_fpath=uncovered_fpath, create_cov_files=False)
    return uncovered_fpath


def run_processing_reads(contigs_fpaths, main_ref_fpath, meta_ref_fpaths, ref_labels, temp_output_dir, output_dir,
                         log_path, err_path):
    required_files = []
    bed_fpath, cov_fpath, physical_cov_fpath = None, None, None
    if main_ref_fpath:
        ref_name = qutils.name_from_fpath(main_ref_fpath)

        bed_fpath = qconfig.bed or join(output_dir, ref_name + '.bed')
        cov_fpath = qconfig.cov_fpath or join(output_dir, ref_name + '.cov')
        physical_cov_fpath = qconfig.phys_cov_fpath or join(output_dir, ref_name + '.physical.cov')
        required_files = [bed_fpath, cov_fpath, physical_cov_fpath]

        if qconfig.no_sv:
            logger.info('  Will not search Structural Variations (--fast or --no-sv is specified)')
            bed_fpath = None
        elif is_non_empty_file(bed_fpath):
            logger.info('  Using existing BED-file: ' + bed_fpath)
        elif not qconfig.forward_reads and not qconfig.interlaced_reads:
            if not qconfig.reference_sam and not qconfig.reference_bam:
                logger.info('  Will not search Structural Variations (needs paired-end reads)')
                bed_fpath = None
                qconfig.no_sv = True
        if qconfig.create_icarus_html:
            if is_non_empty_file(cov_fpath):
                is_correct_file = check_cov_file(cov_fpath)
                if is_correct_file:
                    logger.info('  Using existing reads coverage file: ' + cov_fpath)
            if is_non_empty_file(physical_cov_fpath):
                logger.info('  Using existing physical coverage file: ' + physical_cov_fpath)
        else:
            logger.info('  Will not calculate coverage (--fast or --no-html, or --no-icarus, or --space-efficient is specified)')
            cov_fpath = None
            physical_cov_fpath = None
        if (is_non_empty_file(bed_fpath) or qconfig.no_sv) and \
                (not qconfig.create_icarus_html or (is_non_empty_file(cov_fpath) and is_non_empty_file(physical_cov_fpath))):
            required_files = []

    n_jobs = min(qconfig.max_threads, len(contigs_fpaths) + 1)
    max_threads_per_job = max(1, qconfig.max_threads // n_jobs)
    sam_fpaths = qconfig.sam_fpaths or [None] * len(contigs_fpaths)
    bam_fpaths = qconfig.bam_fpaths or [None] * len(contigs_fpaths)
    parallel_align_args = [(contigs_fpath, temp_output_dir, log_path, err_path, max_threads_per_job,
                            sam_fpaths[index], bam_fpaths[index], index)
                           for index, contigs_fpath in enumerate(contigs_fpaths)]
    if main_ref_fpath:
        parallel_align_args.append((main_ref_fpath, temp_output_dir, log_path, err_path,
                                    max_threads_per_job, qconfig.reference_sam, qconfig.reference_bam, None, required_files, True))
    correct_chr_names, sam_fpaths, bam_fpaths = run_parallel(align_single_file, parallel_align_args, n_jobs)
    qconfig.sam_fpaths = sam_fpaths[:len(contigs_fpaths)]
    qconfig.bam_fpaths = bam_fpaths[:len(contigs_fpaths)]
    add_statistics_to_report(output_dir, contigs_fpaths, main_ref_fpath)
    save_reads(output_dir)
    if not main_ref_fpath:
        return None, None, None

    correct_chr_names = correct_chr_names[-1]
    sam_fpath, bam_fpath = sam_fpaths[-1], bam_fpaths[-1]
    qconfig.reference_sam = sam_fpath
    qconfig.reference_bam = bam_fpath
    if not required_files:
        return bed_fpath, cov_fpath, physical_cov_fpath
    if not all([sam_fpath, bam_fpath]):
        logger.info('  Failed searching structural variations.')
        return None, None, None

    sam_sorted_fpath = get_safe_fpath(temp_output_dir, add_suffix(sam_fpath, 'sorted'))
    bam_mapped_fpath = get_safe_fpath(temp_output_dir, add_suffix(bam_fpath, 'mapped'))
    bam_sorted_fpath = get_safe_fpath(temp_output_dir, add_suffix(bam_mapped_fpath, 'sorted'))

    if is_non_empty_file(sam_sorted_fpath):
        logger.info('  Using existing sorted SAM-file: ' + sam_sorted_fpath)
    else:
        if not is_non_empty_file(bam_sorted_fpath):
            qutils.call_subprocess([sambamba_fpath('sambamba'), 'view', '-t', str(qconfig.max_threads), '-h', '-f', 'bam',
                                    '-F', 'not unmapped', bam_fpath],
                                   stdout=open(bam_mapped_fpath, 'w'), stderr=open(err_path, 'a'), logger=logger)
            sort_bam(bam_mapped_fpath, bam_sorted_fpath, err_path, logger)
        qutils.call_subprocess([sambamba_fpath('sambamba'), 'view', '-t', str(qconfig.max_threads), '-h', bam_sorted_fpath],
                               stdout=open(sam_sorted_fpath, 'w'), stderr=open(err_path, 'a'), logger=logger)
    if qconfig.create_icarus_html and (not is_non_empty_file(cov_fpath) or not is_non_empty_file(physical_cov_fpath)):
        cov_fpath, physical_cov_fpath = get_coverage(temp_output_dir, main_ref_fpath, ref_name, bam_fpath, bam_sorted_fpath,
                                                     log_path, err_path, cov_fpath, physical_cov_fpath, correct_chr_names)
    if not is_non_empty_file(bed_fpath) and not qconfig.no_sv:
        if meta_ref_fpaths:
            logger.info('  Splitting SAM-file by references...')
        headers = []
        seq_lengths = {}
        with open(sam_fpath) as sam_file:
            for line in sam_file:
                if not line.startswith('@'):
                    break
                if line.startswith('@SQ') and 'SN:' in line and 'LN:' in line:
                    seq_name = line.split('\tSN:')[1].split('\t')[0]
                    seq_length = int(line.split('\tLN:')[1].split('\t')[0])
                    seq_lengths[seq_name] = seq_length
                headers.append(line.strip())
        need_ref_splitting = False
        ref_files = {}
        if meta_ref_fpaths:
            global ref_sam_fpaths
            for cur_ref_fpath in meta_ref_fpaths:
                cur_ref_name = qutils.name_from_fpath(cur_ref_fpath)
                ref_sam_fpath = join(temp_output_dir, cur_ref_name + '.sam')
                ref_sam_fpaths[cur_ref_fpath] = ref_sam_fpath
                if is_non_empty_file(ref_sam_fpath):
                    logger.info('    Using existing split SAM-file for %s: %s' % (cur_ref_name, ref_sam_fpath))
                    ref_files[cur_ref_name] = None
                else:
                    ref_sam_file = open(ref_sam_fpath, 'w')
                    if not headers[0].startswith('@SQ'):
                        ref_sam_file.write(headers[0] + '\n')
                    for h in (h for h in headers if h.startswith('@SQ') and 'SN:' in h):
                        seq_name = h.split('\tSN:')[1].split('\t')[0]
                        if seq_name in ref_labels and ref_labels[seq_name] == cur_ref_name:
                            ref_sam_file.write(h + '\n')
                    ref_sam_file.write(headers[-1] + '\n')
                    ref_files[cur_ref_name] = ref_sam_file
                    need_ref_splitting = True

        trivial_deletions_fpath = \
            search_trivial_deletions(temp_output_dir, sam_sorted_fpath, ref_files, ref_labels, seq_lengths, need_ref_splitting)
        if get_gridss_fpath() and isfile(get_gridss_fpath()):
            try:
                gridss_sv_fpath = search_sv_with_gridss(main_ref_fpath, bam_mapped_fpath, meta_ref_fpaths, temp_output_dir, err_path)
                qutils.cat_files([gridss_sv_fpath, trivial_deletions_fpath], bed_fpath)
            except:
                pass
        if isfile(trivial_deletions_fpath) and not is_non_empty_file(bed_fpath):
            shutil.copy(trivial_deletions_fpath, bed_fpath)

    if not qconfig.no_sv:
        if is_non_empty_file(bed_fpath):
            logger.main_info('  Structural variations are in ' + bed_fpath)
        else:
            if isfile(bed_fpath):
                logger.main_info('  No structural variations were found.')
            else:
                logger.main_info('  Failed searching structural variations.')
            bed_fpath = None
    if is_non_empty_file(cov_fpath):
        logger.main_info('  Coverage distribution along the reference genome is in ' + cov_fpath)
    else:
        if not qconfig.create_icarus_html:
            logger.main_info('  Failed to calculate coverage distribution')
        cov_fpath = None
    return bed_fpath, cov_fpath, physical_cov_fpath


def align_single_file(fpath, output_dirpath, log_path, err_path, max_threads, sam_fpath=None, bam_fpath=None,
                      index=None, required_files=None, is_reference=False, alignment_only=False):
    filename = qutils.name_from_fpath(fpath)
    if not sam_fpath and bam_fpath:
        sam_fpath = get_safe_fpath(output_dirpath, bam_fpath[:-4] + '.sam')
    else:
        sam_fpath = sam_fpath or join(output_dirpath, filename + '.sam')
    bam_fpath = bam_fpath or get_safe_fpath(output_dirpath, sam_fpath[:-4] + '.bam')
    if alignment_only or (is_reference and required_files and any(f.endswith('bed') for f in required_files)):
        required_files.append(sam_fpath)

    stats_fpath = get_safe_fpath(dirname(output_dirpath), filename + '.stat')
    index_str = qutils.index_to_str(index) if index is not None else ''

    reads_fpaths = qconfig.reads_fpaths
    correct_chr_names = get_correct_names_for_chroms(output_dirpath, fpath, sam_fpath, err_path, reads_fpaths, logger, is_reference)
    can_reuse = correct_chr_names is not None
    if not can_reuse and not reads_fpaths:
        return None, None, None
    if correct_chr_names and (not required_files or all(isfile(fpath) for fpath in required_files)):
        if not alignment_only:
            if isfile(stats_fpath):
                logger.info('  ' + index_str + 'Using existing flag statistics file ' + stats_fpath)
            elif isfile(bam_fpath):
                qutils.call_subprocess([sambamba_fpath('sambamba'), 'flagstat', '-t', str(max_threads), bam_fpath],
                                       stdout=open(stats_fpath, 'w'), stderr=open(err_path, 'a'))
                analyse_coverage(output_dirpath, fpath, correct_chr_names, bam_fpath, stats_fpath, err_path, logger)
            calc_lap_score(reads_fpaths, sam_fpath, index, index_str, output_dirpath, fpath, filename, err_path)
        if isfile(stats_fpath) or alignment_only:
            return correct_chr_names, sam_fpath, bam_fpath

    logger.info('  ' + index_str + 'Pre-processing reads...')
    if is_non_empty_file(sam_fpath) and can_reuse:
        logger.info('  ' + index_str + 'Using existing SAM-file: ' + sam_fpath)
        correct_chr_names = get_correct_names_for_chroms(output_dirpath, fpath, sam_fpath, err_path, reads_fpaths, logger, is_reference)
    elif is_non_empty_file(bam_fpath) and can_reuse:
        logger.info('  ' + index_str + 'Using existing BAM-file: ' + bam_fpath)
        qutils.call_subprocess([sambamba_fpath('sambamba'), 'view', '-t', str(max_threads), '-h', bam_fpath],
                               stdout=open(sam_fpath, 'w'), stderr=open(err_path, 'a'), logger=logger)
        correct_chr_names = get_correct_names_for_chroms(output_dirpath, fpath, sam_fpath, err_path, reads_fpaths, logger, is_reference)
    if (not correct_chr_names or not is_non_empty_file(sam_fpath)) and reads_fpaths:
        if index is not None:
            logger.info('  ' + index_str + 'Running BWA...')
        else:
            logger.info('  Running BWA for reference...')
        # use absolute paths because we will change workdir
        fpath = abspath(fpath)
        sam_fpath = abspath(sam_fpath)

        prev_dir = os.getcwd()
        os.chdir(output_dirpath)
        bwa_index(fpath, err_path, logger)

        bam_fpaths = []
        bwa_cmd = bwa_fpath('bwa') + ' mem -t ' + str(max_threads)
        need_merge = False
        if len(qconfig.reads_fpaths) > 2:
            need_merge = True
        if len(qconfig.reads_fpaths) > 1 and (qconfig.unpaired_reads or qconfig.interlaced_reads):
            need_merge = True
        for i, (read1, read2) in enumerate(zip(qconfig.forward_reads, qconfig.reverse_reads)):
            cmd = bwa_cmd + ' ' + fpath + ' ' + read1 + ' ' + read2
            output_fpath = sam_fpath if not need_merge else add_suffix(sam_fpath, 'paired' + str(i + 1))
            run_bwa(output_fpath, cmd, bam_fpaths, err_path, need_merge=need_merge)
        for i, interlaced_reads in enumerate(qconfig.interlaced_reads):
            cmd = bwa_cmd + ' -p ' + fpath + ' ' + interlaced_reads
            output_fpath = sam_fpath if not need_merge else add_suffix(sam_fpath, 'interlaced' + str(i + 1))
            run_bwa(output_fpath, cmd, bam_fpaths, err_path, need_merge=need_merge)
        for i, single_reads in enumerate(qconfig.unpaired_reads):
            cmd = bwa_cmd + ' ' + fpath + ' ' + single_reads
            output_fpath = sam_fpath if not need_merge else add_suffix(sam_fpath, 'single' + str(i + 1))
            run_bwa(output_fpath, cmd, bam_fpaths, err_path, need_merge=need_merge)

        if len(bam_fpaths) > 1:
            merged_bam_fpath = add_suffix(bam_fpath, 'merged')
            qutils.call_subprocess([sambamba_fpath('sambamba'), 'merge', '-t', str(max_threads), merged_bam_fpath] + bam_fpaths,
                                   stderr=open(err_path, 'a'), logger=logger)
            qutils.call_subprocess([sambamba_fpath('sambamba'), 'markdup', '-r', '-t', str(max_threads), '--tmpdir',
                                    output_dirpath, merged_bam_fpath, bam_fpath],
                                   stderr=open(err_path, 'a'), logger=logger)
            qutils.call_subprocess([sambamba_fpath('sambamba'), 'view', '-t', str(max_threads), '-h', bam_fpath],
                                   stdout=open(sam_fpath, 'w'), stderr=open(err_path, 'a'), logger=logger)
        elif len(bam_fpaths) == 1:
            bam_fpath = bam_fpaths[0]
            sam_fpath = bam_fpath.replace('.bam', '.sam')

        logger.info('  ' + index_str + 'Done.')
        os.chdir(prev_dir)
        if not is_non_empty_file(sam_fpath):
            logger.error('  Failed running BWA for ' + fpath + '. See ' + log_path + ' for information.')
            return None, None, None
        correct_chr_names = get_correct_names_for_chroms(output_dirpath, fpath, sam_fpath, err_path, reads_fpaths, logger, is_reference)
    elif not correct_chr_names or not is_non_empty_file(sam_fpath):
        return None, None, None
    if index is not None:
        logger.info('  ' + index_str + 'Sorting SAM-file...')
    else:
        logger.info('  Sorting SAM-file for reference...')

    if can_reuse and is_non_empty_file(bam_fpath) and all_read_names_correct(sam_fpath):
        logger.info('  ' + index_str + 'Using existing BAM-file: ' + bam_fpath)
    else:
        correct_sam_fpath = join(output_dirpath, filename + '.correct.sam')  # write in output dir
        sam_fpath = clean_read_names(sam_fpath, correct_sam_fpath)
        qutils.call_subprocess([sambamba_fpath('sambamba'), 'view', '-t', str(max_threads), '-h', '-f', 'bam',
                                '-S', correct_sam_fpath],
                                stdout=open(bam_fpath, 'w'), stderr=open(err_path, 'a'), logger=logger)

    qutils.assert_file_exists(bam_fpath, 'bam file')
    if not alignment_only:
        if isfile(stats_fpath):
            logger.info('  ' + index_str + 'Using existing flag statistics file ' + stats_fpath)
        elif isfile(bam_fpath):
            qutils.call_subprocess([sambamba_fpath('sambamba'), 'flagstat', '-t', str(max_threads), bam_fpath],
                                    stdout=open(stats_fpath, 'w'), stderr=open(err_path, 'a'))
            analyse_coverage(output_dirpath, fpath, correct_chr_names, bam_fpath, stats_fpath, err_path, logger)
        calc_lap_score(reads_fpaths, sam_fpath, index, index_str, output_dirpath, fpath, filename, err_path)
        if index is not None:
            logger.info('  ' + index_str + 'Analysis is finished.')
        else:
            logger.info('  Analysis for reference is finished.')
    return correct_chr_names, sam_fpath, bam_fpath


def run_bwa(sam_fpath, cmd, bam_fpaths, err_path, need_merge=False):
    qutils.call_subprocess(shlex.split(cmd), stdout=open(sam_fpath, 'w'), stderr=open(err_path, 'a'), logger=logger)
    if need_merge and is_non_empty_file(sam_fpath):
        tmp_bam_fpath = sam_fpath.replace('.sam', '.bam')
        tmp_bam_sorted_fpath = add_suffix(tmp_bam_fpath, 'sorted')
        qutils.call_subprocess(
            [sambamba_fpath('sambamba'), 'view', '-t', str(qconfig.max_threads), '-h', '-f', 'bam', '-S', sam_fpath],
            stdout=open(tmp_bam_fpath, 'w'), stderr=open(err_path, 'a'), logger=logger)
        sort_bam(tmp_bam_fpath, tmp_bam_sorted_fpath, err_path, logger)
        bam_fpaths.append(tmp_bam_sorted_fpath)


def parse_reads_stats(stats_fpath):
    reads_stats = defaultdict(int)
    reads_stats['coverage_thresholds'] = []
    with open(stats_fpath) as f:
        for line in f:
            value = line.split()[0]
            if 'total' in line:
                reads_stats['total'] = int(value)
            elif 'read1' in line:
                reads_stats['right'] = value
            elif 'read2' in line:
                reads_stats['left'] = value
            elif 'mapped' in line and '%' in line:
                reads_stats['mapped'] = value
                reads_stats['mapped_pcnt'] = get_pcnt_reads(value, reads_stats['total'])
            elif 'properly paired' in line:
                reads_stats['paired'] = value
                reads_stats['paired_pcnt'] = get_pcnt_reads(value, reads_stats['total'])
            elif 'singletons' in line:
                reads_stats['singletons'] = value
                reads_stats['singletons_pcnt'] = get_pcnt_reads(value, reads_stats['total'])
            elif 'different chr' in line and 'mapQ' not in line:
                reads_stats['misjoint'] = value
                reads_stats['misjoint_pcnt'] = get_pcnt_reads(value, reads_stats['total'])
            elif 'depth' in line:
                reads_stats['depth'] = value
            elif 'coverage' in line:
                reads_stats['coverage_thresholds'].append(float(value))
    return reads_stats


def get_pcnt_reads(reads, total_reads):
    return float('%.2f' % (int(reads) * 100.0 / total_reads)) if total_reads != 0 else None


def add_statistics_to_report(output_dir, contigs_fpaths, ref_fpath):
    from quast_libs import reporting

    ref_reads_stats = None
    ref_lap_score = None
    if ref_fpath:
        ref_name = qutils.name_from_fpath(ref_fpath)
        stats_fpath = join(output_dir, ref_name + '.stat')
        if isfile(stats_fpath):
            ref_reads_stats = parse_reads_stats(stats_fpath)
            if int(ref_reads_stats['mapped']) == 0:
                logger.info('  BWA: nothing aligned for reference.')
        lap_out_fpath = get_safe_fpath(output_dir, ref_name + '.lap.out')
        if is_non_empty_file(lap_out_fpath):
            with open(lap_out_fpath) as f:
                l = f.readline()
                ref_lap_score = float(l.split()[0]) if l else None

    # process all contigs files
    for index, contigs_fpath in enumerate(contigs_fpaths):
        report = reporting.get(contigs_fpath)
        assembly_name = qutils.name_from_fpath(contigs_fpath)
        assembly_label = qutils.label_from_fpath(contigs_fpath)
        stats_fpath = join(output_dir, assembly_name + '.stat')
        if ref_reads_stats:
            report.add_field(reporting.Fields.REF_MAPPED_READS, ref_reads_stats['mapped'])
            report.add_field(reporting.Fields.REF_MAPPED_READS_PCNT, ref_reads_stats['mapped_pcnt'])
            report.add_field(reporting.Fields.REF_PROPERLY_PAIRED_READS, ref_reads_stats['paired'])
            report.add_field(reporting.Fields.REF_PROPERLY_PAIRED_READS_PCNT, ref_reads_stats['paired_pcnt'])
            report.add_field(reporting.Fields.REF_SINGLETONS, ref_reads_stats['singletons'])
            report.add_field(reporting.Fields.REF_SINGLETONS_PCNT, ref_reads_stats['singletons_pcnt'])
            report.add_field(reporting.Fields.REF_MISJOINT_READS, ref_reads_stats['misjoint'])
            report.add_field(reporting.Fields.REF_MISJOINT_READS_PCNT, ref_reads_stats['misjoint_pcnt'])
            report.add_field(reporting.Fields.REF_DEPTH, ref_reads_stats['depth'])
            if ref_reads_stats['coverage_thresholds'] and len(ref_reads_stats['coverage_thresholds']) == len(qconfig.coverage_thresholds):
                report.add_field(reporting.Fields.REF_COVERAGE__FOR_THRESHOLDS,
                                [ref_reads_stats['coverage_thresholds'][i] for i, threshold in enumerate(qconfig.coverage_thresholds)])
                report.add_field(reporting.Fields.REF_COVERAGE_1X_THRESHOLD, ref_reads_stats['coverage_thresholds'][0])
        if not isfile(stats_fpath):
            continue
        reads_stats = parse_reads_stats(stats_fpath)
        report.add_field(reporting.Fields.TOTAL_READS, reads_stats['total'])
        report.add_field(reporting.Fields.LEFT_READS, reads_stats['left'])
        report.add_field(reporting.Fields.RIGHT_READS, reads_stats['right'])
        report.add_field(reporting.Fields.MAPPED_READS, reads_stats['mapped'])
        report.add_field(reporting.Fields.MAPPED_READS_PCNT, reads_stats['mapped_pcnt'])
        report.add_field(reporting.Fields.PROPERLY_PAIRED_READS, reads_stats['paired'])
        report.add_field(reporting.Fields.PROPERLY_PAIRED_READS_PCNT, reads_stats['paired_pcnt'])
        if int(reads_stats['mapped']) == 0:
            logger.info('  ' + qutils.index_to_str(index) + 'BWA: nothing aligned for ' + '\'' + assembly_label + '\'.')
        report.add_field(reporting.Fields.SINGLETONS, reads_stats['singletons'])
        report.add_field(reporting.Fields.SINGLETONS_PCNT, reads_stats['singletons_pcnt'])
        report.add_field(reporting.Fields.MISJOINT_READS, reads_stats['misjoint'])
        report.add_field(reporting.Fields.MISJOINT_READS_PCNT, reads_stats['misjoint_pcnt'])
        report.add_field(reporting.Fields.DEPTH, reads_stats['depth'])
        if reads_stats['coverage_thresholds'] and len(reads_stats['coverage_thresholds']) == len(qconfig.coverage_thresholds):
            report.add_field(reporting.Fields.COVERAGE__FOR_THRESHOLDS,
                            [reads_stats['coverage_thresholds'][i] for i, threshold in enumerate(qconfig.coverage_thresholds)])
            report.add_field(reporting.Fields.COVERAGE_1X_THRESHOLD, reads_stats['coverage_thresholds'][0])

        lap_out_fpath = get_safe_fpath(output_dir, assembly_name + '.lap.out')
        if is_non_empty_file(lap_out_fpath):
            with open(lap_out_fpath) as f:
                l = f.readline()
                lap_score = float(l.split()[0]) if l else None
            report.add_field(reporting.Fields.LAP_SCORE, ('%.3f' % lap_score if lap_score is not None else None))
        report.add_field(reporting.Fields.REF_LAP_SCORE, ('%.3f' % ref_lap_score if ref_lap_score is not None else None))


def calc_lap_score(reads_fpaths, sam_fpath, index, index_str, output_dirpath, fpath, filename, err_path):
    if not reads_fpaths or not sam_fpath:
        return

    lap_out_fpath = get_safe_fpath(dirname(output_dirpath), filename + '.lap.out')
    if not is_non_empty_file(lap_out_fpath):
        if index is not None:
            logger.info('  ' + index_str + 'Running LAP...')
        else:
            logger.info('  Running LAP for reference...')
        prob_out_fpath = get_safe_fpath(output_dirpath, filename + '.prob')
        qutils.call_subprocess([lap_fpath('calc_prob.py'), '-a', fpath, '-i', ','.join(reads_fpaths), '-q', '-s', sam_fpath],
                                stdout=open(prob_out_fpath, 'w'), stderr=open(err_path, 'a'))
        qutils.call_subprocess([lap_fpath('sum_prob.py'), '-i', prob_out_fpath],
                                stdout=open(lap_out_fpath, 'w'), stderr=open(err_path, 'a'))
    else:
        if index is not None:
            logger.info('  ' + index_str + 'Using existing file with LAP score...')
        else:
            logger.info('  Using existing file with LAP score for reference...')


def analyse_coverage(output_dirpath, fpath, chr_names, bam_fpath, stats_fpath, err_path, logger):
    filename = qutils.name_from_fpath(fpath)
    bed_fpath = bam_to_bed(output_dirpath, filename, bam_fpath, err_path, logger)
    chr_len_fpath = get_chr_len_fpath(fpath, chr_names)
    cov_fpath = join(output_dirpath, filename + '.genomecov')
    qutils.call_subprocess([bedtools_fpath('bedtools'), 'genomecov', '-i', bed_fpath, '-g', chr_len_fpath],
                           stdout=open(cov_fpath, 'w'), stderr=open(err_path, 'a'), logger=logger)

    avg_depth = 0
    coverage_for_thresholds = [0 for threshold in qconfig.coverage_thresholds]
    with open(cov_fpath) as f:
        for line in f:
            l = line.split()  # genome; depth; number of bases; size of genome; fraction of bases with depth
            depth, genome_fraction = int(l[1]), float(l[4])
            if l[0] == 'genome':
                avg_depth += depth * genome_fraction
                for i, threshold in enumerate(qconfig.coverage_thresholds):
                    if depth >= threshold:
                        coverage_for_thresholds[i] += genome_fraction

    with open(stats_fpath, 'a') as out_f:
        out_f.write('%s depth\n' % int(avg_depth))
        for i, threshold in enumerate(qconfig.coverage_thresholds):
            out_f.write('%.2f coverage >= %sx\n' % (coverage_for_thresholds[i] * 100, threshold))


def get_physical_coverage(output_dirpath, ref_name, bam_fpath, log_path, err_path, cov_fpath, chr_len_fpath):
    if not isfile(bedtools_fpath('bamToBed')):
        logger.info('  Failed calculating physical coverage...')
        return None
    raw_cov_fpath = add_suffix(cov_fpath, 'raw')
    if not is_non_empty_file(raw_cov_fpath):
        logger.info('  Calculating physical coverage...')
        ## keep properly mapped, unique, and non-duplicate read pairs only
        bam_filtered_fpath = join(output_dirpath, ref_name + '.filtered.bam')
        qutils.call_subprocess([sambamba_fpath('sambamba'), 'view', '-t', str(qconfig.max_threads), '-h', '-f', 'bam',
                                '-F', 'proper_pair and not supplementary and not duplicate', bam_fpath],
                                stdout=open(bam_filtered_fpath, 'w'), stderr=open(err_path, 'a'), logger=logger)
        ## sort by read names
        bam_filtered_sorted_fpath = join(output_dirpath, ref_name + '.filtered.sorted.bam')
        sort_bam(bam_filtered_fpath, bam_filtered_sorted_fpath, err_path, logger, sort_rule='-n')
        bed_fpath = bam_to_bed(output_dirpath, ref_name, bam_filtered_sorted_fpath, err_path, logger, bedpe=True)
        qutils.call_subprocess([bedtools_fpath('bedtools'), 'genomecov', '-bga', '-i', bed_fpath, '-g', chr_len_fpath],
                               stdout=open(raw_cov_fpath, 'w'), stderr=open(err_path, 'a'), logger=logger)
    return raw_cov_fpath


def get_coverage(output_dirpath, ref_fpath, ref_name, bam_fpath, bam_sorted_fpath, log_path, err_path, cov_fpath, physical_cov_fpath,
                 correct_chr_names, uncovered_fpath=None, create_cov_files=True):
    raw_cov_fpath = cov_fpath + '_raw'
    chr_len_fpath = get_chr_len_fpath(ref_fpath, correct_chr_names)
    if not is_non_empty_file(cov_fpath):
        logger.info('  Calculating reads coverage...')
        if not is_non_empty_file(raw_cov_fpath):
            if not is_non_empty_file(bam_sorted_fpath):
                sort_bam(bam_fpath, bam_sorted_fpath, log_path, err_path, logger)
            qutils.call_subprocess([bedtools_fpath('bedtools'), 'genomecov', '-bga', '-ibam', bam_sorted_fpath, '-g', chr_len_fpath],
                                   stdout=open(raw_cov_fpath, 'w'), stderr=open(err_path, 'a'), logger=logger)
            qutils.assert_file_exists(raw_cov_fpath, 'coverage file')
        if uncovered_fpath:
            print_uncovered_regions(raw_cov_fpath, uncovered_fpath, correct_chr_names)
        if create_cov_files:
            proceed_cov_file(raw_cov_fpath, cov_fpath, correct_chr_names)
    if not is_non_empty_file(physical_cov_fpath) and create_cov_files:
        raw_cov_fpath = get_physical_coverage(output_dirpath, ref_name, bam_fpath, log_path, err_path,
                                              physical_cov_fpath, chr_len_fpath)
        proceed_cov_file(raw_cov_fpath, physical_cov_fpath, correct_chr_names)
    return cov_fpath, physical_cov_fpath


def proceed_cov_file(raw_cov_fpath, cov_fpath, correct_chr_names):
    chr_depth = defaultdict(list)
    used_chromosomes = dict()
    chr_index = 0
    cov_factor = 9
    with open(raw_cov_fpath, 'r') as in_coverage:
        with open(cov_fpath, 'w') as out_coverage:
            for line in in_coverage:
                fs = list(line.split())
                name = fs[0]
                depth = int(float(fs[-1]))
                if name not in used_chromosomes:
                    chr_index += 1
                    used_chromosomes[name] = str(chr_index)
                    correct_name = correct_chr_names[name] if correct_chr_names else name
                    out_coverage.write('#' + correct_name + ' ' + used_chromosomes[name] + '\n')
                if len(fs) > 3:
                    start, end = int(fs[1]), int(fs[2])
                    chr_depth[name].extend([depth] * (end - start))
                else:
                    chr_depth[name].append(depth)
                if len(chr_depth[name]) >= cov_factor:
                    max_index = len(chr_depth[name]) - (len(chr_depth[name]) % cov_factor)
                    for index in range(0, max_index, cov_factor):
                        cur_depth = sum(chr_depth[name][index: index + cov_factor]) // cov_factor
                        out_coverage.write(' '.join([used_chromosomes[name], str(cur_depth) + '\n']))
                    chr_depth[name] = chr_depth[name][index + cov_factor:]
            os.remove(raw_cov_fpath)


def calculate_insert_size(sam_fpath, insert_size_fpath):
    if is_non_empty_file(insert_size_fpath):
        try:
            insert_size = int(open(insert_size_fpath).read())
            if insert_size:
                return insert_size
        except:
            pass
    insert_sizes = []
    mapped_flags = ['99', '147', '83', '163']  # reads mapped in correct orientation and within insert size
    with open(sam_fpath) as sam_in:
        for i, l in enumerate(sam_in):
            if i > 1000000:
                break
            if l.startswith('@'):
                continue
            fs = l.split('\t')
            flag = fs[1]
            if flag not in mapped_flags:
                continue
            insert_size = abs(int(fs[8]))
            insert_sizes.append(insert_size)

    if insert_sizes:
        mean_is = sum(insert_sizes) * 1.0 / len(insert_sizes)
        if mean_is <= 0:
            return None
        stddev_is = sqrt(sum([(insert_size - mean_is) ** 2 for insert_size in insert_sizes]) / len(insert_sizes))
        insert_size = int(mean_is + stddev_is)
        insert_size = max(qconfig.ideal_assembly_min_IS, insert_size)
        insert_size = min(qconfig.ideal_assembly_max_IS, insert_size)
        with open(insert_size_fpath, 'w') as out_f:
            out_f.write(str(insert_size))
        return insert_size


def print_uncovered_regions(raw_cov_fpath, uncovered_fpath, correct_chr_names):
    uncovered_regions = defaultdict(list)
    with open(raw_cov_fpath) as in_coverage:
        for line in in_coverage:
            fs = list(line.split())
            name = fs[0]
            depth = int(float(fs[-1]))
            correct_name = correct_chr_names[name] if correct_chr_names else name
            if len(fs) > 3 and depth == 0:
                uncovered_regions[correct_name].append((fs[1], fs[2]))
    with open(uncovered_fpath, 'w') as out_f:
        for chrom, regions in uncovered_regions.items():
            for start, end in regions:
                out_f.write('\t'.join([chrom, start, end]) + '\n')


def do(ref_fpath, contigs_fpaths, output_dir, meta_ref_fpaths=None, external_logger=None):
    if external_logger:
        global logger
        logger = external_logger
    logger.print_timestamp()
    logger.main_info('Running Reads analyzer...')

    if not compile_reads_analyzer_tools(logger):
        logger.main_info('Failed reads analysis')
        return None, None, None

    if not isdir(output_dir):
        os.makedirs(output_dir)
    download_gridss(logger, qconfig.bed)
    temp_output_dir = join(output_dir, 'temp_output')
    if not isdir(temp_output_dir):
        os.mkdir(temp_output_dir)
    if not qconfig.no_check:
        if qconfig.forward_reads and not \
                all([paired_reads_names_are_equal([read1, read2], temp_output_dir, logger)
                     for read1, read2 in zip(qconfig.forward_reads, qconfig.reverse_reads)]):
            logger.error('  Read names are discordant, skipping reads analysis!')
            return None, None, None

    log_path = join(output_dir, 'reads_stats.log')
    err_path = join(output_dir, 'reads_stats.err')
    open(log_path, 'w').close()
    open(err_path, 'w').close()
    logger.info('  ' + 'Logging to files %s and %s...' % (log_path, err_path))

    bed_fpath, cov_fpath, physical_cov_fpath = run_processing_reads(contigs_fpaths, ref_fpath, meta_ref_fpaths, ref_labels_by_chromosomes,
                                                                    temp_output_dir, output_dir, log_path, err_path)

    if not qconfig.debug:
        shutil.rmtree(temp_output_dir, ignore_errors=True)

    logger.info('Done.')
    return bed_fpath, cov_fpath, physical_cov_fpath
