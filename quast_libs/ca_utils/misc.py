############################################################################
# Copyright (c) 2015-2017 Saint Petersburg State University
# Copyright (c) 2011-2015 Saint Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################
#
# This is auxiliary file for contigs_analyzer.py
#
############################################################################

from __future__ import with_statement
import gzip
import os
import shutil
from itertools import repeat
from os.path import isfile, isdir, join, dirname, basename

try:
   from collections import OrderedDict
except ImportError:
   from quast_libs.site_packages.ordered_dict import OrderedDict

from quast_libs import qconfig, qutils
from quast_libs.qutils import compile_tool, val_to_str, check_prev_compilation_failed, write_failed_compilation_flag, \
    fix_configure_timestamps

contig_aligner_dirpath = join(qconfig.LIBS_LOCATION, 'MUMmer')
ref_labels_by_chromosomes = OrderedDict()
intergenomic_misassemblies_by_asm = {}
contigs_aligned_lengths = {}


def bin_fpath(fname):
    return join(contig_aligner_dirpath, fname)


def compile_aligner(logger, only_clean=False):
    default_requirements = ['nucmer', 'delta-filter', 'show-coords', 'show-snps', 'mummer', 'mummerplot', 'mgaps']
    mummer_failed_compilation_flag = join(contig_aligner_dirpath, 'make.failed')

    if only_clean:
        compile_tool('MUMmer', contig_aligner_dirpath, default_requirements, logger=logger, only_clean=only_clean)
        return True

    if check_prev_compilation_failed('MUMmer', mummer_failed_compilation_flag, just_notice=True, logger=logger):
        logger.error("Compilation of contig aligner software was unsuccessful! QUAST functionality will be limited.")
        return False

    fix_configure_timestamps(contig_aligner_dirpath)
    success_compilation = compile_tool('MUMmer', contig_aligner_dirpath, default_requirements,
                                       just_notice=False, logger=logger, only_clean=only_clean,
                                       configure_args=['--prefix=' + contig_aligner_dirpath])
    if success_compilation:
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
    compile_tool('gnuplot', tool_dirpath, [tool_exec_fpath], just_notice=True, logger=logger, only_clean=only_clean,
                 configure_args=['--with-qt=no', '--disable-wxwidgets', '--prefix=' + tool_dirpath])

    if only_clean:
        return True
    elif isfile(tool_exec_fpath):
        return tool_exec_fpath
    else:
        return None


def draw_mummer_plot(logger, nucmer_fpath, delta_fpath, index, log_out_f, log_err_f):
    output_dirpath = dirname(dirname(nucmer_fpath))
    mummer_plot_fpath = join(output_dirpath, basename(nucmer_fpath) + '_mummerplot.html')
    return_code = qutils.call_subprocess(
        [bin_fpath('mummerplot'), '--html', '--layout', '-p', nucmer_fpath, delta_fpath],
        stdout=log_out_f,
        stderr=log_err_f,
        indent='  ' + qutils.index_to_str(index))
    if return_code == 0:
        plot_script_fpath = nucmer_fpath + '.gp'
        temp_plot_fpath = nucmer_fpath + '.html'
        if isfile(plot_script_fpath) and isfile(gnuplot_exec_fpath()):
            qutils.call_subprocess(
                [gnuplot_exec_fpath(), plot_script_fpath],
                stdout=open('/dev/null', 'w'), stderr=log_err_f,
                indent='  ' + qutils.index_to_str(index))
            if isfile(temp_plot_fpath):
                with open(temp_plot_fpath) as template_file:
                    html = template_file.read()
                    html = _embed_css_and_scripts(html)
                    with open(mummer_plot_fpath, 'w') as f_html:
                        f_html.write(html)
                    logger.info('  ' + qutils.index_to_str(index) + 'MUMmer plot saved to ' + mummer_plot_fpath)

    if not isfile(mummer_plot_fpath):
        logger.notice(qutils.index_to_str(index) + ' MUMmer plot cannot be created.\n')


def _embed_css_and_scripts(html):
    js_line_tmpl = '<script src="%s"></script>'
    js_l_tag = '<script type="text/javascript" name="%s">'
    js_r_tag = '    </script>'

    css_line_tmpl = '<link type="text/css" href="%s" rel="stylesheet">'
    css_l_tag = '<style type="text/css" rel="stylesheet" name="%s">'
    css_r_tag = '    </style>'

    css_files = [
        join(qconfig.LIBS_LOCATION, 'gnuplot/term/js/gnuplot_mouse.css')
    ]
    js_files = [
        join(qconfig.LIBS_LOCATION, 'gnuplot/term/js/canvastext.js'),
        join(qconfig.LIBS_LOCATION, 'gnuplot/term/js/gnuplot_common.js'),
        join(qconfig.LIBS_LOCATION, 'gnuplot/term/js/gnuplot_dashedlines.js')
    ]
    for line_tmpl, files, l_tag, r_tag in [
            (js_line_tmpl, js_files, js_l_tag, js_r_tag),
            (css_line_tmpl, css_files, css_l_tag, css_r_tag),
        ]:
        for fpath in files:
            rel_fpath = basename(fpath)
            line = line_tmpl % rel_fpath
            l_tag_formatted = l_tag % rel_fpath

            with open(fpath) as f:
                contents = f.read()
                contents = '\n'.join(' ' * 8 + l for l in contents.split('\n'))
                html = html.replace(line, l_tag_formatted + '\n' + contents + '\n' + r_tag)

    return html


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
