############################################################################
# Copyright (c) 2011-2012 Saint-Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################

import os
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
keys_order = [] # for printing in appropriate order
min_contig = None # for printing info about min contig in TXT reports

# Available fields for report, values (strings) should be unique!
class Fields:
    NAME = 'Assembly'
    CONTIGS = ('# contigs (>= %d bp)', tuple(qconfig.contig_thresholds))
    TOTALLENS = ('Total length (>= %d bp)', tuple(qconfig.contig_thresholds))
    N50 = 'N50'
    NG50 = 'NG50'
    N75 = 'N75'
    NG75 = 'NG75'
    L50 = 'L50'
    LG50 = 'LG50'
    L75 = 'L75'
    LG75 = 'LG75'
    NUMCONTIGS = '# contigs'
    LARGCONTIG = 'Largest contig'
    TOTALLEN = 'Total length'
    GC = 'GC (%)'
    REFLEN = 'Reference length'
    REFGC = 'Reference GC (%)'
    AVGIDY = 'Average %IDY'
    MISLOCAL = '# local misassemblies'
    MISASSEMBL = '# misassemblies'
    MISCONTIGS = '# misassembled contigs'
    MISCONTIGSBASES = 'Misassembled contigs length'
    UNALIGNED = '# unaligned contigs'
    UNALIGNEDBASES = 'Unaligned contigs length'
    AMBIGUOUS = '# ambiguous contigs'
    AMBIGUOUSBASES = 'Ambiguous contigs length'
    MISMATCHES = '# mismatches'
    INDELS = '# indels'
    SUBSERROR = '# mismatches per 100 kbp'
    INDELSERROR = '# indels per 100 kbp'
    UNCALLED = '# N'
    UNCALLED_PERCENT = "N's (%)"
    NA50 = 'NA50'
    NGA50 = 'NGA50'
    NA75 = 'NA75'
    NGA75 = 'NGA75'
    LA50 = 'LA50'
    LGA50 = 'LGA50'
    LA75 = 'LA75'
    LGA75 = 'LGA75'
    MAPPEDGENOME = 'Genome fraction (%)'
    DUPLICATION_RATIO = 'Duplication ratio'
    GENES = '# genes'
    OPERONS = '# operons'
    GENEMARKUNIQUE = '# predicted genes (unique)'
    GENEMARK = ('# predicted genes (>= %d bp)', tuple(qconfig.genes_lengths))

    # order as printed in report:
    order = [NAME, CONTIGS, TOTALLENS, NUMCONTIGS, LARGCONTIG, TOTALLEN, REFLEN,
             N50, NG50, N75, NG75, L50, LG50, L75, LG75,
             AVGIDY, MISASSEMBL, MISCONTIGS, MISCONTIGSBASES,
             UNALIGNED, UNALIGNEDBASES, AMBIGUOUS, AMBIGUOUSBASES, MAPPEDGENOME, GC, REFGC,
             UNCALLED_PERCENT, SUBSERROR, INDELSERROR, GENES, OPERONS, GENEMARKUNIQUE, GENEMARK,
             NA50, NGA50, NA75, NGA75, LA50, LGA50, LA75, LGA75]

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
    if filename not in keys_order:
        keys_order.append(filename)
    return reports.setdefault(filename, Report(filename))

def delete(filename):
    filename = os.path.basename(filename)
    if filename in keys_order:
        keys_order.remove(filename)
    if filename in reports.keys():
        reports.pop(filename)

def reporting_filter(value):
    if value == "":
        return False
    return True

def table(order=Fields.order):
    ans = []
    for field in order:
        if isinstance(field, tuple): # TODO: rewrite it nicer
            for i, x in enumerate(field[1]):
                ls = []
                for name in keys_order:
                    report = get(name)
                    value = report.get_field(field)
                    ls.append(value[i] if i < len(value) else None)
                if filter(reporting_filter, ls): # have at least one element
                    ans.append([field[0] % x] + [str(y) for y in ls])
        else:
            ls = []
            for name in keys_order:
                report = get(name)
                value = report.get_field(field)
                ls.append(value)
            if filter(reporting_filter, ls): # have at least one element
                ans.append([field] + [str(y) for y in ls])
    return ans

def save_txt(filename, table):
    # determine width of columns for nice spaces
    colwidth = [0] * len(table[0])
    for line in table:
        for i, x in enumerate(line):
            colwidth[i] = max(colwidth[i], len(x))
    # output it
    file = open(filename, 'a')
    if min_contig:
        print >>file, 'Contigs of length >= %d are used' % min_contig
        print >>file
    for line in table:
        print >>file, '  '.join('%-*s' % (c, l) for c, l in zip(colwidth, line))
    file.close()

def save_tsv(filename, table):
    file = open(filename, 'a')
    for line in table:
        print >>file, '\t'.join(line)
    file.close()

def save_tex(filename, table):
    file = open(filename, 'a')
    # Header
    print >>file, '\\begin{table}[ht]'
    print >>file, '\\begin{center}'
    print >>file, '\\caption{(Contigs of length $\geq$ ' + str(min_contig) + ' are used)}'
    print >>file, '\\begin{tabular}{|l*{' + str(len(table[0]) - 1) + '}{|r}|}'
    print >>file, '\\hline'
    # Body
    for line in table:
        row = ' & '.join(line)
        # escape characters
        for esc_char in "\\ % $ # _ { } ~ ^".split():
            row = row.replace(esc_char, '\\' + esc_char)
        # more pretty '>='
        row = row.replace('>=', '$\\geq$')
        row += ' \\\\ \\hline'
        print >>file, row
    # Footer
    print >>file, '\\end{tabular}'
    print >>file, '\\end{center}'
    print >>file, '\\end{table}'
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
    print '    Saved to', report_txt_filename, ',', os.path.basename(report_tsv_filename), \
          'and', os.path.basename(report_tex_filename)

    if transposed_report_name:
        print '  Transposed version of total report...'
        tab = [[tab[i][j] for i in xrange(len(tab))] for j in xrange(len(tab[0]))]
        report_txt_filename = os.path.join(output_dirpath, transposed_report_name) + '.txt'
        report_tsv_filename = os.path.join(output_dirpath, transposed_report_name) + '.tsv'
        report_tex_filename = os.path.join(output_dirpath, transposed_report_name) + '.tex'
        save_txt(report_txt_filename, tab)
        save_tsv(report_tsv_filename, tab)
        save_tex(report_tex_filename, tab)
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
