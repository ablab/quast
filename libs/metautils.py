############################################################################
# Copyright (c) 2015-2016 Saint Petersburg State University
# Copyright (c) 2011-2015 Saint Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################

# safe remove from quast_py_args, e.g. removes correctly "--test-no" (full is "--test-no-ref") and corresponding argument

def __remove_from_quast_py_args(quast_py_args, opt, arg=None):
    opt_idx = None
    if opt in quast_py_args:
        opt_idx = quast_py_args.index(opt)
    if opt_idx is None:
        common_length = -1
        for idx, o in enumerate(quast_py_args):
            if opt.startswith(o):
                if len(o) > common_length:
                    opt_idx = idx
                    common_length = len(o)
    if opt_idx is not None:
        if arg:
            del quast_py_args[opt_idx + 1]
        del quast_py_args[opt_idx]
    return quast_py_args
