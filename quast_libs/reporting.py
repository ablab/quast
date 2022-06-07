############################################################################
# Copyright (c) 2015-2022 Saint Petersburg State University
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
    HALF_TAB = '  '

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
    auN = 'auN'
    auNG = 'auNG'
    auNA = 'auNA'
    auNGA = 'auNGA'
    Nx = 'N%d' % qconfig.x_for_additional_Nx
    L50 = 'L50'
    Lx = 'L%d' % qconfig.x_for_additional_Nx
    GC = 'GC (%)'

    # Read statistics
    MAPPED_READS = '# mapped'
    MAPPED_READS_PCNT = 'Mapped (%)'
    PROPERLY_PAIRED_READS = '# properly paired'
    PROPERLY_PAIRED_READS_PCNT = 'Properly paired (%)'
    SINGLETONS = '# singletons'
    SINGLETONS_PCNT = 'Singletons (%)'
    MISJOINT_READS = '# misjoint mates'
    MISJOINT_READS_PCNT = 'Misjoint mates (%)'
    DEPTH = 'Avg. coverage depth'
    COVERAGE__FOR_THRESHOLDS = ('Coverage >= %dx (%%)', tuple(qconfig.coverage_thresholds))
    COVERAGE_1X_THRESHOLD = 'Coverage >= 1x (%)'
    COVERAGE_5X_THRESHOLD = 'Coverage >= 5x (%)'
    COVERAGE_10X_THRESHOLD = 'Coverage >= 10x (%)'

    # Misassemblies
    MISASSEMBL = '# misassemblies'
    MISCONTIGS = '# misassembled contigs'
    MISCONTIGSBASES = 'Misassembled contigs length'
    MISINTERNALOVERLAP = 'Misassemblies inter-contig overlap'
    ### additional list of metrics for detailed misassemblies report
    LARGE_MIS_EXTENSIVE = '# large blocks misassemblies'
    LARGE_MIS_RELOCATION = TAB + '# large relocations'
    LARGE_MIS_TRANSLOCATION = TAB + '# large translocations'
    LARGE_MIS_INVERTION = TAB + '# large inversions'
    LARGE_MIS_ISTRANSLOCATIONS = TAB + '# large i/s translocations'
    ### additional list of metrics for detailed misassemblies report
    MIS_ALL_EXTENSIVE = '# misassemblies'
    MIS_RELOCATION = TAB + '# relocations'
    MIS_TRANSLOCATION = TAB + '# translocations'
    MIS_INVERTION = TAB + '# inversions'
    MIS_ISTRANSLOCATIONS = TAB + '# interspecies translocations'
    MIS_EXTENSIVE_CONTIGS = '# misassembled contigs'
    MIS_EXTENSIVE_BASES = 'Misassembled contigs length'
    MIS_LOCAL = '# local misassemblies'
    MIS_SCAFFOLDS_GAP = '# scaffold gap ext. mis.'
    MIS_LOCAL_SCAFFOLDS_GAP = '# scaffold gap loc. mis.'
    MIS_FRAGMENTED = '# misassemblies caused by fragmented reference'
    CONTIGS_WITH_ISTRANSLOCATIONS = '# possibly misassembled contigs'
    POSSIBLE_MISASSEMBLIES = TAB + '# possible misassemblies'
    POTENTIAL_MGE = '# possible TEs'  # former MGEs -- mobile genetic elements
    ### structural variations
    STRUCT_VARIATIONS = '# structural variations'

    # Special case: metrics for separating Contig/Scaffold misassemblies in detailed misassemblies report
    CTG_MIS_ALL_EXTENSIVE = HALF_TAB + '# contig misassemblies'
    CTG_MIS_RELOCATION = TAB + '# c. relocations'
    CTG_MIS_TRANSLOCATION = TAB + '# c. translocations'
    CTG_MIS_INVERTION = TAB + '# c. inversions'
    CTG_MIS_ISTRANSLOCATIONS = TAB + '# c. interspecies translocations'
    SCF_MIS_ALL_EXTENSIVE = HALF_TAB + '# scaffold misassemblies'
    SCF_MIS_RELOCATION = TAB + '# s. relocations'
    SCF_MIS_TRANSLOCATION = TAB + '# s. translocations'
    SCF_MIS_INVERTION = TAB + '# s. inversions'
    SCF_MIS_ISTRANSLOCATIONS = TAB + '# s. interspecies translocations'

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
    GENES = '# genomic features'
    OPERONS = '# operons'
    LARGALIGN = 'Largest alignment'
    TOTAL_ALIGNED_LEN = 'Total aligned length'
    NG50 = 'NG50'
    NA50 = 'NA50'
    NGA50 = 'NGA50'
    LG50 = 'LG50'
    LA50 = 'LA50'
    LGA50 = 'LGA50'
    NGx = 'NG%d' % qconfig.x_for_additional_Nx
    NAx = 'NA%d' % qconfig.x_for_additional_Nx
    NGAx = 'NGA%d' % qconfig.x_for_additional_Nx
    LGx = 'LG%d' % qconfig.x_for_additional_Nx
    LAx = 'LA%d' % qconfig.x_for_additional_Nx
    LGAx = 'LGA%d' % qconfig.x_for_additional_Nx

    # Unique k-mer statistics
    KMER_COMPLETENESS = 'K-mer-based compl. (%)'
    KMER_CORR_LENGTH = 'K-mer-based cor. length (%)'
    KMER_MIS_LENGTH = 'K-mer-based mis. length (%)'
    KMER_UNDEF_LENGTH = 'K-mer-based undef. length (%)'
    KMER_MISASSEMBLIES = '# k-mer-based misjoins'
    KMER_TRANSLOCATIONS = TAB + '# k-mer-based translocations'
    KMER_RELOCATIONS = TAB + '# k-mer-based 100kbp relocations'

    # Predicted genes
    PREDICTED_GENES_UNIQUE = '# predicted genes (unique)'
    PREDICTED_GENES = ('# predicted genes (>= %d bp)', tuple(qconfig.genes_lengths))
    RNA_GENES = '# predicted rRNA genes'
    BUSCO_COMPLETE = 'Complete BUSCO (%)'
    BUSCO_PART = 'Partial BUSCO (%)'

    # Reference statistics
    REFLEN = 'Reference length'
    ESTREFLEN = 'Estimated reference length'
    REF_FRAGMENTS = 'Reference fragments'
    REFGC = 'Reference GC (%)'
    REF_GENES = 'Reference genomic features'
    REF_OPERONS = 'Reference operons'
    # Reads statistics
    TOTAL_READS = '# total reads'
    LEFT_READS = '# left'
    RIGHT_READS = '# right'
    REF_MAPPED_READS = '# reference mapped'
    REF_MAPPED_READS_PCNT = 'Reference mapped (%)'
    REF_PROPERLY_PAIRED_READS = '# reference properly paired'
    REF_PROPERLY_PAIRED_READS_PCNT = 'Reference properly paired (%)'
    REF_SINGLETONS = '# reference singletons'
    REF_SINGLETONS_PCNT = 'Reference singletons (%)'
    REF_MISJOINT_READS = '# reference misjoint mates'
    REF_MISJOINT_READS_PCNT = 'Reference misjoint mates (%)'
    REF_DEPTH = 'Reference avg. coverage depth'
    REF_COVERAGE__FOR_THRESHOLDS = ('Reference coverage >= %dx (%%)', tuple(qconfig.coverage_thresholds))
    REF_COVERAGE_1X_THRESHOLD = 'Reference coverage >= 1x (%)'
    REF_COVERAGE_5X_THRESHOLD = 'Reference coverage >= 5x (%)'
    REF_COVERAGE_10X_THRESHOLD = 'Reference coverage >= 10x (%)'

    # Icarus statistics
    SIMILAR_CONTIGS = '# similar correct contigs'
    SIMILAR_MIS_BLOCKS = '# similar misassembled blocks'

    ### content and order of metrics in MAIN REPORT (<quast_output_dir>/report.txt, .tex, .tsv):
    order = [NAME, CONTIGS__FOR_THRESHOLDS, TOTALLENS__FOR_THRESHOLDS, CONTIGS, LARGCONTIG, TOTALLEN, REFLEN, ESTREFLEN, GC, REFGC,
             N50, NG50, Nx, NGx, auN, auNG, L50, LG50, Lx, LGx,
             TOTAL_READS, LEFT_READS, RIGHT_READS,
             MAPPED_READS_PCNT, REF_MAPPED_READS_PCNT,
             PROPERLY_PAIRED_READS_PCNT, REF_PROPERLY_PAIRED_READS_PCNT,
             DEPTH, REF_DEPTH, COVERAGE_1X_THRESHOLD, REF_COVERAGE_1X_THRESHOLD,
             LARGE_MIS_EXTENSIVE, MISASSEMBL, MISCONTIGS, MISCONTIGSBASES,
             MISLOCAL, MIS_SCAFFOLDS_GAP, MIS_LOCAL_SCAFFOLDS_GAP,
             STRUCT_VARIATIONS, POTENTIAL_MGE, UNALIGNED_MISASSEMBLED_CTGS,
             UNALIGNED, UNALIGNEDBASES, MAPPEDGENOME, DUPLICATION_RATIO, AVE_READ_SUPPORT,
             UNCALLED_PERCENT, SUBSERROR, INDELSERROR, GENES, OPERONS,
             BUSCO_COMPLETE, BUSCO_PART,
             PREDICTED_GENES_UNIQUE, PREDICTED_GENES, RNA_GENES,
             LARGALIGN, TOTAL_ALIGNED_LEN, NA50, NGA50, NAx, NGAx, auNA, auNGA, LA50, LGA50, LAx, LGAx,
             KMER_COMPLETENESS, KMER_CORR_LENGTH, KMER_MIS_LENGTH, KMER_MISASSEMBLIES]

    reads_order = [NAME, TOTAL_READS, LEFT_READS, RIGHT_READS,
                   MAPPED_READS, MAPPED_READS_PCNT, PROPERLY_PAIRED_READS, PROPERLY_PAIRED_READS_PCNT,
                   SINGLETONS, SINGLETONS_PCNT, MISJOINT_READS, MISJOINT_READS_PCNT,
                   DEPTH, COVERAGE__FOR_THRESHOLDS,
                   REF_MAPPED_READS, REF_MAPPED_READS_PCNT, REF_PROPERLY_PAIRED_READS, REF_PROPERLY_PAIRED_READS_PCNT,
                   REF_SINGLETONS, REF_SINGLETONS_PCNT, REF_MISJOINT_READS, REF_MISJOINT_READS_PCNT,
                   REF_DEPTH, REF_COVERAGE__FOR_THRESHOLDS]

    # content and order of metrics in DETAILED MISASSEMBLIES REPORT (<quast_output_dir>/contigs_reports/misassemblies_report.txt, .tex, .tsv)
    classic_misassemblies_order = [MIS_RELOCATION, MIS_TRANSLOCATION, MIS_INVERTION, MIS_ISTRANSLOCATIONS]
    ctg_scf_misassemblies_order = [CTG_MIS_ALL_EXTENSIVE,
                                   CTG_MIS_RELOCATION, CTG_MIS_TRANSLOCATION, CTG_MIS_INVERTION, CTG_MIS_ISTRANSLOCATIONS,
                                   SCF_MIS_ALL_EXTENSIVE,
                                   SCF_MIS_RELOCATION, SCF_MIS_TRANSLOCATION, SCF_MIS_INVERTION, SCF_MIS_ISTRANSLOCATIONS]
    prefix_misassemblies_order = [NAME, MIS_ALL_EXTENSIVE]
    suffix_misassemblies_order = [MIS_EXTENSIVE_CONTIGS, MIS_EXTENSIVE_BASES,
                                  CONTIGS_WITH_ISTRANSLOCATIONS, POSSIBLE_MISASSEMBLIES,
                                  MIS_LOCAL, MIS_SCAFFOLDS_GAP, MIS_LOCAL_SCAFFOLDS_GAP,
                                  MIS_FRAGMENTED, STRUCT_VARIATIONS, POTENTIAL_MGE, UNALIGNED_MISASSEMBLED_CTGS,
                                  MISMATCHES, INDELS, MIS_SHORT_INDELS, MIS_LONG_INDELS, INDELSBASES]
    misassemblies_order = prefix_misassemblies_order + classic_misassemblies_order + suffix_misassemblies_order
    misassemblies_order_advanced = prefix_misassemblies_order + ctg_scf_misassemblies_order + suffix_misassemblies_order

    # content and order of metrics in DETAILED UNALIGNED REPORT (<quast_output_dir>/contigs_reports/unaligned_report.txt, .tex, .tsv)
    unaligned_order = [NAME, UNALIGNED_FULL_CNTGS, UNALIGNED_FULL_LENGTH, UNALIGNED_PART_CNTGS,
                       UNALIGNED_PART_LENGTH, UNCALLED]

    kmers_order = [NAME, KMER_COMPLETENESS, KMER_CORR_LENGTH, KMER_MIS_LENGTH, KMER_UNDEF_LENGTH,
                   KMER_MISASSEMBLIES, KMER_TRANSLOCATIONS, KMER_RELOCATIONS]

    ### Grouping of metrics and set of main metrics for HTML version of main report
    grouped_order = [
        ('Genome statistics', [MAPPEDGENOME, DUPLICATION_RATIO, AVE_READ_SUPPORT, GENES, OPERONS,
                               LARGALIGN, TOTAL_ALIGNED_LEN,
                               NG50, NGx, auNG, NA50, NAx, auNA, NGA50, NGAx, auNGA,
                               LG50, LGx, LA50, LAx, LGA50, LGAx, BUSCO_COMPLETE, BUSCO_PART]),

        ('Reads mapping', [MAPPED_READS, MAPPED_READS_PCNT, PROPERLY_PAIRED_READS, PROPERLY_PAIRED_READS_PCNT,
                           SINGLETONS, SINGLETONS_PCNT, MISJOINT_READS, MISJOINT_READS_PCNT,
                           DEPTH, COVERAGE__FOR_THRESHOLDS]),

        ('Misassemblies', [LARGE_MIS_EXTENSIVE, LARGE_MIS_RELOCATION, LARGE_MIS_TRANSLOCATION, LARGE_MIS_INVERTION,
                           LARGE_MIS_ISTRANSLOCATIONS,
                           MIS_ALL_EXTENSIVE, MIS_RELOCATION, MIS_TRANSLOCATION, MIS_INVERTION,
                           MIS_ISTRANSLOCATIONS, MIS_EXTENSIVE_CONTIGS, MIS_EXTENSIVE_BASES,
                           CONTIGS_WITH_ISTRANSLOCATIONS, POSSIBLE_MISASSEMBLIES,
                           MIS_LOCAL, MIS_SCAFFOLDS_GAP, MIS_LOCAL_SCAFFOLDS_GAP,
                           STRUCT_VARIATIONS, POTENTIAL_MGE, UNALIGNED_MISASSEMBLED_CTGS]),

        ('Unaligned', [UNALIGNED_FULL_CNTGS, UNALIGNED_FULL_LENGTH, UNALIGNED_PART_CNTGS,
                       UNALIGNED_PART_LENGTH, ]),

        ('Mismatches', [SUBSERROR, MISMATCHES,
                        INDELSERROR, INDELS, MIS_SHORT_INDELS, MIS_LONG_INDELS, INDELSBASES,
                        UNCALLED_PERCENT, UNCALLED, ]),

        ('Statistics without reference', [CONTIGS, CONTIGS__FOR_THRESHOLDS, LARGCONTIG, TOTALLEN, TOTALLENS__FOR_THRESHOLDS,
                                          N50, Nx, auN, L50, Lx, GC,]),

        ('K-mer-based statistics', [KMER_COMPLETENESS, KMER_CORR_LENGTH, KMER_MIS_LENGTH, KMER_UNDEF_LENGTH,
                                    KMER_MISASSEMBLIES, KMER_TRANSLOCATIONS, KMER_RELOCATIONS]),

        ('Predicted genes', [PREDICTED_GENES_UNIQUE, PREDICTED_GENES, RNA_GENES]),

        ('Similarity statistics', [SIMILAR_CONTIGS, SIMILAR_MIS_BLOCKS]),
        
        ('Reference statistics', [REFLEN, ESTREFLEN, REF_FRAGMENTS, REFGC, REF_GENES, REF_OPERONS,
                                  TOTAL_READS, LEFT_READS, RIGHT_READS,
                                  REF_MAPPED_READS, REF_MAPPED_READS_PCNT, REF_PROPERLY_PAIRED_READS, REF_PROPERLY_PAIRED_READS_PCNT,
                                  REF_SINGLETONS, REF_SINGLETONS_PCNT, REF_MISJOINT_READS, REF_MISJOINT_READS_PCNT,
                                  REF_DEPTH, REF_COVERAGE__FOR_THRESHOLDS])
    ]

    # for "short" version of HTML report
    main_metrics = [CONTIGS, LARGCONTIG, TOTALLEN, LARGALIGN, TOTAL_ALIGNED_LEN,
                    TOTALLENS__FOR_1000_THRESHOLD, TOTALLENS__FOR_10000_THRESHOLD, TOTALLENS__FOR_50000_THRESHOLD,
                    LARGE_MIS_EXTENSIVE, MIS_ALL_EXTENSIVE, MIS_EXTENSIVE_BASES,
                    SUBSERROR, INDELSERROR, UNCALLED_PERCENT,
                    MAPPEDGENOME, DUPLICATION_RATIO, AVE_READ_SUPPORT,
                    MAPPED_READS_PCNT, PROPERLY_PAIRED_READS_PCNT, SINGLETONS_PCNT, MISJOINT_READS_PCNT,
                    GENES, OPERONS,
                    BUSCO_COMPLETE, BUSCO_PART,
                    NGA50, LGA50,
                    PREDICTED_GENES_UNIQUE, PREDICTED_GENES, RNA_GENES,
                    DEPTH, COVERAGE_1X_THRESHOLD, KMER_COMPLETENESS, KMER_MISASSEMBLIES]

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
             N50, NG50, Nx, NGx, auN, auNG, NA50, NGA50, NAx, NGAx, auNA, auNGA,
             MAPPEDGENOME, AVE_READ_SUPPORT, GENES, OPERONS, PREDICTED_GENES_UNIQUE, PREDICTED_GENES, RNA_GENES,
             BUSCO_COMPLETE,
             MAPPED_READS, MAPPED_READS_PCNT, PROPERLY_PAIRED_READS, PROPERLY_PAIRED_READS_PCNT,
             KMER_COMPLETENESS, KMER_CORR_LENGTH,
             DEPTH, COVERAGE__FOR_THRESHOLDS],
        Quality.LESS_IS_BETTER:
            [L50, LG50, Lx, LGx,
             MISLOCAL, MISASSEMBL, MISCONTIGS, MISCONTIGSBASES, MISINTERNALOVERLAP,
             LARGE_MIS_EXTENSIVE, MIS_ALL_EXTENSIVE,
             CONTIGS_WITH_ISTRANSLOCATIONS, POSSIBLE_MISASSEMBLIES,
             UNALIGNED, UNALIGNEDBASES, AMBIGUOUS, AMBIGUOUSEXTRABASES,
             UNCALLED, UNCALLED_PERCENT, BUSCO_PART,
             SINGLETONS, SINGLETONS_PCNT, MISJOINT_READS, MISJOINT_READS_PCNT,
             KMER_MIS_LENGTH, KMER_UNDEF_LENGTH, KMER_MISASSEMBLIES,
             LA50, LGA50, LAx, LGAx, DUPLICATION_RATIO, INDELS, INDELSERROR, MISMATCHES, SUBSERROR,
             MIS_SHORT_INDELS, MIS_LONG_INDELS, INDELSBASES],
        Quality.EQUAL:
            [REFLEN, ESTREFLEN, GC, CONTIGS, CONTIGS__FOR_THRESHOLDS, REFGC, STRUCT_VARIATIONS, POTENTIAL_MGE, SIMILAR_CONTIGS, SIMILAR_MIS_BLOCKS],
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

    # TODO: associate floating fields with the default number of decimal points and format them automatically here
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
    required_fields = []

    def define_required_fields():
        if qconfig.report_all_metrics:
            required_fields.extend(Fields.order)
            return

        # if a reference is specified, keep the same number of Nx/Lx-like genome-based metrics in different reports
        # (no matter what percent of the genome was assembled)
        report = get(assembly_fpaths[0], ref_name=ref_name)
        if report.get_field(Fields.REFLEN):
            anyone_aligned = False
            for assembly_fpath in assembly_fpaths:
                report = get(assembly_fpath, ref_name=ref_name)
                if report.get_field(Fields.MAPPEDGENOME):
                    anyone_aligned = True
                    break

            if anyone_aligned:
                required_fields.extend([Fields.NA50, Fields.LA50, Fields.NAx, Fields.LAx])
            if not qconfig.is_combined_ref:
                required_fields.extend([Fields.NG50, Fields.LG50, Fields.NGx, Fields.LGx])
                if anyone_aligned:
                    required_fields.extend([Fields.NGA50, Fields.LGA50, Fields.NGAx, Fields.LGAx])

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

        if list(filter(lambda v: v is not None, values)) or field in required_fields:
            metric_name = field if (feature is None) else pattern % int(feature)
            # ATTENTION! Contents numeric values, needed to be converted to strings.
            rows.append({
                'metricName': metric_name,
                'quality': quality,
                'values': values,
                'isMain': metric_name in Fields.main_metrics,
            })

    define_required_fields()
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


def save_txt(fpath, all_rows):
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
        if row.startswith(Fields.HALF_TAB):
            row = "\hspace{2mm}" + row.lstrip()
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
    save_txt(report_txt_fpath, all_rows)
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
            save_txt(report_txt_fpath, all_rows)
            save_tsv(report_tsv_fpath, all_rows)
            save_tex(report_tex_fpath, all_rows, is_transposed=True)
            transposed_reports_fpaths = report_txt_fpath + ', ' + os.path.basename(report_tsv_fpath) + \
                                        ', and ' + os.path.basename(report_tex_fpath)
            if not silent:
                logger.info('    saved to ' + transposed_reports_fpaths)
    return reports_fpaths, transposed_reports_fpaths


def save_total(output_dirpath, silent=True):
    if not silent:
        logger.print_timestamp()
        logger.info('Summarizing...')
    return save(output_dirpath, qconfig.report_prefix, qconfig.transposed_report_prefix, Fields.order, silent=silent)


def save_misassemblies(output_dirpath):
    save(output_dirpath, "misassemblies_report", qconfig.transposed_report_prefix + "_misassemblies", Fields.misassemblies_order_advanced)


def save_unaligned(output_dirpath):
    save(output_dirpath, "unaligned_report", "", Fields.unaligned_order)


def save_reads(output_dirpath):
    save(output_dirpath, "reads_report", "", Fields.reads_order)


def save_kmers(output_dirpath):
    save(output_dirpath, "kmers_report", "", Fields.kmers_order)