############################################################################
# Copyright (c) 2011-2012 Saint-Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################

import datetime
import json
import os
from libs.genes_parser import Gene

total_report_fn       = '/report.json'
contigs_lengths_fn    = '/contigs_lengths.json'
ref_length_fn         = '/ref_length.json'
aligned_contigs_fn    = '/aligned_contigs_lengths.json'
assemblies_lengths_fn = '/assemblies_lengths.json'
contigs_fn            = '/contigs.json'
genes_fn              = '/genes.json'
operons_fn            = '/operons.json'


def save(filename, what):
    json_file = open(filename, 'w')
    json.dump(what, json_file)
    json_file.close()
    return filename


def save_total_report(output_dir, report_dict):
    results = [row for key, row in report_dict.items() if key != 'header']
    results.sort(key = lambda row: row[0])
    results = [row[1:] for row in results]
    header = report_dict['header'][1:]

    t = datetime.datetime.now()

    return save(output_dir + total_report_fn, {
            'date' : t.strftime('%A, %d %B %Y, %H:%M:%S'),
            'header' : header,
            'results' : results
    })

    #    json.dump({'date' : { 'year' : t.year, 'month' : t.month, 'day' : t.day, 'weekday' : t.weekday(),
    #                          'hour' : t.hour, 'minute' : t.minute, 'second' : t.second },
    #               'header' : report_dict['header'], 'results' : results }, jsn_file)


def save_contigs_lengths(output_dir, filenames, lists_of_lengths):
    lists_of_lengths = [sorted(list, reverse=True) for list in lists_of_lengths]

    return save(output_dir + contigs_lengths_fn, {
        'filenames' : map(os.path.basename, filenames),
        'lists_of_lengths' : lists_of_lengths
    })


def save_reference_length(output_dir, reference_length):
    return save(output_dir + ref_length_fn, { 'reflen' : reference_length })


def save_aligned_contigs_lengths(output_dir, filenames, lists_of_lengths):
    lists_of_lengths = [sorted(list, reverse=True) for list in lists_of_lengths]

    return save(output_dir + aligned_contigs_fn, {
        'filenames' : map(os.path.basename, filenames),
        'lists_of_lengths' : lists_of_lengths
    })


def save_assembly_lengths(output_dir, filenames, assemblies_lengths):
    return save(output_dir + assemblies_lengths_fn, {
        'filenames' : map(os.path.basename, filenames),
        'assemblies_lengths' : assemblies_lengths
    })


def save_contigs(output_dir, filenames, contigs):
    return save(output_dir + contigs_fn, {
        'filenames' : map(os.path.basename, filenames),
        'contigs' : { os.path.basename(fn) : blocks for fn, blocks in contigs.items() },
    })


def save_genes(output_dir, genes, found):
    genes = [[g.start,g.end] for g in genes]
    return save(output_dir + genes_fn, {
        'genes' : genes,
        'found' : found,
    })


def save_operons(output_dir, operons, found):
    operons = [[g.start, g.end] for g in operons]
    return save(output_dir + operons_fn, {
        'operons' : operons,
        'found' : found,
    })
