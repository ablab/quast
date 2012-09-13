from __future__ import with_statement
import re
from libs import json_saver

############################################################################
# Copyright (c) 2011-2012 Saint-Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################

import os

__location__ = os.path.realpath(os.path.join(os.getcwd(), os.path.dirname(__file__)))

def get_real_path(rel_path_in_html_saver):
    return os.path.join(__location__, rel_path_in_html_saver)

fn_report = 'report.html'
afp_template = get_real_path('template.html')
scripts_inserted = False


def init(adp_results):
    with open(afp_template) as f_template:
        html = f_template.read()

        for fp_script in ['jquery-1.7.2.min.js',
                          'flot/jquery.flot.min.js',
                          'flot/excanvas.min.js',
                          'flot/jquery.flot.dashes.js',
                          'report-scripts/build_total_report.js',
                          'report-scripts/draw_cumulative_plot.js',
                          'report-scripts/draw_nx_plot.js',
                          'report-scripts/draw_gc_plot.js',
                          'report-scripts/utils.js',
                          'report-scripts/draw_genes_plot.js',
                          'report-scripts/build_report.js', ]:
            with open(get_real_path(fp_script)) as f:
                html = html.replace(
                    '<script type="text/javascript" src="' + fp_script + '"></script>',
                    '<script type="text/javascript">\n' + f.read() + '\n\t</script>\n')

        html = html.replace('<link rel="stylesheet" href="bootstrap/bootstrap.min.css"/>',
            '<style rel="stylesheet">\n' + open(get_real_path('bootstrap/bootstrap.min.css')).read() + '\n</style>\n\n')

        html = html.replace(
            '<script type="text/javascript" src="ie_html5.js"></script>',
            '<script type="text/javascript" >\n' + open(get_real_path('ie_html5.js')).read() + '\n</script>')

        html = html.replace(
            '<script type="text/javascript" src="bootstrap/bootstrap-tooltip-5px-lower.js"></script>',
            '<script type="text/javascript" >\n' + open(
                get_real_path('bootstrap/bootstrap-tooltip-5pxlower-min.js')).read() + '\n</script>')

        html = html.replace('{{ glossary }}', open(get_real_path('glossary.json')).read())

        with open(os.path.join(adp_results, fn_report), 'w') as f_html:
            f_html.write(html)


def append(adp_results, afp_json, keyword):
    afp_html = os.path.join(adp_results, fn_report)

    if not os.path.isfile(afp_html):
        init(adp_results)

    # reading JSON file
    with open(afp_json) as f_json:
        json_text = f_json.read()
    os.remove(afp_json)

    # reading html template file
    with open(afp_html) as f_html:
        html_text = f_html.read()

    # substituting template text with json
    html_text = re.sub('{{ ' + keyword + ' }}', json_text, html_text)

    # writing substituted html to final file
    with open(afp_html, 'w') as f_html:
        f_html.write(html_text)


def save_total_report(adp_results, min_contig):
    afp_json = json_saver.save_total_report(adp_results, min_contig)
    if afp_json:
        print '  HTML version of total report...'
        append(adp_results, afp_json, 'report')
        print '    Saved to', os.path.join(adp_results, fn_report)


def save_contigs_lengths(adp_results, filenames, lists_of_lengths):
    afp_json = json_saver.save_contigs_lengths(adp_results, filenames, lists_of_lengths)
    if afp_json:
        append(adp_results, afp_json, 'contigsLenghts')


def save_reference_length(adp_results, reference_length):
    afp_json = json_saver.save_reference_length(adp_results, reference_length)
    if afp_json:
        append(adp_results, afp_json, 'referenceLength')


def save_aligned_contigs_lengths(adp_results, filenames, lists_of_lengths):
    afp_json = json_saver.save_aligned_contigs_lengths(adp_results, filenames, lists_of_lengths)
    if afp_json:
        append(adp_results, afp_json, 'alignedContigsLengths')


def save_assembly_lengths(adp_results, filenames, assemblies_lengths):
    afp_json = json_saver.save_assembly_lengths(adp_results, filenames, assemblies_lengths)
    if afp_json:
        append(adp_results, afp_json, 'assembliesLengths')


def save_contigs(adp_results, filenames, contigs):
    afp_json = json_saver.save_contigs(adp_results, filenames, contigs)
    if afp_json:
        append(adp_results, afp_json, 'contigs')


def save_GC_info(adp_results, filenames, lists_of_GC_info):
    afp_json = json_saver.save_GC_info(adp_results, filenames, lists_of_GC_info)
    if afp_json:
        append(adp_results, afp_json, 'gcInfos')


def save_genes(adp_results, genes, found):
    afp_json = json_saver.save_genes(adp_results, genes, found)
    if afp_json:
        append(adp_results, afp_json, 'genes')


def save_operons(adp_results, operons, found):
    afp_json = json_saver.save_operons(adp_results, operons, found)
    if afp_json:
        append(adp_results, afp_json, 'operons')

