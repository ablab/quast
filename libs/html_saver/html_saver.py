############################################################################
# Copyright (c) 2015 Saint Petersburg State University
# Copyright (c) 2011-2015 Saint Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################

from __future__ import with_statement
import os
import shutil
import re
from libs import qconfig, qutils
from libs.html_saver import json_saver

from libs.log import get_logger
log = get_logger(qconfig.LOGGER_DEFAULT_NAME)


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
    'img/icon_plot.png',
    'bootstrap/bootstrap-tooltip-5px-lower.min.js',
    'bootstrap/bootstrap.min.css',
    'bootstrap/bootstrap.min.js',
    'bootstrap/bootstrap-tooltip-vlad.js',
    'report.css',
    'common.css',
    'scripts/build_report_common.js',
    'scripts/build_total_report_common.js',
]

aux_simple_files = ['scripts/build_report.js', 'scripts/build_total_report.js']
aux_meta_files = ['scripts/build_report_meta.js', 'scripts/build_total_report_meta.js',
    'flot/jquery.flot.tickrotor.js', 'flot/jquery.flot.stack.js', 'scripts/draw_metasummary_plot.js', 'scripts/draw_meta_misassembl_plot.js',]
aux_meta_scripts = ['<script type="text/javascript" src="/static/scripts/draw_metasummary_plot.js"></script>',
                    '<script type="text/javascript" src="/static/scripts/draw_meta_misassembl_plot.js"></script>',
                    '<script type="text/javascript" src="/static/flot/jquery.flot.tickrotor.js"></script>',
                    '<script type="text/javascript" src="/static/flot/jquery.flot.stack.js"></script>']


def init(results_dirpath, is_meta=False):
    aux_dirpath = os.path.join(results_dirpath, aux_dirname)
    if os.path.isdir(aux_dirpath):
        shutil.rmtree(aux_dirpath)
    os.mkdir(aux_dirpath)

    for aux_f_relpath in aux_files + (aux_meta_files if is_meta else aux_simple_files):
        src_fpath = os.path.join(static_dirpath, aux_f_relpath)
        dst_fpath = os.path.join(aux_dirpath, aux_f_relpath)

        if not os.path.exists(os.path.dirname(dst_fpath)):
            os.makedirs(os.path.dirname(dst_fpath))

        if not os.path.exists(dst_fpath):
            shutil.copyfile(src_fpath, dst_fpath)

    with open(template_fpath) as template_file:
        html = template_file.read()
        if is_meta:
            html = html.replace('{{ buildreport }}', 'scripts/build_report_meta.js')
            html = html.replace('{{ buildtotalreport }}', 'scripts/build_total_report_meta.js')
            html = html.replace('{{ metascripts }}', '\n'.join(aux_meta_scripts))
        else:
            html = html.replace('{{ buildreport }}', 'scripts/build_report.js')
            html = html.replace('{{ buildtotalreport }}', 'scripts/build_total_report.js')
            html = html.replace('{{ metascripts }}', '')
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


def init_meta_report(results_dirpath):
    html_fpath = os.path.join(results_dirpath, report_fname)
    init(results_dirpath, True)


def create_meta_report(results_dirpath, json_texts):
    html_fpath = os.path.join(results_dirpath, report_fname)
    if not os.path.isfile(html_fpath):
        init(results_dirpath, True)

    from libs import search_references_meta
    taxons_for_krona = search_references_meta.taxons_for_krona
    meta_log = get_logger(qconfig.LOGGER_META_NAME)
    if taxons_for_krona:
        meta_log.info('  Drawing interactive Krona plots...')
        krona_dirpath = os.path.join(qconfig.LIBS_LOCATION, 'kronatools')
        krona_res_dirpath = os.path.join(results_dirpath, 'Krona')
        if not os.path.isdir(krona_res_dirpath):
            os.mkdir(krona_res_dirpath)
        import json
        json_data = json.loads(json_texts[0])
        assemblies = json_data['assembliesNames']
        krona_txt_ext = '_taxonomy.txt'
        krona_common_fpath = os.path.join(krona_res_dirpath, 'overall' + krona_txt_ext)
        krona_common_file = open(krona_common_fpath, 'w')
        for index, name in enumerate(assemblies):
            krona_file = open(os.path.join(krona_res_dirpath, name + krona_txt_ext), 'w')
            krona_file.close()
        for json_text in json_texts[1:]:
            json_data = json.loads(json_text)
            ref = json_data['referenceName']
            report = json_data['report'][0]
            for metric in report[1]:
                if metric['metricName'] == 'Total length':
                    lengths = metric['values']
                    break
            cur_assemblies = json_data['assembliesNames']
            for index, name in enumerate(cur_assemblies):
                krona_fpath = os.path.join(krona_res_dirpath, name + krona_txt_ext)
                with open(krona_fpath, 'a') as f_krona:
                    if ref in taxons_for_krona:
                        f_krona.write(str(lengths[index]) + '\t' + taxons_for_krona[ref] + '\n')
                    else:
                        f_krona.write(str(lengths[index]) + '\n')
            if ref in taxons_for_krona:
                krona_common_file.write(str(sum(lengths)) + '\t' + taxons_for_krona[ref] + '\n')
            else:
                krona_common_file.write(str(sum(lengths)) + '\n')
        krona_common_file.close()
        krona_fpaths=[]
        for index, name in enumerate(assemblies):
            krona_fpath = os.path.join(krona_res_dirpath, name + '_taxonomy_chart.html')
            krona_txt_fpath = os.path.join(krona_res_dirpath, name + krona_txt_ext)
            qutils.call_subprocess(
            ['perl', '-I', krona_dirpath + '/lib', krona_dirpath + '/scripts/ImportText.pl', krona_txt_fpath, '-o', krona_fpath, '-a'],
            stdout=open(os.devnull, 'w'), stderr=open(os.devnull, 'w'))
            krona_fpaths.append(os.path.join('Krona', name + '_taxonomy_chart.html'))
            meta_log.info('  Krona plot for ' + name + ' is saved to ' + krona_fpath)
            os.remove(krona_txt_fpath)
        if len(assemblies) > 1:
            krona_fpath = os.path.join(krona_res_dirpath, 'summary_taxonomy_chart.html')
            qutils.call_subprocess(
                ['perl', '-I', krona_dirpath + '/lib', krona_dirpath + '/scripts/ImportText.pl', krona_common_fpath, '-o', krona_fpath, '-a'],
                stdout=open(os.devnull, 'w'), stderr=open(os.devnull, 'w'))
            meta_log.info('  Summary Krona plot is saved to ' + krona_fpath)
        os.remove(krona_common_fpath)
        save_krona_paths(results_dirpath, krona_fpaths, assemblies)

    # reading html template file
    with open(html_fpath) as f_html:
        html_text = f_html.read()
    keyword = 'totalReport'
    html_text = re.sub('{{ ' + keyword + ' }}', '[' + ','.join(json_texts) + ']', html_text)
    html_text = re.sub(r'{{(\s+\S+\s+)}}', '{}', html_text)
    with open(html_fpath, 'w') as f_html:
        f_html.write(html_text)
    meta_log.info('  Extended version of HTML-report (for all references and assemblies) is saved to ' + html_fpath)


def save_total_report(results_dirpath, min_contig, ref_fpath):
    json_fpath = json_saver.save_total_report(results_dirpath, min_contig, ref_fpath)
    if json_fpath:
        json_saver.json_text = append(results_dirpath, json_fpath, 'totalReport')
        log.info('  HTML version (interactive tables and plots) saved to ' + os.path.join(results_dirpath, report_fname))


def save_coord(results_dirpath, coord_x, coord_y, name_coord, contigs_fpaths):  # coordinates for Nx, NAx, NGx, NGAX
    json_fpath = json_saver.save_coord(results_dirpath, coord_x, coord_y, name_coord, contigs_fpaths)
    if json_fpath:
        append(results_dirpath, json_fpath, name_coord)


def save_meta_summary(results_dirpath, coord_x, coord_y, name_coord, labels, refs):  # coordinates for Nx, NAx, NGx, NGAX
    name_coord = name_coord.replace('_(%)', '')
    name_coord = name_coord.replace('#', 'num')
    json_fpath = json_saver.save_meta_summary(results_dirpath, coord_x, coord_y, name_coord, labels, refs)
    if json_fpath:
        append(results_dirpath, json_fpath, name_coord)


def save_meta_misassemblies(results_dirpath, coords, labels, refs):  # coordinates for Nx, NAx, NGx, NGAX
    name_coord = 'allMisassemblies'
    coords_x = [coord[0] if coord else None for coord in coords]
    coords_y = [coord[1] if coord else None for coord in coords]
    json_fpath = json_saver.save_meta_misassemblies(results_dirpath, coords_x, coords_y, name_coord, labels, refs)
    if json_fpath:
        append(results_dirpath, json_fpath, name_coord)


def save_reference_length(results_dirpath, reference_length):
    json_fpath = json_saver.save_reference_length(results_dirpath, reference_length)
    if json_fpath:
        append(results_dirpath, json_fpath, 'referenceLength')


def save_tick_x(results_dirpath, tick_x):
    json_fpath = json_saver.save_tick_x(results_dirpath, tick_x)
    if json_fpath:
        append(results_dirpath, json_fpath, 'tickX')


def save_contigs_lengths(results_dirpath, contigs_fpaths, lists_of_lengths):
    json_fpath = json_saver.save_contigs_lengths(results_dirpath, contigs_fpaths, lists_of_lengths)
    if json_fpath:
        append(results_dirpath, json_fpath, 'contigsLengths')


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

def save_krona_paths(results_dirpath, krona_fpaths, labels):
    json_fpath = json_saver.save_krona_paths(results_dirpath,krona_fpaths, labels)
    if json_fpath:
        append(results_dirpath, json_fpath, 'krona')