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
from os.path import isfile, isdir, join, dirname, basename

from quast_libs import qconfig, qutils
from quast_libs.qutils import compile_tool, val_to_str, check_prev_compilation_failed

contig_aligner = None
contig_aligner_dirpath = join(qconfig.LIBS_LOCATION, 'MUMmer')
ref_labels_by_chromosomes = {}
intergenomic_misassemblies_by_asm = {}
contigs_aligned_lengths = {}
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

    default_requirements = ['nucmer', 'delta-filter', 'show-coords', 'show-snps', 'mummer', 'mummerplot', 'mgaps']

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


def gnuplot_exec_fpath():
    tool_dirpath = join(qconfig.LIBS_LOCATION, 'gnuplot')
    tool_src_dirpath = join(tool_dirpath, 'src')
    tool_exec_fpath = join(tool_src_dirpath, 'gnuplot')
    return tool_exec_fpath


def compile_gnuplot(logger, only_clean=False):
    tool_dirpath = join(qconfig.LIBS_LOCATION, 'gnuplot')
    tool_exec_fpath = gnuplot_exec_fpath()

    if only_clean:
        if isfile(tool_exec_fpath):
            os.remove(tool_exec_fpath)
        return True

    if not isfile(tool_exec_fpath):
        failed_compilation_flag = join(tool_dirpath, 'make.failed')
        if check_prev_compilation_failed('gnuplot', failed_compilation_flag, just_notice=True, logger=logger):
            return None
        logger.main_info("Compiling gnuplot...")
        prev_dir = os.getcwd()
        os.chdir(tool_dirpath)
        return_code = qutils.call_subprocess(
            ['./configure', '--with-qt=no', '--disable-wxwidgets', '--with-pdf'],
            stdout=open(join(tool_dirpath, 'make.log'), 'w'),
            stderr=open(join(tool_dirpath, 'make.err'), 'w'),
            indent='    ')
        if return_code == 0:
            return_code = qutils.call_subprocess(
                ['make'],
                stdout=open(join(tool_dirpath, 'make.log'), 'w'),
                stderr=open(join(tool_dirpath, 'make.err'), 'w'),
                indent='    ')
        os.chdir(prev_dir)
        if return_code != 0 or not isfile(tool_exec_fpath):
            logger.notice("Failed to compile gnuplot (" + tool_dirpath +
                         ")!\nTry to compile it manually.\nUse --debug option to see the command lines.")
            open(failed_compilation_flag, 'w').close()
            return None
    return tool_exec_fpath


def draw_mummer_plot(logger, nucmer_fpath, delta_fpath, index, log_out_f, log_err_f):
    output_dirpath = dirname(dirname(nucmer_fpath))
    mummer_plot_fpath = join(output_dirpath, basename(nucmer_fpath) + '_mummerplot.pdf')
    return_code = qutils.call_subprocess(
        [bin_fpath('mummerplot'), '--pdf', '--layout', '-p', nucmer_fpath, delta_fpath],
        stdout=log_out_f,
        stderr=log_err_f,
        indent='  ' + qutils.index_to_str(index))
    if return_code == 0:
        plot_script_fpath = nucmer_fpath + '.gp'
        temp_plot_fpath = nucmer_fpath + '.pdf'
        if isfile(plot_script_fpath) and isfile(gnuplot_exec_fpath()):
            qutils.call_subprocess(
                [gnuplot_exec_fpath(), plot_script_fpath],
                stdout=log_out_f, stderr=log_err_f,
                indent='  ' + qutils.index_to_str(index))
            if isfile(temp_plot_fpath):
                shutil.copy(temp_plot_fpath, mummer_plot_fpath)
                logger.info(qutils.index_to_str(index) + ' MUMmer plot saved to ' + mummer_plot_fpath)

    if not isfile(mummer_plot_fpath):
        logger.notice(qutils.index_to_str(index) + ' MUMmer plot cannot be created.\n')


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
            nucmer_output_dir = join(nucmer_output_dir, 'raw')
            if not os.path.isdir(nucmer_output_dir):
                os.mkdir(nucmer_output_dir)
    return nucmer_output_dir


def clean_tmp_files(nucmer_fpath):
    if qconfig.debug:
        return

    # delete temporary files
    for ext in ['.delta', '.coords_tmp', '.coords.headless']:
        if isfile(nucmer_fpath + ext):
            os.remove(nucmer_fpath + ext)


def close_handlers(ca_output):
    for handler in vars(ca_output).values():  # assume that ca_output does not have methods (fields only)
        if handler:
            handler.close()


def compress_nucmer_output(logger, nucmer_fpath):
    for ext in ['.all_snps', '.used_snps']:
        fpath = nucmer_fpath + ext
        if isfile(fpath):
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
