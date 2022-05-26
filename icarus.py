#!/usr/bin/env python

############################################################################
# Copyright (c) 2015-2022 Saint Petersburg State University
# Copyright (c) 2011-2015 Saint Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################

import sys
from quast_libs import qconfig
qconfig.check_python_version()

from os.path import isdir
from optparse import OptionParser, BadOptionError, AmbiguousOptionError


class PassThroughOptionParser(OptionParser):
    """
    An unknown option pass-through implementation of OptionParser.
    """
    def _process_args(self, largs, rargs, values):
        while rargs:
            try:
                OptionParser._process_args(self,largs,rargs,values)
            except (BadOptionError, AmbiguousOptionError):
                exc_value = sys.exc_info()[1]
                largs.append(exc_value.opt_str)


def main(args):
    sys.stderr.write("\n")
    sys.stderr.write("Icarus: visualizer for de novo assembly evaluation\n")
    sys.stderr.write("\n")
    sys.stderr.write("Icarus is embedded into QUAST and MetaQUAST pipelines\n")
    sys.stderr.write("\n")
    if not args:
        sys.stderr.write("Please run ./quast.py -h or ./metaquast.py -h to see the full list of options\n")
        sys.stderr.write("\n")
        sys.exit(0)

    parser = PassThroughOptionParser()
    parser.add_option('-R', '--reference', dest='reference', action='append', default=[])
    parser.add_option('--fast', dest='no_icarus', action='store_true', default=False)
    parser.add_option('--no-html', dest='no_icarus', action='store_true')
    parser.add_option('--unique-mapping', dest='use_metaquast', action='store_true', default=False)
    parser.add_option('--max-ref-number', dest='use_metaquast', action='store_true')
    parser.add_option('--references-list', dest='use_metaquast', action='store_true')
    parser.add_option('--blast-db', dest='use_metaquast', action='store_true')
    parser.add_option('--test-no-ref', dest='use_metaquast', action='store_true')
    parser.add_option('--test-sv', dest='use_metaquast', action='store_false')
    (opts, l_args) = parser.parse_args(args)

    if opts.no_icarus:
        sys.stderr.write("Please remove --fast and --no-html from options and restart Icarus\n")
        sys.exit(0)

    if len(opts.reference) > 1:
        opts.use_metaquast = True
    elif len(opts.reference) == 1:
        if ',' in opts.reference[0] or isdir(opts.reference[0]):
            opts.use_metaquast = True

    if opts.use_metaquast:
        import metaquast
        quast_fn = metaquast.main
        sys.stderr.write("Icarus will run metaquast.py (for metagenomic dataset)\n")
    else:
        import quast
        quast_fn = quast.main
        sys.stderr.write("Icarus will run quast.py (for single-genome evaluation)\n")

    sys.stderr.write("Icarus main menu will be saved to <output_dir>/icarus.html\n")
    sys.stderr.write("Icarus viewers will be saved to <output_dir>/icarus_viewers/\n")
    sys.stderr.write("\n")
    return quast_fn(args)


if __name__ == '__main__':
    try:
        return_code = main(sys.argv[1:])
        exit(return_code)
    except Exception:
        _, exc_value, _ = sys.exc_info()
        sys.stderr.write(str(exc_value) + '\n')
        sys.stderr.write('exception caught!' + '\n')
        exit(1)