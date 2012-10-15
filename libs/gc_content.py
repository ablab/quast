def gc_content(sequence):
    """
    Example:
    s = "AGCCT"
    gc_content = count_gc_content(s)
    print "GC content is %.2f%%"%gc_content

    Result:
    GC content is 60.00%
    """
    GC_count, length = 0, 0
    for c in sequence:
        if c is 'C' or c is 'G':
            GC_count += 1
        elif c is not 'N':
            length += 1
    length += GC_count
    return round((1.0 * GC_count)/length * 100, 2)

