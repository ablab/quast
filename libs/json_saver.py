############################################################################
# Copyright (c) 2011-2012 Saint-Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################

import datetime
import json
import os

dir = ''

total_report_fn       = '/report.json'
contigs_fn            = '/contigs_lengths.json'
ref_length_fn         = '/ref_length.json'
aligned_contigs_fn    = '/aligned_contigs_lengths.json'
assemblies_lengths_fn = '/assemblies_lengths.json'


def save(filename, what):
    json_file = open(dir + filename, 'w')
    json.dump(what, json_file)
    json_file.close()


def save_total_report(report_dict):
    results = [row for key, row in report_dict.items() if key != 'header']
    t = datetime.datetime.now()

    save(total_report_fn, {'date' : t.strftime('%A, %d %B %Y, %H:%M:%S'),
                           'header' : report_dict['header'], 'results' : results })

    #    json.dump({'date' : { 'year' : t.year, 'month' : t.month, 'day' : t.day, 'weekday' : t.weekday(),
    #                          'hour' : t.hour, 'minute' : t.minute, 'second' : t.second },
    #               'header' : report_dict['header'], 'results' : results }, jsn_file)


def save_contigs_lengths(filenames, lists_of_lengths):
    lists_of_lengths = [sorted(list, reverse=True) for list in lists_of_lengths]

    save(contigs_fn, {'filenames' : [os.path.basename(fn) for fn in filenames],
                      'lists_of_lengths' : lists_of_lengths})


def save_reference_length(reference_length):
    save(ref_length_fn, reference_length)


def save_aligned_contigs_lengths(filenames, lists_of_lengths):
    lists_of_lengths = [sorted(list, reverse=True) for list in lists_of_lengths]

    save(aligned_contigs_fn, {'filenames' : [os.path.basename(fn) for fn in filenames],
                              'lists_of_lengths' : lists_of_lengths})


def save_assembly_lengths(filenames, assemblies_lengths):
    save(assemblies_lengths_fn, {'filenames' : [os.path.basename(fn) for fn in filenames],
                                 'assemblies_lengths' : assemblies_lengths})

