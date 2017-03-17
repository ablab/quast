############################################################################
# Copyright (c) 2015-2017 Saint Petersburg State University
# Copyright (c) 2011-2015 Saint Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################

from __future__ import with_statement
import os
from os.path import isfile
import re
import shutil
import shlex
from collections import defaultdict

from quast_libs import qconfig, qutils
from quast_libs.ca_utils.misc import ref_labels_by_chromosomes
from quast_libs.fastaparser import create_fai_file, get_chr_lengths_from_fastafile
from quast_libs.ra_utils.misc import compile_reads_analyzer_tools, get_manta_fpath, sambamba_fpath, \
    bwa_fpath, bedtools_fpath, paired_reads_names_are_equal, download_manta
from .qutils import is_non_empty_file, add_suffix, get_chr_len_fpath, correct_name, is_python2

from quast_libs.log import get_logger

logger = get_logger(qconfig.LOGGER_DEFAULT_NAME)


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


def process_one_ref(cur_ref_fpath, output_dirpath, err_path, bed_fpath=None):
    ref = qutils.name_from_fpath(cur_ref_fpath)
    ref_sam_fpath = os.path.join(output_dirpath, ref + '.sam')
    ref_bam_fpath = os.path.join(output_dirpath, ref + '.bam')
    ref_bamsorted_fpath = os.path.join(output_dirpath, ref + '.sorted.bam')
    ref_bed_fpath = bed_fpath if bed_fpath else os.path.join(output_dirpath, ref + '.bed')
    if os.path.getsize(ref_sam_fpath) < 1024 * 1024:  # TODO: make it better (small files will cause Manta crush -- "not enough reads...")
        logger.info('  SAM file is too small for Manta (%d Kb), skipping..' % (os.path.getsize(ref_sam_fpath) // 1024))
        return None
    if is_non_empty_file(ref_bed_fpath):
        logger.info('  Using existing Manta BED-file: ' + ref_bed_fpath)
        return ref_bed_fpath
    if not os.path.exists(ref_bamsorted_fpath):
        qutils.call_subprocess([sambamba_fpath('sambamba'), 'view', '-t', str(qconfig.max_threads), '-h', '-S', '-f', 'bam',
                                ref_sam_fpath], stdout=open(ref_bam_fpath, 'w'), stderr=open(err_path, 'a'), logger=logger)
        qutils.call_subprocess([sambamba_fpath('sambamba'), 'sort', '-t', str(qconfig.max_threads), ref_bam_fpath,
                                '-o', ref_bamsorted_fpath], stderr=open(err_path, 'a'), logger=logger)
    if not is_non_empty_file(ref_bamsorted_fpath + '.bai'):
        qutils.call_subprocess([sambamba_fpath('sambamba'), 'index', ref_bamsorted_fpath],
                               stderr=open(err_path, 'a'), logger=logger)
    create_fai_file(cur_ref_fpath)
    vcfoutput_dirpath = os.path.join(output_dirpath, ref + '_manta')
    found_SV_fpath = os.path.join(vcfoutput_dirpath, 'results/variants/diploidSV.vcf.gz')
    unpacked_SV_fpath = found_SV_fpath + '.unpacked'
    if not is_non_empty_file(found_SV_fpath):
        if os.path.exists(vcfoutput_dirpath):
            shutil.rmtree(vcfoutput_dirpath, ignore_errors=True)
        os.makedirs(vcfoutput_dirpath)
        qutils.call_subprocess([get_manta_fpath(), '--normalBam', ref_bamsorted_fpath,
                                '--referenceFasta', cur_ref_fpath, '--runDir', vcfoutput_dirpath],
                               stdout=open(err_path, 'a'), stderr=open(err_path, 'a'), logger=logger)
        if not os.path.exists(os.path.join(vcfoutput_dirpath, 'runWorkflow.py')):
            return None
        env = os.environ.copy()
        env['LC_ALL'] = 'C'
        qutils.call_subprocess([os.path.join(vcfoutput_dirpath, 'runWorkflow.py'), '-m', 'local', '-j', str(qconfig.max_threads)],
                               stderr=open(err_path, 'a'), logger=logger, env=env)
    if not is_non_empty_file(unpacked_SV_fpath):
        cmd = 'gunzip -c %s' % found_SV_fpath
        qutils.call_subprocess(shlex.split(cmd), stdout=open(unpacked_SV_fpath, 'w'),
                               stderr=open(err_path, 'a'), logger=logger)
    from quast_libs.ra_utils import vcfToBedpe
    vcfToBedpe.vcfToBedpe(open(unpacked_SV_fpath), open(ref_bed_fpath, 'w'))
    return ref_bed_fpath


def search_sv_with_manta(main_ref_fpath, meta_ref_fpaths, output_dirpath, err_path):
    logger.info('  Searching structural variations with Manta...')
    final_bed_fpath = os.path.join(output_dirpath, qconfig.manta_sv_fname)
    if os.path.exists(final_bed_fpath):
        logger.info('    Using existing file: ' + final_bed_fpath)
        return final_bed_fpath

    if meta_ref_fpaths:
        if is_python2():
            from joblib import Parallel, delayed
        else:
            from joblib3 import Parallel, delayed
        n_jobs = min(len(meta_ref_fpaths), qconfig.max_threads)
        if not qconfig.memory_efficient:
            bed_fpaths = Parallel(n_jobs=n_jobs)(delayed(process_one_ref)(cur_ref_fpath, output_dirpath, err_path) for cur_ref_fpath in meta_ref_fpaths)
        else:
            bed_fpaths = [process_one_ref(cur_ref_fpath, output_dirpath, err_path) for cur_ref_fpath in meta_ref_fpaths]
        bed_fpaths = [f for f in bed_fpaths if f is not None]
        if bed_fpaths:
            qutils.cat_files(bed_fpaths, final_bed_fpath)
    else:
        process_one_ref(main_ref_fpath, output_dirpath, err_path, bed_fpath=final_bed_fpath)
    logger.info('    Saving to: ' + final_bed_fpath)
    return final_bed_fpath


def get_safe_fpath(output_dirpath, fpath):  # reuse file if it exists; else write in output_dir
    if not os.path.exists(fpath):
        return os.path.join(output_dirpath, os.path.basename(fpath))
    return fpath


def run_processing_reads(main_ref_fpath, meta_ref_fpaths, ref_labels, reads_fpaths, output_dirpath, res_path, log_path,
                         err_path, sam_fpath=None, bam_fpath=None, bed_fpath=None):
    ref_name = qutils.name_from_fpath(main_ref_fpath)

    if not sam_fpath and bam_fpath:
        sam_fpath = get_safe_fpath(output_dirpath, bam_fpath[:-4] + '.sam')
    else:
        sam_fpath = sam_fpath or os.path.join(output_dirpath, ref_name + '.sam')
    bam_fpath = bam_fpath or get_safe_fpath(output_dirpath, sam_fpath[:-4] + '.bam')
    sam_sorted_fpath = get_safe_fpath(output_dirpath, add_suffix(sam_fpath, 'sorted'))
    bam_sorted_fpath = get_safe_fpath(output_dirpath, add_suffix(bam_fpath, 'sorted'))

    bed_fpath = bed_fpath or os.path.join(res_path, ref_name + '.bed')
    cov_fpath = os.path.join(res_path, ref_name + '.cov')
    physical_cov_fpath = os.path.join(res_path, ref_name + '.physical.cov')

    if qconfig.no_sv:
        logger.info('  Will not search Structural Variations (--fast or --no-sv is specified)')
        bed_fpath = None
    elif is_non_empty_file(bed_fpath):
        logger.info('  Using existing BED-file: ' + bed_fpath)
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
        return bed_fpath, cov_fpath, physical_cov_fpath

    logger.info('  ' + 'Pre-processing reads...')
    correct_chr_names = None
    if is_non_empty_file(sam_fpath):
        logger.info('  Using existing SAM-file: ' + sam_fpath)
        correct_chr_names = get_correct_names_for_chroms(output_dirpath, main_ref_fpath, sam_fpath, err_path, reads_fpaths)
    elif is_non_empty_file(bam_fpath):
        logger.info('  Using existing BAM-file: ' + bam_fpath)
        qutils.call_subprocess([sambamba_fpath('sambamba'), 'view', '-t', str(qconfig.max_threads), '-h', bam_fpath],
                               stdout=open(sam_fpath, 'w'), stderr=open(err_path, 'a'), logger=logger)
        correct_chr_names = get_correct_names_for_chroms(output_dirpath, main_ref_fpath, sam_fpath, err_path, reads_fpaths)
    if not correct_chr_names and reads_fpaths:
        logger.info('  Running BWA...')
        # use absolute paths because we will change workdir
        sam_fpath = os.path.abspath(sam_fpath)
        abs_reads_fpaths = []
        for reads_fpath in reads_fpaths:
            abs_reads_fpaths.append(os.path.abspath(reads_fpath))

        if len(abs_reads_fpaths) != 2:
            logger.error('  You should specify files with forward and reverse reads.')
            logger.info('  Failed searching structural variations.')
            return None, None, None

        if not qconfig.no_check:
            if not paired_reads_names_are_equal(reads_fpaths, logger):
                logger.error('  Read names are discordant, skipping reads analysis!')
                logger.info('  Failed searching structural variations.')
                return None, None, None

        prev_dir = os.getcwd()
        os.chdir(output_dirpath)
        cmd = [bwa_fpath('bwa'), 'index', '-p', ref_name, main_ref_fpath]
        if os.path.getsize(main_ref_fpath) > 2 * 1024 ** 3:  # if reference size bigger than 2GB
            cmd += ['-a', 'bwtsw']
        qutils.call_subprocess(cmd, stdout=open(log_path, 'a'), stderr=open(err_path, 'a'), logger=logger)

        cmd = bwa_fpath('bwa') + ' mem -t ' + str(qconfig.max_threads) + ' ' + ref_name + ' ' + abs_reads_fpaths[0] + ' ' + abs_reads_fpaths[1]

        qutils.call_subprocess(shlex.split(cmd), stdout=open(sam_fpath, 'w'), stderr=open(err_path, 'a'), logger=logger)
        logger.info('  Done.')
        os.chdir(prev_dir)
        if not os.path.exists(sam_fpath) or os.path.getsize(sam_fpath) == 0:
            logger.error('  Failed running BWA for the reference. See ' + log_path + ' for information.')
            logger.info('  Failed searching structural variations.')
            return None, None, None
    elif not correct_chr_names:
        logger.info('  Failed searching structural variations.')
        return None, None, None
    logger.info('  Sorting SAM-file...')
    if (is_non_empty_file(sam_sorted_fpath) and all_read_names_correct(sam_sorted_fpath)) and is_non_empty_file(bam_fpath):
        logger.info('  Using existing sorted SAM-file: ' + sam_sorted_fpath)
    else:
        correct_sam_fpath = os.path.join(output_dirpath, ref_name + '.sam.correct')  # write in output dir
        clean_read_names(sam_fpath, correct_sam_fpath)
        bam_fpath = os.path.join(output_dirpath, ref_name + '.bam')
        bam_sorted_fpath = add_suffix(bam_fpath, 'sorted')
        qutils.call_subprocess([sambamba_fpath('sambamba'), 'view', '-t', str(qconfig.max_threads), '-h', '-f', 'bam',
                                '-F', 'not unmapped',  '-S', correct_sam_fpath],
                                stdout=open(bam_fpath, 'w'), stderr=open(err_path, 'a'), logger=logger)
        qutils.call_subprocess([sambamba_fpath('sambamba'), 'sort', '-t', str(qconfig.max_threads), '-o', bam_sorted_fpath,
                                bam_fpath], stderr=open(err_path, 'a'), logger=logger)
        qutils.call_subprocess([sambamba_fpath('sambamba'), 'view', '-t', str(qconfig.max_threads), '-h', bam_sorted_fpath],
                                stdout=open(sam_sorted_fpath, 'w'), stderr=open(err_path, 'a'), logger=logger)

    if qconfig.create_icarus_html and (not is_non_empty_file(cov_fpath) or not is_non_empty_file(physical_cov_fpath)):
        cov_fpath, physical_cov_fpath = get_coverage(output_dirpath, main_ref_fpath, ref_name, bam_fpath, bam_sorted_fpath,
                                                     log_path, err_path, cov_fpath, physical_cov_fpath, correct_chr_names)
    if not is_non_empty_file(bed_fpath) and not qconfig.no_sv:
        if meta_ref_fpaths:
            logger.info('  Splitting SAM-file by references...')
        headers = []
        seq_name_length = {}
        with open(sam_fpath) as sam_file:
            for line in sam_file:
                if not line.startswith('@'):
                    break
                if line.startswith('@SQ') and 'SN:' in line and 'LN:' in line:
                    seq_name = line.split('\tSN:')[1].split('\t')[0]
                    seq_length = int(line.split('\tLN:')[1].split('\t')[0])
                    seq_name_length[seq_name] = seq_length
                headers.append(line.strip())
        need_ref_splitting = False
        if meta_ref_fpaths:
            ref_files = {}
            for cur_ref_fpath in meta_ref_fpaths:
                ref = qutils.name_from_fpath(cur_ref_fpath)
                new_ref_sam_fpath = os.path.join(output_dirpath, ref + '.sam')
                if is_non_empty_file(new_ref_sam_fpath):
                    logger.info('    Using existing split SAM-file for %s: %s' % (ref, new_ref_sam_fpath))
                    ref_files[ref] = None
                else:
                    new_ref_sam_file = open(new_ref_sam_fpath, 'w')
                    if not headers[0].startswith('@SQ'):
                        new_ref_sam_file.write(headers[0] + '\n')
                    chrs = []
                    for h in (h for h in headers if h.startswith('@SQ') and 'SN:' in h):
                        seq_name = h.split('\tSN:')[1].split('\t')[0]
                        if seq_name in ref_labels and ref_labels[seq_name] == ref:
                            new_ref_sam_file.write(h + '\n')
                            chrs.append(seq_name)
                    new_ref_sam_file.write(headers[-1] + '\n')
                    ref_files[ref] = new_ref_sam_file
                    need_ref_splitting = True
        deletions = []
        trivial_deletions_fpath = os.path.join(output_dirpath, qconfig.trivial_deletions_fname)
        logger.info('  Looking for trivial deletions (long zero-covered fragments)...')
        need_trivial_deletions = True
        if os.path.exists(trivial_deletions_fpath):
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
                                    if cur_deletion.is_valid():   # add previous fragment's deletion if needed
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
                            if cur_deletion and cur_deletion.ref in seq_name_length:  # switched to the next ref
                                cur_deletion.set_next_good(position=seq_name_length[cur_deletion.ref])
                                if cur_deletion.is_valid():
                                    deletions.append(cur_deletion)
                            cur_deletion = QuastDeletion(mapping.ref).set_prev_good(mapping)

                        if need_ref_splitting:
                            cur_ref = ref_labels[mapping.ref]
                            if mapping.ref_next.strip() == '=' or cur_ref == ref_labels[mapping.ref_next]:
                                if ref_files[cur_ref] is not None:
                                    ref_files[cur_ref].write(line)
                if cur_deletion and cur_deletion.ref in seq_name_length:  # switched to the next ref
                    cur_deletion.set_next_good(position=seq_name_length[cur_deletion.ref])
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

        if get_manta_fpath() and isfile(get_manta_fpath()):
            try:
                manta_sv_fpath = search_sv_with_manta(main_ref_fpath, meta_ref_fpaths, output_dirpath, err_path)
                qutils.cat_files([manta_sv_fpath, trivial_deletions_fpath], bed_fpath)
            except:
                pass
        if os.path.exists(trivial_deletions_fpath) and not is_non_empty_file(bed_fpath):
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


def get_physical_coverage(output_dirpath, ref_fpath, ref_name, bam_fpath, log_path, err_path, cov_fpath, chr_len_fpath):
    if not os.path.exists(bedtools_fpath('bamToBed')):
        logger.info('  Failed calculating physical coverage...')
        return None
    raw_cov_fpath = cov_fpath + '_raw'
    if not is_non_empty_file(raw_cov_fpath):
        logger.info('  Calculating physical coverage...')
        ## keep properly mapped, unique, and non-duplicate read pairs only
        bam_filtered_fpath = os.path.join(output_dirpath, ref_name + '.filtered.bam')
        qutils.call_subprocess([sambamba_fpath('sambamba'), 'view', '-t', str(qconfig.max_threads), '-h', '-f', 'bam',
                                '-F', 'proper_pair and not supplementary and not duplicate', bam_fpath],
                                stdout=open(bam_filtered_fpath, 'w'), stderr=open(err_path, 'a'), logger=logger)
        ## sort by read names
        bam_filtered_sorted_fpath = os.path.join(output_dirpath, ref_name + '.filtered.sorted.bam')
        qutils.call_subprocess([sambamba_fpath('sambamba'), 'sort', '-t', str(qconfig.max_threads), '-o', bam_filtered_sorted_fpath,
                                '-n', bam_filtered_fpath], stdout=open(log_path, 'a'), stderr=open(err_path, 'a'), logger=logger)
        bedpe_fpath = os.path.join(output_dirpath, ref_name + '.bedpe')
        qutils.call_subprocess([bedtools_fpath('bamToBed'), '-i', bam_filtered_sorted_fpath, '-bedpe'], stdout=open(bedpe_fpath, 'w'),
                               stderr=open(err_path, 'a'), logger=logger)
        raw_bed_fpath = os.path.join(output_dirpath, ref_name + '.bed')
        with open(bedpe_fpath, 'r') as bedpe:
            with open(raw_bed_fpath, 'w') as bed_file:
                for line in bedpe:
                    fs = line.split()
                    bed_file.write('\t'.join([fs[0], fs[1], fs[5] + '\n']))
        sorted_bed_fpath = os.path.join(output_dirpath, ref_name + '.sorted.bed')
        qutils.call_subprocess([bedtools_fpath('bedtools'), 'sort', '-i', raw_bed_fpath],
                               stdout=open(sorted_bed_fpath, 'w'), stderr=open(err_path, 'a'), logger=logger)
        qutils.call_subprocess([bedtools_fpath('bedtools'), 'genomecov', '-bga', '-i', sorted_bed_fpath, '-g', chr_len_fpath],
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
        raw_cov_fpath = get_physical_coverage(output_dirpath, ref_fpath, ref_name, bam_fpath, log_path, err_path,
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


def get_correct_names_for_chroms(output_dirpath, ref_fpath, sam_fpath, err_path, reads_fpaths):
    correct_chr_names = dict()
    ref_chr_lengths = get_chr_lengths_from_fastafile(ref_fpath)
    sam_chr_lengths = dict()
    sam_header_fpath = os.path.join(output_dirpath, os.path.basename(sam_fpath) + '.header')
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
    if len(ref_chr_lengths) != len(sam_chr_lengths):
        inconsistency = 'Number of chromosomes'
    else:
        for ref_chr, sam_chr in zip(ref_chr_lengths.keys(), sam_chr_lengths.keys()):
            if correct_name(sam_chr) == ref_chr[:len(sam_chr)] and sam_chr_lengths[sam_chr] == ref_chr_lengths[ref_chr]:
                correct_chr_names[sam_chr] = ref_chr
            elif sam_chr_lengths[sam_chr] != ref_chr_lengths[ref_chr]:
                inconsistency = 'Chromosome lengths'
                break
            else:
                inconsistency = 'Chromosome names'
                break
    if inconsistency:
        if reads_fpaths:
            logger.warning(inconsistency + ' in reference and SAM file do not match. ' +
                           'QUAST will try to realign reads to the reference genome.')
        else:
            logger.error(inconsistency + ' in reference and SAM file do not match. ' +
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


def do(ref_fpath, contigs_fpaths, reads_fpaths, meta_ref_fpaths, output_dir, external_logger=None, sam_fpath=None, bam_fpath=None, bed_fpath=None):
    if external_logger:
        global logger
        logger = external_logger
    logger.print_timestamp()
    logger.main_info('Running Reads analyzer...')

    if not os.path.isdir(output_dir):
        os.makedirs(output_dir)
    if not compile_reads_analyzer_tools(logger):
        logger.main_info('Failed searching structural variations')
        return None, None, None

    download_manta(logger, bed_fpath)
    temp_output_dir = os.path.join(output_dir, 'temp_output')

    if not os.path.isdir(temp_output_dir):
        os.mkdir(temp_output_dir)

    log_path = os.path.join(output_dir, 'sv_calling.log')
    err_path = os.path.join(output_dir, 'sv_calling.err')
    open(log_path, 'w').close()
    open(err_path, 'w').close()
    logger.info('  ' + 'Logging to files %s and %s...' % (log_path, err_path))
    try:
        bed_fpath, cov_fpath, physical_cov_fpath = run_processing_reads(ref_fpath, meta_ref_fpaths, ref_labels_by_chromosomes,
                                                                        reads_fpaths, temp_output_dir, output_dir, log_path, err_path,
                                                                        bed_fpath=bed_fpath, sam_fpath=sam_fpath, bam_fpath=bam_fpath)
    except:
        bed_fpath, cov_fpath, physical_cov_fpath = None, None, None
        logger.error('Failed searching structural variations! This function is experimental and may work improperly. Sorry for the inconvenience.')

    if not qconfig.debug:
        shutil.rmtree(temp_output_dir, ignore_errors=True)

    logger.info('Done.')
    return bed_fpath, cov_fpath, physical_cov_fpath
