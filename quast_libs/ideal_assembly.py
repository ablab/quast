############################################################################
# Copyright (c) 2015-2017 Saint Petersburg State University
# Copyright (c) 2011-2015 Saint Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################

from __future__ import with_statement
import os
from collections import defaultdict
from os.path import join, basename, dirname
import shutil
from distutils import dir_util

from quast_libs import fastaparser, qconfig, qutils, reads_analyzer
from quast_libs.log import get_logger
from quast_libs.qutils import splitext_for_fasta_file, is_non_empty_file, split_by_ns, download_external_tool

logger = get_logger(qconfig.LOGGER_DEFAULT_NAME)


def preprocess_reference(ref_fpath, tmp_dir, uncovered_fpath):
    uncovered_regions = defaultdict(list)
    if is_non_empty_file(uncovered_fpath):
        with open(uncovered_fpath) as f:
            for line in f:
                chrom, start, end = line.split('\t')
                uncovered_regions[chrom].append((int(start), int(end)))

    splitted_fasta = []
    for name, seq in fastaparser.read_fasta(ref_fpath):
        if name in uncovered_regions:
            cur_contig_start = 0
            total_contigs = 0
            for start, end in uncovered_regions[name]:
                total_contigs = split_by_ns(seq[cur_contig_start: start], name, splitted_fasta, total_contigs=total_contigs)
                cur_contig_start = end
            split_by_ns(seq[cur_contig_start:], name, splitted_fasta, total_contigs=total_contigs)
        else:
            split_by_ns(seq, name, splitted_fasta)
    processed_ref_fpath = join(tmp_dir, basename(ref_fpath))
    fastaparser.write_fasta(processed_ref_fpath, splitted_fasta)
    return processed_ref_fpath


def prepare_config_spades(fpath, kmer, ref_fpath, tmp_dir):
    subst_dict = dict()
    subst_dict["K"] = str(kmer)
    subst_dict["dataset"] = os.path.abspath(ref_fpath)
    subst_dict["output_base"] = os.path.abspath(tmp_dir)
    subst_dict["tmp_dir"] = subst_dict["output_base"]
    subst_dict["max_threads"] = str(qconfig.max_threads)

    with open(fpath) as config:
        template_content = config.readlines()
    with open(fpath, 'w') as config:
        for line in template_content:
            if len(line.split()) > 1 and line.split()[0] in subst_dict:
                config.write("%s  %s\n" % (line.split()[0], subst_dict[line.split()[0]]))
            else:
                config.write(line)


def do(ref_fpath, original_ref_fpath, output_dirpath):
    logger.print_timestamp()
    logger.main_info("Simulating Ideal Assembly...")

    uncovered_fpath = None
    if qconfig.reads_fpaths or qconfig.reference_sam or qconfig.reference_sam:
        uncovered_fpath = reads_analyzer.align_reference(ref_fpath, join(dirname(output_dirpath), qconfig.reads_stats_dirname))
    insert_size = qconfig.ideal_assembly_insert_size
    if insert_size == 'auto' or not insert_size:
        insert_size = qconfig.ideal_assembly_default_IS
    if insert_size % 2 == 0:
        insert_size += 1
        logger.notice('  Current implementation cannot work with even insert sizes, '
                      'will use the closest odd value (%d)' % insert_size)

    ref_basename, fasta_ext = splitext_for_fasta_file(os.path.basename(ref_fpath))
    result_basename = '%s.%s.is%d.fasta' % (ref_basename, qconfig.ideal_assembly_basename, insert_size)
    result_fpath = os.path.join(output_dirpath, result_basename)

    original_ref_basename, fasta_ext = splitext_for_fasta_file(os.path.basename(original_ref_fpath))
    prepared_ideal_assembly_basename = '%s.%s.is%d.fasta' % (original_ref_basename, qconfig.ideal_assembly_basename, insert_size)
    ref_prepared_ideal_assembly = os.path.join(os.path.dirname(original_ref_fpath), prepared_ideal_assembly_basename)

    if os.path.isfile(result_fpath) or os.path.isfile(ref_prepared_ideal_assembly):
        already_done_fpath = result_fpath if os.path.isfile(result_fpath) else ref_prepared_ideal_assembly
        logger.notice('  Will reuse already generated Ideal Assembly with insert size %d (%s)' %
                      (insert_size, already_done_fpath))
        return already_done_fpath

    if qconfig.platform_name == 'linux_32':
        logger.warning('  Sorry, can\'t create Ideal Assembly on this platform, skipping...')
        return None

    base_aux_dir = os.path.join(qconfig.LIBS_LOCATION, 'ideal_assembly')
    configs_dir = os.path.join(base_aux_dir, 'configs')
    binary_fpath = download_external_tool('spades', os.path.join(base_aux_dir, 'bin'), 'spades', platform_specific=True)
    if not os.path.isfile(binary_fpath):
        logger.warning('  Sorry, can\'t create Ideal Assembly, skipping...')
        return None

    log_fpath = os.path.join(output_dirpath, 'spades.log')

    tmp_dir = os.path.join(output_dirpath, 'tmp')
    if os.path.isdir(tmp_dir):
        shutil.rmtree(tmp_dir)
    os.makedirs(tmp_dir)

    ref_fpath = preprocess_reference(ref_fpath, tmp_dir, uncovered_fpath)

    dst_configs = os.path.join(tmp_dir, 'configs')
    main_config = os.path.join(dst_configs, 'config.info')
    dir_util._path_created = {}  # see http://stackoverflow.com/questions/9160227/dir-util-copy-tree-fails-after-shutil-rmtree
    dir_util.copy_tree(configs_dir, dst_configs, preserve_times=False)

    prepare_config_spades(main_config, insert_size, ref_fpath, tmp_dir)

    log_file = open(log_fpath, 'w')
    spades_output_fpath = os.path.join(tmp_dir, 'K%d' % insert_size, 'ideal_assembly.fasta')
    logger.info('  ' + 'Running SPAdes with K=' + str(insert_size) + '...')
    return_code = qutils.call_subprocess(
        [binary_fpath, main_config], stdout=log_file, stderr=log_file, indent='    ')
    if return_code != 0 or not os.path.isfile(spades_output_fpath):
        logger.error('  Failed to create Ideal Assembly, see log for details: ' + log_fpath)
        return None

    shutil.move(spades_output_fpath, result_fpath)
    logger.info('  ' + 'Ideal Assembly saved to ' + result_fpath)
    logger.notice('You can copy it to ' + ref_prepared_ideal_assembly +
                  ' and QUAST will reuse it in further runs against the same reference (' + original_ref_fpath + ')')

    if not qconfig.debug:
        shutil.rmtree(tmp_dir)

    logger.main_info('Done.')
    return result_fpath
