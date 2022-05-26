############################################################################
# Copyright (c) 2015-2022 Saint Petersburg State University
# Copyright (c) 2011-2015 Saint Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################

def NG50(numlist, reference_length, percentage=50.0):
    """
    Abstract: Returns the NG50 value of the passed list of numbers.
    Comments: Works for any percentage (e.g. NG60, NG70) with optional argument
    Usage: NG50(numlist, reference_length)

    Based on the definition from this SEQanswers post
    http://seqanswers.com/forums/showpost.php?p=7496&postcount=4
    (modified Broad Institute's definition
    https://www.broad.harvard.edu/crd/wiki/index.php/N50)

    See SEQanswers threads for details:
    http://seqanswers.com/forums/showthread.php?t=2857
    http://seqanswers.com/forums/showthread.php?t=2332
    """
    ng50, lg50 = NG50_and_LG50(numlist, reference_length, percentage)
    return ng50


def LG50(numlist, reference_length, percentage=50.0):
    """
    Abstract: Returns the LG50 value of the passed list of numbers.
    Comments: Works for any percentage (e.g. LG60, LG70) with optional argument
    Usage: LG50(numlist, reference_length)
    """
    ng50, lg50 = NG50_and_LG50(numlist, reference_length, percentage)
    return lg50


def N50(numlist, percentage=50.0):
    """
    Abstract: Returns the N50 value of the passed list of numbers.
    Comments: Works for any percentage (e.g. N60, N70) with optional argument
    Usage: N50(numlist)
    """
    return NG50(numlist, sum(numlist), percentage)


def L50(numlist, percentage=50.0):
    """
    Abstract: Returns the L50 value of the passed list of numbers.
    Comments: Works for any percentage (e.g. L60, L70) with optional argument
    Usage: L50(numlist)
    """
    return LG50(numlist, sum(numlist), percentage)


def NG50_and_LG50(numlist, reference_length, percentage=50.0, need_sort=False):
    assert percentage >= 0.0
    assert percentage <= 100.0
    if need_sort:
        numlist.sort(reverse=True)
    s = reference_length
    limit = reference_length * (100.0 - percentage) / 100.0
    lg50 = 0
    for l in numlist:
        s -= l
        lg50 += 1
        if s <= limit:
            ng50 = l
            return ng50, lg50

    return None, None


def N50_and_L50(numlist, percentage=50.0):
    return NG50_and_LG50(numlist, sum(numlist), percentage)


def au_metric(numlist, reference_length=None):
    """
    numlist - length of contigs
    metric explanation http://lh3.github.io/2020/04/08/a-new-metric-on-assembly-contiguity
    """
    try:
        assert len(numlist) > 0, 'Empty list as input'
        assert all([isinstance(i, (int, float, complex)) for i in numlist]), 'Non-numerical input'
        if reference_length:
            denum = float(reference_length)
        else:
            denum = float(sum(numlist))
        assert denum > 0.0, 'all contigs are 0'
        return float(sum([n ** 2 for n in numlist])) / denum
    except AssertionError:
        return None
