#! /usr/bin/env python

"""
sum_prob.py : Takes a newline separated list of probabilities and computes
the sum of their logs.
"""

import fileinput, math, os, sys, time
from optparse import OptionParser

USAGE = """Usage: cat input_prob | ./calc_prob.py -t threshold
    -t  --threshold         any probabilities below this threshold are
                            set to the threshold value. (default 1e-18)
    -i  --input             input file containing probabilities (reads 
                            from stdin by default)
    -d  --debug_level           determines how much debug output.
"""

def main():
    if len(sys.argv) < 1:
        print USAGE
        sys.exit()

    parser = OptionParser()
    parser.add_option("-t", "--threshold", dest="threshold", default="1e-18")
    parser.add_option("-i", "--input", dest="input", default=None)
    parser.add_option("-d", "--debug_level" , dest="debug_level", default = 0)
    parser.set_usage(USAGE)
    (options, args) = parser.parse_args(sys.argv[1:])
    debug_level = int(options.debug_level)

    threshold = "1e-30"
    if options.threshold:
        threshold = options.threshold
    threshold = float(threshold)

    # Reading from stdin or file?
    input_file = sys.stdin
    if options.input:
        input_file = open(options.input, 'r')

    # Additional statistics
    total_values = 0
    values_below_threshold = 0
    std_dev = 0.0

    # Go through the list of probabilities and compute the sum of their logs.
    summation = 0.0
    M = 0.0
    S = 0.0
    for line in input_file:
        line = line.rstrip().split('\t')
        prob = float(line[1])
        if prob < threshold:
            prob = threshold
            values_below_threshold += 1
        
        summation += math.log10(prob)
        total_values += 1
        
        tmpM = M
        M +=(prob - tmpM) / total_values
        S +=(prob - tmpM) * (prob - M)

    stream_std_dev = math.sqrt(S / (total_values-1))
    std_dev = math.fabs(math.log10(threshold) / (2 * math.pow(total_values, .5)))

    if debug_level > 2:
        print 'Threshold:\t\t' + str(threshold)
        print 'Total values:\t\t' + str(total_values)
        print 'Values below threshold:\t{0} ({1:.2%})'.format(values_below_threshold,
                float(values_below_threshold) / total_values)
        
        # TODO(cmhill): Experimental normalized score.  Lowest possible score is the
        # threshold * total values.
        max_score = (-1 * (math.log10(threshold) * total_values))
        adjusted_score = (float(summation) - (math.log10(threshold) * total_values))
        print 'Normalized score:\t{0:.4}'.format(adjusted_score/max_score)

    if debug_level == 0:
        print  str(summation / total_values) + '\t' + str(std_dev)   
    else:
        if debug_level == 1:
            print str(threshold) + '\t' + str(summation) + '\t' + str(summation / total_values)
        else:
            std_dev = math.fabs(math.log10(threshold) / (2 * math.pow(total_values, .5)))
            print str(threshold) + '\t' + str(summation) + '\t' + str(summation / total_values) + '\t' + str(std_dev) + '\t' + str(stream_std_dev)

if __name__ == '__main__':
    main()
