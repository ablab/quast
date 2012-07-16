import re
from libs import json_saver

__author__ = 'vladsaveliev'

############################################################################
# Copyright (c) 2011-2012 Saint-Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################

import datetime
import json
import os

report_fn = 'report.html'
template_path = 'libs/html_saver/report.html'
scripts_inserted = False


def init(results_dir):
    # reading html template file
    template_file = open(os.path.abspath(template_path))
    template_text = template_file.read()

    # reading script files
    jquery                  = open('libs/html_saver/js/jquery-1.7.2.min.js').read()
    flot                    = open('libs/html_saver/js/flot/jquery.flot.min.js').read()
    excanvas                = open('libs/html_saver/js/flot/excanvas.min.js').read()
    build_total_report      = open('libs/html_saver/js/build_total_report.js').read()
    draw_commulative_plot   = open('libs/html_saver/js/draw_commulative_plot.js').read()
    draw_nx_plot            = open('libs/html_saver/js/draw_nx_plot.js').read()
    number_to_pretty_string = open('libs/html_saver/js/number_to_pretty_string.js').read()

    # substituting
    final_html = template_text.replace(
        '<script type="text/javascript" src="js/jquery-1.7.2.min.js"></script>',
        '<script type="text/javascript">\n' + jquery + '\n\t</script>')\
    .replace(
        '<script type="text/javascript" src="js/flot/jquery.flot.js"></script>',
        '<script type="text/javascript">\n' + flot + '\n\t</script>')\
    .replace(
        '<!-[if lte IE 8]><script type="text/javascript" src="js/flot/excanvas.min.js"></script><![endif]->',
        '<!-[if lte IE 8]><script type="text/javascript">' + excanvas + '\t\n</script><![endif]->')\
    .replace(
        '<script type="text/javascript" src="js/build_total_report.js"></script>',
        '<script type="text/javascript">\n' + draw_commulative_plot + '\n\t</script>')\
    .replace(
        '<script type="text/javascript" src="js/draw_commulative_plot.js"></script>',
        '<script type="text/javascript">\n' + build_total_report + '\n\t</script>')\
    .replace(
        '<script type="text/javascript" src="js/draw_nx_plot.js"></script>',
        '<script type="text/javascript">\n' + draw_nx_plot + '\n\t</script>')\
    .replace(
        '<script type="text/javascript" src="js/number_to_pretty_string.js"></script>',
        '<script type="text/javascript">\n' + number_to_pretty_string + '\n\t</script>')

    # writing substituted html to final file
    f = open(results_dir + '/' + report_fn, 'w')
    f.write(final_html)

    # cleaning and closing
    f.close()
    template_file.close()


def append(results_dir, json_fn, keyword):
    html_file_path = results_dir + '/' + report_fn

    if not os.path.isfile(html_file_path):
        init(results_dir)

    json_file = open(json_fn)
    json_text = json_file.read()
    os.remove(json_fn)

    # reading html template file
    html_file = open(os.path.abspath(html_file_path))
    html_text = html_file.read()
    html_file.close()

    # substituting template text with json
    html_text = re.sub('{{ ' + keyword + ' }}', json_text, html_text)

    # writing substituted html to final file
    html_file = open(html_file_path, 'w')
    html_file.write(html_text)

    # cleaning and closing
    html_file.close()



def save_total_report(results_dir, report_dict):
    json_fn = json_saver.save_total_report(results_dir, report_dict)
    append(results_dir, json_fn, 'report')


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

