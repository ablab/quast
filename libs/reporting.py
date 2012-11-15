############################################################################
# Copyright (c) 2011-2012 Saint-Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################

import os
import sys
from libs import qconfig

####################################################################################
# Reporting module (singleton) for QUAST
#
# See class Fields to available fields for report.
# Usage from QUAST modules:
#  from libs import reporting
#  report = reporting.get(fasta_filename)
#  report.add_field(reporting.Field.N50, n50)
#
# Import this module only after final changes in qconfig!
#
####################################################################################

reports = {} # basefilename -> Report
assemblies_order = [] # for printing in appropriate order
min_contig = None # for printing info about min contig in TXT reports

# Available fields for report, values (strings) should be unique!
class Fields:
    NAME = 'Assembly'
    # Basic stats
    NUMCONTIGS = '# contigs'
    CONTIGS = ('# contigs (>= %d bp)', tuple(qconfig.contig_thresholds))
    LARGCONTIG = 'Largest contig'
    TOTALLEN = 'Total length'
    TOTALLENS = ('Total length (>= %d bp)', tuple(qconfig.contig_thresholds))
    REFLEN = 'Reference length'
    N50 = 'N50'
    NG50 = 'NG50'
    N75 = 'N75'
    NG75 = 'NG75'
    L50 = 'L50'
    LG50 = 'LG50'
    L75 = 'L75'
    LG75 = 'LG75'

    # Misassemblies
    MISLOCAL = '# local misassemblies'
    MISASSEMBL = '# misassemblies'
    MISCONTIGS = '# misassembled contigs'
    MISCONTIGSBASES = 'Misassembled contigs length'
    UNALIGNED = '# unaligned contigs'
    UNALIGNEDBASES = 'Unaligned contigs length'
    REPEATS = '# contigs with repeats'
    REPEATSEXTRABASES = 'Extra bases in contigs with repeats'

    UNCALLED = "# N's"
    UNCALLED_PERCENT = "# N's per 100 kbp"

    # Aligned
    LARGALIGN = 'Largest alignment'
    NA50 = 'NA50'
    NGA50 = 'NGA50'
    NA75 = 'NA75'
    NGA75 = 'NGA75'
    LA50 = 'LA50'
    LGA50 = 'LGA50'
    LA75 = 'LA75'
    LGA75 = 'LGA75'

    # Genes and operons
    MAPPEDGENOME = 'Genome fraction (%)'
    DUPLICATION_RATIO = 'Duplication ratio'
    GENES = '# genes'
    OPERONS = '# operons'
    GENEMARKUNIQUE = '# predicted genes (unique)'
    GENEMARK = ('# predicted genes (>= %d bp)', tuple(qconfig.genes_lengths))
    MISMATCHES = '# mismatches'
    INDELS = '# indels'
    SUBSERROR = '# mismatches per 100 kbp'
    INDELSERROR = '# indels per 100 kbp'
    GC = 'GC (%)'
    REFGC = 'Reference GC (%)'
    AVGIDY = 'Average %IDY'

    # order as printed in report:
    order = [NAME, CONTIGS, TOTALLENS, NUMCONTIGS, LARGCONTIG, TOTALLEN, REFLEN, GC, REFGC,
             N50, NG50, N75, NG75,
             MISASSEMBL, MISLOCAL,
             UNALIGNED, UNALIGNEDBASES, MAPPEDGENOME, DUPLICATION_RATIO,
             UNCALLED_PERCENT, SUBSERROR, INDELSERROR, GENES, OPERONS, GENEMARKUNIQUE, GENEMARK,
             LARGALIGN, NA50, NGA50, NA75, NGA75]

    MIS_ALL_EXTENSIVE = '# misassemblies'
    MIS_RELOCATION = '    # relocations'
    MIS_TRANSLOCATION = '    # translocations'
    MIS_INVERTION = '    # inversions'
    MIS_EXTENSIVE_CONTIGS = '# misassembled contigs'
    MIS_EXTENSIVE_BASES = 'Misassembled contigs length'
    MIS_LOCAL = '# local misassemblies'

    # for detailed misassemblies report
    misassemblies_order = [NAME, MIS_ALL_EXTENSIVE, MIS_RELOCATION, MIS_TRANSLOCATION, MIS_INVERTION,
                           MIS_EXTENSIVE_CONTIGS, MIS_EXTENSIVE_BASES, MIS_LOCAL, MISMATCHES, INDELS]

    UNALIGNED_FULL_CNTGS = '# fully unaligned contigs'
    UNALIGNED_FULL_LENGTH = 'Fully unaligned length'
    UNALIGNED_PART_CNTGS = '# partially unaligned contigs'
    UNALIGNED_PART_WITH_MISASSEMBLY = '    # with misassembly'
    UNALIGNED_PART_SIGNIFICANT_PARTS = '    # both parts are significant'
    UNALIGNED_PART_LENGTH = 'Partially unaligned length'

    # for detailed unaligned report
    unaligned_order = [NAME, UNALIGNED_FULL_CNTGS, UNALIGNED_FULL_LENGTH, UNALIGNED_PART_CNTGS,
                       UNALIGNED_PART_WITH_MISASSEMBLY, UNALIGNED_PART_SIGNIFICANT_PARTS, UNALIGNED_PART_LENGTH, UNCALLED]

    # GAGE fields
    GAGE_NUMCONTIGS = 'Contigs #'
    GAGE_MINCONTIG = 'Min contig'
    GAGE_MAXCONTIG = 'Max contig'
    GAGE_N50 = 'N50'
    GAGE_GENOMESIZE = 'Genome size'
    GAGE_ASSEMBLY_SIZE = 'Assembly size'
    GAGE_CHAFFBASES = 'Chaff bases'
    GAGE_MISSINGREFBASES = 'Missing reference bases'
    GAGE_MISSINGASMBLYBASES = 'Missing assembly bases'
    GAGE_MISSINGASMBLYCONTIGS = 'Missing assembly contigs'
    GAGE_DUPREFBASES = 'Duplicated reference bases'
    GAGE_COMPRESSEDREFBASES = 'Compressed reference bases'
    GAGE_BADTRIM = 'Bad trim'
    GAGE_AVGIDY = 'Avg idy'
    GAGE_SNPS = 'SNPs'
    GAGE_SHORTINDELS = 'Indels < 5bp'
    GAGE_LONGINDELS = 'Indels >= 5'
    GAGE_INVERSIONS = 'Inversions'
    GAGE_RELOCATION = 'Relocation'
    GAGE_TRANSLOCATION = 'Translocation'
    GAGE_NUMCORCONTIGS = 'Corrected contig #'
    GAGE_CORASMBLYSIZE = 'Corrected assembly size'
    GAGE_MINCORCONTIG = 'Min correct contig'
    GAGE_MAXCORCOTING = 'Max correct contig'
    GAGE_CORN50 = 'Corrected N50'

    # GAGE order
    gage_order = [NAME, GAGE_NUMCONTIGS, GAGE_MINCONTIG, GAGE_MAXCONTIG, GAGE_N50, GAGE_GENOMESIZE, GAGE_ASSEMBLY_SIZE,
                  GAGE_CHAFFBASES, GAGE_MISSINGREFBASES, GAGE_MISSINGASMBLYBASES, GAGE_MISSINGASMBLYCONTIGS, GAGE_DUPREFBASES,
                  GAGE_COMPRESSEDREFBASES, GAGE_BADTRIM, GAGE_AVGIDY, GAGE_SNPS, GAGE_SHORTINDELS, GAGE_LONGINDELS, GAGE_INVERSIONS,
                  GAGE_RELOCATION, GAGE_TRANSLOCATION, GAGE_NUMCORCONTIGS, GAGE_CORASMBLYSIZE, GAGE_MINCORCONTIG, GAGE_MAXCORCOTING,
                  GAGE_CORN50]

    grouped_order = [
        ('Basic statistics', [NUMCONTIGS, CONTIGS, LARGCONTIG, TOTALLEN, TOTALLENS, REFLEN,
                              N50, N75, NG50, NG75, L50, L75, LG50, LG75,]),

        ('Misassemblies', [MIS_ALL_EXTENSIVE,
                           MIS_RELOCATION, MIS_TRANSLOCATION, MIS_INVERTION,
                           MIS_EXTENSIVE_CONTIGS, MIS_EXTENSIVE_BASES,
                           MIS_LOCAL]),

        ('Unaligned', [UNALIGNED_FULL_CNTGS, UNALIGNED_FULL_LENGTH, UNALIGNED_PART_CNTGS,
                       UNALIGNED_PART_WITH_MISASSEMBLY, UNALIGNED_PART_SIGNIFICANT_PARTS,
                       UNALIGNED_PART_LENGTH,]),

        #('Ambiguous', [AMBIGUOUS, AMBIGUOUSBASES,]),

        ('Genome statistics', [MAPPEDGENOME, DUPLICATION_RATIO, GENES, OPERONS,
                               GENEMARKUNIQUE, GENEMARK, GC, REFGC,
                               MISMATCHES, SUBSERROR, INDELS, INDELSERROR,
                               UNCALLED, UNCALLED_PERCENT,]),

        ('Aligned statistics', [LARGALIGN, NA50, NA75, NGA50, NGA75, LA50, LA75, LGA50, LGA75,]),
        ]

    main_metrics = [NUMCONTIGS, LARGCONTIG, TOTALLEN, NG50, UNCALLED_PERCENT,
                    MISASSEMBL, MISCONTIGSBASES,
                    MAPPEDGENOME, SUBSERROR, INDELSERROR,
                    GENES, OPERONS, GENEMARKUNIQUE, GENEMARK,]

    class Quality:
        MORE_IS_BETTER='More is better'
        LESS_IS_BETTER='Less is better'
        EQUAL='Equal'

    quality_dict = {
        Quality.MORE_IS_BETTER:
            [LARGCONTIG, TOTALLEN, TOTALLENS, N50, NG50, N75, NG75, NA50, NGA50, NA75, NGA75, LARGALIGN,
             MAPPEDGENOME, GENES, OPERONS, GENEMARKUNIQUE, GENEMARK,],
        Quality.LESS_IS_BETTER:
            [NUMCONTIGS, CONTIGS, L50, LG50, L75, LG75,
             MISLOCAL, MISASSEMBL, MISCONTIGS, MISCONTIGSBASES, UNALIGNED, UNALIGNEDBASES, #AMBIGUOUS, AMBIGUOUSBASES,
             UNCALLED, UNCALLED_PERCENT,
             LA50, LGA50, LA75, LGA75, DUPLICATION_RATIO, INDELS, INDELSERROR, MISMATCHES, SUBSERROR,],
        Quality.EQUAL:
            [REFLEN, GC, REFGC, AVGIDY],
        }

    #for name, metrics in filter(lambda (name, metrics): name in ['Misassemblies', 'Unaligned', 'Ambiguous'], grouped_order):
    for name, metrics in filter(lambda (name, metrics): name in ['Misassemblies', 'Unaligned'], grouped_order):
        quality_dict['Less is better'].extend(metrics)



def get_main_metrics():
    lists = map(take_tuple_metric_apart, Fields.main_metrics)
    m_metrics = []
    for l in lists:
        for m in l:
            m_metrics.append(m)
    return m_metrics


def take_tuple_metric_apart(field):
    metrics = []

    if isinstance(field, tuple): # TODO: rewrite it nicer
        thresholds = map(int, ''.join(field[1]).split(','))
        for i, feature in enumerate(thresholds):
            metrics.append(field[0] % feature)
    else:
        metrics = [field]

    return metrics


def get_quality(metric):
    for quality, metrics in Fields.quality_dict.iteritems():
        if metric in Fields.quality_dict[quality]:
            return quality
    return Fields.Quality.EQUAL


# Report for one filename, dict: field -> value
class Report(object):
    def __init__(self, name):
        self.d = {}
        self.add_field(Fields.NAME, name)

    def add_field(self, field, value):
        assert field in Fields.__dict__.itervalues(), 'Unknown field: %s' % field
        self.d[field] = value

    def append_field(self, field, value):
        assert field in Fields.__dict__.itervalues(), 'Unknown field: %s' % field
        self.d.setdefault(field, []).append(value)

    def get_field(self, field):
        assert field in Fields.__dict__.itervalues(), 'Unknown field: %s' % field
        return self.d.get(field, '')


def get(filename):
    filename = os.path.basename(filename)
    if filename not in assemblies_order:
        assemblies_order.append(filename)
    return reports.setdefault(filename, Report(filename))


def delete(filename):
    filename = os.path.basename(filename)
    if filename in assemblies_order:
        assemblies_order.remove(filename)
    if filename in reports.keys():
        reports.pop(filename)


def reporting_filter(value):
    if value == "":
        return False
    return True


#def simple_table(order=Fields.order):
#    return grouped_table(grouped_order=)
#
#    table = []
#
#    def append_line(rows, field, pattern=None, feature=None, i=None):
#        quality = get_quality(field)
#        values = []
#
#        for assembly_name in assemblies_order:
#            report = get(assembly_name)
#            value = report.get_field(field)
#            if feature is None:
#                values.append(value)
#            else:
#                values.append(value[i] if i < len(value) else None)
#
#        if filter(reporting_filter, values):
#            metric_name = field if (feature is None) else pattern % feature
#            #ATTENTION! Contents numeric values, needed to be converted to strings.
#            rows.append({
#                'metricName': metric_name,
#                'quality': quality,
#                'values': values,
#                'isMain': field in Fields.main_metrics
#            })
#
#    for field in order:
#        if isinstance(field, tuple): # TODO: rewrite it nicer
#            for i, feature in enumerate(field[1]):
#                append_line(table, field, field[0], feature, i)
#        else:
#            append_line(table, field)
#
#    return table


#ATTENTION! Contents numeric values, needed to be converted into strings
def table(order=Fields.order):
    if not isinstance(order[0], tuple): # is not a groupped metrics order
        order = [('', order)]

    table = []

    def append_line(rows, field, pattern=None, feature=None, i=None):
        quality = get_quality(field)
        values = []

        for assembly_name in assemblies_order:
            report = get(assembly_name)
            value = report.get_field(field)
            if feature is None:
                values.append(value)
            else:
                values.append(value[i] if i < len(value) else None)

        if filter(reporting_filter, values):
            metric_name = field if (feature is None) else pattern % feature
            #ATTENTION! Contents numeric values, needed to be converted to strings.
            rows.append({
                'metricName': metric_name,
                'quality': quality,
                'values': values,
                'isMain': field in Fields.main_metrics,
            })

    for group_name, metrics in order:
        rows = []
        table.append((group_name, rows))

        for field in metrics:
            if isinstance(field, tuple): # TODO: rewrite it nicer
                for i, feature in enumerate(field[1]):
                    append_line(rows, field, field[0], feature, i)
            else:
                append_line(rows, field)

    if not isinstance(order[0], tuple): # is not a groupped metrics order
        group_name, rows = table[0]
        return rows
    else:
        return table


def is_groupped_table(table):
    return isinstance(table[0], tuple)


def get_all_rows_out_of_table(table, is_transposed=False):
    all_rows = []
    if is_groupped_table(table):
        for group_name, rows in table:
            all_rows += rows
    else:
        all_rows = table

    return all_rows


def save_txt(filename, table, is_transposed=False):
    all_rows = get_all_rows_out_of_table(table)

    # determine width of columns for nice spaces
    colwidths = [0] * (len(all_rows[0]['values']) + 1)
    for row in all_rows:
        for i, cell in enumerate([row['metricName']] + map(str, row['values'])):
            colwidths[i] = max(colwidths[i], len(cell))
            # output it

    file = open(filename, 'w')

    if min_contig:
        print >>file, 'Contigs of length >= %d are used' % min_contig
        print >>file
    for row in all_rows:
        print >>file, '  '.join('%-*s' % (colwidth, cell) for colwidth, cell
            in zip(colwidths, [row['metricName']] + map(str, row['values'])))

    file.close()


def save_tsv(filename, table, is_transposed=False):
    all_rows = get_all_rows_out_of_table(table)

    file = open(filename, 'w')

    for row in all_rows:
        print >>file, '\t'.join([row['metricName']] + map(str, row['values']))

    file.close()


def parse_number(val):
    num = None
    # Float?
    try:
        num = int(val)
    except ValueError:
        # Int?
        try:
            num = float(val)
        except ValueError:
            num = None

    return num


def get_num_from_table_value(val):
    num = None
    if isinstance(val, int) or isinstance(val, float):
        num = val

    elif isinstance(val, basestring):
                                                                      # 'x + y part' format?
        tokens = val.split()[0]                                       # tokens = [x, +, y, part]
        if len(tokens) >= 3:                                          # Yes, 'y + x part' format
            x, y = parse_number(tokens[0]), parse_number(tokens[2])
            if x is None or y is None:
                val = None
            else:
                val = x, y                                            # Tuple value. Can be compared lexicographically.
        else:
            num = parse_number(tokens[0])
    else:
        num = val

    return num


def save_tex(filename, table, is_transposed=False):
    all_rows = get_all_rows_out_of_table(table)

    file = open(filename, 'w')
    # Header
    print >>file, '\\documentclass[12pt,a4paper]{article}'
    print >>file, '\\begin{document}'
    print >>file, '\\begin{table}[ht]'
    print >>file, '\\begin{center}'
    print >>file, '\\caption{(Contigs of length $\geq$ ' + str(min_contig) + ' are used)}'

    rows_n = len(all_rows[0]['values'])
    print >>file, '\\begin{tabular}{|l*{' + str(rows_n) + '}{|r}|}'
    print >>file, '\\hline'

    # Body
    for row in all_rows:
        values = row['values']
        quality = row['quality'] if ('quality' in row) else Fields.Quality.EQUAL

        if is_transposed or quality not in [Fields.Quality.MORE_IS_BETTER, Fields.Quality.LESS_IS_BETTER]:
            cells = map(str, values)
        else:
            # Checking the first value, assuming the others are the same type and format
            num = get_num_from_table_value(values[0])
            if num is None:
                cells = map(str, values)
            else:
                nums = map(get_num_from_table_value, values)
                best = None
                if quality == Fields.Quality.MORE_IS_BETTER:
                    best = max(nums)
                if quality == Fields.Quality.LESS_IS_BETTER:
                    best = min(nums)

                if len([num for num in nums if num != best]) == 0:
                    cells = map(str, values)
                else:
                    cells = ['HIGHLIGHTEDSTART' + str(v) + 'HIGHLIGHTEDEND'
                             if get_num_from_table_value(v) == best
                             else str(v)
                             for v in values]

        row = ' & '.join([row['metricName']] + cells)
        # escape characters
        for esc_char in "\\ % $ # _ { } ~ ^".split():
            row = row.replace(esc_char, '\\' + esc_char)
        # more pretty '>='
        row = row.replace('>=', '$\\geq$')
        # pretty highlight
        row = row.replace('HIGHLIGHTEDSTART', '{\\bf ')
        row = row.replace('HIGHLIGHTEDEND', '}')
        row += ' \\\\ \\hline'
        print >>file, row

    # Footer
    print >>file, '\\end{tabular}'
    print >>file, '\\end{center}'
    print >>file, '\\end{table}'
    print >>file, '\\end{document}'
    file.close()


def save(output_dirpath, report_name, transposed_report_name, order):
    # Where total report will be saved
    tab = table(order)

    print '  Creating total report...'
    report_txt_filename = os.path.join(output_dirpath, report_name) + '.txt'
    report_tsv_filename = os.path.join(output_dirpath, report_name) + '.tsv'
    report_tex_filename = os.path.join(output_dirpath, report_name) + '.tex'
    save_txt(report_txt_filename, tab)
    save_tsv(report_tsv_filename, tab)
    save_tex(report_tex_filename, tab)
    print '    Saved to', report_txt_filename, ',', os.path.basename(report_tsv_filename),\
    'and', os.path.basename(report_tex_filename)

    if transposed_report_name:
        print '  Transposed version of total report...'

        all_rows = get_all_rows_out_of_table(tab)
        if all_rows[0]['metricName'] != Fields.NAME:
            print >>sys.stderr, 'To transpose table, first column have to be assemblies names'
        else:
            # Transposing table
            transposed_table = [{'metricName': all_rows[0]['metricName'],
                                 'values': [all_rows[i]['metricName'] for i in xrange(1, len(all_rows))],}]
            for i in range(len(all_rows[0]['values'])):
                values = []
                for j in range(1, len(all_rows)):
                    values.append(all_rows[j]['values'][i])
                transposed_table.append({'metricName': all_rows[0]['values'][i], # name of assembly, assuming the first line is assemblies names
                                         'values': values,})

            report_txt_filename = os.path.join(output_dirpath, transposed_report_name) + '.txt'
            report_tsv_filename = os.path.join(output_dirpath, transposed_report_name) + '.tsv'
            report_tex_filename = os.path.join(output_dirpath, transposed_report_name) + '.tex'
            save_txt(report_txt_filename, transposed_table, is_transposed=True)
            save_tsv(report_tsv_filename, transposed_table, is_transposed=True)
            save_tex(report_tex_filename, transposed_table, is_transposed=True)
            print '    Saved to', report_txt_filename, ',', os.path.basename(report_tsv_filename),\
            'and', os.path.basename(report_tex_filename)


def save_gage(output_dirpath):
    save(output_dirpath, "gage_report", "gage_transposed_report", Fields.gage_order)


def save_total(output_dirpath):
    print 'Summarizing...'
    save(output_dirpath, "report", "transposed_report", Fields.order)


def save_misassemblies(output_dirpath):
    save(output_dirpath, "misassemblies_report", "", Fields.misassemblies_order)


def save_unaligned(output_dirpath):
    save(output_dirpath, "unaligned_report", "", Fields.unaligned_order)
