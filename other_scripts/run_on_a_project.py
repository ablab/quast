#!/usr/bin/python

############################################################################
# Copyright (c) 2015 Saint Petersburg State University
# Copyright (c) 2011-2015 Saint Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################

import getopt
import os
import re
import shutil
import sys
import quast
from libs import qconfig


def main(args):
    try:
        options, different_arguments = getopt.gnu_getopt(args, qconfig.short_options, qconfig.long_options)
    except getopt.GetoptError, err:
        print >>sys.stderr, err
        print >>sys.stderr
        sys.exit(1)

    if len(different_arguments) != 1:
        print >>sys.stderr, 'Works with only one project directory.'


    project_path = os.path.abspath(different_arguments[0])
    if not os.path.isdir(project_path):
        print >>sys.stderr, 'Specify project directory, not file.'

    project_name = os.path.basename(project_path)


    spades_dir_pattern = re.compile(r'spades_(?P<suffix>\S+)')


    tmp_dir_name = 'temporary'
    if os.path.isdir(tmp_dir_name):
        i = 2
        base_dir_name = tmp_dir_name
        while os.path.isdir(tmp_dir_name):
            tmp_dir_name = base_dir_name + '_' + str(i)
            i += 1
    tmp_dir_path = os.path.abspath(tmp_dir_name)
    if not os.path.isdir(tmp_dir_path):
        os.makedirs(tmp_dir_path)


    contigs_filepaths = []
    # In spades_<suffix> directories finding files <project_name>.fasta
    # and copying them to the temporary directory as <suffix>.fasta.
    #
    # Paths to found files are stored in the array contigs_filepaths[]
    for dirname in os.listdir(project_path):
        dirpath = os.path.join(project_path, dirname)
        if os.path.isdir(dirpath):
            m = spades_dir_pattern.match(dirname)
            if m:
                for fasta_filename in os.listdir(dirpath):
                    fasta_filepath = os.path.join(dirpath, fasta_filename)
                    if os.path.isfile(fasta_filepath) and fasta_filename == project_name + '.fasta':
                        new_filepath = os.path.join(tmp_dir_path, m.group('suffix') + '.fasta')
                        shutil.copyfile(fasta_filepath, new_filepath)
                        contigs_filepaths.append(new_filepath)

    if len(contigs_filepaths) == 0:
        print >>sys.stderr, 'No ' + project_name + '.fasta files in spades_* subdirectories'
    else:
        # Constructing a new command line arguments array to run quast.py with.
        new_args = []

        for opt, arg in options:
            new_args.extend([opt, arg])

        for contig_filepath in contigs_filepaths:
            new_args.append(contig_filepath)

        quast.main(new_args)

    if os.path.isdir(tmp_dir_name):
        shutil.rmtree(tmp_dir_name)


if __name__ == '__main__':
    main(sys.argv[1:])
