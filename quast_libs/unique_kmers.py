import os
import sys
from collections import defaultdict
from os.path import join, abspath, exists, getsize, basename
from site import addsitedir

from quast_libs import qconfig, reporting, qutils
from quast_libs.fastaparser import read_fasta
from quast_libs.qutils import safe_rm, check_prev_compilation_failed, \
    call_subprocess, write_failed_compilation_flag


KMERS_LEN = 101
MIN_MARKERS = 10

jellyfish_dirpath = abspath(join(qconfig.LIBS_LOCATION, 'jellyfish'))
jellyfish_bin_fpath = join(jellyfish_dirpath, 'bin', 'jellyfish')
jellyfish_src_dirpath = join(jellyfish_dirpath, 'src')
jellyfish_python_dirpath = join(jellyfish_dirpath, 'lib')


def compile_jellyfish(logger, only_clean=False):
    make_logs_basepath = join(jellyfish_src_dirpath, 'make')
    failed_compilation_flag = make_logs_basepath + '.failed'

    if only_clean:
        safe_rm(jellyfish_bin_fpath)
        safe_rm(failed_compilation_flag)
        return True

    if not exists(jellyfish_bin_fpath):
        if check_prev_compilation_failed('Jellyfish', failed_compilation_flag, logger=logger):
            return False

        # making
        logger.main_info('Compiling Jellyfish (details are in ' + make_logs_basepath +
                         '.log and make.err)')
        os.utime(join(jellyfish_src_dirpath, 'aclocal.m4'), None)
        os.utime(join(jellyfish_src_dirpath, 'Makefile.in'), None)
        os.utime(join(jellyfish_src_dirpath, 'config.h.in'), None)
        os.utime(join(jellyfish_src_dirpath, 'configure'), None)
        prev_dir = os.getcwd()
        os.chdir(jellyfish_src_dirpath)
        call_subprocess(['./configure', '--prefix=' + jellyfish_dirpath,
                         '--enable-python-binding=' + jellyfish_python_dirpath,
                         'PYTHON_VERSION=' + str(sys.version_info[0]) + '.' + str(sys.version_info[1])],
                        stdout=open(make_logs_basepath + '.log', 'w'), stderr=open(make_logs_basepath + '.err', 'w'))
        try:
            return_code = call_subprocess(['make'], stdout=open(make_logs_basepath + '.log', 'a'),
                                          stderr=open(make_logs_basepath + '.err', 'a'), logger=logger)
            return_code = call_subprocess(['make', 'install'], stdout=open(make_logs_basepath + '.log', 'a'),
                                          stderr=open(make_logs_basepath + '.err', 'a'), logger=logger)
        except IOError:
            os.chdir(prev_dir)
            msg = 'Permission denied accessing ' + jellyfish_src_dirpath + '. Did you forget sudo?'
            logger.notice(msg)
            return False

        os.chdir(prev_dir)
        if return_code != 0 or not exists(jellyfish_bin_fpath):
            write_failed_compilation_flag('Jellyfish', jellyfish_src_dirpath, failed_compilation_flag, logger=logger)
            return False
    return True


def do(output_dir, ref_fpath, contigs_fpaths, logger):
    logger.print_timestamp()
    logger.main_info('Running analysis based on unique 101-mers...')
    addsitedir(jellyfish_python_dirpath)
    try:
        compile_jellyfish(logger)
        import jellyfish
        jellyfish.MerDNA.k(KMERS_LEN)
    except:
        logger.warning('Failed unique 101-mers analysis.')
        return

    logger.info('Running Jellyfish on reference...')
    jf_out_fpath = join(output_dir, basename(ref_fpath) + '.jf')
    qutils.call_subprocess([jellyfish_bin_fpath, 'count', '-m', '101', '-U', '1', '-s', str(getsize(ref_fpath)),
                           '-o', jf_out_fpath, '-t', str(qconfig.max_threads), ref_fpath])
    ref_kmers = jellyfish.ReadMerFile(jf_out_fpath)
    os.remove(jf_out_fpath)

    logger.info('Running Jellyfish on assemblies...')
    contigs_kmers = []
    for contigs_fpath in contigs_fpaths:
        jf_out_fpath = join(output_dir, basename(contigs_fpath) + '.jf')
        qutils.call_subprocess([jellyfish_bin_fpath, 'count', '-m', '101', '-U', '1', '-s', str(getsize(contigs_fpath)),
                               '-o', jf_out_fpath, '-t', str(qconfig.max_threads), contigs_fpath])
        contigs_kmers.append(jellyfish.QueryMerFile(jf_out_fpath))
        os.remove(jf_out_fpath)

    logger.info('Analyzing completeness and accuracy of assemblies...')
    unique_kmers = 0
    matched_kmers = defaultdict(int)
    shared_kmers = set()
    kmer_i = 0
    for kmer, count in ref_kmers:
        unique_kmers += 1
        matches = 0
        for idx in range(len(contigs_fpaths)):
            if contigs_kmers[idx][kmer]:
                matched_kmers[idx] += 1
                matches += 1
        if matches == len(contigs_fpaths):
            if kmer_i % 100 == 0:
                shared_kmers.add(str(kmer))
            kmer_i += 1

    for idx, contigs_fpath in enumerate(contigs_fpaths):
        report = reporting.get(contigs_fpath)
        completeness = matched_kmers[idx] * 100.0 / unique_kmers
        report.add_field(reporting.Fields.KMER_COMPLETENESS, '%.2f' % completeness)

    shared_kmers_by_chrom = dict()
    ref_contigs = dict((name, seq) for name, seq in read_fasta(ref_fpath))
    for name, seq in ref_contigs.items():
        seq_kmers = jellyfish.string_mers(seq)
        for kmer in seq_kmers:
            if str(kmer) in shared_kmers:
                shared_kmers_by_chrom[str(kmer)] = name

    for contigs_fpath in contigs_fpaths:
        report = reporting.get(contigs_fpath)
        len_map_to_one_chrom = 0
        len_map_to_multi_chrom = 0
        total_len = 0

        for name, seq in read_fasta(contigs_fpath):
            total_len += len(seq)
            seq_kmers = jellyfish.string_mers(seq)
            chrom_markers = []
            for kmer in seq_kmers:
                kmer_str = str(kmer)
                if kmer_str in shared_kmers_by_chrom:
                    chrom = shared_kmers_by_chrom[kmer_str]
                    chrom_markers.append(chrom)
            if len(chrom_markers) < MIN_MARKERS:
                continue
            if len(set(chrom_markers)) == 1:
                len_map_to_one_chrom += len(seq)
            else:
                len_map_to_multi_chrom += len(seq)

        report.add_field(reporting.Fields.KMER_SCAFFOLDS_ONE_CHROM, '%.2f' % (len_map_to_one_chrom * 100.0 / total_len))
        report.add_field(reporting.Fields.KMER_SCAFFOLDS_MULTI_CHROM, '%.2f' % (len_map_to_multi_chrom * 100.0 / total_len))
        report.add_field(reporting.Fields.KMER_SCAFFOLDS_NONE_CHROM, '%.2f' % ((total_len - len_map_to_one_chrom - len_map_to_multi_chrom)
                                                                               * 100.0 / total_len))
    logger.info('Done.')

