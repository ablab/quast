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
from quast_libs.fastaparser import read_fasta
from quast_libs.qutils import get_total_memory, is_non_empty_file

KMERS_LEN = 101
MIN_MARKERS = 10

kmc_dirpath = abspath(join(qconfig.LIBS_LOCATION, 'KMC', qconfig.platform_name))
kmc_bin_fpath = join(kmc_dirpath, 'kmc')
kmc_tools_fpath = join(kmc_dirpath, 'kmc_tools')


def create_kmc_stats_file(output_dir, contigs_fpath, contigs_fpaths, ref_fpath, completeness,
                         len_map_to_one_chrom, len_map_to_multi_chrom, len_map_to_none_chrom, total_len):
    label = qutils.label_from_fpath_for_fname(contigs_fpath)
    kmc_check_fpath = join(output_dir, label + '.sf')
    kmc_stats_fpath = join(output_dir, label + '.stat')
    with open(kmc_check_fpath, 'w') as check_f:
        check_f.write("Assembly file size in bytes: %d\n" % getsize(contigs_fpath))
        check_f.write("Reference file size in bytes: %d\n" % getsize(ref_fpath))
        check_f.write("Used assemblies: %s\n" % ','.join(contigs_fpaths))
    with open(kmc_stats_fpath, 'w') as stats_f:
        stats_f.write("Completeness: %s\n" % completeness)
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
    if not successful_check_content[0].strip().endswith(str(getsize(contigs_fpath))):
        return False
    if not successful_check_content[1].strip().endswith(str(getsize(ref_fpath))):
        return False
    used_assemblies = successful_check_content[2].strip().split(': ')[-1]
    if used_assemblies and sorted(used_assemblies.split(',')) != sorted(contigs_fpaths):
        return False
    return True


def get_kmers_cnt(tmp_dirpath, kmc_db_fpath, log_fpath, err_fpath):
    histo_fpath = join(tmp_dirpath, basename(kmc_db_fpath) + '.histo.txt')
    run_kmc(kmc_tools_fpath, ['histogram', kmc_db_fpath, histo_fpath], log_fpath, err_fpath)
    kmers_cnt = 0
    if exists(histo_fpath):
        kmers_cnt = float(open(histo_fpath).read().split()[-1])
    return kmers_cnt


def count_kmers(tmp_dirpath, fpath, log_fpath, err_fpath):
    kmc_out_fpath = join(tmp_dirpath, basename(fpath) + '.kmc')
    if not is_non_empty_file(kmc_out_fpath):
        max_mem = max(2, get_total_memory() // 4)
        run_kmc(kmc_bin_fpath, ['-m' + str(max_mem), '-k' + str(KMERS_LEN), '-fm', '-cx1', '-ci1',
                            fpath, kmc_out_fpath, tmp_dirpath], log_fpath, err_fpath)
    return kmc_out_fpath


def get_string_kmers(tmp_dirpath, log_fpath, err_fpath, fasta_fpath=None, seq=None, kmer_fraction=1):
    if not fasta_fpath:
        fasta_fpath = join(tmp_dirpath, 'tmp.fa')
        with open(fasta_fpath, 'w') as out_f:
            out_f.write(seq)
    kmc_out_fpath = count_kmers(tmp_dirpath, fasta_fpath, log_fpath, err_fpath)
    return kmc_to_txt(tmp_dirpath, kmc_out_fpath, log_fpath, err_fpath, kmer_fraction=kmer_fraction)


def kmc_to_txt(tmp_dirpath, kmc_out_fpath, log_fpath, err_fpath, kmer_fraction=1):
    shared_kmers = set()
    shared_kmc_txt = join(tmp_dirpath, 'shared_kmers.txt')
    run_kmc(kmc_tools_fpath, ['transform', kmc_out_fpath, 'dump', shared_kmc_txt], log_fpath, err_fpath)
    with open(shared_kmc_txt) as kmer_in:
        for kmer_i, line in enumerate(kmer_in):
            if kmer_i % kmer_fraction == 0:
                kmer, _ = line.split()
                shared_kmers.add(kmer)
    return shared_kmers


def get_clear_name(fpath):
    return basename(fpath).replace('.kmc', '')


def intersect_kmers(tmp_dirpath, kmc_out_fpaths, log_fpath, err_fpath):
    intersect_out_fpath = join(tmp_dirpath, '_'.join([get_clear_name(kmc_out_fpath)[:30] for kmc_out_fpath in kmc_out_fpaths]) + '.kmc')
    if len(kmc_out_fpaths) == 2:
        run_kmc(kmc_tools_fpath, ['simple'] + kmc_out_fpaths + ['intersect', intersect_out_fpath], log_fpath, err_fpath)
    else:
        prev_kmc_out_fpath = kmc_out_fpaths[0]
        for i in range(1, len(kmc_out_fpaths)):
            tmp_out_fpath = join(tmp_dirpath, get_clear_name(prev_kmc_out_fpath) + get_clear_name(kmc_out_fpaths[i]) + '.kmc')
            run_kmc(kmc_tools_fpath, ['simple', prev_kmc_out_fpath, kmc_out_fpaths[i], 'intersect', tmp_out_fpath], log_fpath, err_fpath)
            prev_kmc_out_fpath = tmp_out_fpath
        intersect_out_fpath = prev_kmc_out_fpath
    return intersect_out_fpath


def run_kmc(kmc_fpath, params, log_fpath, err_fpath):
    qutils.call_subprocess([kmc_fpath, '-t' + str(qconfig.max_threads), '-hp'] + params,
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
            if len(stats_content) < 5:
                continue
            logger.info('  Using existing results for ' + label + '... ')
            report = reporting.get(contigs_fpath)
            report.add_field(reporting.Fields.KMER_COMPLETENESS, '%.2f' % float(stats_content[0].strip().split(': ')[-1]))
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

    if not exists(kmc_bin_fpath) or not exists(kmc_tools_fpath):
        logger.warning('  Sorry, can\'t run KMC on this platform, skipping...')
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

    logger.info('Analyzing completeness and accuracy of assemblies...')
    kmc_out_fpaths = []
    for contigs_fpath in contigs_fpaths:
        report = reporting.get(contigs_fpath)
        kmc_out_fpath = count_kmers(tmp_dirpath, contigs_fpath, log_fpath, err_fpath)
        intersect_out_fpath = intersect_kmers(tmp_dirpath, [ref_kmc_out_fpath, kmc_out_fpath], log_fpath, err_fpath)
        matched_kmers = get_kmers_cnt(tmp_dirpath, intersect_out_fpath, log_fpath, err_fpath)
        completeness = matched_kmers * 100.0 / unique_kmers
        report.add_field(reporting.Fields.KMER_COMPLETENESS, '%.2f' % completeness)
        kmc_out_fpaths.append(kmc_out_fpath)

    if len(kmc_out_fpaths) > 1:
        merged_kmc_out = intersect_kmers(tmp_dirpath, kmc_out_fpaths, log_fpath, err_fpath)
    else:
        merged_kmc_out = kmc_out_fpaths[0]

    kmer_fraction = 100 if getsize(ref_fpath) < 500 * 1024 ** 2 else 1000
    shared_kmers = kmc_to_txt(tmp_dirpath, merged_kmc_out, log_fpath, err_fpath, kmer_fraction=kmer_fraction)

    shared_kmers_by_chrom = dict()
    shared_kmers_txt = join(tmp_dirpath, 'shared.txt')
    ref_contigs = dict((name, seq) for name, seq in read_fasta(ref_fpath))
    with open(shared_kmers_txt, 'w') as out_f:
        for name, seq in ref_contigs.items():
            seq_kmers = get_string_kmers(tmp_dirpath, log_fpath, err_fpath, seq=seq, kmer_fraction=1)
            for kmer in seq_kmers:
                if str(kmer) in shared_kmers:
                    shared_kmers_by_chrom[str(kmer)] = name
                    out_f.write(kmer + '\n')

    shared_kmc_db = count_kmers(tmp_dirpath, shared_kmers_txt, log_fpath, err_fpath)
    for contigs_fpath in contigs_fpaths:
        report = reporting.get(contigs_fpath)
        len_map_to_one_chrom = 0
        len_map_to_multi_chrom = 0
        total_len = 0
        contig_lens = dict()
        contig_markers = defaultdict(list)
        inline_contigs_fpath = join(tmp_dirpath, basename(contigs_fpath) + '.fa')
        filtered_contigs_fpath = join(tmp_dirpath, basename(contigs_fpath) + '.filtered.fa')
        with open(inline_contigs_fpath, 'w') as out_f:
            for name, seq in read_fasta(contigs_fpath):
                out_f.write('>' + name + '\n')
                out_f.write(seq + '\n')
        run_kmc(kmc_tools_fpath, ['filter', shared_kmc_db, inline_contigs_fpath, '-ci10', '-fa', filtered_contigs_fpath], log_fpath, err_fpath)
        for ref_name, ref_seq in ref_contigs.items():
            ref_contig_fpath = join(tmp_dirpath, ref_name + '.fa')
            if not is_non_empty_file(ref_contig_fpath):
                with open(ref_contig_fpath, 'w') as out_f:
                    out_f.write(ref_seq)
            ref_kmc_db = count_kmers(tmp_dirpath, ref_contig_fpath, log_fpath, err_fpath)
            ref_intersect_fpath = join(tmp_dirpath, get_clear_name(contigs_fpath) + '_' + ref_name + '.fa')
            run_kmc(kmc_tools_fpath, ['filter', ref_kmc_db, filtered_contigs_fpath, '-ci1', '-fa', ref_intersect_fpath], log_fpath, err_fpath)
            for name, _ in read_fasta(ref_intersect_fpath):
                contig_markers[name].append(ref_name)

        for name, seq in read_fasta(contigs_fpath):
            total_len += len(seq)
            contig_lens[name] = len(seq)
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

