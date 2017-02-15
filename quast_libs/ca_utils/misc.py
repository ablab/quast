############################################################################
# Copyright (c) 2015-2016 Saint Petersburg State University
# Copyright (c) 2011-2015 Saint Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################
#
# This is auxiliary file for contigs_analyzer.py and gage.py
#
############################################################################

from __future__ import with_statement
import gzip
import platform
import os
import shutil
from itertools import repeat
from os.path import isfile, isdir, join

from quast_libs import qconfig, qutils
from quast_libs.qutils import compile_tool, val_to_str, check_prev_compilation_failed

contig_aligner = None
contig_aligner_dirpath = join(qconfig.LIBS_LOCATION, 'MUMmer')
ref_labels_by_chromosomes = {}
intergenomic_misassemblies_by_asm = {}
e_mem_failed_compilation_flag = join(contig_aligner_dirpath, 'make.emem.failed')
mummer_failed_compilation_flag = join(contig_aligner_dirpath, 'make.mummer.failed')


def reset_aligner_selection():
    global contig_aligner
    contig_aligner = None


def bin_fpath(fname):
    return join(contig_aligner_dirpath, fname)


def is_emem_aligner():
    return contig_aligner == 'E-MEM'


def get_installed_emem():
    return qutils.get_path_to_program('e-mem')


def compile_aligner(logger, only_clean=False, compile_all_aligners=False):
    global contig_aligner

    if not compile_all_aligners:
        if contig_aligner is not None:
            failed_compilation_flag = e_mem_failed_compilation_flag if is_emem_aligner() else mummer_failed_compilation_flag
            if not check_prev_compilation_failed(contig_aligner, failed_compilation_flag, just_notice=True, logger=logger):
                return True

    default_requirements = ['nucmer', 'delta-filter', 'show-coords', 'show-snps', 'mummer', 'mgaps']

    if not qconfig.force_nucmer:
        if get_installed_emem() or isfile(join(contig_aligner_dirpath, 'e-mem')):
            emem_requirements = default_requirements
        elif qconfig.platform_name == 'macosx' and isfile(join(contig_aligner_dirpath, 'e-mem-osx')):
            shutil.copy(join(contig_aligner_dirpath, 'e-mem-osx'), join(contig_aligner_dirpath, 'e-mem'))
            emem_requirements = default_requirements
        else:
            emem_requirements = default_requirements + ['e-mem']
        aligners_to_try = [
            ('E-MEM', 'emem', emem_requirements),
            ('MUMmer', 'mummer', default_requirements)]
    else:
        aligners_to_try = [
            ('MUMmer', 'mummer', default_requirements)]

    for i, (name, flag_name, requirements) in enumerate(aligners_to_try):
        success_compilation = compile_tool(name, contig_aligner_dirpath, requirements, just_notice=(i < len(aligners_to_try) - 1),
                                           logger=logger, only_clean=only_clean, flag_suffix='.' + flag_name,
                                           make_cmd='no-emem' if 'e-mem' not in requirements else None)
        if not success_compilation:
            continue
        contig_aligner = name  # successfully compiled
        if not compile_all_aligners:
            return True

    if compile_all_aligners and contig_aligner:
        return True
    logger.error("Compilation of contig aligner software was unsuccessful! QUAST functionality will be limited.")
    return False


def is_same_reference(chr1, chr2):
    return ref_labels_by_chromosomes[chr1] == ref_labels_by_chromosomes[chr2]


def get_ref_by_chromosome(chrom):
    return ref_labels_by_chromosomes[chrom] if chrom in ref_labels_by_chromosomes else ''


def print_file(all_rows, fpath, append_to_existing_file=False):
    colwidths = repeat(0)
    for row in all_rows:
        colwidths = [max(len(v), w) for v, w in zip([row['metricName']] + [val_to_str(v) for v in row['values']], colwidths)]
    txt_file = open(fpath, 'a' if append_to_existing_file else 'w')
    for row in all_rows:
        txt_file.write('  '.join('%-*s' % (colwidth, cell) for colwidth, cell
                                     in zip(colwidths, [row['metricName']] + [val_to_str(v) for v in row['values']])) + '\n')


def create_nucmer_output_dir(output_dir):
    nucmer_output_dirname = qconfig.nucmer_output_dirname
    nucmer_output_dir = join(output_dir, nucmer_output_dirname)
    if not isdir(nucmer_output_dir):
        os.mkdir(nucmer_output_dir)
    if qconfig.is_combined_ref:
        from quast_libs import search_references_meta
        if search_references_meta.is_quast_first_run:
            nucmer_output_dir = os.path.join(nucmer_output_dir, 'raw')
            if not os.path.isdir(nucmer_output_dir):
                os.mkdir(nucmer_output_dir)
    return nucmer_output_dir


def clean_tmp_files(nucmer_fpath):
    if qconfig.debug:
        return

    # delete temporary files
    for ext in ['.delta', '.coords_tmp', '.coords.headless']:
        if os.path.isfile(nucmer_fpath + ext):
            os.remove(nucmer_fpath + ext)


def close_handlers(ca_output):
    for handler in vars(ca_output).values():  # assume that ca_output does not have methods (fields only)
        if handler:
            handler.close()


def compress_nucmer_output(logger, nucmer_fpath):
    for ext in ['.all_snps', '.used_snps']:
        fpath = nucmer_fpath + ext
        if os.path.isfile(fpath):
            logger.info('  Gzipping ' + fpath + ' to reduce disk space usage...')
            with open(fpath, 'rb') as f_in:
                f_out = gzip.open(fpath + '.gz', 'wb')
                f_out.writelines(f_in)
                f_out.close()
            os.remove(fpath)
            logger.info('    saved to ' + fpath + '.gz')


def open_gzipsafe(f, mode='rt'):
    if not os.path.exists(f) and 'r' in mode:
        f += '.gz'
    if f.endswith('.gz') or f.endswith('.gzip'):
        if 't' not in mode:
            mode += 't'
        try:
            h = gzip.open(f, mode=mode)
        except IOError:
            return open(f, mode=mode)
        else:
            if 'w' in mode:
                return h
            else:
                try:
                    h.read(1)
                except IOError:
                    h.close()
                    return open(f, mode=mode)
                else:
                    h.close()
                    h = gzip.open(f, mode=mode)
                    return h
    else:
        return open(f, mode=mode)
