############################################################################
# Copyright (c) 2011-2012 Saint-Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################

def id_to_str(id):
    return ('%d' + (' ' if id >= 10 else '')) % (id + 1)