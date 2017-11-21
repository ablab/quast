############################################################################
# Copyright (c) 2015-2017 Saint Petersburg State University
# Copyright (c) 2011-2015 Saint Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################

from __future__ import with_statement

import os
import shutil
from collections import defaultdict
from os.path import join, abspath, exists, getsize, basename, isdir

from quast_libs import qconfig, reporting, qutils
from quast_libs.fastaparser import read_fasta, get_chr_lengths_from_fastafile
from quast_libs.qutils import get_free_memory, is_non_empty_file, md5, add_suffix, download_external_tool, \
    get_dir_for_download
from quast_libs.ra_utils.misc import minimap_fpath, compile_minimap

KMERS_LEN = 101
MAX_CONTIGS_NUM = 10000
MAX_REF_CONTIGS_NUM = 200
MIN_CONTIGS_LEN = 10000
MIN_MARKERS = 10
MIN_MISJOIN_MARKERS = 1

kmc_dirname = 'kmc'
kmc_dirpath = abspath(join(qconfig.LIBS_LOCATION, kmc_dirname, qconfig.platform_name))
kmc_bin_fpath = join(kmc_dirpath, 'kmc')
kmc_tools_fpath = join(kmc_dirpath, 'kmc_tools')


def create_kmc_stats_file(output_dir, contigs_fpath, contigs_fpaths, ref_fpath, completeness,
                         len_map_to_one_chrom, len_map_to_multi_chrom, len_map_to_none_chrom, total_len):
    label = qutils.label_from_fpath_for_fname(contigs_fpath)
    kmc_check_fpath = join(output_dir, label + '.sf')
    kmc_stats_fpath = join(output_dir, label + '.stat')
    with open(kmc_check_fpath, 'w') as check_f:
        check_f.write("Assembly md5 checksum: %s\n" % md5(contigs_fpath))
        check_f.write("Reference md5 checksum: %s\n" % md5(ref_fpath))
        check_f.write("Used assemblies: %s\n" % ','.join(contigs_fpaths))
    with open(kmc_stats_fpath, 'w') as stats_f:
        stats_f.write("Completeness: %s\n" % completeness)
        if len_map_to_one_chrom or len_map_to_multi_chrom:
            stats_f.write("Length assigned to one chromosome: %d\n" % len_map_to_one_chrom)
            stats_f.write("Length assigned to multi chromosomes: %d\n" % len_map_to_multi_chrom)
            stats_f.write("Length assigned to none chromosome: %d\n" % len_map_to_none_chrom)
            stats_f.write("Total length: %d\n" % total_len)


def check_kmc_successful_check(output_dir, contigs_fpath, contigs_fpaths, ref_fpath):
    label = qutils.label_from_fpath_for_fname(contigs_fpath)
    kmc_check_fpath = join(output_dir, label + '.sf')
    if not exists(kmc_check_fpath):
        return False
    successful_check_content = open(kmc_check_fpath).read().split('\n')
    if len(successful_check_content) < 3:
        return False
    if successful_check_content[0].strip().split()[-1] != str(md5(contigs_fpath)):
        return False
    if successful_check_content[1].strip().split()[-1] != str(md5(ref_fpath)):
        return False
    used_assemblies = successful_check_content[2].strip().split(': ')[-1]
    if used_assemblies and sorted(used_assemblies.split(',')) != sorted(contigs_fpaths):
        return False
    return True


def get_kmers_cnt(tmp_dirpath, kmc_db_fpath, log_fpath, err_fpath):
    histo_fpath = join(tmp_dirpath, basename(kmc_db_fpath) + '.histo.txt')
    run_kmc(['histogram', kmc_db_fpath, histo_fpath], log_fpath, err_fpath)
    kmers_cnt = 0
    if exists(histo_fpath):
        kmers_cnt = int(open(histo_fpath).read().split()[-1])
    return kmers_cnt


def count_kmers(tmp_dirpath, fpath, log_fpath, err_fpath, can_reuse=True):
    kmc_out_fpath = join(tmp_dirpath, basename(fpath) + '.kmc')
    if can_reuse and is_non_empty_file(kmc_out_fpath + '.kmc_pre') and is_non_empty_file(kmc_out_fpath + '.kmc_suf'):
        return kmc_out_fpath
    max_mem = max(2, get_free_memory())
    run_kmc(['-m' + str(max_mem), '-k' + str(KMERS_LEN), '-fm', '-cx1', '-ci1', fpath, kmc_out_fpath, tmp_dirpath],
            log_fpath, err_fpath, use_kmc_tools=False)
    return kmc_out_fpath


def seq_to_kmc_db(tmp_dirpath, log_fpath, err_fpath, fasta_fpath=None, seq=None, name=None, is_ref=False,
                     intersect_with=None, kmer_fraction=1):
    can_reuse = True
    if not fasta_fpath:
        can_reuse = False
        fasta_fpath = join(tmp_dirpath, name + '.fasta')
        if is_ref:
            fasta_fpath = add_suffix(fasta_fpath, 'reference')
        with open(fasta_fpath, 'w') as out_f:
            out_f.write(seq)
    kmc_out_fpath = count_kmers(tmp_dirpath, fasta_fpath, log_fpath, err_fpath, can_reuse=can_reuse)
    if intersect_with:
        kmc_out_fpath = intersect_kmers(tmp_dirpath, [kmc_out_fpath, intersect_with], log_fpath, err_fpath)
    return kmc_out_fpath


def align_kmers(output_dir, ref_fpath, kmers_fpath, log_err_fpath, max_threads):
    out_fpath = join(output_dir, 'kmers.coords')
    cmdline = [minimap_fpath(), '-c', '-r1', '-s101', '-A1', '-B10', '-O39,81', '--secondary=no', '-t', str(max_threads), ref_fpath, kmers_fpath]
    qutils.call_subprocess(cmdline, stdout=open(out_fpath, 'w'), stderr=open(log_err_fpath, 'a'), indent='  ')
    return out_fpath


def parse_kmer_coords(kmers_coords, ref_fpath, kmer_fraction):
    kmers_pos_by_chrom = defaultdict(list)
    kmers_by_chrom = defaultdict(list)
    with open(kmers_coords) as f:
        for line in f:
            fs = line.split('\t')
            if len(fs) < 10:
                continue
            contig, chrom, pos = fs[0], fs[5], fs[7]
            kmers_pos_by_chrom[chrom].append(int(pos))
            kmers_by_chrom[chrom].append(int(contig))
    downsampled_kmers_cnt = sum([len(kmers) for kmers in kmers_by_chrom.values()]) * kmer_fraction
    genome_size = sum(get_chr_lengths_from_fastafile(ref_fpath).values())
    interval = int(genome_size / downsampled_kmers_cnt)
    downsampled_kmers = set()
    for chrom in kmers_by_chrom.keys():
        sorted_kmers = [kmers for kmers_pos, kmers in sorted(zip(kmers_pos_by_chrom[chrom], kmers_by_chrom[chrom]))]
        for kmer_i in sorted_kmers[::interval]:
            downsampled_kmers.add(kmer_i)
    return downsampled_kmers


def downsample_kmers(tmp_dirpath, ref_fpath, kmc_out_fpath, log_fpath, err_fpath, kmer_fraction=1):
    kmc_txt_fpath = join(tmp_dirpath, 'kmc.full.txt')
    downsampled_txt_fpath = join(tmp_dirpath, 'kmc.downsampled.txt')
    run_kmc(['transform', kmc_out_fpath, 'dump', kmc_txt_fpath], log_fpath, err_fpath)
    kmc_fasta_fpath = join(tmp_dirpath, 'kmc.full.fasta')
    with open(kmc_txt_fpath) as in_f:
        with open(kmc_fasta_fpath, 'w') as out_f:
            for kmer_i, line in enumerate(in_f):
                kmer, _ = line.split()
                out_f.write('>' + str(kmer_i) + '\n')
                out_f.write(kmer + '\n')

    kmers_coords = align_kmers(tmp_dirpath, ref_fpath, kmc_fasta_fpath, err_fpath, qconfig.max_threads)
    ordered_kmers = parse_kmer_coords(kmers_coords, ref_fpath, kmer_fraction)
    with open(kmc_txt_fpath) as in_f:
        with open(downsampled_txt_fpath, 'w') as out_f:
            for kmer_i, line in enumerate(in_f):
                if kmer_i in ordered_kmers:
                    kmer, _ = line.split()
                    out_f.write('>' + str(kmer_i) + '\n')
                    out_f.write(kmer + '\n')
    shared_kmc_db = count_kmers(tmp_dirpath, downsampled_txt_fpath, log_fpath, err_fpath)
    return shared_kmc_db


def kmc_to_str(tmp_dirpath, kmc_out_fpath, log_fpath, err_fpath, kmer_fraction=1):
    kmers = set()
    kmc_txt_fpath = join(tmp_dirpath, 'tmp.kmc.txt')
    run_kmc(['transform', kmc_out_fpath, 'dump', kmc_txt_fpath], log_fpath, err_fpath)
    with open(kmc_txt_fpath) as kmer_in:
        for kmer_i, line in enumerate(kmer_in):
            if kmer_i % kmer_fraction == 0:
                kmer, _ = line.split()
                kmers.add(kmer)
    return kmers


def get_clear_name(fpath):
    return basename(fpath).replace('.kmc', '')


def intersect_kmers(tmp_dirpath, kmc_out_fpaths, log_fpath, err_fpath):
    intersect_out_fpath = join(tmp_dirpath, '_'.join([get_clear_name(kmc_out_fpath)[:30] for kmc_out_fpath in kmc_out_fpaths]) + '.kmc')
    if len(kmc_out_fpaths) == 2:
        run_kmc(['simple'] + kmc_out_fpaths + ['intersect', intersect_out_fpath], log_fpath, err_fpath)
    else:
        prev_kmc_out_fpath = kmc_out_fpaths[0]
        for i in range(1, len(kmc_out_fpaths)):
            tmp_out_fpath = join(tmp_dirpath, get_clear_name(prev_kmc_out_fpath) + '_' + str(i) + '.kmc')
            run_kmc(['simple', prev_kmc_out_fpath, kmc_out_fpaths[i], 'intersect', tmp_out_fpath], log_fpath, err_fpath)
            prev_kmc_out_fpath = tmp_out_fpath
        intersect_out_fpath = prev_kmc_out_fpath
    return intersect_out_fpath


def filter_contigs(input_fpath, output_fpath, db_fpath, log_fpath, err_fpath, min_kmers=1):
    if input_fpath.endswith('.txt'):
        input_fpath = '@' + input_fpath
    run_kmc(['filter', db_fpath, input_fpath, '-ci' + str(min_kmers), '-fa', output_fpath], log_fpath, err_fpath)


def run_kmc(params, log_fpath, err_fpath, use_kmc_tools=True):
    tool_fpath = kmc_tools_fpath if use_kmc_tools else kmc_bin_fpath
    qutils.call_subprocess([tool_fpath, '-t' + str(qconfig.max_threads), '-hp'] + params,
                           stdout=open(log_fpath, 'a'), stderr=open(err_fpath, 'a'))


def do(output_dir, ref_fpath, contigs_fpaths, logger):
    logger.print_timestamp()
    logger.main_info('Running analysis based on unique ' + str(KMERS_LEN) + '-mers...')

    checked_assemblies = []
    for contigs_fpath in contigs_fpaths:
        label = qutils.label_from_fpath_for_fname(contigs_fpath)
        if check_kmc_successful_check(output_dir, contigs_fpath, contigs_fpaths, ref_fpath):
            kmc_stats_fpath = join(output_dir, label + '.stat')
            stats_content = open(kmc_stats_fpath).read().split('\n')
            if len(stats_content) < 1:
                continue
            logger.info('  Using existing results for ' + label + '... ')
            report = reporting.get(contigs_fpath)
            report.add_field(reporting.Fields.KMER_COMPLETENESS, '%.2f' % float(stats_content[0].strip().split(': ')[-1]))
            if len(stats_content) >= 5:
                len_map_to_one_chrom = int(stats_content[1].strip().split(': ')[-1])
                len_map_to_multi_chrom = int(stats_content[2].strip().split(': ')[-1])
                len_map_to_none_chrom = int(stats_content[3].strip().split(': ')[-1])
                total_len = int(stats_content[4].strip().split(': ')[-1])
                report.add_field(reporting.Fields.KMER_SCAFFOLDS_ONE_CHROM, '%.2f' % (len_map_to_one_chrom * 100.0 / total_len))
                report.add_field(reporting.Fields.KMER_SCAFFOLDS_MULTI_CHROM, '%.2f' % (len_map_to_multi_chrom * 100.0 / total_len))
                report.add_field(reporting.Fields.KMER_SCAFFOLDS_NONE_CHROM, '%.2f' % (len_map_to_none_chrom * 100.0 / total_len))
            checked_assemblies.append(contigs_fpath)

    contigs_fpaths = [fpath for fpath in contigs_fpaths if fpath not in checked_assemblies]
    if len(contigs_fpaths) == 0:
        logger.info('Done.')
        return

    if qconfig.platform_name == 'linux_32':
        logger.warning('  Sorry, can\'t run KMC on this platform, skipping...')
        return None

    kmc_dirpath = get_dir_for_download(kmc_dirname, 'KMC', ['kmc', 'kmc_tools'], logger)
    global kmc_bin_fpath
    global kmc_tools_fpath
    kmc_bin_fpath = download_external_tool('kmc', kmc_dirpath, 'KMC', platform_specific=True)
    kmc_tools_fpath = download_external_tool('kmc_tools', kmc_dirpath, 'KMC', platform_specific=True)
    if not exists(kmc_bin_fpath) or not exists(kmc_tools_fpath) or not compile_minimap(logger):
        logger.warning('  Sorry, can\'t run KMC, skipping...')
        return None

    logger.info('Running KMC on reference...')
    log_fpath = join(output_dir, 'kmc.log')
    err_fpath = join(output_dir, 'kmc.err')
    open(log_fpath, 'w').close()
    open(err_fpath, 'w').close()

    tmp_dirpath = join(output_dir, 'tmp')
    if not isdir(tmp_dirpath):
        os.makedirs(tmp_dirpath)
    ref_kmc_out_fpath = count_kmers(tmp_dirpath, ref_fpath, log_fpath, err_fpath)
    unique_kmers = get_kmers_cnt(tmp_dirpath, ref_kmc_out_fpath, log_fpath, err_fpath)
    if not unique_kmers:
        return

    logger.info('Analyzing assemblies completeness...')
    kmc_out_fpaths = []
    for contigs_fpath in contigs_fpaths:
        report = reporting.get(contigs_fpath)
        kmc_out_fpath = count_kmers(tmp_dirpath, contigs_fpath, log_fpath, err_fpath)
        intersect_out_fpath = intersect_kmers(tmp_dirpath, [ref_kmc_out_fpath, kmc_out_fpath], log_fpath, err_fpath)
        matched_kmers = get_kmers_cnt(tmp_dirpath, intersect_out_fpath, log_fpath, err_fpath)
        completeness = matched_kmers * 100.0 / unique_kmers
        report.add_field(reporting.Fields.KMER_COMPLETENESS, '%.2f' % completeness)
        kmc_out_fpaths.append(intersect_out_fpath)

    logger.info('Analyzing assemblies accuracy...')
    if len(kmc_out_fpaths) > 1:
        shared_kmc_db = intersect_kmers(tmp_dirpath, kmc_out_fpaths, log_fpath, err_fpath)
    else:
        shared_kmc_db = kmc_out_fpaths[0]

    kmer_fraction = 0.001

    ref_contigs = [name for name, _ in read_fasta(ref_fpath)]
    ref_kmc_dbs = []

    if len(ref_contigs) <= MAX_REF_CONTIGS_NUM:
        shared_downsampled_kmc_db = downsample_kmers(tmp_dirpath, ref_fpath, shared_kmc_db, log_fpath, err_fpath, kmer_fraction=kmer_fraction)
        for name, seq in read_fasta(ref_fpath):
            seq_kmc_db = seq_to_kmc_db(tmp_dirpath, log_fpath, err_fpath, seq=seq, name=name, is_ref=True,
                                                     intersect_with=shared_downsampled_kmc_db)
            ref_kmc_dbs.append((name, seq_kmc_db))

    for contigs_fpath in contigs_fpaths:
        report = reporting.get(contigs_fpath)
        len_map_to_one_chrom = None
        len_map_to_multi_chrom = None
        len_map_to_none_chrom = None
        total_len = 0
        long_contigs = []
        contig_lens = dict()
        contig_markers = defaultdict(list)
        label = qutils.label_from_fpath_for_fname(contigs_fpath)
        list_files_fpath = join(tmp_dirpath, label + '_files.txt')
        with open(list_files_fpath, 'w') as list_files:
            for name, seq in read_fasta(contigs_fpath):
                total_len += len(seq)
                contig_lens[name] = len(seq)
                if len(seq) >= MIN_CONTIGS_LEN:
                    long_contigs.append(len(seq))
                    tmp_contig_fpath = join(tmp_dirpath, name + '.fasta')
                    with open(tmp_contig_fpath, 'w') as out_f:
                        out_f.write('>%s\n' % name)
                        out_f.write('%s\n' % seq)
                    list_files.write(tmp_contig_fpath + '\n')

        if len(long_contigs) > MAX_CONTIGS_NUM or sum(long_contigs) < total_len * 0.5:
            logger.warning('Assembly is too fragmented. Scaffolding accuracy will not be assessed.')
        elif len(ref_contigs) > MAX_REF_CONTIGS_NUM:
            logger.warning('Reference is too fragmented. Scaffolding accuracy will not be assessed.')
        else:
            len_map_to_one_chrom = 0
            len_map_to_multi_chrom = 0
            filtered_fpath = join(tmp_dirpath, label + '.filtered.fasta')
            filter_contigs(list_files_fpath, filtered_fpath, shared_kmc_db, log_fpath, err_fpath, min_kmers=MIN_MARKERS)
            filtered_list_files_fpath = join(tmp_dirpath, label + '_files.filtered.txt')
            with open(filtered_list_files_fpath, 'w') as list_files:
                for name, _ in read_fasta(filtered_fpath):
                    tmp_contig_fpath = join(tmp_dirpath, name + '.fasta')
                    list_files.write(tmp_contig_fpath + '\n')
            for ref_name, ref_kmc_db in ref_kmc_dbs:
                tmp_filtered_fpath = join(tmp_dirpath, ref_name + '.filtered.fasta')
                filter_contigs(filtered_list_files_fpath, tmp_filtered_fpath, ref_kmc_db, log_fpath, err_fpath, min_kmers=MIN_MISJOIN_MARKERS)
                if exists(tmp_filtered_fpath):
                    for name, _ in read_fasta(tmp_filtered_fpath):
                        contig_markers[name].append(ref_name)
            for name, chr_markers in contig_markers.items():
                if len(chr_markers) == 1:
                    len_map_to_one_chrom += contig_lens[name]
                else:
                    len_map_to_multi_chrom += contig_lens[name]
            len_map_to_none_chrom = total_len - len_map_to_one_chrom - len_map_to_multi_chrom
            report.add_field(reporting.Fields.KMER_SCAFFOLDS_ONE_CHROM, '%.2f' % (len_map_to_one_chrom * 100.0 / total_len))
            report.add_field(reporting.Fields.KMER_SCAFFOLDS_MULTI_CHROM, '%.2f' % (len_map_to_multi_chrom * 100.0 / total_len))
            report.add_field(reporting.Fields.KMER_SCAFFOLDS_NONE_CHROM, '%.2f' % (len_map_to_none_chrom * 100.0 / total_len))

        create_kmc_stats_file(output_dir, contigs_fpath, contigs_fpaths, ref_fpath,
                             report.get_field(reporting.Fields.KMER_COMPLETENESS),
                             len_map_to_one_chrom, len_map_to_multi_chrom, len_map_to_none_chrom, total_len)

    if not qconfig.debug:
        shutil.rmtree(tmp_dirpath)
    logger.info('Done.')

