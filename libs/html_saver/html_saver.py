from __future__ import with_statement
import shutil
import re
from libs.html_saver import json_saver

############################################################################
# Copyright (c) 2011-2012 Saint-Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################

import os

__location__ = os.path.realpath(os.path.join(os.getcwd(), os.path.dirname(__file__)))

def get_real_path(relpath_in_html_saver):
    return os.path.join(__location__, relpath_in_html_saver)

report_fname = 'report.html'
template_fpath = get_real_path('template.html')
static_dirname = 'static'
static_dirpath = get_real_path(static_dirname)
aux_dirname = 'report_html_aux'
scripts_inserted = False
aux_files = [
    'static/jquery-1.8.2.min.js'
    'static/flot/jquery.flot.min.js'
    'static/flot/excanvas.min.js'
    'static/flot/jquery.flot.dashes.js'
    'static/scripts/build_total_report.js'
    'static/scripts/draw_cumulative_plot.js'
    'static/scripts/draw_nx_plot.js'
    'static/scripts/draw_gc_plot.js'
    'static/scripts/utils.js'
    'static/scripts/hsvToRgb.js'
    'static/scripts/draw_genes_plot.js'
    'static/scripts/build_report.js'
    'static/ie_html5.js'
    'static/bootstrap-tooltip-5px-lower.min.js'
    'static/report.css'
    'static/common.css'
]

def init(results_dirpath):
#    shutil.copy(template_fpath,     os.path.join(results_dirpath, report_fname))
    aux_dirpath = os.path.join(results_dirpath, aux_dirname)
    shutil.copytree(static_dirpath, aux_dirpath)
    with open(template_fpath) as template_file:
        html = template_file.read()
        html = html.replace("/" + static_dirname, aux_dirname)
        html = html.replace('{{ glossary }}', open(get_real_path('glossary.json')).read())

        with open(os.path.join(results_dirpath, report_fname), 'w') as f_html:
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
    afp_html = os.path.join(results_dirpath, report_fname)

    if not os.path.isfile(afp_html):
        init(results_dirpath)

    # reading JSON file
    with open(json_fpath) as f_json:
        json_text = f_json.read()
    os.remove(json_fpath)

    # reading html template file
    with open(afp_html) as f_html:
        html_text = f_html.read()

    # substituting template text with json
    html_text = re.sub('{{ ' + keyword + ' }}', json_text, html_text)

    # writing substituted html to final file
    with open(afp_html, 'w') as f_html:
        f_html.write(html_text)


def save_total_report(results_dirpath, min_contig):
    json_fpath = json_saver.save_total_report(results_dirpath, min_contig)
    if json_fpath:
        append(results_dirpath, json_fpath, 'totalReport')
        print '  HTML version to', os.path.join(results_dirpath, report_fname) + '.'


def save_contigs_lengths(results_dirpath, filenames, lists_of_lengths):
    json_fpath = json_saver.save_contigs_lengths(results_dirpath, filenames, lists_of_lengths)
    if json_fpath:
        append(results_dirpath, json_fpath, 'contigsLenghts')


def save_reference_length(results_dirpath, reference_length):
    json_fpath = json_saver.save_reference_length(results_dirpath, reference_length)
    if json_fpath:
        append(results_dirpath, json_fpath, 'referenceLength')


def save_aligned_contigs_lengths(results_dirpath, filenames, lists_of_lengths):
    json_fpath = json_saver.save_aligned_contigs_lengths(results_dirpath, filenames, lists_of_lengths)
    if json_fpath:
        append(results_dirpath, json_fpath, 'alignedContigsLengths')


def save_assembly_lengths(results_dirpath, filenames, assemblies_lengths):
    json_fpath = json_saver.save_assembly_lengths(results_dirpath, filenames, assemblies_lengths)
    if json_fpath:
        append(results_dirpath, json_fpath, 'assembliesLengths')


def save_features_in_contigs(results_dirpath, filenames, feature_name, feature_in_contigs):
    json_fpath = json_saver.save_features_in_contigs(results_dirpath, filenames, feature_name, feature_in_contigs)
    if json_fpath:
        append(results_dirpath, json_fpath, feature_name + 'InContigs')


def save_GC_info(results_dirpath, filenames, lists_of_GC_info):
    json_fpath = json_saver.save_GC_info(results_dirpath, filenames, lists_of_GC_info)
    if json_fpath:
        append(results_dirpath, json_fpath, 'gcInfos')
