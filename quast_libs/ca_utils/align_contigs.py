############################################################################
# Copyright (c) 2015-2018 Saint Petersburg State University
# Copyright (c) 2011-2015 Saint Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################

from __future__ import with_statement

import re
from os.path import isfile
import datetime

from quast_libs import qconfig, qutils
from quast_libs.ca_utils.analyze_misassemblies import Mapping
from quast_libs.ca_utils.misc import minimap_fpath, parse_cs_tag

from quast_libs.log import get_logger
from quast_libs.qutils import md5, is_non_empty_file

logger = get_logger(qconfig.LOGGER_DEFAULT_NAME)


class AlignerStatus:
    FAILED = 0
    OK = 1
    NOT_ALIGNED = 2
    ERROR = 3


def create_successful_check(fpath, contigs_fpath, ref_fpath):
    successful_check_file = open(fpath, 'w')
    successful_check_file.write("Assembly md5 checksum: %s\n" % md5(contigs_fpath))
    successful_check_file.write("Reference md5 checksum: %s\n" % md5(ref_fpath))
    successful_check_file.write("Successfully finished on " +
                                       datetime.datetime.now().strftime('%Y/%m/%d %H:%M:%S') + '\n')
    successful_check_file.close()


def check_successful_check(fpath, contigs_fpath, ref_fpath):
    successful_check_content = open(fpath).read().split('\n')
    if len(successful_check_content) < 2:
        return False
    if successful_check_content[0].strip().split()[-1] != str(md5(contigs_fpath)):
        return False
    if successful_check_content[1].strip().split()[-1] != str(md5(ref_fpath)):
        return False
    return True


def run_minimap_agb(out_fpath, ref_fpath, contigs_fpath, log_err_fpath, index, max_threads):  # run minimap2 for AGB
    mask_level = '1' if qconfig.min_IDY < 95 else '0.9'
    cmdline = [minimap_fpath(), '-cx', 'asm20', '--mask-level', mask_level, '-N', '100',
               '--score-N', '0', '-E', '1,0', '-f', '200', '--cs', '-t', str(max_threads), ref_fpath, contigs_fpath]
    return_code = qutils.call_subprocess(cmdline, stdout=open(out_fpath, 'w'), stderr=open(log_err_fpath, 'a'),
                                         indent='  ' + qutils.index_to_str(index))
    return return_code


def run_minimap(out_fpath, ref_fpath, contigs_fpath, log_err_fpath, index, max_threads):
    if qconfig.is_agb_mode:
        return run_minimap_agb(out_fpath, ref_fpath, contigs_fpath, log_err_fpath, index, max_threads)

    if qconfig.min_IDY < 90:
        preset = 'asm20'
    elif qconfig.min_IDY < 95 or qconfig.is_combined_ref:
        preset = 'asm10'
    else:
        preset = 'asm5'
    # -s -- min CIGAR score, -z -- affects how often to stop alignment extension, -B -- mismatch penalty
    # -O -- gap penalty, -r -- max gap size
    mask_level = '1' if qconfig.is_combined_ref else '0.9'
    num_alignments = '100' if qconfig.is_combined_ref else '50'
    additional_options = ['-B5', '-O4,16', '--no-long-join', '-r', str(qconfig.MAX_INDEL_LENGTH),
                          '-N', num_alignments, '-s', str(qconfig.min_alignment), '-z', '200']
    cmdline = [minimap_fpath(), '-c', '-x', preset] + (additional_options if not qconfig.large_genome else []) + \
              ['--mask-level', mask_level, '--min-occ', '200', '-g', '2500', '--score-N', '2', '--cs', '-t', str(max_threads), ref_fpath, contigs_fpath]
    return_code = qutils.call_subprocess(cmdline, stdout=open(out_fpath, 'w'), stderr=open(log_err_fpath, 'a'),
                                         indent='  ' + qutils.index_to_str(index))

    return return_code


def get_aux_out_fpaths(fname):
    coords_fpath = fname + '.coords'
    coords_filtered_fpath = fname + '.coords.filtered'
    unaligned_fpath = fname + '.unaligned' if not qconfig.space_efficient else '/dev/null'
    used_snps_fpath = fname + '.used_snps' + ('.gz' if not qconfig.no_gzip else '') if not qconfig.space_efficient else '/dev/null'
    return coords_fpath, coords_filtered_fpath, unaligned_fpath, used_snps_fpath


def parse_minimap_output(raw_coords_fpath, coords_fpath):
    cigar_pattern = re.compile(r'(\d+[M=XIDNSH])')

    total_aligned_bases = 0
    with open(raw_coords_fpath) as f:
        with open(coords_fpath, 'w') as coords_file:
            for line in f:
                fs = line.split('\t')
                if len(fs) < 10:
                    continue
                contig, align_start, align_end, strand, ref_name, ref_start = \
                    fs[0], fs[2], fs[3], fs[4], fs[5], fs[7]
                align_start, align_end, ref_start = map(int, (align_start, align_end, ref_start))
                align_start += 1
                ref_start += 1
                if fs[-1].startswith('cs'):
                    cs = fs[-1].strip()
                    cigar = fs[-2]
                else:
                    cs = ''
                    cigar = fs[-1]
                cigar = cigar.split(':')[-1]

                strand_direction = 1
                if strand == '-':
                    align_start, align_end = align_end, align_start
                    strand_direction = -1
                align_len = 0
                ref_len = 0
                matched_bases, bases_in_mapping = map(int, (fs[9], fs[10]))
                operations = cigar_pattern.findall(cigar)

                for op in operations:
                    n_bases, operation = int(op[:-1]), op[-1]
                    if operation == 'S' or operation == 'H':
                        align_start += n_bases
                    elif operation == 'M' or operation == '=' or operation == 'X':
                        align_len += n_bases
                        ref_len += n_bases
                    elif operation == 'D':
                        ref_len += n_bases
                    elif operation == 'I':
                        align_len += n_bases

                align_end = align_start + (align_len - 1) * strand_direction
                ref_end = ref_start + ref_len - 1
                total_aligned_bases += align_len

                idy = '%.2f' % (matched_bases * 100.0 / bases_in_mapping)
                if ref_name != "*":
                    if float(idy) >= qconfig.min_IDY:
                        align = Mapping(s1=ref_start, e1=ref_end, s2=align_start, e2=align_end, len1=ref_len,
                                        len2=align_len, idy=idy, ref=ref_name, contig=contig, cigar=cs)
                        coords_file.write(align.coords_str() + '\n')
                    else:
                        split_align(coords_file, align_start, strand_direction, ref_start, ref_name, contig, cs)


def split_align(coords_file, align_start, strand_direction, ref_start, ref_name, contig, cs):
    def _write_align():
        if align_len < qconfig.min_alignment or not ref_len or not align_cs:
            return
        align_end = align_start + (align_len - 1) * strand_direction
        ref_end = ref_start + ref_len - 1
        align_idy = '%.2f' % (matched_bases * 100.0 / ref_len)
        if float(align_idy) >= qconfig.min_IDY:
            align = Mapping(s1=ref_start, e1=ref_end, s2=align_start, e2=align_end, len1=ref_len,
                            len2=align_len, idy=align_idy, ref=ref_name, contig=contig, cigar=align_cs)
            coords_file.write(align.coords_str() + '\n')

    ref_len, align_len, align_end = 0, 0, 0
    align_cs = ''
    matched_bases = 0
    for op in parse_cs_tag(cs):
        if op.startswith(':'):
            n_bases = int(op[1:])
        else:
            n_bases = len(op) - 1
        if op.startswith('*'):
            align_cs += op
            ref_len += 1
            align_len += 1
        elif op.startswith('+'):
            _write_align()
            align_start += (align_len + n_bases) * strand_direction
            ref_start += ref_len
            align_len, ref_len, matched_bases = 0, 0, 0
            align_cs = ''
        elif op.startswith('-'):
            _write_align()
            align_start += align_len * strand_direction
            ref_start += ref_len + n_bases
            align_len, ref_len, matched_bases = 0, 0, 0
            align_cs = ''
        else:
            align_cs += op
            ref_len += n_bases
            align_len += n_bases
            matched_bases += n_bases
    _write_align()


def align_contigs(output_fpath, out_basename, ref_fpath, contigs_fpath, old_contigs_fpath, index, threads, log_out_fpath, log_err_fpath):
    log_out_f = open(log_out_fpath, 'w')

    successful_check_fpath = out_basename + '.sf'
    log_out_f.write('Aligning contigs to reference...\n')

    # Checking if there are existing previous alignments.
    # If they exist, using them to save time.
    using_existing_alignments = False
    if isfile(successful_check_fpath) and isfile(output_fpath):
        if check_successful_check(successful_check_fpath, old_contigs_fpath, ref_fpath):
            log_out_f.write('\tUsing existing alignments...\n')
            logger.info('  ' + qutils.index_to_str(index) + 'Using existing alignments... ')
            using_existing_alignments = True

    if not using_existing_alignments:
        log_out_f.write('\tAligning contigs to the reference\n')
        logger.info('  ' + qutils.index_to_str(index) + 'Aligning contigs to the reference')

        tmp_output_fpath = output_fpath + '_tmp'
        exit_code = run_minimap(tmp_output_fpath, ref_fpath, contigs_fpath, log_err_fpath, index, threads)
        if exit_code != 0:
            return AlignerStatus.ERROR

        if not isfile(tmp_output_fpath):
            return AlignerStatus.FAILED
        if not is_non_empty_file(tmp_output_fpath):
            return AlignerStatus.NOT_ALIGNED

        create_successful_check(successful_check_fpath, old_contigs_fpath, ref_fpath)
        log_out_f.write('Filtering alignments...\n')
        parse_minimap_output(tmp_output_fpath, output_fpath)
    return AlignerStatus.OK