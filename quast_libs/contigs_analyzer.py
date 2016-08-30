############################################################################
# Copyright (c) 2015-2016 Saint Petersburg State University
# Copyright (c) 2011-2015 Saint Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################
#
# Some comments in this script was kindly provided by Plantagora team and
# modified by QUAST team. It is not licensed under GPL as other parts of QUAST,
# but it can be distributed and used in QUAST pipeline with current remarks and
# citation. For more details about assess_assembly.pl please refer to
# http://www.plantagora.org website and to the following paper:
#
# Barthelson R, McFarlin AJ, Rounsley SD, Young S (2011) Plantagora: Modeling
# Whole Genome Sequencing and Assembly of Plant Genomes. PLoS ONE 6(12):
# e28436. doi:10.1371/journal.pone.0028436
############################################################################

from __future__ import with_statement
import os
import re
from collections import defaultdict
from os.path import join

from quast_libs import reporting, qconfig, qutils, fastaparser
from quast_libs.ca_utils.analyze_contigs import analyze_contigs
from quast_libs.ca_utils.analyze_coverage import analyze_coverage
from quast_libs.ca_utils.analyze_misassemblies import Mapping
from quast_libs.ca_utils.misc import print_file, ref_labels_by_chromosomes, clean_tmp_files, compile_aligner, \
    create_nucmer_output_dir, open_gzipsafe, compress_nucmer_output
from quast_libs.ca_utils.align_contigs import align_contigs, get_nucmer_aux_out_fpaths, NucmerStatus
from quast_libs.ca_utils.save_results import print_results, save_result, save_result_for_unaligned

from quast_libs.log import get_logger
logger = get_logger(qconfig.LOGGER_DEFAULT_NAME)


class CAOutput():
    def __init__(self, stdout_f, misassembly_f=None, coords_filtered_f=None, used_snps_f=None, icarus_out_f=None):
        self.stdout_f = stdout_f
        self.misassembly_f = misassembly_f
        self.coords_filtered_f = coords_filtered_f
        self.used_snps_f = used_snps_f
        self.icarus_out_f = icarus_out_f


class SNP():
    def __init__(self, ref=None, ctg=None, ref_pos=None, ctg_pos=None, ref_nucl=None, ctg_nucl=None):
        self.ref_pos = ref_pos
        self.ctg_pos = ctg_pos
        self.ref_nucl = ref_nucl
        self.ctg_nucl = ctg_nucl
        self.type = 'I' if self.ref_nucl == '.' else ('D' if ctg_nucl == '.' else 'S')


# former plantagora and plantakolya
def align_and_analyze(cyclic, index, contigs_fpath, output_dirpath, ref_fpath,
                      old_contigs_fpath, bed_fpath, parallel_by_chr=False, threads=1):
    nucmer_output_dirpath = create_nucmer_output_dir(output_dirpath)
    assembly_label = qutils.label_from_fpath_for_fname(contigs_fpath)
    nucmer_fpath = join(nucmer_output_dirpath, assembly_label)

    logger.info('  ' + qutils.index_to_str(index) + assembly_label)

    log_out_fpath = join(output_dirpath, qconfig.contig_report_fname_pattern % assembly_label + '.stdout')
    log_err_fpath = join(output_dirpath, qconfig.contig_report_fname_pattern % assembly_label + '.stderr')
    icarus_out_fpath = join(output_dirpath, qconfig.icarus_report_fname_pattern % assembly_label)
    misassembly_fpath = join(output_dirpath, qconfig.contig_report_fname_pattern % assembly_label + '.mis_contigs.info')

    icarus_out_f = open(icarus_out_fpath, 'w')
    icarus_header_cols = ['S1', 'E1', 'S2', 'E2', 'Reference', 'Contig', 'IDY', 'Ambiguous', 'Best_group']
    print >> icarus_out_f, '\t'.join(icarus_header_cols)
    misassembly_f = open(misassembly_fpath, 'w')

    logger.info('  ' + qutils.index_to_str(index) + 'Logging to files ' + log_out_fpath +
                ' and ' + os.path.basename(log_err_fpath) + '...')

    coords_fpath, coords_filtered_fpath, unaligned_fpath, show_snps_fpath, used_snps_fpath = \
        get_nucmer_aux_out_fpaths(nucmer_fpath)

    nucmer_status = align_contigs(nucmer_fpath, ref_fpath, contigs_fpath, old_contigs_fpath, index,
                                  parallel_by_chr, threads, log_out_fpath, log_err_fpath)
    if nucmer_status != NucmerStatus.OK:
        with open(log_err_fpath, 'a') as log_err_f:
            if nucmer_status == NucmerStatus.ERROR:
                logger.error('  ' + qutils.index_to_str(index) +
                         'Failed aligning contigs ' + qutils.label_from_fpath(contigs_fpath) +
                         ' to the reference (non-zero exit code). ' +
                         ('Run with the --debug flag to see additional information.' if not qconfig.debug else ''))
            elif nucmer_status == NucmerStatus.FAILED:
                print >> log_err_f, qutils.index_to_str(index) + 'Alignment failed for', contigs_fpath + ':', coords_fpath, 'doesn\'t exist.'
                logger.info('  ' + qutils.index_to_str(index) + 'Alignment failed for ' + '\'' + assembly_label + '\'.')
            elif nucmer_status == NucmerStatus.NOT_ALIGNED:
                print >> log_err_f, qutils.index_to_str(index) + 'Nothing aligned for', contigs_fpath
                logger.info('  ' + qutils.index_to_str(index) + 'Nothing aligned for ' + '\'' + assembly_label + '\'.')
        clean_tmp_files(nucmer_fpath)
        return nucmer_status, {}, []

    log_out_f = open(log_out_fpath, 'a')
    # Loading the alignment files
    print >> log_out_f, 'Parsing coords...'
    aligns = {}
    coords_file = open(coords_fpath)
    coords_filtered_file = open(coords_filtered_fpath, 'w')
    coords_filtered_file.write(coords_file.readline())
    coords_filtered_file.write(coords_file.readline())
    for line in coords_file:
        if line.strip() == '':
            break
        assert line[0] != '='
        #Clear leading spaces from nucmer output
        #Store nucmer lines in an array
        mapping = Mapping.from_line(line)
        aligns.setdefault(mapping.contig, []).append(mapping)

    # Loading the reference sequences
    print >> log_out_f, 'Loading reference...'  # TODO: move up
    references = {}
    ref_features = {}
    for name, seq in fastaparser.read_fasta(ref_fpath):
        name = name.split()[0]  # no spaces in reference header
        references[name] = seq
        print >> log_out_f, '\tLoaded [%s]' % name

    #Loading the SNP calls
    if qconfig.show_snps:
        print >> log_out_f, 'Loading SNPs...'

    used_snps_file = None
    snps = {}
    if qconfig.show_snps:
        prev_line = None
        for line in open_gzipsafe(show_snps_fpath):
            #print "$line";
            line = line.split()
            if not line[0].isdigit():
                continue
            if prev_line and line == prev_line:
                continue
            ref = line[10]
            ctg = line[11]
            pos = int(line[0]) # Kolya: python don't convert int<->str types automatically
            loc = int(line[3]) # Kolya: same as above

            # if (! exists $line[11]) { die "Malformed line in SNP file.  Please check that show-snps has completed succesfully.\n$line\n[$line[9]][$line[10]][$line[11]]\n"; }
            if pos in snps.setdefault(ref, {}).setdefault(ctg, {}):
                snps.setdefault(ref, {}).setdefault(ctg, {})[pos].append(SNP(ref_pos=pos, ctg_pos=loc, ref_nucl=line[1], ctg_nucl=line[2]))
            else:
                snps.setdefault(ref, {}).setdefault(ctg, {})[pos] = [SNP(ref_pos=pos, ctg_pos=loc, ref_nucl=line[1], ctg_nucl=line[2])]
            prev_line = line
        used_snps_file = open_gzipsafe(used_snps_fpath, 'w')

    # Loading the regions (if any)
    regions = {}
    ref_lens = {}
    total_reg_len = 0
    total_regions = 0
    # # TODO: gff
    # print >> log_out_f, 'Loading regions...'
    # print >> log_out_f, '\tNo regions given, using whole reference.'
    for name, seq in references.iteritems():
        regions.setdefault(name, []).append([1, len(seq)])
        ref_lens[name] = len(seq)
        total_regions += 1
        total_reg_len += ref_lens[name]
    print >> log_out_f, '\tTotal Regions: %d' % total_regions
    print >> log_out_f, '\tTotal Region Length: %d' % total_reg_len

    ca_output = CAOutput(stdout_f=log_out_f, misassembly_f=misassembly_f, coords_filtered_f=coords_filtered_file,
                         used_snps_f=used_snps_file, icarus_out_f=icarus_out_f)

    print >> log_out_f, 'Analyzing contigs...'
    result, ref_aligns, total_indels_info, aligned_lengths, misassembled_contigs = analyze_contigs(ca_output, contigs_fpath,
                                        unaligned_fpath, aligns, ref_features, ref_lens, cyclic)

    print >> log_out_f, 'Analyzing coverage...'
    if qconfig.show_snps:
        print >> log_out_f, 'Writing SNPs into', used_snps_fpath
    result.update(analyze_coverage(ca_output, regions, ref_aligns, ref_features, snps, total_indels_info))
    result = print_results(contigs_fpath, log_out_f, used_snps_fpath, total_indels_info, result)

    ## outputting misassembled contigs to separate file
    fasta = [(name, seq) for name, seq in fastaparser.read_fasta(contigs_fpath)
             if name in misassembled_contigs.keys()]
    fastaparser.write_fasta(join(output_dirpath, qutils.name_from_fpath(contigs_fpath) + '.mis_contigs.fa'), fasta)

    alignment_tsv_fpath = join(output_dirpath, "alignments_" + assembly_label + '.tsv')
    logger.debug('  ' + qutils.index_to_str(index) + 'Alignments: ' + qutils.relpath(alignment_tsv_fpath))
    alignment_tsv_f = open(alignment_tsv_fpath, 'w')

    if qconfig.is_combined_ref:
        unique_contigs_fpath = join(output_dirpath, qconfig.unique_contigs_fname_pattern % assembly_label)
        unique_contigs_f = open(unique_contigs_fpath, 'w')
        used_contigs = set()

    for chr_name, aligns in ref_aligns.iteritems():
        alignment_tsv_f.write(chr_name)
        contigs = set([align.contig for align in aligns])
        for contig in contigs:
            alignment_tsv_f.write('\t' + contig)

        if qconfig.is_combined_ref:
            ref_name = ref_labels_by_chromosomes[chr_name]
            align_by_contigs = defaultdict(int)
            for align in aligns:
                align_by_contigs[align.contig] += align.len2
            for contig, aligned_len in align_by_contigs.iteritems():
                if contig in used_contigs:
                    continue
                used_contigs.add(contig)
                len_cov_pattern = re.compile(r'_length_([\d\.]+)_cov_([\d\.]+)')
                if len_cov_pattern.findall(contig):
                    contig_len, contig_cov = len_cov_pattern.findall(contig)[0][0], len_cov_pattern.findall(contig)[0][1]
                    if aligned_len / float(contig_len) > 0.9:
                        unique_contigs_f.write(ref_name + '\t' + str(aligned_len) + '\t' + contig_cov + '\n')

        alignment_tsv_f.write('\n')
    alignment_tsv_f.close()

    log_out_f.close()
    if qconfig.show_snps:
        used_snps_file.close()
    logger.info('  ' + qutils.index_to_str(index) + 'Analysis is finished.')
    logger.debug('')
    clean_tmp_files(nucmer_fpath)
    if not qconfig.no_gzip:
        compress_nucmer_output(logger, nucmer_fpath)
    if not ref_aligns:
        return NucmerStatus.NOT_ALIGNED, result, aligned_lengths
    else:
        return NucmerStatus.OK, result, aligned_lengths


def do(reference, contigs_fpaths, cyclic, output_dir, old_contigs_fpaths, bed_fpath=None):
    if not os.path.isdir(output_dir):
        os.mkdir(output_dir)

    logger.print_timestamp()
    logger.main_info('Running Contig analyzer...')
    num_nf_errors = logger._num_nf_errors

    if not compile_aligner(logger):
        logger.main_info('Failed aligning the contigs for all the assemblies. Only basic stats are going to be evaluated.')
        return dict(zip(contigs_fpaths, [NucmerStatus.FAILED] * len(contigs_fpaths))), None

    create_nucmer_output_dir(output_dir)
    n_jobs = min(len(contigs_fpaths), qconfig.max_threads)
    if qconfig.memory_efficient:
        threads = 1
    else:
        threads = max(int(qconfig.max_threads / n_jobs), 1)
    from joblib import Parallel, delayed
    if not qconfig.splitted_ref:
        statuses_results_lengths_tuples = Parallel(n_jobs=n_jobs)(delayed(align_and_analyze)(
        cyclic, i, contigs_fpath, output_dir, reference, old_contigs_fpath, bed_fpath, threads=threads)
             for i, (contigs_fpath, old_contigs_fpath) in enumerate(zip(contigs_fpaths, old_contigs_fpaths)))
    else:
        if len(contigs_fpaths) >= len(qconfig.splitted_ref) and not qconfig.memory_efficient:
            statuses_results_lengths_tuples = Parallel(n_jobs=n_jobs)(delayed(align_and_analyze)(
            cyclic, i, contigs_fpath, output_dir, reference, old_contigs_fpath, bed_fpath, threads=threads)
                for i, (contigs_fpath, old_contigs_fpath) in enumerate(zip(contigs_fpaths, old_contigs_fpaths)))
        else:
            statuses_results_lengths_tuples = []
            for i, (contigs_fpath, old_contigs_fpath) in enumerate(zip(contigs_fpaths, old_contigs_fpaths)):
                statuses_results_lengths_tuples.append(align_and_analyze(
                cyclic, i, contigs_fpath, output_dir, reference, old_contigs_fpath, bed_fpath,
                parallel_by_chr=True, threads=qconfig.max_threads))

    # unzipping
    statuses, results, aligned_lengths = [x[0] for x in statuses_results_lengths_tuples], \
                                         [x[1] for x in statuses_results_lengths_tuples], \
                                         [x[2] for x in statuses_results_lengths_tuples]
    reports = []

    if qconfig.is_combined_ref:
        ref_misassemblies = [result['istranslocations_by_refs'] if result else [] for result in results]
        if ref_misassemblies:
            for i, fpath in enumerate(contigs_fpaths):
                if ref_misassemblies[i]:
                    assembly_name = qutils.name_from_fpath(fpath)
                    all_rows = []
                    all_refs = sorted(list(set([ref for ref in ref_labels_by_chromosomes.values()])))
                    row = {'metricName': 'References', 'values': [ref_num+1 for ref_num in range(len(all_refs))]}
                    all_rows.append(row)
                    for k in all_refs:
                        row = {'metricName': k, 'values': []}
                        for ref in all_refs:
                            if ref == k or ref not in ref_misassemblies[i]:
                                row['values'].append(None)
                            else:
                                row['values'].append(ref_misassemblies[i][ref][k])
                        all_rows.append(row)
                    misassembly_by_ref_fpath = join(output_dir, 'interspecies_translocations_by_refs_%s.info' % assembly_name)
                    print >> open(misassembly_by_ref_fpath, 'w'), 'Number of interspecies translocations by references: \n'
                    print_file(all_rows, misassembly_by_ref_fpath, append_to_existing_file=True)

                    print >> open(misassembly_by_ref_fpath, 'a'), '\nReferences: '
                    for ref_num, ref in enumerate(all_refs):
                        print >> open(misassembly_by_ref_fpath, 'a'), str(ref_num+1) + ' - ' + ref
                    logger.info('  Information about interspecies translocations by references for %s is saved to %s' %
                                (assembly_name, misassembly_by_ref_fpath))

    for index, fname in enumerate(contigs_fpaths):
        report = reporting.get(fname)
        if statuses[index] == NucmerStatus.OK:
            reports.append(save_result(results[index], report, fname))
        elif statuses[index] == NucmerStatus.NOT_ALIGNED:
            save_result_for_unaligned(results[index], report)

    nucmer_statuses = dict(zip(contigs_fpaths, statuses))
    aligned_lengths_per_fpath = dict(zip(contigs_fpaths, aligned_lengths))

    if NucmerStatus.OK in nucmer_statuses.values():
        reporting.save_misassemblies(output_dir)
        reporting.save_unaligned(output_dir)
    if qconfig.draw_plots:
        import plotter
        plotter.draw_misassembl_plot(reports, join(output_dir, 'misassemblies_plot'), 'Misassemblies')

    oks = nucmer_statuses.values().count(NucmerStatus.OK)
    not_aligned = nucmer_statuses.values().count(NucmerStatus.NOT_ALIGNED)
    failed = nucmer_statuses.values().count(NucmerStatus.FAILED)
    errors = nucmer_statuses.values().count(NucmerStatus.ERROR)
    problems = not_aligned + failed + errors
    all = len(nucmer_statuses)

    logger._num_nf_errors = num_nf_errors + errors

    if oks == all:
        logger.main_info('Done.')
    if oks < all and problems < all:
        logger.main_info('Done for ' + str(all - problems) + ' out of ' + str(all) + '. For the rest, only basic stats are going to be evaluated.')
    if problems == all:
        logger.main_info('Failed aligning the contigs for all the assemblies. Only basic stats are going to be evaluated.')

    return nucmer_statuses, aligned_lengths_per_fpath
