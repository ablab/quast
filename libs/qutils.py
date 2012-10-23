############################################################################
# Copyright (c) 2011-2012 Saint-Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################

assemblies_number = 0

def id_to_str(id):
    if assemblies_number == 1:
        return ''
    else:
        return ('%d ' + ('' if id >= 10 else ' ')) % (id + 1)