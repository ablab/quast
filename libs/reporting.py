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

# Available fields for report, values (strings) should be unique!
class Fields:
    NAME = 'Assembly'
    CONTIGS = ('# contigs (>= %d bp)', tuple(qconfig.contig_thresholds))
    TOTALLENS = ('Total length (>= %d bp)', tuple(qconfig.contig_thresholds))
    N50 = 'N50'
    NG50 = 'NG50'
    N75 = 'N75'
    NG75 = 'NG75'
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
    MISUNALIGNED = '# misassembled and unaligned'
    UNALIGNED = '# unaligned contigs'
    UNALIGNEDBASES = 'Unaligned contigs length'
    AMBIGUOUS = '# ambiguous contigs'
    AMBIGUOUSBASES = 'Ambiguous contigs length'
    SNPS = '# mismatches'
    SUBSERROR = '# mismatches per 100 Kbp'
    NA50 = 'NA50'
    NGA50 = 'NGA50'
    NA75 = 'NA75'
    NGA75 = 'NGA75'
    MAPPEDGENOME = 'Genome fraction (%)'
    GENES = '# genes'
    OPERONS = '# operons'
    GENEMARKUNIQUE = '# predicted genes (unique)'
    GENEMARK = ('# predicted genes (>= %d bp)', tuple(qconfig.genes_lengths))

    # order as printed in report:
    order = [NAME, CONTIGS, TOTALLENS, NUMCONTIGS, LARGCONTIG, TOTALLEN, REFLEN, N50, NG50, N75, NG75,             
             # AVGIDY, 
	         MISASSEMBL, MISCONTIGS, MISCONTIGSBASES, MISLOCAL, # MISUNALIGNED,
             UNALIGNED, UNALIGNEDBASES, AMBIGUOUS, AMBIGUOUSBASES, 
             MAPPEDGENOME, GC, REFGC, SUBSERROR, GENES, OPERONS, GENEMARKUNIQUE, GENEMARK,
             NA50, NGA50, NA75, NGA75]
    
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

def get(name):
    name = os.path.basename(name)
    if name not in keys_order:
        keys_order.append(name)
    return reports.setdefault(name, Report(name))

def reporting_filter(value):
    if value == "":
        return False
    return True

def table(gage_mode=False):
    order = Fields.gage_order if gage_mode else Fields.order
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

def save_txt(filename, table, min_contig = None):
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

def save(output_dirpath, min_contig, gage_mode=False):
    # Where total report will be saved
    if not gage_mode:
        print 'Summarizing...'
    tab = table(gage_mode)

    gage_prefix = "gage_" if gage_mode else "" 
    print '  Creating total report...'
    report_txt_filename = os.path.join(output_dirpath, gage_prefix + "report") + '.txt'
    report_tsv_filename = os.path.join(output_dirpath, gage_prefix + "report") + '.tsv'
    save_txt(report_txt_filename, tab, min_contig)
    save_tsv(report_tsv_filename, tab)
    print '    Saved to', report_txt_filename, 'and', report_tsv_filename

    print '  Transposed version of total report...'
    tab = [[tab[i][j] for i in xrange(len(tab))] for j in xrange(len(tab[0]))]
    report_txt_filename = os.path.join(output_dirpath, gage_prefix + "transposed_report") + '.txt'
    report_tsv_filename = os.path.join(output_dirpath, gage_prefix + "transposed_report") + '.tsv'
    save_txt(report_txt_filename, tab, min_contig)
    save_tsv(report_tsv_filename, tab)
    print '    Saved to', report_txt_filename, 'and', report_tsv_filename
