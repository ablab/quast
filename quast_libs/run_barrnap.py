############################################################################
# Copyright (c) 2015-2022 Saint Petersburg State University
# Copyright (c) 2011-2015 Saint Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################
from __future__ import with_statement
import os
from os.path import join

from quast_libs import reporting, qconfig, qutils
from quast_libs.genes_parser import parse_gff
from quast_libs.qutils import run_parallel, call_subprocess, is_non_empty_file


def run(contigs_fpath, gff_fpath, log_fpath, threads, kingdom):
    barrnap_fpath = join(qconfig.LIBS_LOCATION, 'barrnap', 'bin', 'barrnap')
    if is_non_empty_file(gff_fpath):
        return
    call_subprocess([barrnap_fpath, '--quiet', '-k', kingdom, '--threads', str(threads), contigs_fpath],
                     stdout=open(gff_fpath, 'w'), stderr=open(log_fpath, 'a'))


def do(contigs_fpaths, output_dir, logger):
    logger.print_timestamp()
    logger.info('Running Barrnap...')

    n_jobs = min(len(contigs_fpaths), qconfig.max_threads)
    threads = max(1, qconfig.max_threads // n_jobs)
    if not os.path.isdir(output_dir):
        os.makedirs(output_dir)

    log_fpath = join(output_dir, 'barrnap.log')
    logger.info('Logging to ' + log_fpath + '...')
    kingdom = 'bac' if qconfig.prokaryote else 'euk'
    gff_fpaths = [join(output_dir, qutils.label_from_fpath_for_fname(contigs_fpath) + '.rna.gff') for contigs_fpath in contigs_fpaths]

    barrnap_args = [(contigs_fpath, gff_fpath, log_fpath, threads, kingdom) for contigs_fpath, gff_fpath in zip(contigs_fpaths, gff_fpaths)]
    run_parallel(run, barrnap_args, qconfig.max_threads)

    if not any(fpath for fpath in gff_fpaths):
        logger.info('Failed predicting the location of ribosomal RNA genes.')
        return

    # saving results
    for index, (contigs_fpath, gff_fpath) in enumerate(zip(contigs_fpaths, gff_fpaths)):
        genes = parse_gff(open(gff_fpath), 'rrna')
        report = reporting.get(contigs_fpath)

        if not os.path.isfile(gff_fpath):
            logger.error('Failed running Barrnap for ' + contigs_fpath + '. See ' + log_fpath + ' for information.')
            continue

        part_count = len([gene for gene in genes if 'product' in gene.attributes and 'partial' in gene.attributes['product']])
        total_count = len(genes)
        report.add_field(reporting.Fields.RNA_GENES, '%s + %s part' % (total_count - part_count, part_count))

        logger.info('  ' + qutils.index_to_str(index) + '  Ribosomal RNA genes = ' + str(total_count))
        logger.info('  ' + qutils.index_to_str(index) + '  Predicted genes (GFF): ' + gff_fpath)

    logger.info('Done.')