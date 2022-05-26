############################################################################
# Copyright (c) 2015-2022 Saint Petersburg State University
# Copyright (c) 2011-2015 Saint Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################

import datetime
import os
from os.path import join
from quast_libs import qutils, qconfig
from quast_libs.ca_utils.misc import ref_labels_by_chromosomes

from quast_libs.log import get_logger
log = get_logger(qconfig.LOGGER_DEFAULT_NAME)

simplejson_error = False
try:
    import json
except ImportError:
    try:
        import simplejson as json
    except ImportError:
        log.warning('Can\'t build html report - please install python-simplejson')
        simplejson_error = True

total_report_fname    = 'report.json'
contigs_lengths_fn    = 'contigs_lengths.json'
ref_length_fn         = 'ref_length.json'
tick_x_fn             = 'tick_x.json'
aligned_contigs_fn    = 'aligned_contigs_lengths.json'
assemblies_lengths_fn = 'assemblies_lengths.json'
in_contigs_suffix_fn  = '_in_contigs.json'
gc_fn                 = 'gc.json'
krona_fn              = 'krona.json'
icarus_fn             = 'icarus.json'

suffix_fn             = '.json'

json_text = ''


def save(fpath, what):
    if simplejson_error:
        return None

    if os.path.exists(fpath):
        os.remove(fpath)

    json_file = open(fpath, 'w')
    json.dump(what, json_file, separators=(',', ':'))
    json_file.close()
    return fpath


def save_as_text(fpath, what):
    json_file = open(fpath, 'w')
    json_file.write(what)
    json_file.close()
    return fpath


def save_empty_report(output_dirpath, min_contig, ref_fpath):
    from quast_libs import reporting
    t = datetime.datetime.now()

    return save(join(output_dirpath, total_report_fname), {
        'date': t.strftime('%d %B %Y, %A, %H:%M:%S'),
        'assembliesNames': [],
        'referenceName': qutils.name_from_fpath(ref_fpath) if ref_fpath else '',
        'order': [],
        'report': None,
        'subreferences': [],
        'subreports': [],
        'minContig': min_contig
    })


def save_total_report(output_dirpath, min_contig, ref_fpath):
    from quast_libs import reporting
    asm_names = [qutils.label_from_fpath(this) for this in reporting.assembly_fpaths]
    report = reporting.table(reporting.Fields.grouped_order)
    subreports = []
    ref_names = []
    if qconfig.is_combined_ref and ref_labels_by_chromosomes:
        ref_names = sorted(list(set([ref for ref in ref_labels_by_chromosomes.values()])))
        subreports = [reporting.table(reporting.Fields.grouped_order, ref_name=ref_name) for ref_name in ref_names]
    t = datetime.datetime.now()

    return save(join(output_dirpath, total_report_fname), {
        'date': t.strftime('%d %B %Y, %A, %H:%M:%S'),
        'assembliesNames': asm_names,
        'referenceName': qutils.name_from_fpath(ref_fpath) if ref_fpath else '',
        'order': [i for i, _ in enumerate(asm_names)],
        'report': report,
        'subreferences': ref_names,
        'subreports': subreports,
        'minContig': min_contig
    })

#def save_old_total_report(output_dir, min_contig):
#    from libs import reporting
#    table = reporting.table()
#
#    def try_convert_back_to_number(str):
#        try:
#            val = int(str)
#        except ValueError:
#            try:
#                val = float(str)
#            except ValueError:
#                val = strref_length_fn
#
#        return val
#
#
#    table = [[try_convert_back_to_number(table[i][j]) for i in xrange(len(table))] for j in xrange(len(table[0]))]
#
#    # TODO: check correctness, not sure that header and result are correct:
#    header = table[0]
#    results = table[1:]
#
#    t = datetime.datetime.now()
#
#    return save(output_dir + total_report_fn, {
#        'date' : t.strftime('%d %B %Y, %A, %H:%M:%S'),
#        'header' : header,
#        'results' : results,
#        'min_contig' : min_contig,
#        })


def save_contigs_lengths(output_dirpath, contigs_fpaths, lists_of_lengths):
    lists_of_lengths = [sorted(list, reverse=True) for list in lists_of_lengths]

    return save(join(output_dirpath, contigs_lengths_fn), {
        'filenames': [qutils.label_from_fpath(label) for label in contigs_fpaths],
        'lists_of_lengths': lists_of_lengths
    })


def save_reference_lengths(output_dirpath, reference_lengths):
    return save(join(output_dirpath, ref_length_fn), {'reflen': reference_lengths})


def save_tick_x(output_dirpath, tick_x):
    return save(join(output_dirpath, tick_x_fn), {'tickX': tick_x})


def save_coord(output_dirpath, coord_x, coord_y, name_coord, contigs_fpaths):
    coord_fn = name_coord + suffix_fn
    return save(join(output_dirpath, coord_fn), {
        'coord_x': coord_x,
        'coord_y': coord_y,
        'filenames': [qutils.label_from_fpath(label) for label in contigs_fpaths]
    })


def save_record(output_dirpath, filename, record):
    return save(join(output_dirpath, filename + suffix_fn), record)


def save_meta_summary(output_dirpath, coord_x, coord_y, name_coord, labels, refs_names):
    coord_fn = 'coord' + name_coord + suffix_fn
    return save(join(output_dirpath, coord_fn), {
        'coord_x': coord_x,
        'coord_y': coord_y,
        'filenames': labels,
        'refnames': refs_names
    })

def save_meta_misassemblies(output_dirpath, coord_x, coord_y, name_coord, labels, refs_names):
    coord_fn = 'coord' + name_coord + suffix_fn
    return save(join(output_dirpath, coord_fn), {
        'coord_x': coord_x,
        'coord_y': coord_y,
        'filenames': labels,
        'refnames': refs_names
    })

def save_assembly_lengths(output_dirpath, contigs_fpaths, assemblies_lengths):
    return save(join(output_dirpath, assemblies_lengths_fn), {
        'filenames': [qutils.label_from_fpath(label) for label in contigs_fpaths],
        'assemblies_lengths': assemblies_lengths
    })


def save_features_in_contigs(output_dirpath, contigs_fpaths, feature_name, features_in_contigs, ref_features_num):
    return save(join(output_dirpath, feature_name + in_contigs_suffix_fn), {
        'filenames': [qutils.label_from_fpath(label) for label in contigs_fpaths],
        feature_name + '_in_contigs': dict((qutils.label_from_fpath(contigs_fpath), feature_amounts)
                                           for (contigs_fpath, feature_amounts) in features_in_contigs.items()),
        'ref_' + feature_name + '_number': ref_features_num,
    })


def save_GC_info(output_dirpath, contigs_fpaths, list_of_GC_distributions, list_of_GC_contigs_distributions, reference_index):
    return save(join(output_dirpath, gc_fn), {
        'filenames': [qutils.label_from_fpath(label) for label in  contigs_fpaths],
        'reference_index': reference_index,
        'list_of_GC_distributions': list_of_GC_distributions,
        'list_of_GC_contigs_distributions': list_of_GC_contigs_distributions,
        'lists_of_gc_info': None,
    })

def save_krona_paths(output_dirpath, krona_fpaths, labels):
    return save(join(output_dirpath, krona_fn), {
        'assemblies': labels,
        'paths': krona_fpaths,
    })

def save_icarus_links(output_dirpath, icarus_links):
    return save(join(output_dirpath, icarus_fn), {
        'links': icarus_links['links'],
        'links_names': icarus_links['links_names'],
    })

def save_icarus_data(output_dirpath, keyword, icarus_data, as_text):
    if as_text:
        return save_as_text(join(output_dirpath, keyword + suffix_fn), icarus_data)
    return save(join(output_dirpath, keyword + suffix_fn), {
        keyword: icarus_data
    })

















