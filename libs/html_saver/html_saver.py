############################################################################
# Copyright (c) 2015-2016 Saint Petersburg State University
# Copyright (c) 2011-2015 Saint Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################

from __future__ import with_statement
import os
import shutil
import re
from collections import defaultdict

from libs import reporting, qconfig, qutils
from libs.html_saver import json_saver

from libs.log import get_logger
log = get_logger(qconfig.LOGGER_DEFAULT_NAME)


def get_real_path(relpath_in_html_saver):
    return os.path.join(qconfig.LIBS_LOCATION, 'html_saver', relpath_in_html_saver)

html_colors = [
    '#FF0000',  #red
    '#0000FF',  #blue
    '#008000',  #green
    '#A22DCC',  #fushua
    '#FFA500',  #orange
    '#800000',  #maroon
    '#00CCCC',  #aqua
    '#B2DF8A',  #light green
    '#333300',  #dark green
    '#CCCC00',  #olive
    '#000080',  #navy
    '#008080',  #team
    '#00FF00',  #lime
]

scripts_inserted = False

report_fname = qconfig.report_prefix + ".html"

template_fpath = get_real_path('template.html')

static_dirname = 'static'
static_dirpath = get_real_path(static_dirname)

aux_dirname = qconfig.html_aux_dir
aux_files = [
    'static/jquery-1.8.2.min.js',
    'static/flot/jquery.flot.min.js',
    'static/flot/excanvas.min.js',
    'static/flot/jquery.flot.dashes.js',
    'static/scripts/draw_cumulative_plot.js',
    'static/scripts/draw_nx_plot.js',
    'static/scripts/draw_gc_plot.js',
    'static/scripts/utils.js',
    'static/scripts/hsvToRgb.js',
    'static/scripts/draw_genes_plot.js',
    'static/dragtable.js',
    'static/ie_html5.js',
    'static/bootstrap/bootstrap-tooltip-5px-lower.min.js',
    'static/bootstrap/bootstrap.min.js',
    'static/bootstrap/bootstrap-tooltip-vlad.js',
    'static/scripts/build_report_common.js',
    'static/scripts/build_total_report_common.js',
]

aux_meta_files = ['static/flot/jquery.flot.tickrotor.js', 'static/flot/jquery.flot.stack.js', 'static/scripts/draw_metasummary_plot.js', 'static/scripts/draw_meta_misassembl_plot.js',]
jquery_fpath = 'static/jquery-1.8.2.min.js'
jquery_ui_fpath = 'static/jquery-ui.js'

def js_html(script_rel_path):
    if qconfig.debug:
        return '<script type="text/javascript" src="' + get_real_path(script_rel_path) + '"/></script>\n'
    else:
        return '<script type="text/javascript">\n' + open(get_real_path(script_rel_path)).read() + '\n</script>\n'


def css_html(css_rel_path):
    if qconfig.debug:
        return '<link rel="stylesheet" href="' + get_real_path(css_rel_path) + '"/>\n'
    else:
        return '<style rel="stylesheet">\n' + open(get_real_path(css_rel_path)).read() + '\n</style>\n'


def init(html_fpath, is_meta=False):
    with open(template_fpath) as template_file:
        html = template_file.read()

        script_texts = []
        for aux_f_rel_path in aux_files:
            if qconfig.no_gc and "draw_gc_plot" in aux_f_rel_path:
                continue
            script_texts.append(js_html(aux_f_rel_path))
        html = html.replace('{{ allscripts }}', '\n'.join(script_texts))
        if is_meta:
            html = html.replace('{{ buildreport }}', js_html('static/scripts/build_report_meta.js'))
            html = html.replace('{{ buildtotalreport }}', js_html('static/scripts/build_total_report_meta.js'))
            html = html.replace('{{ metascripts }}', '\n'.join([js_html(aux_meta_file) for aux_meta_file in aux_meta_files]))
        else:
            html = html.replace('{{ buildreport }}', js_html('static/scripts/build_report.js'))
            html = html.replace('{{ buildtotalreport }}', js_html('static/scripts/build_total_report.js'))
            html = html.replace('{{ metascripts }}', '')
        html = html.replace('{{ bootstrap }}', css_html('static/bootstrap/bootstrap.min.css'))
        html = html.replace('{{ common }}', css_html('static/common.css'))
        html = html.replace('{{ report }}', css_html('static/report.css'))
        html = html.replace('{{ glossary }}', open(get_real_path('glossary.json')).read())
        if os.path.exists(html_fpath):
            os.remove(html_fpath)
        with open(html_fpath, 'w') as f_html:
            f_html.write(html)


def init_icarus(template_f, html_fpath):
    with open(template_f) as template_file:
        html = template_file.read()

        html = html.replace('{{ d3 }}', js_html('static/d3.js'))
        html = html.replace('{{ jquery }}', js_html(jquery_fpath))
        html = html.replace('{{ jquery_ui }}', js_html(jquery_ui_fpath))
        html = html.replace('{{ build_icarus }}', js_html('static/scripts/build_icarus.js'))
        html = html.replace('{{ display_icarus }}', js_html('static/scripts/display_icarus.js'))
        html = html.replace('{{ icarus_interface }}', js_html('static/scripts/icarus_interface.js'))
        html = html.replace('{{ icarus_utils }}', js_html('static/scripts/icarus_utils.js'))
        html = html.replace('{{ bootstrap }}', css_html('static/bootstrap/bootstrap.css'))
        html = html.replace('{{ jquery_css }}', css_html('static/jquery.css'))
        html = html.replace('{{ common }}', css_html('static/common.css'))
        html = html.replace('{{ icarus }}', css_html('static/icarus.css'))
        with open(html_fpath, 'w') as f_html:
            f_html.write(html)


def insert_text_icarus(text_to_insert, keyword, html_fpath):
    if not os.path.isfile(html_fpath):
        return None

    # reading html template file
    with open(html_fpath) as f_html:
        html_text = f_html.read()

    # substituting template text with json
    html_text = re.sub('{{ ' + keyword + ' }}', text_to_insert, html_text)

    # writing substituted html to final file
    with open(html_fpath, 'w') as f_html:
        f_html.write(html_text)
    return


def trim_ref_name(ref_name):
    if len(ref_name) > 50:
        ref_hash = hash(ref_name) & 0xffffffff
        ref_name = ref_name[:50] + '_' + str(ref_hash)
    return ref_name


def clean_html(html_fpath):
    with open(html_fpath) as f_html:
        html_text = f_html.read()
    html_text = re.sub(r'{{(\s+\S+\s+)}}', '', html_text)
    with open(html_fpath, 'w') as f_html:
        f_html.write(html_text)


def append(results_dirpath, json_fpath, keyword, html_fpath=None):
    if html_fpath is None:
        html_fpath = os.path.join(results_dirpath, report_fname)

    if not os.path.isfile(html_fpath):
        init(html_fpath)

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
    init(html_fpath, is_meta=True)
    return html_fpath


def create_meta_report(results_dirpath, json_texts):
    html_fpath = os.path.join(results_dirpath, report_fname)
    if not os.path.isfile(html_fpath):
        init(html_fpath, is_meta=True)

    from libs import search_references_meta
    taxons_for_krona = search_references_meta.taxons_for_krona
    meta_log = get_logger(qconfig.LOGGER_META_NAME)
    if taxons_for_krona:
        create_krona_charts(taxons_for_krona, meta_log, results_dirpath, json_texts)

    # reading html template file
    with open(html_fpath) as f_html:
        html_text = f_html.read()
    keyword = 'totalReport'
    html_text = re.sub('{{ ' + keyword + ' }}', '[' + ','.join(json_texts) + ']', html_text)
    html_text = re.sub(r'{{(\s+\S+\s+)}}', '{}', html_text)
    with open(html_fpath, 'w') as f_html:
        f_html.write(html_text)
    meta_log.main_info('  Extended version of HTML-report (for all references and assemblies) is saved to ' + html_fpath)


def save_total_report(results_dirpath, min_contig, ref_fpath):
    json_fpath = json_saver.save_total_report(results_dirpath, min_contig, ref_fpath)
    if json_fpath:
        json_saver.json_text = append(results_dirpath, json_fpath, 'totalReport')
        log.info('  HTML version (interactive tables and plots) saved to ' + os.path.join(results_dirpath, report_fname))


def create_meta_icarus(results_dirpath, ref_names):
    icarus_dirpath = os.path.join(results_dirpath, qconfig.icarus_dirname)
    icarus_links = defaultdict(list)
    if not os.path.isdir(icarus_dirpath):
        os.mkdir(icarus_dirpath)
    icarus_links["links"].append(qconfig.icarus_html_fname)
    icarus_links["links_names"].append(qconfig.icarus_link)
    contig_size_fpath = os.path.join(results_dirpath, qconfig.combined_output_name, qconfig.icarus_dirname, qconfig.contig_size_viewer_fname)
    contig_size_top_fpath = os.path.join(results_dirpath, qconfig.icarus_dirname, qconfig.contig_size_viewer_fname)
    shutil.copy(contig_size_fpath, contig_size_top_fpath)
    for index, ref in enumerate(ref_names):
        html_name = trim_ref_name(ref)
        if len(ref_names) == 1:
            html_name = qconfig.one_alignment_viewer_name
        icarus_ref_fpath = os.path.join(results_dirpath, qconfig.combined_output_name, qconfig.icarus_dirname, html_name + '.html')
        icarus_top_ref_fpath = os.path.join(results_dirpath, qconfig.icarus_dirname, html_name + '.html')
        shutil.copy(icarus_ref_fpath, icarus_top_ref_fpath)
    icarus_menu_fpath = os.path.join(results_dirpath, qconfig.combined_output_name, qconfig.icarus_html_fname)
    icarus_menu_top_fpath = os.path.join(results_dirpath, qconfig.icarus_html_fname)
    with open(icarus_menu_fpath, 'r') as template:
        with open(icarus_menu_top_fpath, 'w') as result:
            skipping_tr = False
            for line in template:
                if line.find('</tr>') != -1:
                    skipping_tr = False
                if line.find('<a href="icarus_viewers') != -1 and 'QUAST report' not in line:
                    ref_name = re.findall('<a.*>(.*)<\/a>', line)[0]
                    ref_name = ref_name.replace(' ', '_')
                    if ref_name not in ref_names:
                        skipping_tr = True
                if not skipping_tr:
                    result.write(line)
    save_icarus_links(results_dirpath, icarus_links)
    return icarus_menu_top_fpath


def create_krona_charts(taxons_for_krona, meta_log, results_dirpath, json_texts):
    meta_log.info('  Drawing interactive Krona plots...')
    krona_dirpath = os.path.join(qconfig.LIBS_LOCATION, 'kronatools')
    krona_res_dirpath = os.path.join(results_dirpath, qconfig.krona_dirname)
    simplejson_error = False
    try:
        import json
    except ImportError:
        try:
            import simplejson as json
        except ImportError:
            meta_log.warning('Can\'t draw Krona charts - please install python-simplejson')
            simplejson_error = True
    if not simplejson_error:
        if not os.path.isdir(krona_res_dirpath):
            os.mkdir(krona_res_dirpath)
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
            lengths = []
            report = json_data['report']
            for section in report:
                for metric in section[1]:
                    if metric['metricName'] == reporting.Fields.TOTALLEN and not lengths:
                        lengths = metric['values']
                    elif metric['metricName'] == reporting.Fields.TOTAL_ALIGNED_LEN:
                        lengths = metric['values']
                        break
            if not lengths:
                continue
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
        krona_fpaths = []
        krona_log_fpath = os.path.join(krona_res_dirpath, 'krona.log')
        krona_err_fpath = os.path.join(krona_res_dirpath, 'krona.err')
        open(krona_log_fpath, 'w').close()
        open(krona_err_fpath, 'w').close()
        for index, name in enumerate(assemblies):
            krona_fpath = os.path.join(krona_res_dirpath, name + '_taxonomy_chart.html')
            krona_txt_fpath = os.path.join(krona_res_dirpath, name + krona_txt_ext)
            return_code = qutils.call_subprocess(
                ['perl', '-I', krona_dirpath + '/lib', krona_dirpath + '/scripts/ImportText.pl', krona_txt_fpath, '-o', krona_fpath],
                stdout=open(krona_log_fpath, 'a'), stderr=open(krona_err_fpath, 'a'))
            if return_code != 0:
                meta_log.warning('Error occurred while Krona was processing assembly ' + name +
                                 '. See Krona error log for details: %s' % krona_err_fpath)
            else:
                krona_fpaths.append(os.path.join(qconfig.krona_dirname, name + '_taxonomy_chart.html'))
                meta_log.main_info('  Krona chart for ' + name + ' is saved to ' + krona_fpath)
            os.remove(krona_txt_fpath)
        if len(krona_fpaths) > 1:
            name = 'summary'
            krona_fpath = os.path.join(krona_res_dirpath, name + '_taxonomy_chart.html')
            return_code = qutils.call_subprocess(
                            ['perl', '-I', krona_dirpath + '/lib', krona_dirpath + '/scripts/ImportText.pl', krona_common_fpath, '-o', krona_fpath],
                            stdout=open(krona_log_fpath, 'a'), stderr=open(krona_err_fpath, 'a'))
            if return_code != 0:
                meta_log.warning('Error occurred while Krona was building summary chart. '
                                 'See Krona error log for details: %s' % krona_err_fpath)
            else:
                meta_log.main_info('  Summary Krona chart is saved to ' + krona_fpath)
                krona_fpaths.append(os.path.join(qconfig.krona_dirname, name + '_taxonomy_chart.html'))  # extra fpath!
        os.remove(krona_common_fpath)
        save_krona_paths(results_dirpath, krona_fpaths, assemblies)


def save_coord(results_dirpath, coord_x, coord_y, name_coord, contigs_fpaths):  # coordinates for Nx, NAx, NGx, NGAX
    json_fpath = json_saver.save_coord(results_dirpath, coord_x, coord_y, name_coord, contigs_fpaths)
    if json_fpath:
        append(results_dirpath, json_fpath, name_coord)


def save_colors(results_dirpath, contigs_fpaths, dict_colors, meta=False):  # coordinates for Nx, NAx, NGx, NGAX
    from libs import plotter
    if meta:
        html_fpath = os.path.join(results_dirpath, report_fname)
        with open(html_fpath) as f_html:
            html_text = f_html.read()
        html_text = re.sub('{{ ' + 'colors' + ' }}', 'standard_colors', html_text)
        with open(html_fpath, 'w') as f_html:
            f_html.write(html_text)
    else:
        colors_and_ls = [dict_colors[qutils.label_from_fpath(contigs_fpath)] for contigs_fpath in contigs_fpaths]
        colors = [color_and_ls[0] for color_and_ls in colors_and_ls]
        colors_for_html = [html_colors[plotter.colors.index(color)] for color in colors]
        json_fpath = json_saver.save_colors(results_dirpath, colors_for_html)
        append(results_dirpath, json_fpath, 'colors')


def save_meta_summary(html_fpath, results_dirpath, coord_x, coord_y, name_coord, labels, refs):
    name_coord = name_coord.replace('_(%)', '')
    name_coord = name_coord.replace('#', 'num')
    json_fpath = json_saver.save_meta_summary(results_dirpath, coord_x, coord_y, name_coord, labels, refs)
    if json_fpath:
        append(results_dirpath, json_fpath, name_coord, html_fpath)


def save_meta_misassemblies(html_fpath, results_dirpath, coords, labels, refs):
    name_coord = 'allMisassemblies'
    coords_x = [coord[0] if coord else None for coord in coords]
    coords_y = [coord[1] if coord else None for coord in coords]
    json_fpath = json_saver.save_meta_misassemblies(results_dirpath, coords_x, coords_y, name_coord, labels, refs)
    if json_fpath:
        append(results_dirpath, json_fpath, name_coord, html_fpath)


def save_reference_lengths(results_dirpath, reference_lengths):
    json_fpath = json_saver.save_reference_lengths(results_dirpath, reference_lengths)
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


def save_GC_info(results_dirpath, contigs_fpaths, list_of_GC_distributions, reference_index):
    json_fpath = json_saver.save_GC_info(results_dirpath, contigs_fpaths, list_of_GC_distributions, reference_index)
    if json_fpath:
        append(results_dirpath, json_fpath, 'gcInfos')


def save_krona_paths(results_dirpath, krona_fpaths, labels):
    json_fpath = json_saver.save_krona_paths(results_dirpath, krona_fpaths, labels)
    if json_fpath:
        append(results_dirpath, json_fpath, 'krona')


def save_icarus_links(results_dirpath, icarus_links):
    json_fpath = json_saver.save_icarus_links(results_dirpath, icarus_links)
    if json_fpath:
        append(results_dirpath, json_fpath, 'icarus')


def save_icarus_data(results_dirpath, icarus_data, keyword, html_fpath):
    if results_dirpath:
        json_fpath = json_saver.save_icarus_data(results_dirpath, keyword, icarus_data)
    insert_text_icarus(icarus_data, keyword, html_fpath)
