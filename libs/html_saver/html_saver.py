############################################################################
# Copyright (c) 2011-2014 Saint-Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################

from __future__ import with_statement
import os
import shutil
import re
from libs import qconfig
from libs.html_saver import json_saver

from libs.log import get_logger
log = get_logger('quast')


def get_real_path(relpath_in_html_saver):
    return os.path.join(qconfig.LIBS_LOCATION, 'html_saver', relpath_in_html_saver)


scripts_inserted = False

report_fname = qconfig.report_prefix + ".html"

template_fpath = get_real_path('template.html')

static_dirname = 'static'
static_dirpath = get_real_path(static_dirname)

aux_dirname = qconfig.html_aux_dir
aux_files = [
    'jquery-1.8.2.min.js',
    'flot/jquery.flot.min.js',
    'flot/excanvas.min.js',
    'flot/jquery.flot.dashes.js',
    'scripts/draw_cumulative_plot.js',
    'scripts/draw_nx_plot.js',
    'scripts/draw_gc_plot.js',
    'scripts/utils.js',
    'scripts/hsvToRgb.js',
    'scripts/draw_genes_plot.js',
    'dragtable.js',
    'ie_html5.js',
    'img/draggable.png',
    'bootstrap/bootstrap-tooltip-5px-lower.min.js',
    'bootstrap/bootstrap.min.css',
    'bootstrap/bootstrap.min.js',
    'bootstrap/bootstrap-tooltip-vlad.js',
    'report.css',
    'common.css',
    'scripts/build_report.js',
    'scripts/build_total_report.js',
    'scripts/build_report_meta.js',
    'scripts/build_total_report_meta.js',
]


def init(results_dirpath, meta=False):
#    shutil.copy(template_fpath,     os.path.join(results_dirpath, report_fname))
    aux_dirpath = os.path.join(results_dirpath, aux_dirname)
    os.mkdir(aux_dirpath)

    for aux_f_relpath in aux_files:
        src_fpath = os.path.join(static_dirpath, aux_f_relpath)
        dst_fpath = os.path.join(aux_dirpath, aux_f_relpath)

        if not os.path.exists(os.path.dirname(dst_fpath)):
            os.makedirs(os.path.dirname(dst_fpath))

        if not os.path.exists(dst_fpath):
            shutil.copyfile(src_fpath, dst_fpath)

    with open(template_fpath) as template_file:
        html = template_file.read()
        if not meta:
            html = html.replace('{{ buildreport }}', 'scripts/build_report.js')
            html = html.replace('{{ buildtotalreport }}', 'scripts/build_total_report.js')
        else:
            html = html.replace('{{ buildreport }}', 'scripts/build_report_meta.js')
            html = html.replace('{{ buildtotalreport }}', 'scripts/build_total_report_meta.js')
        html = html.replace("/" + static_dirname, aux_dirname)
        html = html.replace('{{ glossary }}', open(get_real_path('glossary.json')).read())
        html_fpath = os.path.join(results_dirpath, report_fname)
        if os.path.exists(html_fpath):
            os.remove(html_fpath)
        with open(html_fpath, 'w') as f_html:
            f_html.write(html)

#def init_old(results_dirpath):
#    with open(template_fpath) as template_file:
#        html = template_file.read()
#
#        for fp_script in support_files:
#            with open(get_real_path(fp_script)) as f:
#                html = html.replace(
#                    '<script type="text/javascript" src="' + fp_script + '"></script>',
#                    '<script type="text/javascript">\n' + f.read() + '\n\t</script>\n')
#
#        html = html.replace('<link rel="stylesheet" type="text/css" href="static/report.css" />',
#                            '<style rel="stylesheet">\n' + open(get_real_path('static/report.css')).read() + '\n</style>\n\n')
#
#        html = html.replace('<link rel="stylesheet" href="static/bootstrap/bootstrap.min.css"/>',
#                            '<style rel="stylesheet">\n' + open(get_real_path('static/bootstrap/bootstrap.min.css')).read() + '\n</style>\n\n')
#
#        html = html.replace(
#            '<script type="text/javascript" src="static/ie_html5.js"></script>',
#            '<script type="text/javascript" >\n' + open(get_real_path('static/ie_html5.js')).read() + '\n</script>')
#
#        html = html.replace(
#            '<script type="text/javascript" src="static/bootstrap/bootstrap-tooltip-5px-lower-min.js"></script>',
#            '<script type="text/javascript" >\n' + open(
#                get_real_path('static/bootstrap/bootstrap-tooltip-5px-lower-min.js')).read() + '\n</script>')
#
#        html = html.replace('{{ glossary }}', open(get_real_path('glossary.json')).read())
#
#        with open(os.path.join(results_dirpath, report_fname), 'w') as f_html:
#            f_html.write(html)


def append(results_dirpath, json_fpath, keyword):
    html_fpath = os.path.join(results_dirpath, report_fname)

    if not os.path.isfile(html_fpath):
        init(results_dirpath)

    # reading JSON file
    with open(json_fpath) as f_json:
        json_text = f_json.read()
    os.remove(json_fpath)

    # reading html template file
    with open(html_fpath) as f_html:
        html_text = f_html.read()

    # substituting template text with json
    html_text = re.sub('{{ ' + keyword + ' }}', json_text, html_text)

    # writing substituted html to final file
    with open(html_fpath, 'w') as f_html:
        f_html.write(html_text)
    return json_text


def create_meta_report(results_dirpath, json_texts):
    html_fpath = os.path.join(results_dirpath, report_fname)
    if not os.path.isfile(html_fpath):
        init(results_dirpath, True)
    # reading html template file
    with open(html_fpath) as f_html:
        html_text = f_html.read()
    keyword = 'totalReport'
    html_text = re.sub('{{ ' + keyword + ' }}', '[' + ','.join(json_texts) + ']', html_text)
    with open(html_fpath, 'w') as f_html:
        f_html.write(html_text)


def save_total_report(results_dirpath, min_contig, ref_fpath):
    json_fpath = json_saver.save_total_report(results_dirpath, min_contig, ref_fpath)
    if json_fpath:
        json_saver.json_text = append(results_dirpath, json_fpath, 'totalReport')
        log.info('  HTML version (interactive tables and plots) saved to ' + os.path.join(results_dirpath, report_fname))


def save_contigs_lengths(results_dirpath, contigs_fpaths, lists_of_lengths):
    json_fpath = json_saver.save_contigs_lengths(results_dirpath, contigs_fpaths, lists_of_lengths)
    if json_fpath:
        append(results_dirpath, json_fpath, 'contigsLenghts')


def save_reference_length(results_dirpath, reference_length):
    json_fpath = json_saver.save_reference_length(results_dirpath, reference_length)
    if json_fpath:
        append(results_dirpath, json_fpath, 'referenceLength')


def save_aligned_contigs_lengths(results_dirpath, contigs_fpaths, lists_of_lengths):
    json_fpath = json_saver.save_aligned_contigs_lengths(results_dirpath, contigs_fpaths, lists_of_lengths)
    if json_fpath:
        append(results_dirpath, json_fpath, 'alignedContigsLengths')


def save_assembly_lengths(results_dirpath, contigs_fpaths, assemblies_lengths):
    json_fpath = json_saver.save_assembly_lengths(results_dirpath, contigs_fpaths, assemblies_lengths)
    if json_fpath:
        append(results_dirpath, json_fpath, 'assembliesLengths')


def save_features_in_contigs(results_dirpath, contigs_fpaths, feature_name, feature_in_contigs, ref_feature_num):
    json_fpath = json_saver.save_features_in_contigs(results_dirpath, contigs_fpaths, feature_name, feature_in_contigs, ref_feature_num)
    if json_fpath:
        append(results_dirpath, json_fpath, feature_name + 'InContigs')


def save_GC_info(results_dirpath, contigs_fpaths, list_of_GC_distributions):
    json_fpath = json_saver.save_GC_info(results_dirpath, contigs_fpaths, list_of_GC_distributions)
    if json_fpath:
        append(results_dirpath, json_fpath, 'gcInfos')
