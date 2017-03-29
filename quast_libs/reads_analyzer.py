############################################################################
# Copyright (c) 2015-2017 Saint Petersburg State University
# Copyright (c) 2011-2015 Saint Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################

from __future__ import with_statement
import os
import re
import shutil
import shlex
from collections import defaultdict
from os.path import isfile, join, basename, abspath, isdir, getsize, dirname

from quast_libs import qconfig, qutils
from quast_libs.ca_utils.misc import ref_labels_by_chromosomes
from quast_libs.fastaparser import create_fai_file, get_chr_lengths_from_fastafile
from quast_libs.ra_utils import vcfToBedpe
from quast_libs.ra_utils.misc import compile_reads_analyzer_tools, get_manta_fpath, sambamba_fpath, \
    bwa_fpath, bedtools_fpath, paired_reads_names_are_equal, download_manta
from quast_libs.qutils import is_non_empty_file, add_suffix, get_chr_len_fpath, correct_name, run_parallel

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
                          self.id]) + ['-'] * 4)


def process_one_ref(cur_ref_fpath, output_dirpath, err_path, max_threads, bed_fpath=None):
    ref_name = qutils.name_from_fpath(cur_ref_fpath)
    sam_fpath = join(output_dirpath, ref_name + '.sam')
    bam_fpath = join(output_dirpath, ref_name + '.bam')
    bam_sorted_fpath = join(output_dirpath, ref_name + '.sorted.bam')
    bed_fpath = bed_fpath or join(output_dirpath, ref_name + '.bed')
    if os.path.getsize(sam_fpath) < 1024 * 1024:  # TODO: make it better (small files will cause Manta crush -- "not enough reads...")
        logger.info('  SAM file is too small for Manta (%d Kb), skipping..' % (getsize(sam_fpath) // 1024))
        return None
    if is_non_empty_file(bed_fpath):
        logger.info('  Using existing Manta BED-file: ' + bed_fpath)
        return bed_fpath
    if not isfile(bam_sorted_fpath):
        qutils.call_subprocess([sambamba_fpath('sambamba'), 'view', '-t', max_threads, '-h', '-S', '-f', 'bam',
                                sam_fpath], stdout=open(bam_fpath, 'w'), stderr=open(err_path, 'a'), logger=logger)
        qutils.call_subprocess([sambamba_fpath('sambamba'), 'sort', '-t', max_threads, bam_fpath,
                                '-o', bam_sorted_fpath], stderr=open(err_path, 'a'), logger=logger)
    if not is_non_empty_file(bam_sorted_fpath + '.bai'):
        qutils.call_subprocess([sambamba_fpath('sambamba'), 'index', bam_sorted_fpath],
                               stderr=open(err_path, 'a'), logger=logger)
    create_fai_file(cur_ref_fpath)
    vcf_output_dirpath = join(output_dirpath, ref_name + '_manta')
    found_SV_fpath = join(vcf_output_dirpath, 'results/variants/diploidSV.vcf.gz')
    unpacked_SV_fpath = found_SV_fpath + '.unpacked'
    if not is_non_empty_file(found_SV_fpath):
        if isfile(vcf_output_dirpath):
            shutil.rmtree(vcf_output_dirpath, ignore_errors=True)
        os.makedirs(vcf_output_dirpath)
        qutils.call_subprocess([get_manta_fpath(), '--normalBam', bam_sorted_fpath,
                                '--referenceFasta', cur_ref_fpath, '--runDir', vcf_output_dirpath],
                               stdout=open(err_path, 'a'), stderr=open(err_path, 'a'), logger=logger)
        if not isfile(join(vcf_output_dirpath, 'runWorkflow.py')):
            return None
        env = os.environ.copy()
        env['LC_ALL'] = 'C'
        qutils.call_subprocess([join(vcf_output_dirpath, 'runWorkflow.py'), '-m', 'local', '-j', max_threads],
                               stderr=open(err_path, 'a'), logger=logger, env=env)
    if not is_non_empty_file(unpacked_SV_fpath):
        cmd = 'gunzip -c %s' % found_SV_fpath
        qutils.call_subprocess(shlex.split(cmd), stdout=open(unpacked_SV_fpath, 'w'), stderr=open(err_path, 'a'), logger=logger)
    vcfToBedpe.vcfToBedpe(open(unpacked_SV_fpath), open(bed_fpath, 'w'))
    return bed_fpath


def search_sv_with_manta(main_ref_fpath, meta_ref_fpaths, output_dirpath, err_path):
    logger.info('  Searching structural variations with Manta...')
    final_bed_fpath = join(output_dirpath, qconfig.manta_sv_fname)
    if isfile(final_bed_fpath):
        logger.info('    Using existing file: ' + final_bed_fpath)
        return final_bed_fpath

    if meta_ref_fpaths:
        n_jobs = min(len(meta_ref_fpaths), qconfig.max_threads)
        threads_per_job = max(1, qconfig.max_threads // n_jobs)
        parallel_args = [(cur_ref_fpath, output_dirpath, err_path, threads_per_job) for cur_ref_fpath in meta_ref_fpaths]
        bed_fpaths = run_parallel(process_one_ref, parallel_args, n_jobs, filter_results=True)
        if bed_fpaths:
            qutils.cat_files(bed_fpaths, final_bed_fpath)
    else:
        process_one_ref(main_ref_fpath, output_dirpath, err_path, qconfig.max_threads, bed_fpath=final_bed_fpath)
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
                            if mapping.mapq >= Mapping.MIN_MAP_QUALITY:
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


def get_safe_fpath(output_dirpath, fpath):  # reuse file if it exists; else write in output_dir
    if not isfile(fpath):
        return join(output_dirpath, basename(fpath))
    return fpath


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
            logger.info('  Will not search Structural Variations (needs paired-end reads)')
            bed_fpath = None
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
    parallel_align_args = [(index, contigs_fpath, temp_output_dir, log_path, err_path, max_threads_per_job)
                           for index, contigs_fpath in enumerate(contigs_fpaths)]
    if main_ref_fpath:
        parallel_align_args.append((None, main_ref_fpath, temp_output_dir, log_path, err_path,
                                    max_threads_per_job, required_files, qconfig.sam, qconfig.bam))
    correct_chr_names, sam_fpaths, bam_fpaths = run_parallel(align_single_file, parallel_align_args, n_jobs)
    add_statistics_to_report(output_dir, contigs_fpaths, main_ref_fpath)
    save_reads(output_dir)
    if not main_ref_fpath:
        return None, None, None

    correct_chr_names = correct_chr_names[-1]
    sam_fpath, bam_fpath = sam_fpaths[-1], bam_fpaths[-1]
    qconfig.sam = sam_fpath
    if not required_files:
        return bed_fpath, cov_fpath, physical_cov_fpath
    if not all([sam_fpath, bam_fpath]):
        logger.info('  Failed searching structural variations.')
        return None, None, None

    sam_sorted_fpath = get_safe_fpath(output_dir, add_suffix(sam_fpath, 'sorted'))
    bam_mapped_fpath = get_safe_fpath(output_dir, add_suffix(bam_fpath, 'mapped'))
    bam_sorted_fpath = get_safe_fpath(output_dir, add_suffix(bam_fpath, 'sorted'))

    if is_non_empty_file(sam_sorted_fpath):
        logger.info('  Using existing sorted SAM-file: ' + sam_sorted_fpath)
    else:
        qutils.call_subprocess([sambamba_fpath('sambamba'), 'view', '-t', str(qconfig.max_threads), '-h', '-f', 'bam',
                                '-F', 'not unmapped', bam_fpath],
                               stdout=open(bam_mapped_fpath, 'w'), stderr=open(err_path, 'a'), logger=logger)
        qutils.call_subprocess([sambamba_fpath('sambamba'), 'sort', '-t', str(qconfig.max_threads), '-o', bam_sorted_fpath,
                                bam_mapped_fpath], stderr=open(err_path, 'a'), logger=logger)
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
        if get_manta_fpath() and isfile(get_manta_fpath()):
            try:
                manta_sv_fpath = search_sv_with_manta(main_ref_fpath, meta_ref_fpaths, temp_output_dir, err_path)
                qutils.cat_files([manta_sv_fpath, trivial_deletions_fpath], bed_fpath)
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


def align_single_file(index, fpath, output_dirpath, log_path, err_path, max_threads, required_files=None,
                      sam_fpath=None, bam_fpath=None):
    filename = qutils.name_from_fpath(fpath)
    if not sam_fpath and bam_fpath:
        sam_fpath = get_safe_fpath(output_dirpath, bam_fpath[:-4] + '.sam')
    else:
        sam_fpath = sam_fpath or join(output_dirpath, filename + '.sam')
    bam_fpath = bam_fpath or get_safe_fpath(output_dirpath, sam_fpath[:-4] + '.bam')
    stats_fpath = get_safe_fpath(dirname(output_dirpath), filename + '.stat')
    index_str = qutils.index_to_str(index) if index is not None else ''

    reads_fpaths = qconfig.reads_fpaths
    correct_chr_names = get_correct_names_for_chroms(output_dirpath, fpath, sam_fpath, err_path, reads_fpaths)
    can_reuse = correct_chr_names is not None
    if not can_reuse and not reads_fpaths:
        return None, None, None
    if correct_chr_names and (not required_files or all(isfile(fpath) for fpath in required_files)):
        if isfile(stats_fpath):
            logger.info('  ' + index_str + 'Using existing flag statistics file ' + stats_fpath)
        elif isfile(bam_fpath):
            qutils.call_subprocess([sambamba_fpath('sambamba'), 'flagstat', '-t', str(max_threads), bam_fpath],
                                   stdout=open(stats_fpath, 'w'), stderr=open(err_path, 'a'))
            analyse_coverage(output_dirpath, fpath, correct_chr_names, bam_fpath, stats_fpath, err_path, logger)
        if isfile(stats_fpath):
            return correct_chr_names, sam_fpath, bam_fpath

    logger.info('  ' + index_str + 'Pre-processing reads...')
    if is_non_empty_file(sam_fpath) and can_reuse:
        logger.info('  ' + index_str + 'Using existing SAM-file: ' + sam_fpath)
        correct_chr_names = get_correct_names_for_chroms(output_dirpath, fpath, sam_fpath, err_path, reads_fpaths)
    elif is_non_empty_file(bam_fpath) and can_reuse:
        logger.info('  ' + index_str + 'Using existing BAM-file: ' + bam_fpath)
        qutils.call_subprocess([sambamba_fpath('sambamba'), 'view', '-t', str(max_threads), '-h', bam_fpath],
                               stdout=open(sam_fpath, 'w'), stderr=open(err_path, 'a'), logger=logger)
        correct_chr_names = get_correct_names_for_chroms(output_dirpath, fpath, sam_fpath, err_path, reads_fpaths)
    if not correct_chr_names and reads_fpaths:
        if index is not None:
            logger.info('  ' + index_str + 'Running BWA...')
        else:
            logger.info('  Running BWA for reference...')
        # use absolute paths because we will change workdir
        fpath = abspath(fpath)
        sam_fpath = abspath(sam_fpath)

        if not qconfig.no_check:
            if qconfig.forward_reads and not paired_reads_names_are_equal([qconfig.forward_reads, qconfig.reverse_reads], logger):
                logger.error('  Read names are discordant, skipping reads analysis!')
                return None, None, None

        prev_dir = os.getcwd()
        os.chdir(output_dirpath)
        cmd = [bwa_fpath('bwa'), 'index', '-p', filename, fpath]
        if getsize(fpath) > 2 * 1024 ** 3:  # if reference size bigger than 2GB
            cmd += ['-a', 'bwtsw']
        qutils.call_subprocess(cmd, stdout=open(log_path, 'a'), stderr=open(err_path, 'a'), logger=logger)

        bam_fpaths = []
        bwa_cmd = bwa_fpath('bwa') + ' mem -t ' + str(max_threads)
        paired_library = qconfig.forward_reads + ' ' + qconfig.reverse_reads if qconfig.forward_reads else qconfig.interlaced_reads
        need_merge = paired_library and qconfig.unpaired_reads
        if paired_library:
            cmd = bwa_cmd + (' -p ' if qconfig.interlaced_reads else '') + ' ' + filename + ' ' + paired_library
            output_fpath = sam_fpath if not need_merge else add_suffix(sam_fpath, 'paired')
            run_bwa(output_fpath, cmd, bam_fpaths, log_path, err_path, need_merge=need_merge)
        if qconfig.unpaired_reads:
            cmd = bwa_cmd + ' ' + filename + ' ' + qconfig.unpaired_reads
            output_fpath = sam_fpath if not need_merge else add_suffix(sam_fpath, 'single')
            run_bwa(output_fpath, cmd, bam_fpaths, log_path, err_path, need_merge=need_merge)
        if len(bam_fpaths) > 1:
            qutils.call_subprocess([sambamba_fpath('sambamba'), 'merge', '-t', str(max_threads), bam_fpath] + bam_fpaths,
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
        correct_chr_names = get_correct_names_for_chroms(output_dirpath, fpath, sam_fpath, err_path, reads_fpaths)
    elif not correct_chr_names:
        return None, None, None
    if index is not None:
        logger.info('  ' + index_str + 'Sorting SAM-file...')
    else:
        logger.info('  Sorting SAM-file for reference...')

    if can_reuse and is_non_empty_file(bam_fpath) and all_read_names_correct(sam_fpath):
        logger.info('  ' + index_str + 'Using existing BAM-file: ' + bam_fpath)
    else:
        correct_sam_fpath = join(output_dirpath, filename + '.sam.correct')  # write in output dir
        sam_fpath = clean_read_names(sam_fpath, correct_sam_fpath)
        qutils.call_subprocess([sambamba_fpath('sambamba'), 'view', '-t', str(max_threads), '-h', '-f', 'bam',
                                '-S', correct_sam_fpath],
                                stdout=open(bam_fpath, 'w'), stderr=open(err_path, 'a'), logger=logger)

    qutils.assert_file_exists(bam_fpath, 'bam file')
    qutils.call_subprocess([sambamba_fpath('sambamba'), 'flagstat', '-t', str(max_threads), bam_fpath],
                            stdout=open(stats_fpath, 'w'), stderr=open(err_path, 'a'))
    analyse_coverage(output_dirpath, fpath, correct_chr_names, bam_fpath, stats_fpath, err_path, logger)
    if index is not None:
        logger.info('  ' + index_str + 'Analysis is finished.')
    else:
        logger.info('  Analysis for reference is finished.')
    return correct_chr_names, sam_fpath, bam_fpath


def run_bwa(sam_fpath, cmd, bam_fpaths, log_path, err_path, need_merge=False):
    qutils.call_subprocess(shlex.split(cmd), stdout=open(sam_fpath, 'w'), stderr=open(err_path, 'a'), logger=logger)
    if need_merge and is_non_empty_file(sam_fpath):
        tmp_bam_fpath = sam_fpath.replace('.sam', '.bam')
        tmp_bam_sorted_fpath = add_suffix(tmp_bam_fpath, 'sorted')
        qutils.call_subprocess(
            [sambamba_fpath('sambamba'), 'view', '-t', str(qconfig.max_threads), '-h', '-f', 'bam', '-S', sam_fpath],
            stdout=open(tmp_bam_fpath, 'w'), stderr=open(err_path, 'a'), logger=logger)
        qutils.call_subprocess(
            [sambamba_fpath('sambamba'), 'sort', '-t', str(qconfig.max_threads), '-o', tmp_bam_sorted_fpath,
             tmp_bam_fpath], stdout=open(log_path, 'a'), stderr=open(err_path, 'a'), logger=logger)
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
    if ref_fpath:
        ref_name = qutils.name_from_fpath(ref_fpath)
        stats_fpath = join(output_dir, ref_name + '.stat')
        if isfile(stats_fpath):
            ref_reads_stats = parse_reads_stats(stats_fpath)
            if int(ref_reads_stats['mapped']) == 0:
                logger.info('  BWA: nothing aligned for reference.')

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


def bam_to_bed(output_dirpath, name, bam_fpath, err_path, logger, bedpe=False):
    raw_bed_fpath = join(output_dirpath, name + '.bed')
    if bedpe:
        bedpe_fpath = join(output_dirpath, name + '.bedpe')
        qutils.call_subprocess([bedtools_fpath('bamToBed'), '-i', bam_fpath, '-bedpe'],
                               stdout=open(bedpe_fpath, 'w'), stderr=open(err_path, 'a'), logger=logger)
        with open(bedpe_fpath, 'r') as bedpe:
            with open(raw_bed_fpath, 'w') as bed_file:
                for line in bedpe:
                    fs = line.split()
                    bed_file.write('\t'.join([fs[0], fs[1], fs[5] + '\n']))
    else:
        qutils.call_subprocess([bedtools_fpath('bamToBed'), '-i', bam_fpath],
                               stdout=open(raw_bed_fpath, 'w'), stderr=open(err_path, 'a'), logger=logger)

    sorted_bed_fpath = join(output_dirpath, name + '.sorted.bed')
    qutils.call_subprocess([bedtools_fpath('bedtools'), 'sort', '-i', raw_bed_fpath],
                           stdout=open(sorted_bed_fpath, 'w'), stderr=open(err_path, 'a'), logger=logger)
    return sorted_bed_fpath


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
        qutils.call_subprocess([sambamba_fpath('sambamba'), 'sort', '-t', str(qconfig.max_threads), '-o', bam_filtered_sorted_fpath,
                                '-n', bam_filtered_fpath], stdout=open(log_path, 'a'), stderr=open(err_path, 'a'), logger=logger)
        bed_fpath = bam_to_bed(output_dirpath, ref_name, bam_filtered_sorted_fpath, err_path, logger, bedpe=True)
        qutils.call_subprocess([bedtools_fpath('bedtools'), 'genomecov', '-bga', '-i', bed_fpath, '-g', chr_len_fpath],
                               stdout=open(raw_cov_fpath, 'w'), stderr=open(err_path, 'a'), logger=logger)
    return raw_cov_fpath


def get_coverage(output_dirpath, ref_fpath, ref_name, bam_fpath, bam_sorted_fpath, log_path, err_path, cov_fpath, physical_cov_fpath, correct_chr_names):
    raw_cov_fpath = cov_fpath + '_raw'
    chr_len_fpath = get_chr_len_fpath(ref_fpath, correct_chr_names)
    if not is_non_empty_file(cov_fpath):
        logger.info('  Calculating reads coverage...')
        if not is_non_empty_file(raw_cov_fpath):
            if not is_non_empty_file(bam_sorted_fpath):
                qutils.call_subprocess([sambamba_fpath('sambamba'), 'sort', '-t', str(qconfig.max_threads), '-o', bam_sorted_fpath,
                                        bam_fpath], stdout=open(log_path, 'a'), stderr=open(err_path, 'a'), logger=logger)
            qutils.call_subprocess([bedtools_fpath('bedtools'), 'genomecov', '-bga', '-ibam', bam_sorted_fpath, '-g', chr_len_fpath],
                                   stdout=open(raw_cov_fpath, 'w'), stderr=open(err_path, 'a'), logger=logger)
            qutils.assert_file_exists(raw_cov_fpath, 'coverage file')
        proceed_cov_file(raw_cov_fpath, cov_fpath, correct_chr_names)
    if not is_non_empty_file(physical_cov_fpath):
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
                depth = int(fs[-1])
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
                        cur_depth = sum(chr_depth[name][index: index + cov_factor]) / cov_factor
                        out_coverage.write(' '.join([used_chromosomes[name], str(cur_depth) + '\n']))
                    chr_depth[name] = chr_depth[name][index + cov_factor:]
            os.remove(raw_cov_fpath)


def get_correct_names_for_chroms(output_dirpath, fasta_fpath, sam_fpath, err_path, reads_fpaths):
    correct_chr_names = dict()
    fasta_chr_lengths = get_chr_lengths_from_fastafile(fasta_fpath)
    sam_chr_lengths = dict()
    sam_header_fpath = join(dirname(output_dirpath), basename(sam_fpath) + '.header')
    if not isfile(sam_fpath) and not isfile(sam_header_fpath):
        return None
    if isfile(sam_fpath):
        qutils.call_subprocess([sambamba_fpath('sambamba'), 'view', '-H', '-S', sam_fpath],
                           stdout=open(sam_header_fpath, 'w'), stderr=open(err_path, 'w'), logger=logger)
    chr_name_pattern = 'SN:(\S+)'
    chr_len_pattern = 'LN:(\d+)'

    with open(sam_header_fpath) as sam_in:
        for l in sam_in:
            if l.startswith('@SQ'):
                chr_name = re.findall(chr_name_pattern, l)[0]
                chr_len = re.findall(chr_len_pattern, l)[0]
                sam_chr_lengths[chr_name] = int(chr_len)

    inconsistency = ''
    if len(fasta_chr_lengths) != len(sam_chr_lengths):
        inconsistency = 'Number of chromosomes'
    else:
        for fasta_chr, sam_chr in zip(fasta_chr_lengths.keys(), sam_chr_lengths.keys()):
            if correct_name(sam_chr) == fasta_chr[:len(sam_chr)] and sam_chr_lengths[sam_chr] == fasta_chr_lengths[fasta_chr]:
                correct_chr_names[sam_chr] = fasta_chr
            elif sam_chr_lengths[sam_chr] != fasta_chr_lengths[fasta_chr]:
                inconsistency = 'Chromosome lengths'
                break
            else:
                inconsistency = 'Chromosome names'
                break
    if inconsistency:
        if reads_fpaths:
            logger.warning(inconsistency + ' in ' + fasta_fpath + ' and corresponding SAM file ' + sam_fpath + ' do not match. ' +
                           'QUAST will try to realign reads to the reference genome.')
        else:
            logger.error(inconsistency + ' in ' + fasta_fpath + ' and corresponding SAM file ' + sam_fpath + ' do not match. ' +
                         'Use SAM file obtained by aligning reads to the reference genome.')
        return None
    return correct_chr_names


def all_read_names_correct(sam_fpath):
    with open(sam_fpath) as sam_in:
        for i, l in enumerate(sam_in):
            if i > 1000000:
                return True
            if not l:
                continue
            fs = l.split('\t')
            read_name = fs[0]
            if read_name[-2:] == '/1' or read_name[-2:] == '/2':
                return False
    return True


def clean_read_names(sam_fpath, correct_sam_fpath):
    with open(sam_fpath) as sam_in:
        with open(correct_sam_fpath, 'w') as sam_out:
            for l in sam_in:
                if not l:
                    continue
                fs = l.split('\t')
                read_name = fs[0]
                if read_name[-2:] == '/1' or read_name[-2:] == '/2':
                    fs[0] = read_name[:-2]
                    l = '\t'.join(fs)
                sam_out.write(l)
    return correct_sam_fpath


def check_cov_file(cov_fpath):
    raw_cov_fpath = cov_fpath + '_raw'
    with open(cov_fpath, 'r') as coverage:
        for line in coverage:
            if len(line.split()) != 2:
                shutil.copy(cov_fpath, raw_cov_fpath)
                os.remove(cov_fpath)
                return False
            else:
                return True


def do(ref_fpath, contigs_fpaths, output_dir, meta_ref_fpaths=None, external_logger=None):
    if external_logger:
        global logger
        logger = external_logger
    logger.print_timestamp()
    logger.main_info('Running Reads analyzer...')

    if not isdir(output_dir):
        os.makedirs(output_dir)
    if not compile_reads_analyzer_tools(logger):
        logger.main_info('Failed searching structural variations')
        return None, None, None

    download_manta(logger, qconfig.bed)
    temp_output_dir = join(output_dir, 'temp_output')

    if not isdir(temp_output_dir):
        os.mkdir(temp_output_dir)

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
