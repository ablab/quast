__author__ = 'vladsaveliev'

############################################################################
# Copyright (c) 2011-2012 Saint-Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################

import datetime
import json
import os

total_report_fn       = '/report.html'



def save(filename, what):
    json_file = open(filename, 'w')
    json.dump(what, json_file)
    json_file.close()


def save_total_report(output_dir, report_dict):
    results = [row for key, row in report_dict.items() if key != 'header']
    results.sort(key = lambda row: row[0])
    results = [row[1:] for row in results]
    header = report_dict['header'][1:]

    t = datetime.datetime.now()

    save(output_dir + total_report_fn, {
        'date' : t.strftime('%A, %d %B %Y, %H:%M:%S'),
        'header' : header,
        'results' : results
    })

    #    json.dump({'date' : { 'year' : t.year, 'month' : t.month, 'day' : t.day, 'weekday' : t.weekday(),
    #                          'hour' : t.hour, 'minute' : t.minute, 'second' : t.second },
    #               'header' : report_dict['header'], 'results' : results }, jsn_file)


def save_contigs_lengths(output_dir, filenames, lists_of_lengths):
    lists_of_lengths = [sorted(list, reverse=True) for list in lists_of_lengths]

    save(output_dir + contigs_fn, {
        'filenames' : [os.path.basename(fn) for fn in filenames],
        'lists_of_lengths' : lists_of_lengths
    })


def save_reference_length(output_dir, reference_length):
    save(output_dir + ref_length_fn, reference_length)


def save_aligned_contigs_lengths(output_dir, filenames, lists_of_lengths):
    lists_of_lengths = [sorted(list, reverse=True) for list in lists_of_lengths]

    save(output_dir + aligned_contigs_fn, {
        'filenames' : [os.path.basename(fn) for fn in filenames],
        'lists_of_lengths' : lists_of_lengths
    })


def save_assembly_lengths(output_dir, filenames, assemblies_lengths):
    save(output_dir + assemblies_lengths_fn, {
        'filenames' : [os.path.basename(fn) for fn in filenames],
        'assemblies_lengths' : assemblies_lengths
    })

