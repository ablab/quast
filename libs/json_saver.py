############################################################################
# Copyright (c) 2011-2012 Saint-Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################

import datetime
import os

simplejson_error = False
try:
    import json
except:
    try:
        import simplejson as json
    except:
        print 'Warning! Can\'t build html report - please install python-simplejson'
        simplejson_error = True

total_report_fn       = '/report.json'
contigs_lengths_fn    = '/contigs_lengths.json'
ref_length_fn         = '/ref_length.json'
aligned_contigs_fn    = '/aligned_contigs_lengths.json'
assemblies_lengths_fn = '/assemblies_lengths.json'
contigs_fn            = '/contigs.json'
gc_fn                 = '/gc.json'
genes_fn              = '/genes.json'
operons_fn            = '/operons.json'


def save(filename, what):
    if simplejson_error:
        return None

    json_file = open(filename, 'w')
    json.dump(what, json_file)
    json_file.close()
    return filename


def save_total_report(output_dir, min_contig):
    import reporting
    table = reporting.table()

    def try_convert_back_to_number(str):
        try:
            val = int(str)
        except ValueError:
            try:
                val = float(str)
            except ValueError:
                val = str

        return val


    table = [[try_convert_back_to_number(table[i][j]) for i in xrange(len(table))] for j in xrange(len(table[0]))]

    # TODO: check correctness, not sure that header and result are correct:
    header = table[0]
    results = table[1:]

    t = datetime.datetime.now()

    return save(output_dir + total_report_fn, {
            'date' : t.strftime('%d %B %Y, %A, %H:%M:%S'),
            'header' : header,
            'results' : results,
            'min_contig' : min_contig
    })

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
        'contigs' : dict((os.path.basename(fn), blocks) for (fn, blocks) in contigs.items()),
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


def save_GC_info(output_dir, filenames, lists_of_GC_info):
    return save(output_dir + gc_fn, {
        'filenames' : map(os.path.basename, filenames),
        'lists_of_gc_info' : lists_of_GC_info,
    })


















