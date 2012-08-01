import re
from libs import json_saver
from libs import qconfig

############################################################################
# Copyright (c) 2011-2012 Saint-Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################

import os

__location__ = os.path.realpath(os.path.join(os.getcwd(), os.path.dirname(__file__)))

def get_real_path(rel_path_in_html_saver):
    return os.path.join(__location__, rel_path_in_html_saver)

fn_report = qconfig.report_basename + '.html'
afp_template = get_real_path('template.html')
scripts_inserted = False


def init(adp_results):
    with open(afp_template) as f_template:
        html = f_template.read()

        for fp_script in ['jquery-1.7.2.min.js',
                          'flot/jquery.flot.min.js',
                          'flot/excanvas.min.js',
                          'flot/jquery.flot.dashes.js',
                          'build_total_report.js',
                          'draw_cumulative_plot.js',
                          'draw_nx_plot.js',
                          'utils.js',
                          'draw_genes_plot.js',
                          'build_report.js', ]:
            with open(get_real_path('js/' + fp_script)) as f:
                html = html.replace(
                    '<script type="text/javascript" src="/static/scripts/' + fp_script + '"></script>',
                    '<script type="text/javascript">\n' + f.read() + '\n\t</script>\n')

        html = html.replace('<link rel="stylesheet" href="/static/bootstrap/css/bootstrap.css"/>',
            '<style rel="stylesheet">\n' + open(get_real_path('css/bootstrap.min.css')).read() + '\n</style>\n\n')

        html = html.replace(
            '<script type="text/javascript" src="http://html5shim.googlecode.com/svn/trunk/html5.js"></script>',
            '<script type="text/javascript" >\n' + open(get_real_path('js/ie_html5.js')).read() + '\n</script>')

        html = html.replace(
            '<script type="text/javascript" src="/static/bootstrap/js/bootstrap-tooltip-5px-lower.js"></script>',
            '<script type="text/javascript" >\n' + open(
                get_real_path('js/bootstrap-tooltip-5pxlower-min.js')).read() + '\n</script>')

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


def save_total_report(adp_results, report_dict):
    print '  HTML version of total report...'
    afp_json = json_saver.save_total_report(adp_results, report_dict)
    append(adp_results, afp_json, 'report')
    print '    Saved to', os.path.join(adp_results, fn_report)


def save_contigs_lengths(adp_results, filenames, lists_of_lengths):
    afp_json = json_saver.save_contigs_lengths(adp_results, filenames, lists_of_lengths)
    append(adp_results, afp_json, 'contigsLenghts')


def save_reference_length(adp_results, reference_length):
    afp_json = json_saver.save_reference_length(adp_results, reference_length)
    append(adp_results, afp_json, 'referenceLength')


def save_aligned_contigs_lengths(adp_results, filenames, lists_of_lengths):
    afp_json = json_saver.save_aligned_contigs_lengths(adp_results, filenames, lists_of_lengths)
    append(adp_results, afp_json, 'alignedContigsLengths')


def save_assembly_lengths(adp_results, filenames, assemblies_lengths):
    afp_json = json_saver.save_assembly_lengths(adp_results, filenames, assemblies_lengths)
    append(adp_results, afp_json, 'assembliesLengths')


def save_contigs(adp_results, filenames, contigs):
    afp_json = json_saver.save_contigs(adp_results, filenames, contigs)
    append(adp_results, afp_json, 'contigs')


def save_genes(adp_results, genes, found):
    afp_json = json_saver.save_genes(adp_results, genes, found)
    append(adp_results, afp_json, 'genes')


def save_operons(adp_results, operons, found):
    afp_json = json_saver.save_operons(adp_results, operons, found)
    append(adp_results, afp_json, 'operons')

