############################################################################
# Copyright (c) 2015-2022 Saint Petersburg State University
# Copyright (c) 2011-2015 Saint Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################

from __future__ import with_statement
from __future__ import division
import os
import re
from os.path import join

from quast_libs import fastaparser, qconfig, qutils, reporting, plotter
from quast_libs.circos import set_window_size
from quast_libs.log import get_logger
logger = get_logger(qconfig.LOGGER_DEFAULT_NAME)
MIN_HISTOGRAM_POINTS = 5
MIN_GC_WINDOW_SIZE = qconfig.GC_window_size // 2


def GC_content(contigs_fpath, skip=False):
    """
       Returns percent of GC for assembly and GC distribution: (list of GC%, list of # windows)
    """
    total_GC_amount = 0
    total_contig_length = 0
    GC_contigs_bin_num = int(100 / qconfig.GC_contig_bin_size) + 1
    GC_contigs_distribution_x = [i * qconfig.GC_contig_bin_size for i in range(0, GC_contigs_bin_num)] # list of X-coordinates, i.e. GC %
    GC_contigs_distribution_y = [0] * GC_contigs_bin_num # list of Y-coordinates, i.e. # contigs with GC % = x

    GC_bin_num = int(100 / qconfig.GC_bin_size) + 1
    GC_distribution_x = [i * qconfig.GC_bin_size for i in range(0, GC_bin_num)] # list of X-coordinates, i.e. GC %
    GC_distribution_y = [0] * GC_bin_num # list of Y-coordinates, i.e. # windows with GC % = x
    total_GC = None
    if skip:
        return total_GC, (GC_distribution_x, GC_distribution_y), (GC_contigs_distribution_x, GC_contigs_distribution_y)

    for name, seq_full in fastaparser.read_fasta(contigs_fpath): # in tuples: (name, seq)
        contig_ACGT_len = len(seq_full) - seq_full.count("N")
        if not contig_ACGT_len:
            continue
        contig_GC_len = seq_full.count("G") + seq_full.count("C")
        contig_GC_percent = 100.0 * contig_GC_len / contig_ACGT_len
        GC_contigs_distribution_y[int(contig_GC_percent // qconfig.GC_contig_bin_size)] += 1

        n = qconfig.GC_window_size
        # non-overlapping windows
        for seq in (seq_full[i:i+n] for i in range(0, len(seq_full), n)):
            GC_percent = get_GC_percent(seq)
            if GC_percent is not None:
                GC_distribution_y[int(int(GC_percent / qconfig.GC_bin_size) * qconfig.GC_bin_size)] += 1
        total_GC_amount += contig_GC_len
        total_contig_length += contig_ACGT_len

    if total_contig_length == 0:
        total_GC = None
    else:
        total_GC = total_GC_amount * 100.0 / total_contig_length

    return total_GC, (GC_distribution_x, GC_distribution_y), (GC_contigs_distribution_x, GC_contigs_distribution_y)


def get_GC_percent(seq):
    if len(seq) < MIN_GC_WINDOW_SIZE:
        return None
    ACGT_len = len(seq) - seq.count("N")
    # skip block if it has less than half of ACGT letters (it also helps with "ends of contigs")
    if ACGT_len < len(seq) // 2:
        return None

    GC_len = seq.count("G") + seq.count("C")
    GC_percent = 100 * GC_len // ACGT_len
    return GC_percent


def save_icarus_GC(ref_fpath, gc_fpath):
    chr_index = 0
    window_size = qconfig.GC_window_size_large if qconfig.large_genome else qconfig.GC_window_size  # non-overlapping windows
    with open(gc_fpath, 'w') as out_f:
        for name, seq_full in fastaparser.read_fasta(ref_fpath):
            out_f.write('#' + name + ' ' + str(chr_index) + '\n')
            for i in range(0, len(seq_full), window_size):
                seq = seq_full[i:i + window_size]
                GC_percent = get_GC_percent(seq)
                if GC_percent is not None:
                    out_f.write(str(chr_index) + ' ' + str(GC_percent) + '\n')


def save_circos_GC(ref_fpath, reference_length, gc_fpath):
    window_size = set_window_size(reference_length)
    with open(gc_fpath, 'w') as out_f:
        for name, seq_full in fastaparser.read_fasta(ref_fpath):
            for i in range(0, len(seq_full), window_size):
                seq = seq_full[i:i + window_size]
                GC_percent = get_GC_percent(seq)
                if GC_percent is not None:
                    out_f.write('\t'.join([name, str(i), str(i + len(seq)), str(GC_percent) + '\n']))


def binning_coverage(cov_values, nums_contigs):
    min_bins_cnt = 5
    bin_sizes = []
    low_thresholds = []
    high_thresholds = []
    cov_by_bins = []
    max_cov = max(len(v) for v in cov_values)
    for values, num_contigs in zip(cov_values, nums_contigs):
        assembly_len = sum(values)
        bases_by_cov = []
        for coverage, bases in enumerate(values):
            bases_by_cov.extend([coverage] * bases)
        q1 = bases_by_cov[assembly_len // 4]
        q2 = bases_by_cov[assembly_len // 2]
        q3 = bases_by_cov[assembly_len * 3 // 4]
        iqr = q3 - q1
        low_thresholds.append(int(q2 - 1.5 * iqr))
        high_thresholds.append(int(q2 + 1.5 * iqr))
        bin_sizes.append(int(2 * iqr / num_contigs ** (1.0 / 3)))

    bin_size = max(min(bin_sizes), 1)
    low_threshold = max(min(low_thresholds), 0)
    high_threshold = min(max(high_thresholds), max_cov)
    if (high_threshold - low_threshold) // bin_size < min_bins_cnt and bin_size > 1:
        bin_size = max((high_threshold - low_threshold) // min_bins_cnt, 1)
    low_threshold -= low_threshold % bin_size
    high_threshold -= high_threshold % bin_size
    max_points = (high_threshold // bin_size) + 1  # add last bin
    if high_threshold - low_threshold < MIN_HISTOGRAM_POINTS:
        low_threshold -= MIN_HISTOGRAM_POINTS // 2
        high_threshold += MIN_HISTOGRAM_POINTS // 2
        max_points = (high_threshold // bin_size) + 1
    offset = 0
    if low_threshold > bin_size:  # add first bin
        offset = low_threshold // bin_size - 1
        max_points -= offset
    else:
        low_threshold = 0
    for index, values in enumerate(cov_values):
        cov_by_bins.append([0] * int(max_points))
        for coverage, bases in enumerate(values):
            bin_idx = coverage // bin_size - offset
            if coverage < low_threshold:
                bin_idx = 0
            elif coverage >= high_threshold:
                bin_idx = max_points - 1
            cov_by_bins[int(index)][int(bin_idx)] += bases
    return cov_by_bins, bin_size, low_threshold, high_threshold, max_cov


def draw_coverage_histograms(coverage_dict, contigs_fpaths, output_dirpath):
    total_len = dict()
    contigs_dict = dict()

    contigs_with_coverage = [contigs_fpath for contigs_fpath in contigs_fpaths if coverage_dict[contigs_fpath]]
    for contigs_fpath in contigs_fpaths:
        total_len[contigs_fpath] = reporting.get(contigs_fpath).get_field(reporting.Fields.TOTALLEN)
        contigs_dict[contigs_fpath] = reporting.get(contigs_fpath).get_field(reporting.Fields.CONTIGS)
    cov_values = [coverage_dict[contigs_fpath] for contigs_fpath in contigs_with_coverage]
    num_contigs = [contigs_dict[contigs_fpath] for contigs_fpath in contigs_with_coverage]

    common_coverage_values, bin_size, low_threshold, high_threshold, max_cov = binning_coverage(cov_values, num_contigs)
    histogram_title = 'Coverage histogram (bin size: ' + str(bin_size) + 'x)'
    plotter.coverage_histogram(contigs_with_coverage, common_coverage_values, output_dirpath + '/coverage_histogram',
                               histogram_title, bin_size=bin_size, max_cov=max_cov, low_threshold=low_threshold, high_threshold=high_threshold)
    for contigs_fpath in contigs_with_coverage:
        coverage_values, bin_size, low_threshold, high_threshold, max_cov = binning_coverage([coverage_dict[contigs_fpath]],
                                                                                             [contigs_dict[contigs_fpath]])
        label = qutils.label_from_fpath(contigs_fpath)
        corr_label = qutils.label_from_fpath_for_fname(contigs_fpath)
        histogram_title = label + ' coverage histogram (bin size: ' + str(bin_size) + 'x)'
        histogram_fpath = os.path.join(output_dirpath, corr_label + '_coverage_histogram')
        plotter.coverage_histogram([contigs_fpath], coverage_values, histogram_fpath,
                                   histogram_title, draw_bars=True, bin_size=bin_size, max_cov=max_cov,
                                   low_threshold=low_threshold, high_threshold=high_threshold)


def do(ref_fpath, contigs_fpaths, output_dirpath, results_dir):
    logger.print_timestamp()
    logger.main_info("Running Basic statistics processor...")
    
    if not os.path.isdir(output_dirpath):
        os.mkdir(output_dirpath)

    reference_length = None
    reference_lengths = []
    reference_fragments = None
    icarus_gc_fpath = None
    circos_gc_fpath = None
    if ref_fpath:
        reference_lengths = sorted(fastaparser.get_chr_lengths_from_fastafile(ref_fpath).values(), reverse=True)
        reference_fragments = len(reference_lengths)
        reference_length = sum(reference_lengths)
        reference_GC, reference_GC_distribution, reference_GC_contigs_distribution = GC_content(ref_fpath)
        if qconfig.create_icarus_html or qconfig.draw_plots:
            icarus_gc_fpath = join(output_dirpath, 'gc.icarus.txt')
            save_icarus_GC(ref_fpath, icarus_gc_fpath)
        if qconfig.draw_circos:
            circos_gc_fpath = join(output_dirpath, 'gc.circos.txt')
            save_circos_GC(ref_fpath, reference_length, circos_gc_fpath)

        logger.info('  Reference genome:')
        logger.info('    ' + os.path.basename(ref_fpath) + ', length = ' + str(reference_length) +
                    ', num fragments = ' + str(reference_fragments) + ', GC % = ' +
                    '%.2f' % reference_GC if reference_GC is not None else 'undefined')
        if reference_fragments > 30 and not qconfig.check_for_fragmented_ref:
            logger.warning('  Reference genome is fragmented. You may consider rerunning QUAST using --fragmented option.'
                           ' QUAST will try to detect misassemblies caused by the fragmentation and mark them fake (will be excluded from # misassemblies).')
    elif qconfig.estimated_reference_size:
        reference_length = qconfig.estimated_reference_size
        reference_lengths = [reference_length]
        logger.info('  Estimated reference length = ' + str(reference_length))

    logger.info('  Contig files: ')
    lists_of_lengths = []
    numbers_of_Ns = []
    coverage_dict = dict()
    cov_pattern = re.compile(r'_cov_(\d+\.?\d*)')
    for id, contigs_fpath in enumerate(contigs_fpaths):
        coverage_dict[contigs_fpath] = []
        assembly_label = qutils.label_from_fpath(contigs_fpath)

        logger.info('    ' + qutils.index_to_str(id) + assembly_label)
        # lists_of_lengths.append(fastaparser.get_lengths_from_fastafile(contigs_fpath))
        list_of_length = []
        number_of_Ns = 0
        for (name, seq) in fastaparser.read_fasta(contigs_fpath):
            list_of_length.append(len(seq))
            number_of_Ns += seq.count('N')
            if cov_pattern.findall(name):
                cov = int(float(cov_pattern.findall(name)[0]))
                if len(coverage_dict[contigs_fpath]) <= cov:
                    coverage_dict[contigs_fpath] += [0] * (cov - len(coverage_dict[contigs_fpath]) + 1)
                coverage_dict[contigs_fpath][cov] += len(seq)

        lists_of_lengths.append(list_of_length)
        numbers_of_Ns.append(number_of_Ns)

    lists_of_lengths = [sorted(list, reverse=True) for list in lists_of_lengths]
    num_contigs = max([len(list_of_length) for list_of_length in lists_of_lengths])
    multiplicator = 1
    if num_contigs >= (qconfig.max_points * 2):
        import math
        multiplicator = int(num_contigs / qconfig.max_points)
        max_points = num_contigs // multiplicator
        corr_lists_of_lengths = [[sum(list_of_length[((i - 1) * multiplicator):(i * multiplicator)]) for i in range(1, max_points)
                                  if (i * multiplicator) < len(list_of_length)] for list_of_length in lists_of_lengths]
        if len(reference_lengths) > 1:
            reference_lengths = [sum(reference_lengths[((i - 1) * multiplicator):(i * multiplicator)])
                                 if (i * multiplicator) < len(reference_lengths) else
                                 sum(reference_lengths[((i - 1) * multiplicator):])
                                 for i in range(1, max_points)] + [sum(reference_lengths[(max_points - 1) * multiplicator:])]
        for num_list in range(len(corr_lists_of_lengths)):
            last_index = len(corr_lists_of_lengths[num_list])
            corr_lists_of_lengths[num_list].append(sum(lists_of_lengths[num_list][last_index * multiplicator:]))
    else:
        corr_lists_of_lengths = [sorted(list, reverse=True) for list in lists_of_lengths]

    if reference_lengths:
        # Saving for an HTML report
        if qconfig.html_report:
            from quast_libs.html_saver import html_saver
            html_saver.save_reference_lengths(results_dir, reference_lengths)

    if qconfig.html_report:
        from quast_libs.html_saver import html_saver
        html_saver.save_contigs_lengths(results_dir, contigs_fpaths, corr_lists_of_lengths)
        html_saver.save_tick_x(results_dir, multiplicator)

    ########################################################################

    logger.info('  Calculating N50 and L50...')

    list_of_GC_distributions = []
    list_of_GC_contigs_distributions = []
    largest_contig = 0
    from . import N50
    for id, (contigs_fpath, lengths_list, number_of_Ns) in enumerate(zip(contigs_fpaths, lists_of_lengths, numbers_of_Ns)):
        report = reporting.get(contigs_fpath)
        n50, l50 = N50.N50_and_L50(lengths_list)
        auN = N50.au_metric(lengths_list)
        ng50, lg50 = None, None
        if reference_length:
            ng50, lg50 = N50.NG50_and_LG50(lengths_list, reference_length)
            auNG = N50.au_metric(lengths_list, reference_length)
        nx, lx = N50.N50_and_L50(lengths_list, qconfig.x_for_additional_Nx)
        ngx, lgx = None, None
        if reference_length:
            ngx, lgx = N50.NG50_and_LG50(lengths_list, reference_length, qconfig.x_for_additional_Nx)
        total_length = sum(lengths_list)
        total_GC, GC_distribution, GC_contigs_distribution = GC_content(contigs_fpath, skip=qconfig.no_gc)
        list_of_GC_distributions.append(GC_distribution)
        list_of_GC_contigs_distributions.append(GC_contigs_distribution)
        logger.info('    ' + qutils.index_to_str(id) +
                    qutils.label_from_fpath(contigs_fpath) + \
                    ', N50 = ' + str(n50) + \
                    ', L50 = ' + str(l50) + \
                    ', auN = ' + ('%.1f' % auN if auN is not None else None) + \
                    ', Total length = ' + str(total_length) + \
                    ', GC % = ' + ('%.2f' % total_GC if total_GC is not None else 'undefined') + \
                    ', # N\'s per 100 kbp = ' + ' %.2f' % (float(number_of_Ns) * 100000.0 / float(total_length)) if total_length != 0 else 'undefined')
        
        report.add_field(reporting.Fields.N50, n50)
        report.add_field(reporting.Fields.L50, l50)
        report.add_field(reporting.Fields.auN, ('%.1f' % auN if auN is not None else None))
        if reference_length and not qconfig.is_combined_ref:
            report.add_field(reporting.Fields.NG50, ng50)
            report.add_field(reporting.Fields.LG50, lg50)
            report.add_field(reporting.Fields.auNG, ('%.1f' % auNG if auNG is not None else None))
        report.add_field(reporting.Fields.Nx, nx)
        report.add_field(reporting.Fields.Lx, lx)
        if reference_length and not qconfig.is_combined_ref:
            report.add_field(reporting.Fields.NGx, ngx)
            report.add_field(reporting.Fields.LGx, lgx)
        report.add_field(reporting.Fields.CONTIGS, len(lengths_list))
        if lengths_list:
            report.add_field(reporting.Fields.LARGCONTIG, max(lengths_list))
            largest_contig = max(largest_contig, max(lengths_list))
            report.add_field(reporting.Fields.TOTALLEN, total_length)
            if not qconfig.is_combined_ref:
                report.add_field(reporting.Fields.GC, ('%.2f' % total_GC if total_GC is not None else None))
            report.add_field(reporting.Fields.UNCALLED, number_of_Ns)
            report.add_field(reporting.Fields.UNCALLED_PERCENT, ('%.2f' % (float(number_of_Ns) * 100000.0 / float(total_length))))
        if ref_fpath:
            report.add_field(reporting.Fields.REFLEN, int(reference_length))
            report.add_field(reporting.Fields.REF_FRAGMENTS, reference_fragments)
            if not qconfig.is_combined_ref:
                report.add_field(reporting.Fields.REFGC, ('%.2f' % reference_GC if reference_GC is not None else None))
        elif reference_length:
            report.add_field(reporting.Fields.ESTREFLEN, int(reference_length))

    import math
    qconfig.min_difference = math.ceil((largest_contig / 1000) / 600)  # divide on height of plot

    list_of_GC_distributions_with_ref = list_of_GC_distributions
    reference_index = None
    if ref_fpath:
        reference_index = len(list_of_GC_distributions_with_ref)
        list_of_GC_distributions_with_ref.append(reference_GC_distribution)

    if qconfig.html_report and not qconfig.no_gc:
        from quast_libs.html_saver import html_saver
        html_saver.save_GC_info(results_dir, contigs_fpaths, list_of_GC_distributions_with_ref, list_of_GC_contigs_distributions, reference_index)

    ########################################################################
    # Drawing Nx and NGx plots...
    plotter.Nx_plot(results_dir, num_contigs > qconfig.max_points, contigs_fpaths, lists_of_lengths, join(output_dirpath, 'Nx_plot'), 'Nx', [])
    if reference_length and not qconfig.is_combined_ref:
        plotter.Nx_plot(results_dir, num_contigs > qconfig.max_points, contigs_fpaths, lists_of_lengths, join(output_dirpath, 'NGx_plot'), 'NGx',
                        [reference_length for i in range(len(contigs_fpaths))])

    if qconfig.draw_plots:
        ########################################################################import plotter
        # Drawing cumulative plot...
        plotter.cumulative_plot(ref_fpath, contigs_fpaths, lists_of_lengths, join(output_dirpath, 'cumulative_plot'), 'Cumulative length')
        if not qconfig.no_gc:
            ########################################################################
            # Drawing GC content plot...
            plotter.GC_content_plot(ref_fpath, contigs_fpaths, list_of_GC_distributions_with_ref, join(output_dirpath, 'GC_content_plot'))
            for contigs_fpath, GC_distribution in zip(contigs_fpaths, list_of_GC_contigs_distributions):
                plotter.contigs_GC_content_plot(contigs_fpath, GC_distribution,
                                                join(output_dirpath, qutils.label_from_fpath(contigs_fpath) + '_GC_content_plot'))

        if any(coverage_dict[contigs_fpath] for contigs_fpath in contigs_fpaths):
            draw_coverage_histograms(coverage_dict, contigs_fpaths, output_dirpath)

    logger.main_info('Done.')
    return icarus_gc_fpath, circos_gc_fpath
