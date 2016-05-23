############################################################################
# Copyright (c) 2015-2016 Saint Petersburg State University
# Copyright (c) 2011-2015 Saint Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################
#
# This is auxiliary file for contigs_analyzer.py and gage.py
#
############################################################################

from __future__ import with_statement
import platform
from os.path import isfile, join
from libs import qconfig, qutils

# it will be set to actual dirpath after successful compilation
contig_aligner = None
contig_aligner_dirpath = None


def __all_required_binaries_exist(aligner_dirpath, required_binaries):
    for required_binary in required_binaries:
        if not isfile(join(aligner_dirpath, required_binary)):
            return False
    return True


def is_emem_aligner():
    return contig_aligner == 'E-MEM'


def compile_aligner(logger):
    if contig_aligner_dirpath is not None:
        return True

    global contig_aligner
    global contig_aligner_dirpath
    default_requirements = ['nucmer', 'delta-filter', 'show-coords', 'show-snps', 'mummer', 'mgaps']

    aligners_to_try = []
    if platform.system() == 'Darwin':
        aligners_to_try.append(('MUMmer', join(qconfig.LIBS_LOCATION, 'MUMmer3.23-osx'), default_requirements))
    else:
        aligners_to_try.append(('E-MEM', join(qconfig.LIBS_LOCATION, 'E-MEM-linux'), default_requirements + ['e-mem']))
        aligners_to_try.append(('MUMmer', join(qconfig.LIBS_LOCATION, 'MUMmer3.23-linux'), default_requirements))

    for name, dirpath, requirements in aligners_to_try:
        make_logs_basepath = join(dirpath, 'make')
        failed_compilation_flag = make_logs_basepath + '.failed'
        if isfile(failed_compilation_flag):
            logger.warning('Previous try of ' + name + ' compilation was unsuccessful! ' + \
                           'For forced retrying, please remove ' + failed_compilation_flag + ' and restart QUAST.')
            continue

        if not __all_required_binaries_exist(dirpath, requirements):
            # making
            logger.main_info('Compiling ' + name + ' (details are in ' + make_logs_basepath +
                             '.log and make.err)')
            return_code = qutils.call_subprocess(
                ['make', '-C', dirpath],
                stdout=open(make_logs_basepath + '.log', 'w'),
                stderr=open(make_logs_basepath + '.err', 'w'),)

            if return_code != 0 or not __all_required_binaries_exist(dirpath, requirements):
                logger.warning("Failed to compile " + name + " (" + dirpath + ")! "
                               "Try to compile it manually. " + ("You can restart Quast with the --debug flag "
                               "to see the command line." if not qconfig.debug else ""))
                open(failed_compilation_flag, 'w').close()
                continue
        contig_aligner = name
        contig_aligner_dirpath = dirpath  # successfully compiled
        return True
    logger.error("Compilation of contig aligner software was unsuccessful! QUAST functionality will be limited.")
    return False