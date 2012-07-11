############################################################################
# Copyright (c) 2011-2012 Saint-Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################

import datetime
import json
import os

dir = ''

total_report_filename = '/report.json'
contigs_filename      = '/contigs.json'

def save_total_report(report_dict):
    json_file = open(dir + total_report_filename, 'w')

    results = [row for key, row in report_dict.items() if key != 'header']
    t = datetime.datetime.now()

    json.dump({'date' : t.strftime('%A, %d %B %Y, %H:%M:%S'),
               'header' : report_dict['header'], 'results' : results }, json_file)

    json_file.close()

    #    json.dump({'date' : { 'year' : t.year, 'month' : t.month, 'day' : t.day, 'weekday' : t.weekday(),
    #                          'hour' : t.hour, 'minute' : t.minute, 'second' : t.second },
    #               'header' : report_dict['header'], 'results' : results }, jsn_file)

def save_contigs(filenames, lists_of_lengths):
    json_file = open(dir + contigs_filename, 'w')

    lists_of_lengths = [sorted(list, reverse=True) for list in lists_of_lengths]

    json.dump({'filenames' : [os.path.basename(fn) for fn in filenames], 'lists_of_lengths' : lists_of_lengths}, json_file)

    json_file.close()