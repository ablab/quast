############################################################################
# Copyright (c) 2015-2017 Saint Petersburg State University
# Copyright (c) 2011-2015 Saint Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################
import os

from quast_libs import qconfig, qutils

from quast_libs.log import get_logger
from quast_libs.qutils import val_to_str

logger = get_logger(qconfig.LOGGER_DEFAULT_NAME)

try:
    basestring
except:
    basestring = (str, bytes)


# Here you can modify content and order of metrics in QUAST reports and names of metrcis as well
class Fields:

####################################################################################
###########################  CONFIGURABLE PARAMETERS  ##############################
####################################################################################
    ### for indent before submetrics
    TAB = '    '

    ### List of available fields for reports. Values (strings) should be unique! ###

    # Header
    NAME = 'Assembly'

    # Basic statistics
    CONTIGS = '# contigs'
    CONTIGS__FOR_THRESHOLDS = ('# contigs (>= %d bp)', tuple(qconfig.contig_thresholds))
    LARGCONTIG = 'Largest contig'
    TOTALLEN = 'Total length'
    TOTALLENS__FOR_THRESHOLDS = ('Total length (>= %d bp)', tuple(qconfig.contig_thresholds))
    TOTALLENS__FOR_1000_THRESHOLD = 'Total length (>= 1000 bp)'
    TOTALLENS__FOR_10000_THRESHOLD = 'Total length (>= 10000 bp)'
    TOTALLENS__FOR_50000_THRESHOLD = 'Total length (>= 50000 bp)'
    N50 = 'N50'
    N75 = 'N75'
    L50 = 'L50'
    L75 = 'L75'
    GC = 'GC (%)'

    # Misassemblies
    MISASSEMBL = '# misassemblies'
    MISCONTIGS = '# misassembled contigs'
    MISCONTIGSBASES = 'Misassembled contigs length'
    MISINTERNALOVERLAP = 'Misassemblies inter-contig overlap'
    ### additional list of metrics for detailed misassemblies report
    MIS_ALL_EXTENSIVE = '# misassemblies'
    MIS_RELOCATION = TAB + '# relocations'
    MIS_TRANSLOCATION = TAB + '# translocations'
    MIS_INVERTION = TAB + '# inversions'
    MIS_ISTRANSLOCATIONS = TAB + '# interspecies translocations'
    MIS_EXTENSIVE_CONTIGS = '# misassembled contigs'
    MIS_EXTENSIVE_BASES = 'Misassembled contigs length'
    MIS_LOCAL = '# local misassemblies'
    MIS_SCAFFOLDS_GAP = '# scaffold gap size misassemblies'
    MIS_FRAGMENTED = '# misassemblies caused by fragmented reference'
    CONTIGS_WITH_ISTRANSLOCATIONS = '# possibly misassembled contigs'
    POSSIBLE_MISASSEMBLIES = TAB + '# possible misassemblies'
    ### structural variations
    STRUCT_VARIATIONS = '# structural variations'

    # Unaligned
    UNALIGNED = '# unaligned contigs'
    UNALIGNEDBASES = 'Unaligned length'
    AMBIGUOUS = '# ambiguously mapped contigs'
    AMBIGUOUSEXTRABASES = 'Extra bases in ambiguously mapped contigs'
    MISLOCAL = '# local misassemblies'
    ### additional list of metrics for detailed unaligned report
    UNALIGNED_FULL_CNTGS = '# fully unaligned contigs'
    UNALIGNED_FULL_LENGTH = 'Fully unaligned length'
    UNALIGNED_PART_CNTGS = '# partially unaligned contigs'
    UNALIGNED_PART_LENGTH = 'Partially unaligned length'
    UNALIGNED_MISASSEMBLED_CTGS = '# unaligned mis. contigs'

    # Indels and mismatches
    MISMATCHES = '# mismatches'
    INDELS = '# indels'
    INDELSBASES = 'Indels length'
    SUBSERROR = '# mismatches per 100 kbp'
    INDELSERROR = '# indels per 100 kbp'
    MIS_SHORT_INDELS = TAB + '# indels (<= 5 bp)'
    MIS_LONG_INDELS = TAB + '# indels (> 5 bp)'
    UNCALLED = "# N's"
    UNCALLED_PERCENT = "# N's per 100 kbp"

    # Genome statistics
    MAPPEDGENOME = 'Genome fraction (%)'
    DUPLICATION_RATIO = 'Duplication ratio'
    AVE_READ_SUPPORT = 'Avg contig read support'
    GENES = '# genes'
    OPERONS = '# operons'
    LARGALIGN = 'Largest alignment'
    TOTAL_ALIGNED_LEN = 'Total aligned length'
    NG50 = 'NG50'
    NA50 = 'NA50'
    NGA50 = 'NGA50'
    LG50 = 'LG50'
    LA50 = 'LA50'
    LGA50 = 'LGA50'
    NG75 = 'NG75'
    NA75 = 'NA75'
    NGA75 = 'NGA75'
    LG75 = 'LG75'
    LA75 = 'LA75'
    LGA75 = 'LGA75'

    # Predicted genes
    PREDICTED_GENES_UNIQUE = '# predicted genes (unique)'
    PREDICTED_GENES = ('# predicted genes (>= %d bp)', tuple(qconfig.genes_lengths))

    # Reference statistics
    REFLEN = 'Reference length'
    ESTREFLEN = 'Estimated reference length'
    REF_FRAGMENTS = 'Reference fragments'
    REFGC = 'Reference GC (%)'
    REF_GENES = 'Reference genes'
    REF_OPERONS = 'Reference operons'

    # Icarus statistics
    SIMILAR_CONTIGS = '# similar correct contigs'
    SIMILAR_MIS_BLOCKS = '# similar misassembled blocks'

    ### content and order of metrics in MAIN REPORT (<quast_output_dir>/report.txt, .tex, .tsv):
    order = [NAME, CONTIGS__FOR_THRESHOLDS, TOTALLENS__FOR_THRESHOLDS, CONTIGS, LARGCONTIG, TOTALLEN, REFLEN, ESTREFLEN, GC, REFGC,
             N50, NG50, N75, NG75, L50, LG50, L75, LG75, MISASSEMBL, MISCONTIGS, MISCONTIGSBASES,
             MISLOCAL, MIS_SCAFFOLDS_GAP, STRUCT_VARIATIONS, UNALIGNED_MISASSEMBLED_CTGS,
             UNALIGNED, UNALIGNEDBASES, MAPPEDGENOME, DUPLICATION_RATIO, AVE_READ_SUPPORT,
             UNCALLED_PERCENT, SUBSERROR, INDELSERROR, GENES, OPERONS, PREDICTED_GENES_UNIQUE, PREDICTED_GENES,
             LARGALIGN, TOTAL_ALIGNED_LEN, NA50, NGA50, NA75, NGA75, LA50, LGA50, LA75, LGA75, ]

    # content and order of metrics in DETAILED MISASSEMBLIES REPORT (<quast_output_dir>/contigs_reports/misassemblies_report.txt, .tex, .tsv)
    misassemblies_order = [NAME, MIS_ALL_EXTENSIVE, MIS_RELOCATION, MIS_TRANSLOCATION, MIS_INVERTION,
                           MIS_ISTRANSLOCATIONS, MIS_EXTENSIVE_CONTIGS, MIS_EXTENSIVE_BASES,
                           CONTIGS_WITH_ISTRANSLOCATIONS, POSSIBLE_MISASSEMBLIES,
                           MIS_LOCAL, MIS_SCAFFOLDS_GAP, MIS_FRAGMENTED, STRUCT_VARIATIONS, UNALIGNED_MISASSEMBLED_CTGS,
                           MISMATCHES, INDELS, MIS_SHORT_INDELS, MIS_LONG_INDELS, INDELSBASES]

    # content and order of metrics in DETAILED UNALIGNED REPORT (<quast_output_dir>/contigs_reports/unaligned_report.txt, .tex, .tsv)
    unaligned_order = [NAME, UNALIGNED_FULL_CNTGS, UNALIGNED_FULL_LENGTH, UNALIGNED_PART_CNTGS,
                       UNALIGNED_PART_LENGTH, UNCALLED]

    ### list of GAGE metrics (--gage option)
    GAGE_NUMCONTIGS = 'Contigs #'
    GAGE_MINCONTIG = 'Min contig'
    GAGE_MAXCONTIG = 'Max contig'
    GAGE_N50 = 'Not corrected N50'
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

    # content and order of metrics in GAGE report (<quast_output_dir>/gage_report.txt, .tex, .tsv)
    gage_order = [NAME, GAGE_NUMCONTIGS, GAGE_MINCONTIG, GAGE_MAXCONTIG, GAGE_N50, GAGE_GENOMESIZE, GAGE_ASSEMBLY_SIZE,
                  GAGE_CHAFFBASES, GAGE_MISSINGREFBASES, GAGE_MISSINGASMBLYBASES, GAGE_MISSINGASMBLYCONTIGS, GAGE_DUPREFBASES,
                  GAGE_COMPRESSEDREFBASES, GAGE_BADTRIM, GAGE_AVGIDY, GAGE_SNPS, GAGE_SHORTINDELS, GAGE_LONGINDELS, GAGE_INVERSIONS,
                  GAGE_RELOCATION, GAGE_TRANSLOCATION, GAGE_NUMCORCONTIGS, GAGE_CORASMBLYSIZE, GAGE_MINCORCONTIG, GAGE_MAXCORCOTING,
                  GAGE_CORN50]

    ### Grouping of metrics and set of main metrics for HTML version of main report
    grouped_order = [
        ('Genome statistics', [MAPPEDGENOME, DUPLICATION_RATIO, AVE_READ_SUPPORT, GENES, OPERONS, LARGALIGN, TOTAL_ALIGNED_LEN,
                               NG50, NG75, NA50, NA75, NGA50, NGA75, LG50, LG75, LA50, LA75, LGA50, LGA75,]),

        ('Misassemblies', [MIS_ALL_EXTENSIVE,
                           MIS_RELOCATION, MIS_TRANSLOCATION, MIS_INVERTION,
                           MIS_ISTRANSLOCATIONS, MIS_EXTENSIVE_CONTIGS, MIS_EXTENSIVE_BASES,
                           CONTIGS_WITH_ISTRANSLOCATIONS, POSSIBLE_MISASSEMBLIES,
                           MIS_LOCAL, MIS_SCAFFOLDS_GAP, STRUCT_VARIATIONS, UNALIGNED_MISASSEMBLED_CTGS]),

        ('Unaligned', [UNALIGNED_FULL_CNTGS, UNALIGNED_FULL_LENGTH, UNALIGNED_PART_CNTGS,
                       UNALIGNED_PART_LENGTH, ]),

        ('Mismatches', [MISMATCHES, INDELS, INDELSBASES, SUBSERROR, INDELSERROR,
                        MIS_SHORT_INDELS, MIS_LONG_INDELS, UNCALLED, UNCALLED_PERCENT,]),

        ('Statistics without reference', [CONTIGS, CONTIGS__FOR_THRESHOLDS, LARGCONTIG, TOTALLEN, TOTALLENS__FOR_THRESHOLDS,
                                          N50, N75, L50, L75, GC,]),

        ('Predicted genes', [PREDICTED_GENES_UNIQUE, PREDICTED_GENES,]),

        ('Similarity statistics', [SIMILAR_CONTIGS, SIMILAR_MIS_BLOCKS]),
        
        ('Reference statistics', [REFLEN, ESTREFLEN, REF_FRAGMENTS, REFGC, REF_GENES, REF_OPERONS,])
    ]

    # for "short" version of HTML report
    main_metrics = [CONTIGS, LARGCONTIG, TOTALLEN, LARGALIGN, TOTAL_ALIGNED_LEN,
                    TOTALLENS__FOR_1000_THRESHOLD, TOTALLENS__FOR_10000_THRESHOLD, TOTALLENS__FOR_50000_THRESHOLD,
                    MIS_ALL_EXTENSIVE, MIS_EXTENSIVE_BASES,
                    SUBSERROR, INDELSERROR, UNCALLED_PERCENT,
                    MAPPEDGENOME, DUPLICATION_RATIO, AVE_READ_SUPPORT, GENES, OPERONS, NGA50, LGA50,
                    PREDICTED_GENES_UNIQUE, PREDICTED_GENES,]

####################################################################################
########################  END OF CONFIGURABLE PARAMETERS  ##########################
####################################################################################

    class Quality:
        MORE_IS_BETTER = 'More is better'
        LESS_IS_BETTER = 'Less is better'
        EQUAL = 'Equal'

    quality_dict = {
        Quality.MORE_IS_BETTER:
            [LARGCONTIG, TOTALLEN, TOTALLENS__FOR_THRESHOLDS, TOTALLENS__FOR_1000_THRESHOLD, TOTALLENS__FOR_10000_THRESHOLD,
             TOTALLENS__FOR_50000_THRESHOLD, LARGALIGN, TOTAL_ALIGNED_LEN,
             N50, NG50, N75, NG75, NA50, NGA50, NA75, NGA75,
             MAPPEDGENOME, AVE_READ_SUPPORT, GENES, OPERONS, PREDICTED_GENES_UNIQUE, PREDICTED_GENES],
        Quality.LESS_IS_BETTER:
            [L50, LG50, L75, LG75,
             MISLOCAL, MISASSEMBL, MISCONTIGS, MISCONTIGSBASES, MISINTERNALOVERLAP,
             CONTIGS_WITH_ISTRANSLOCATIONS, POSSIBLE_MISASSEMBLIES,
             UNALIGNED, UNALIGNEDBASES, AMBIGUOUS, AMBIGUOUSEXTRABASES,
             UNCALLED, UNCALLED_PERCENT,
             LA50, LGA50, LA75, LGA75, DUPLICATION_RATIO, INDELS, INDELSERROR, MISMATCHES, SUBSERROR,
             MIS_SHORT_INDELS, MIS_LONG_INDELS, INDELSBASES],
        Quality.EQUAL:
            [REFLEN, ESTREFLEN, GC, CONTIGS, CONTIGS__FOR_THRESHOLDS, REFGC, STRUCT_VARIATIONS, SIMILAR_CONTIGS, SIMILAR_MIS_BLOCKS],
        }

    # Old Python2 only version
    #for name, metrics in filter(lambda (name, metrics): name in ['Misassemblies', 'Unaligned'], grouped_order):
    #    quality_dict['Less is better'].extend(metrics)
    # Python3 version
    for metrics in [v for name,v in grouped_order if name in ["Misassemblies", "Unaligned"]]:
        quality_dict['Less is better'].extend(metrics)



#################################################


from quast_libs.log import get_logger
logger = get_logger(qconfig.LOGGER_DEFAULT_NAME)

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

reports = {}  # basefilename -> Report
assembly_fpaths = []  # for printing in appropriate order

#################################################


def _mapme(func, data):
    return [func(this) for this in data]


def get_main_metrics():
    lists = _mapme(take_tuple_metric_apart, Fields.main_metrics)
    m_metrics = []
    for l in lists:
        for m in l:
            m_metrics.append(m)
    return m_metrics


def take_tuple_metric_apart(field):
    metrics = []

    if isinstance(field, tuple): # TODO: rewrite it nicer
        thresholds = _mapme(int, ''.join(field[1]).split(','))
        for i, feature in enumerate(thresholds):
            metrics.append(field[0] % feature)
    else:
        metrics = [field]

    return metrics


def get_quality(metric):
    for quality, metrics in Fields.quality_dict.items():
        if metric in Fields.quality_dict[quality]:
            return quality
    return Fields.Quality.EQUAL


# Report for one filename, dict: field -> value
class Report(object):
    def __init__(self, name):
        self.d = {}
        self.add_field(Fields.NAME, name)

    def add_field(self, field, value):
        try:
            assert field in Fields.__dict__.itervalues(), 'Unknown field: %s' % field
        except:
            assert field in Fields.__dict__.values(), 'Unknown field: %s' % field

        self.d[field] = value

    def append_field(self, field, value):
        try:
            assert field in Fields.__dict__.itervalues(), 'Unknown field: %s' % field
        except:
            assert field in Fields.__dict__.values(), 'Unknown field: %s' % field
        self.d.setdefault(field, []).append(value)

    def get_field(self, field):
        try:
            assert field in Fields.__dict__.itervalues(), 'Unknown field: %s' % field
        except:
            assert field in Fields.__dict__.values(), 'Unknown field: %s' % field
        return self.d.get(field, None)


def get(assembly_fpath, ref_name=None):
    if not ref_name and qconfig.reference:
        ref_name = qutils.name_from_fpath(qconfig.reference)
    if assembly_fpath not in assembly_fpaths:
        assembly_fpaths.append(assembly_fpath)
    return reports.setdefault((os.path.abspath(assembly_fpath), ref_name), Report(qutils.label_from_fpath(assembly_fpath)))


def delete(assembly_fpath):
    if assembly_fpath in assembly_fpaths:
        assembly_fpaths.remove(assembly_fpath)
    if assembly_fpath in reports.keys():
        reports.pop(assembly_fpath)


# ATTENTION! Contents numeric values, needed to be converted into strings
def table(order=Fields.order, ref_name=None):
    if not isinstance(order[0], tuple):  # is not a groupped metrics order
        order = [('', order)]

    table = []

    def append_line(rows, field, are_multiple_thresholds=False, pattern=None, feature=None, i=None, ref_name=None):
        quality = get_quality(field)
        values = []

        for assembly_fpath in assembly_fpaths:
            report = get(assembly_fpath, ref_name=ref_name)
            value = report.get_field(field)

            if are_multiple_thresholds:
                values.append(value[i] if (value and i < len(value)) else None)
            else:
                values.append(value)

        if list(filter(lambda v: v is not None, values)) or \
                (field == 'NGA50' and not qconfig.is_combined_ref and report.get_field(Fields.REFLEN)):

            metric_name = field if (feature is None) else pattern % int(feature)
            # ATTENTION! Contents numeric values, needed to be converted to strings.
            rows.append({
                'metricName': metric_name,
                'quality': quality,
                'values': values,
                'isMain': metric_name in Fields.main_metrics,
            })

    for group_name, metrics in order:
        rows = []
        table.append((group_name, rows))

        for field in metrics:
            if isinstance(field, tuple):  # TODO: rewrite it nicer
                for i, feature in enumerate(field[1]):
                    append_line(rows, field, are_multiple_thresholds=True, pattern=field[0], feature=feature, i=i,
                                ref_name=ref_name)
            else:
                append_line(rows, field, ref_name=ref_name)

    if not isinstance(order[0], tuple):  # is not a groupped metrics order
        group_name, rows = table[0]
        return rows
    else:
        return table


def is_groupped_table(table):
    return isinstance(table[0], tuple)


def get_all_rows_out_of_table(table):
    all_rows = []
    if is_groupped_table(table):
        for group_name, rows in table:
            all_rows += rows
    else:
        all_rows = table

    return all_rows


def save_txt(fpath, all_rows, potential_scaffolds_assemblies_info=None):
    # determine width of columns for nice spaces
    colwidths = [0] * (len(all_rows[0]['values']) + 1)
    for row in all_rows:
        for i, cell in enumerate([row['metricName']] + [val_to_str(this) for
                                                        this in row['values']]):
            colwidths[i] = max(colwidths[i], len(cell))
            # output it

    txt_file = open(fpath, 'w')

    if qconfig.min_contig:
        txt_file.write('All statistics are based on contigs of size >= %d bp, unless otherwise noted ' % qconfig.min_contig + \
                          '(e.g., "# contigs (>= 0 bp)" and "Total length (>= 0 bp)" include all contigs).\n')

        if potential_scaffolds_assemblies_info:
            txt_file.write("Suggestion: " + potential_scaffolds_assemblies_info + " continuous fragments of N's of " \
                          "length >= 10 bp. You may consider rerunning QUAST using --scaffolds (-s) option!\n")
        txt_file.write('\n')
    for row in all_rows:
        txt_file.write('  '.join('%-*s' % (colwidth, cell) for colwidth, cell
            in zip(colwidths, [row['metricName']] + [val_to_str(this) for this in row['values']])) + "\n")

    txt_file.close()


def save_tsv(fpath, all_rows):
    tsv_file = open(fpath, 'w')


    for row in all_rows:
        tsv_file.write('\t'.join([row['metricName']] + [val_to_str(this) for
                this in row['values']]) + "\n")
    tsv_file.close()


def parse_number(val):
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
    if isinstance(val, int) or isinstance(val, float):
        num = val

    elif isinstance(val, basestring) and len(val.split()) > 0:
                                                                      # 'x + y part' format?
        tokens = val.split()                                          # tokens = [x, +, y, part]
        if len(tokens) >= 3:                                          # Yes, 'y + x part' format
            x, y = parse_number(tokens[0]), parse_number(tokens[2])
            if x is None or y is None:
                num = None
            else:
                num = (x, y)                                          # Tuple value. Can be compared lexicographically.
        else:
            num = parse_number(tokens[0])
    else:
        num = val

    return num


def save_tex(fpath, all_rows, is_transposed=False):
    tex_file = open(fpath, 'w')
    # Header
    tex_file.write('\\documentclass[12pt,a4paper]{article}\n')
    tex_file.write('\\begin{document}\n')
    tex_file.write('\\begin{table}[ht]\n')
    tex_file.write('\\begin{center}\n')
    tex_file.write('\\caption{All statistics are based on contigs of size $\geq$ %d bp, unless otherwise noted ' % qconfig.min_contig + \
                      '(e.g., "\# contigs ($\geq$ 0 bp)" and "Total length ($\geq$ 0 bp)" include all contigs).}\n')

    rows_n = len(all_rows[0]['values'])
    tex_file.write('\\begin{tabular}{|l*{' + val_to_str(rows_n) + '}{|r}|}\n')
    tex_file.write('\\hline\n')

    # Body
    for row in all_rows:
        values = row['values']
        quality = row['quality'] if ('quality' in row) else Fields.Quality.EQUAL

        if is_transposed or quality not in [Fields.Quality.MORE_IS_BETTER, 
                Fields.Quality.LESS_IS_BETTER]:
            cells = _mapme(val_to_str, values)
        else:
            # Checking the first value, assuming the others are the same type and format
            num = get_num_from_table_value(values[0])
            if num is None:  # Not a number
                cells = _mapme(val_to_str, values)
            else:
                nums = _mapme(get_num_from_table_value, values)
                best = None
                if quality == Fields.Quality.MORE_IS_BETTER:
                    best = max(n for n in nums if n is not None)
                if quality == Fields.Quality.LESS_IS_BETTER:
                    best = min(n for n in nums if n is not None)

                if len([num for num in nums if num != best]) == 0:
                    cells = _mapme(val_to_str, values)
                else:
                    cells = ['HIGHLIGHTEDSTART' + val_to_str(v) + 'HIGHLIGHTEDEND'
                             if get_num_from_table_value(v) == best
                             else val_to_str(v)
                             for v in values]

        row = ' & '.join([row['metricName']] + cells)
        # escape characters
        for esc_char in "\\ % $ # _ { } ~ ^".split():
            row = row.replace(esc_char, '\\' + esc_char)
        # more pretty '>=' and '<=', '>'
        row = row.replace('>=', '$\\geq$')
        row = row.replace('<=', '$\\leq$')
        row = row.replace('>', '$>$')
        # pretty indent
        if row.startswith(Fields.TAB):
            row = "\hspace{5mm}" + row.lstrip()
        # pretty highlight
        row = row.replace('HIGHLIGHTEDSTART', '{\\bf ')
        row = row.replace('HIGHLIGHTEDEND', '}')
        row += ' \\\\ \\hline'
        tex_file.write(row +"\n")

    # Footer
    tex_file.write('\\end{tabular}\n')
    tex_file.write('\\end{center}\n')
    tex_file.write('\\end{table}\n')
    tex_file.write('\\end{document}\n')
    tex_file.close()

    if os.path.basename(fpath) == 'report.tex':
        pass


def save_pdf(report_name, table):
    if not qconfig.draw_plots:
        return

    all_rows = get_all_rows_out_of_table(table)

    column_widths = [0] * (len(all_rows[0]['values']) + 1)
    for row in all_rows:
        for i, cell in enumerate([row['metricName']] + _mapme(val_to_str, row['values'])):
            column_widths[i] = max(column_widths[i], len(cell))

    if qconfig.min_contig:
        extra_info = 'All statistics are based on contigs of size >= %d bp, unless otherwise noted ' % qconfig.min_contig +\
                     '\n(e.g., "# contigs (>= 0 bp)" and "Total length (>= 0 bp)" include all contigs).'
    else:
        extra_info = ''
    table_to_draw = []
    for row in all_rows:
        table_to_draw.append(['%s' % cell for cell
            in [row['metricName']] + _mapme(val_to_str, row['values'])])
    from quast_libs import plotter
    plotter.draw_report_table(report_name, extra_info, table_to_draw, column_widths)


def save(output_dirpath, report_name, transposed_report_name, order, silent=False):
    # Where total report will be saved
    tab = table(order)

    if not silent:
        logger.info('  Creating total report...')
    report_txt_fpath = os.path.join(output_dirpath, report_name) + '.txt'
    report_tsv_fpath = os.path.join(output_dirpath, report_name) + '.tsv'
    report_tex_fpath = os.path.join(output_dirpath, report_name) + '.tex'

    all_rows = get_all_rows_out_of_table(tab)
    potential_scaffolds_assemblies_info = ''
    if qconfig.potential_scaffolds_assemblies:
        if len(qconfig.potential_scaffolds_assemblies) == 1:
            potential_scaffolds_assemblies_info = 'assembly ' + qconfig.potential_scaffolds_assemblies[0] + ' contains'
        elif len(qconfig.potential_scaffolds_assemblies) == len(all_rows[0]['values']):
            potential_scaffolds_assemblies_info = 'all assemblies contain'
        else:
            potential_scaffolds_assemblies_info = 'assemblies ' + ', '.join(qconfig.potential_scaffolds_assemblies) + ' contain'
    save_txt(report_txt_fpath, all_rows, potential_scaffolds_assemblies_info)
    save_tsv(report_tsv_fpath, all_rows)
    save_tex(report_tex_fpath, all_rows)
    save_pdf(report_name, tab)
    reports_fpaths = report_txt_fpath + ', ' + os.path.basename(report_tsv_fpath) + ', and ' + \
                     os.path.basename(report_tex_fpath)
    transposed_reports_fpaths = None
    if not silent:
        logger.info('    saved to ' + reports_fpaths)

    if transposed_report_name:
        if not silent:
            logger.info('  Transposed version of total report...')

        all_rows = get_all_rows_out_of_table(tab)
        if all_rows[0]['metricName'] != Fields.NAME:
            logger.warning('transposed version can\'t be created! First column have to be assemblies names')
        else:
            # Transposing table
            transposed_table = [{'metricName': all_rows[0]['metricName'],
                                 'values': [all_rows[i]['metricName'] for i in range(1, len(all_rows))],}]
            for i in range(len(all_rows[0]['values'])):
                values = []
                for j in range(1, len(all_rows)):
                    values.append(all_rows[j]['values'][i])
                transposed_table.append({'metricName': all_rows[0]['values'][i], # name of assembly, assuming the first line is assemblies names
                                         'values': values,})

            report_txt_fpath = os.path.join(output_dirpath, transposed_report_name) + '.txt'
            report_tsv_fpath = os.path.join(output_dirpath, transposed_report_name) + '.tsv'
            report_tex_fpath = os.path.join(output_dirpath, transposed_report_name) + '.tex'
            all_rows = get_all_rows_out_of_table(transposed_table)
            save_txt(report_txt_fpath, all_rows, potential_scaffolds_assemblies_info)
            save_tsv(report_tsv_fpath, all_rows)
            save_tex(report_tex_fpath, all_rows, is_transposed=True)
            transposed_reports_fpaths = report_txt_fpath + ', ' + os.path.basename(report_tsv_fpath) + \
                                        ', and ' + os.path.basename(report_tex_fpath)
            if not silent:
                logger.info('    saved to ' + transposed_reports_fpaths)
    return reports_fpaths, transposed_reports_fpaths


def save_gage(output_dirpath):
    save(output_dirpath, qconfig.gage_report_prefix + qconfig.report_prefix,
         qconfig.gage_report_prefix + qconfig.transposed_report_prefix, Fields.gage_order)


def save_total(output_dirpath, silent=True):
    if not silent:
        logger.print_timestamp()
        logger.info('Summarizing...')
    return save(output_dirpath, qconfig.report_prefix, qconfig.transposed_report_prefix, Fields.order, silent=silent)


def save_misassemblies(output_dirpath):
    save(output_dirpath, "misassemblies_report", qconfig.transposed_report_prefix + "_misassemblies", Fields.misassemblies_order)


def save_unaligned(output_dirpath):
    save(output_dirpath, "unaligned_report", "", Fields.unaligned_order)
