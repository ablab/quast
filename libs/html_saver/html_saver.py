import re
from libs import json_saver
from libs import qconfig

__author__ = 'vladsaveliev'

############################################################################
# Copyright (c) 2011-2012 Saint-Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################

import datetime
import json
import os

report_fn = qconfig.report_basename + '.html'
template_path = 'libs/html_saver/template.html'
scripts_inserted = False


def init(results_dir):
    with open(os.path.abspath(template_path)) as template_file:
        html = template_file.read()

        for scriptpath in ['jquery-1.7.2.min.js',
                           'flot/jquery.flot.min.js',
                           'flot/excanvas.min.js',
                           'build_total_report.js',
                           'draw_comulative_plot.js',
                           'draw_nx_plot.js',
                           'number_to_pretty_string.js',
                           'draw_genes_plot.js',
                           'build_report.js',]:

            with open('libs/html_saver/js/' + scriptpath) as f:
                html = html.replace(
                    '<script type="text/javascript" src="/static/scripts/' + scriptpath + '"></script>',
                    '<script type="text/javascript">\n' + f.read() + '\n\t</script>\n')

        html = html.replace('<link rel="stylesheet" href="/static/bootstrap/css/bootstrap.css"/>',
            '<style rel="stylesheet">\n' + open('libs/html_saver/css/bootstrap.min.css').read() + '\n</style>\n\n')

        html = html.replace('<script type="text/javascript" src="http://html5shim.googlecode.com/svn/trunk/html5.js"></script>',
            '<script type="text/javascript" >\n' + open('libs/html_saver/js/ie_html5.js').read() + '\n</script>')

        html = html.replace('<script type="text/javascript" src="/static/bootstrap/js/bootstrap-tooltip-5px-lower.js"></script>',
            '<script type="text/javascript" >\n' + open('libs/html_saver/js/bootstrap-tooltip-5pxlower-min.js').read() + '\n</script>')

        html = html.replace('{{ glossary }}', open('libs/html_saver/glossary.json').read())

        with open(os.path.join(results_dir, report_fn), 'w') as html_file:
            html_file.write(html)


def append(results_dir, json_fn, keyword):
    html_file_path = os.path.join(results_dir, report_fn)

    if not os.path.isfile(html_file_path):
        init(results_dir)

    # reading JSON file
    with open(json_fn) as json_file:
        json_text = json_file.read()
    os.remove(json_fn)

    # reading html template file
    with open(os.path.abspath(html_file_path)) as html_file:
        html_text = html_file.read()

    # substituting template text with json
    html_text = re.sub('{{ ' + keyword + ' }}', json_text, html_text)

    # writing substituted html to final file
    with open(html_file_path, 'w') as html_file:
        html_file.write(html_text)


def save_total_report(results_dir, report_dict):
    print '  HTML version of total report...'
    json_fn = json_saver.save_total_report(results_dir, report_dict)
    append(results_dir, json_fn, 'report')
    print '    Saved to', os.path.abspath(os.path.join(results_dir, report_fn))

def save_contigs_lengths(results_dir, filenames, lists_of_lengths):
    json_fn = json_saver.save_contigs_lengths(results_dir, filenames, lists_of_lengths)
    append(results_dir, json_fn, 'contigsLenghts')

def save_reference_length(results_dir, reference_length):
    json_fn = json_saver.save_reference_length(results_dir, reference_length)
    append(results_dir, json_fn, 'referenceLength')

def save_aligned_contigs_lengths(results_dir, filenames, lists_of_lengths):
    json_fn = json_saver.save_aligned_contigs_lengths(results_dir, filenames, lists_of_lengths)
    append(results_dir, json_fn, 'alignedContigsLengths')

def save_assembly_lengths(results_dir, filenames, assemblies_lengths):
    json_fn = json_saver.save_assembly_lengths(results_dir, filenames, assemblies_lengths)
    append(results_dir, json_fn, 'assembliesLengths')

def save_contigs(results_dir, filenames, contigs):
    json_fn = json_saver.save_contigs(results_dir, filenames, contigs)
    append(results_dir, json_fn, 'contigs')

def save_genes(results_dir, genes, found):
    json_fn = json_saver.save_genes(results_dir, genes, found)
    append(results_dir, json_fn, 'genes')

def save_operons(results_dir, operons, found):
    json_fn = json_saver.save_operons(results_dir, operons, found)
    append(results_dir, json_fn, 'operons')

