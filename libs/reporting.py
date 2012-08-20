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

# Available fields for report, values (strings) should be unique!
class Fields:
    NAME = 'Assembly'
    CONTIGS = ('# contigs >= %d', tuple(qconfig.contig_thresholds))
    TOTALLENS = ('Total length (>= %d)', tuple(qconfig.contig_thresholds))
    N50 = 'N50'
    NG50 = 'NG50'
    N75 = 'N75'
    NG75 = 'NG75'
    NUMCONTIGS = 'Number of contigs'
    LARGCONTIG = 'Largest contig'
    TOTALLEN = 'Total length'
    GC = 'GC %'
    REFLEN = 'Reference length'
    REFGC = 'Reference GC %'
    AVGIDY = 'Average %IDY'
    MISLOCAL = 'Local misassemblies'
    MISASSEMBL = 'Misassemblies'
    MISCONTIGS = 'Misassembled contigs'
    MISCONTIGSBASES = 'Misassembled contig bases'
    MISUNALIGNED = 'Misassembled and unaligned'
    UNALIGNED = 'Unaligned contigs'
    UNALIGNEDBASES = 'Unaligned contig bases'
    AMBIGOUS = 'Ambiguous contigs'
    SNPS = 'SNPs'
    NA50 = 'NA50'
    NGA50 = 'NGA50'
    NA75 = 'NA75'
    NGA75 = 'NGA75'
    MAPPEDGENOME = 'Mapped genome (%)'
    GENES = 'Genes'
    OPERONS = 'Operons'
    ORFS = ('# ORFs >= %d aa', tuple(qconfig.orf_lengths))
    GENEMARKUNIQUE = 'GeneMark (# unique genes)'
    GENEMARK = ('GeneMark (# genes >= %d bp)', tuple(qconfig.genes_lengths))

    # order as printed in report:
    order = [NAME, CONTIGS, TOTALLENS, N50, NG50, N75, NG75, NUMCONTIGS, LARGCONTIG, TOTALLEN, GC, REFLEN, REFGC,
             AVGIDY, MISLOCAL, MISASSEMBL, MISCONTIGS, MISCONTIGSBASES, MISUNALIGNED, UNALIGNED, UNALIGNEDBASES, AMBIGOUS, SNPS,
             NA50, NGA50, NA75, NGA75,
             MAPPEDGENOME, GENES, OPERONS,
             ORFS,
             GENEMARKUNIQUE, GENEMARK]


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
    return reports.setdefault(name, Report(name))

def table():
    ans = []
    for field in Fields.order:
        if isinstance(field, tuple): # TODO: rewrite it nicer
            for i, x in enumerate(field[1]):
                ls = []
                for report in reports.itervalues():
                    value = report.get_field(field)
                    ls.append(value[i] if i < len(value) else None)
                if filter(None, ls): # have at least one element
                    ans.append([field[0] % x] + [str(y) for y in ls])
        else:
            ls = []
            for report in reports.itervalues():
                value = report.get_field(field)
                ls.append(value)
            if filter(None, ls): # have at least one element
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
        print >>file, 'Only contigs of length >= %d were taken into account' % min_contig
        print >>file
    for line in table:
        print >>file, '  '.join('%-*s' % (c, l) for c, l in zip(colwidth, line))
    file.close()

def save_tsv(filename, table):
    file = open(filename, 'a')
    for line in table:
        print >>file, '\t'.join(line)
    file.close()

def save(output_dirpath, min_contig):
    # Where total report will be saved
    print 'Summarizing...'
    tab = table()

    print '  Creating total report...'
    report_txt_filename = os.path.join(output_dirpath, "report") + '.txt'
    report_tsv_filename = os.path.join(output_dirpath, "report") + '.tsv'
    save_txt(report_txt_filename, tab, min_contig)
    save_tsv(report_tsv_filename, tab)
    print '    Saved to', report_txt_filename, 'and', report_tsv_filename

    print '  Transposed version of total report...'
    tab = [[tab[i][j] for i in xrange(len(tab))] for j in xrange(len(tab[0]))]
    report_txt_filename = os.path.join(output_dirpath, "transposed_report") + '.txt'
    report_tsv_filename = os.path.join(output_dirpath, "transposed_report") + '.tsv'
    save_txt(report_txt_filename, tab, min_contig)
    save_tsv(report_tsv_filename, tab)
    print '    Saved to', report_txt_filename, 'and', report_tsv_filename
