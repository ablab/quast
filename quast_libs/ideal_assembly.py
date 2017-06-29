############################################################################
# Copyright (c) 2015-2017 Saint Petersburg State University
# Copyright (c) 2011-2015 Saint Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################

from __future__ import with_statement
import os
import re
from os.path import join
import shutil
from distutils import dir_util

from quast_libs import fastaparser, qconfig, qutils, reporting, plotter
from quast_libs.log import get_logger
logger = get_logger(qconfig.LOGGER_DEFAULT_NAME)


def preprocess_reference(ref_fpath, tmp_dir):
    # TODO: split by Ns (even single Ns are important here!)
    # TODO: align reads & remove not covered regions
    return ref_fpath


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


def do(ref_fpath, output_dirpath, insert_size):
    logger.print_timestamp()
    logger.main_info("Simulating Ideal Assembly...")
    if insert_size == 'auto':
        # TODO: auto detect from reads
        insert_size = qconfig.ideal_assembly_default_IS
    if insert_size % 2 == 0:
        insert_size += 1
        logger.notice('  Current implementation cannot work with even insert sizes, '
                      'will use the closest odd value (%d)' % insert_size)

    base_aux_dir = os.path.join(qconfig.LIBS_LOCATION, 'ideal_assembly')
    binary_fpath = os.path.join(base_aux_dir, 'bin_' + qconfig.platform_name, 'spades')
    configs_dir = os.path.join(base_aux_dir, 'configs')

    if not os.path.isfile(binary_fpath):
        logger.warning('  Sorry, can\'t create Ideal Assembly on this platform, skipping...')
        return None

    result_fpath = os.path.join(output_dirpath, qconfig.ideal_assembly_basename + '.fasta')
    log_fpath = os.path.join(output_dirpath, 'spades.log')

    tmp_dir = os.path.join(output_dirpath, 'tmp')
    if os.path.isdir(tmp_dir):
        shutil.rmtree(tmp_dir)
    os.makedirs(tmp_dir)

    preprocessing = False
    if preprocessing:
        ref_fpath = preprocess_reference(ref_fpath, tmp_dir)

    dst_configs = os.path.join(tmp_dir, 'configs')
    main_config = os.path.join(dst_configs, 'config.info')
    dir_util._path_created = {}  # see http://stackoverflow.com/questions/9160227/dir-util-copy-tree-fails-after-shutil-rmtree
    dir_util.copy_tree(configs_dir, dst_configs, preserve_times=False)

    prepare_config_spades(main_config, insert_size, ref_fpath, tmp_dir)

    log_file = open(log_fpath, 'w')
    spades_output_fpath = os.path.join(tmp_dir, 'K%d' % insert_size, 'ideal_assembly.fasta')
    return_code = qutils.call_subprocess(
        [binary_fpath, main_config], stdout=log_file, stderr=log_file, indent='    ')
    if return_code != 0 or not os.path.isfile(spades_output_fpath):
        logger.error('  Failed to create Ideal Assembly, see log for details: ' + log_fpath)
        return None

    shutil.move(spades_output_fpath, result_fpath)
    logger.info('  ' + 'Ideal Assembly saved to ' + result_fpath)

    if not qconfig.debug:
        shutil.rmtree(tmp_dir)

    logger.main_info('Done.')
    return result_fpath
