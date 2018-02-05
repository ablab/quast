############################################################################
# Copyright (c) 2015-2017 Saint Petersburg State University
# Copyright (c) 2011-2015 Saint Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################

from __future__ import with_statement

import os
from os.path import isfile, join, basename, dirname
import datetime
import shutil
import sys

from quast_libs import options_parser
from quast_libs import qconfig, qutils
from quast_libs.ca_utils.misc import bin_fpath, is_emem_aligner, compile_aligner, e_mem_failed_compilation_flag, \
    create_nucmer_output_dir, clean_tmp_files, get_installed_emem, reset_aligner_selection, draw_mummer_plot

from quast_libs.log import get_logger
from quast_libs.qutils import is_python2, md5, safe_create

logger = get_logger(qconfig.LOGGER_DEFAULT_NAME)


class NucmerStatus:
    FAILED = 0
    OK = 1
    NOT_ALIGNED = 2
    ERROR = 3


def create_nucmer_successful_check(fpath, contigs_fpath, ref_fpath):
    nucmer_successful_check_file = open(fpath, 'w')
    nucmer_successful_check_file.write("Assembly md5 checksum: %s\n" % md5(contigs_fpath))
    nucmer_successful_check_file.write("Reference md5 checksum: %s\n" % md5(ref_fpath))
    nucmer_successful_check_file.write("Successfully finished on " +
                                       datetime.datetime.now().strftime('%Y/%m/%d %H:%M:%S') + '\n')
    nucmer_successful_check_file.close()


def check_nucmer_successful_check(fpath, contigs_fpath, ref_fpath):
    successful_check_content = open(fpath).read().split('\n')
    if len(successful_check_content) < 2:
        return False
    if successful_check_content[0].strip().split()[-1] != str(md5(contigs_fpath)):
        return False
    if successful_check_content[1].strip().split()[-1] != str(md5(ref_fpath)):
        return False
    return True


def check_emem_functionality(logger):
    if not is_emem_aligner():
        return True
    logger.debug('Checking correctness of E-MEM compilation...')
    nucmer_output_dirpath = create_nucmer_output_dir(qconfig.output_dirpath)
    nucmer_fpath = join(nucmer_output_dirpath, 'test')
    return_code = run_nucmer(nucmer_fpath, options_parser.test_contigs_fpaths[0], options_parser.test_contigs_fpaths[1],
                             '/dev/null', '/dev/null', 0, emem_threads=1)
    if return_code != 0:
        if get_installed_emem():
            logger.main_info('Preinstalled E-MEM does not work properly.')
        else:
            logger.main_info('E-MEM does not work properly. QUAST will try to use Nucmer.')
        reset_aligner_selection()
        qconfig.force_nucmer = True
        safe_create(e_mem_failed_compilation_flag, logger, is_required=True)
    clean_tmp_files(nucmer_fpath)
    return compile_aligner(logger)


def run_nucmer(prefix, ref_fpath, contigs_fpath, log_out_fpath, log_err_fpath, index, emem_threads=1):
    # additional GAGE params of Nucmer: '-l', '30', '-banded'
    nucmer_cmdline = [bin_fpath('nucmer'), '-c', str(qconfig.min_cluster),
                      '-l', str(qconfig.min_cluster), '--maxmatch',
                      '-p', prefix]
    env = os.environ.copy()
    if is_emem_aligner():
        nucmer_cmdline += ['--emem']
        nucmer_cmdline += ['-t', str(emem_threads)]
        installed_emem_fpath = get_installed_emem()
        if installed_emem_fpath:
            env['NUCMER_E_MEM_OUTPUT_DIRPATH'] = dirname(prefix)
            nucmer_cmdline += ['--emempath', installed_emem_fpath]

    nucmer_cmdline += [ref_fpath, contigs_fpath]
    return_code = qutils.call_subprocess(nucmer_cmdline, stdout=open(log_out_fpath, 'a'), stderr=open(log_err_fpath, 'a'),
                                         indent='  ' + qutils.index_to_str(index), env=env)

    return return_code


def get_nucmer_aux_out_fpaths(nucmer_fpath):
    coords_fpath = nucmer_fpath + '.coords'
    coords_filtered_fpath = nucmer_fpath + '.coords.filtered'
    unaligned_fpath = nucmer_fpath + '.unaligned' if not qconfig.space_efficient else '/dev/null'
    show_snps_fpath = nucmer_fpath + '.all_snps'
    used_snps_fpath = nucmer_fpath + '.used_snps' + ('.gz' if not qconfig.no_gzip else '') if not qconfig.space_efficient else '/dev/null'
    return coords_fpath, coords_filtered_fpath, unaligned_fpath, show_snps_fpath, used_snps_fpath


def align_contigs(nucmer_fpath, ref_fpath, contigs_fpath, old_contigs_fpath, index,
                  parallel_by_chr, threads, log_out_fpath, log_err_fpath):
    log_out_f = open(log_out_fpath, 'w')
    log_err_f = open(log_err_fpath, 'w')

    nucmer_successful_check_fpath = nucmer_fpath + '.sf'
    delta_fpath = nucmer_fpath + '.delta'
    filtered_delta_fpath = nucmer_fpath + '.fdelta'

    coords_fpath, _, _, show_snps_fpath, _ = \
        get_nucmer_aux_out_fpaths(nucmer_fpath)

    log_out_f.write('Aligning contigs to reference...\n')

    # Checking if there are existing previous nucmer alignments.
    # If they exist, using them to save time.
    using_existing_alignments = False
    if isfile(nucmer_successful_check_fpath) and isfile(coords_fpath) and \
       (isfile(show_snps_fpath) or isfile(show_snps_fpath + '.gz') or not qconfig.show_snps):
        if check_nucmer_successful_check(nucmer_successful_check_fpath, old_contigs_fpath, ref_fpath):
            log_out_f.write('\tUsing existing alignments...\n')
            logger.info('  ' + qutils.index_to_str(index) + 'Using existing alignments... ')
            using_existing_alignments = True

    if not using_existing_alignments:
        log_out_f.write('\tAligning contigs to the reference\n')
        logger.info('  ' + qutils.index_to_str(index) + 'Aligning contigs to the reference')

        if not qconfig.splitted_ref:
            nucmer_exit_code = run_nucmer(nucmer_fpath, ref_fpath, contigs_fpath,
                                          log_out_fpath, log_err_fpath, index, threads)
            if nucmer_exit_code != 0:
                return NucmerStatus.ERROR
        else:
            prefixes_and_chr_files = [(nucmer_fpath + "_" + basename(chr_fname), chr_fname)
                                      for chr_fname in qconfig.splitted_ref]

            # Daemonic processes are not allowed to have children,
            # so if we are already one of parallel processes
            # (i.e. daemonic) we can't start new daemonic processes
            if parallel_by_chr and not qconfig.memory_efficient:
                n_jobs = min(qconfig.max_threads, len(prefixes_and_chr_files))
                threads = max(1, threads // n_jobs)
            else:
                n_jobs = 1
                threads = 1
            if n_jobs > 1:
                logger.info('    ' + 'Aligning to different chromosomes in parallel'
                                     ' (' + str(n_jobs) + ' threads)')

            # processing each chromosome separately (if we can)
            if is_python2():
                from joblib import Parallel, delayed
            else:
                from joblib3 import Parallel, delayed
            if not qconfig.memory_efficient:
                nucmer_exit_codes = Parallel(n_jobs=n_jobs)(delayed(run_nucmer)(
                    prefix, chr_file, contigs_fpath, log_out_fpath, log_err_fpath + "_part%d" % (i + 1), index, threads)
                    for i, (prefix, chr_file) in enumerate(prefixes_and_chr_files))
            else:
                nucmer_exit_codes = [run_nucmer(prefix, chr_file, contigs_fpath, log_out_fpath, log_err_fpath + "_part%d" % (i + 1), index, threads)
                                     for i, (prefix, chr_file) in enumerate(prefixes_and_chr_files)]

            log_err_f.write("Stderr outputs for reference parts are in:\n")
            for i in range(len(prefixes_and_chr_files)):
                log_err_f.write(log_err_fpath + "_part%d" % (i + 1) + '\n')
            log_err_f.write("\n")

            if 0 not in nucmer_exit_codes:
                return NucmerStatus.ERROR
            else:
                # filling common delta file
                delta_file = open(delta_fpath, 'w')
                delta_file.write(ref_fpath + " " + contigs_fpath + "\n")
                delta_file.write("NUCMER\n")
                for i, (prefix, chr_fname) in enumerate(prefixes_and_chr_files):
                    if nucmer_exit_codes[i] != 0:
                        logger.warning('  ' + qutils.index_to_str(index) +
                        'Failed aligning contigs %s to reference part %s! Skipping this part. ' % (qutils.label_from_fpath(contigs_fpath),
                        chr_fname) + ('Run with the --debug flag to see additional information.' if not qconfig.debug else ''))
                        continue

                    chr_delta_fpath = prefix + '.delta'
                    if isfile(chr_delta_fpath):
                        chr_delta_file = open(chr_delta_fpath)
                        chr_delta_file.readline()
                        chr_delta_file.readline()
                        for line in chr_delta_file:
                            delta_file.write(line)
                        chr_delta_file.close()

                delta_file.close()

        # By default: filtering by IDY% = 95 (as GAGE did)
        return_code = qutils.call_subprocess(
            [bin_fpath('delta-filter'), '-i', str(qconfig.min_IDY), '-l', str(qconfig.min_alignment), delta_fpath],
            stdout=open(filtered_delta_fpath, 'w'),
            stderr=log_err_f,
            indent='  ' + qutils.index_to_str(index))

        if return_code != 0:
            log_err_f.write(qutils.index_to_str(index) + ' Delta filter failed for ' + contigs_fpath + '\n')
            return NucmerStatus.ERROR

        shutil.move(filtered_delta_fpath, delta_fpath)

        if qconfig.draw_plots:
            draw_mummer_plot(logger, nucmer_fpath, delta_fpath, index, log_out_f, log_err_f)

        tmp_coords_fpath = coords_fpath + '_tmp'

        return_code = qutils.call_subprocess(
            [bin_fpath('show-coords'), delta_fpath],
            stdout=open(tmp_coords_fpath, 'w'),
            stderr=log_err_f,
            indent='  ' + qutils.index_to_str(index))
        if return_code != 0:
            log_err_f.write(qutils.index_to_str(index) + ' Show-coords failed for ' + contigs_fpath + '\n')
            return NucmerStatus.ERROR

        # removing waste lines from coords file
        coords_file = open(coords_fpath, 'w')
        header = []
        tmp_coords_file = open(tmp_coords_fpath)
        for line in tmp_coords_file:
            header.append(line)
            if line.startswith('====='):
                break
        coords_file.write(header[-2])
        coords_file.write(header[-1])
        for line in tmp_coords_file:
            coords_file.write(line)
        coords_file.close()
        tmp_coords_file.close()

        if not isfile(coords_fpath):
            return NucmerStatus.FAILED
        if len(open(coords_fpath).readlines()[-1].split()) < 13:
            return NucmerStatus.NOT_ALIGNED

        if qconfig.show_snps:
            with open(coords_fpath) as coords_file:
                headless_coords_fpath = coords_fpath + '.headless'
                headless_coords_f = open(headless_coords_fpath, 'w')
                coords_file.readline()
                coords_file.readline()
                headless_coords_f.write(coords_file.read())
                headless_coords_f.close()
                headless_coords_f = open(headless_coords_fpath)

                return_code = qutils.call_subprocess(
                    [bin_fpath('show-snps'), '-S', '-T', '-H', delta_fpath],
                    stdin=headless_coords_f,
                    stdout=open(show_snps_fpath, 'w'),
                    stderr=log_err_f,
                    indent='  ' + qutils.index_to_str(index))
                if return_code != 0:
                    log_err_f.write(qutils.index_to_str(index) + ' Show-snps failed for ' + contigs_fpath + '\n')
                    return NucmerStatus.ERROR

        create_nucmer_successful_check(nucmer_successful_check_fpath, old_contigs_fpath, ref_fpath)
    return NucmerStatus.OK