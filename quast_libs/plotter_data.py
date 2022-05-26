############################################################################
# Copyright (c) 2015-2022 Saint Petersburg State University
# Copyright (c) 2011-2015 Saint Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################

from quast_libs import qconfig

# Feel free to add more colors
colors = ['#E31A1C', '#1F78B4', '#33A02C', '#6A3D9A', '#FF7F00', '#800000', '#A6CEE3', '#B2DF8A','#333300', '#CCCC00',
          '#000080', '#008080', '#00FF00'] # 14-color palette

# Line params
primary_line_style = 'solid' # 'solid', 'dashed', 'dashdot', or 'dotted'
secondary_line_style = 'dashed' # used only if --scaffolds option is set

dict_color_and_ls = {}
####################################################################################


def save_colors_and_ls(fpaths, labels=None):
    from quast_libs import qutils
    if not labels:
        labels = [qutils.label_from_fpath(fpath) for fpath in fpaths]
    if not dict_color_and_ls:
        color_id = 0
        for i, fpath in enumerate(fpaths):
            ls = primary_line_style
            label = labels[i]
            # contigs and scaffolds should be equally colored but scaffolds should be dashed
            if fpath and fpath in qconfig.dict_of_broken_scaffolds:
                color = dict_color_and_ls[qutils.label_from_fpath(qconfig.dict_of_broken_scaffolds[fpath])][0]
                ls = secondary_line_style
            else:
                 color = colors[color_id % len(colors)]
                 color_id += 1
            dict_color_and_ls[label] = (color, ls)


def get_color_and_ls(fpath, label=None):
    from quast_libs import qutils
    if not label:
        label = qutils.label_from_fpath(fpath)
    if not dict_color_and_ls:
        return None, None
    """
    Returns tuple: color, line style
    """
    return dict_color_and_ls[label]