#! /usr/bin/env python

"""
gen_rand_samp.py : Generate a random sampling reads from input mate pair files.

./gen_rand_samp.py -1 reads/gage/Staphylococcus_aureus/Data/original/frag_1.fastq,reads/gage/Staphylococcus_aureus/Data/original/shortjump_1.fastq -2 reads/gage/Staphylococcus_aureus/Data/original/frag_2.fastq,reads/gage/Staphylococcus_aureus/Data/original/shortjump_2.fastq  -k 10,100,1000 -o tmp/

./gen_rand_samp.py \
-1 reads/gage/Hg_chr14/Data/original/frag_1.fastq,reads/gage/Hg_chr14/Data/original/shortjump_1.fastq,reads/gage/Hg_chr14/Data/original/longjump_1.fastq \
-2 reads/gage/Hg_chr14/Data/original/frag_2.fastq,reads/gage/Hg_chr14/Data/original/shortjump_2.fastq,reads/gage/Hg_chr14/Data/original/longjump_2.fastq \
-k 10,100 \
-o reads/gage/Hg_chr14/Data/original/subsets/0/
"""

import fileinput, math, os, random, sys, time
import SeqIO
from collections import defaultdict
from optparse import OptionParser

USAGE = """Usage: ./gen_rand_samp.py -1 [first mate files sep by comma] -2 [second mate files sep by comma] -k [num_samples]
    -1  --1                 Fastq filenames separated by commas that contain the first
                            mates.
    -2  --2                 Fastq filenames separated by commas that contain the second
                            mates.
    -k  --samples           Number of samples
    -o  --output_dir        Base output directory.
    -d  --debug_level       determines how much debug output.
"""

def main():
	if len(sys.argv) < 1:
	    print USAGE
	    sys.exit()

	parser = OptionParser()
	parser.add_option("-n", "--num_trials", dest="num_trials", default="1000")
	parser.add_option("-s", "--sample_size", dest="sample_size", default="10000")
	parser.add_option("-i", "--input", dest="input", default=None)
	parser.add_option("-1", "--1", dest="first_mates")
	parser.add_option("-2", "--2", dest="second_mates")
	parser.add_option("-k", "--samples" , dest="samples", default = 0)
	parser.add_option("-o", "--output_dir", dest="output_dir", default="./")
	parser.add_option("-t", "--trials" , dest="trials", default = 0)
	parser.add_option("-d", "--debug_level" , dest="debug_level", default = 0)
	parser.set_usage(USAGE)
	(options, args) = parser.parse_args(sys.argv[1:])
	debug_level = int(options.debug_level)

	# Read through each reads, and add their respective input_number to sample_set.
	# [1 1 1 1 2 2 2 2 2 ... 6 6 6]
	# This way we can choose how many reads of what input file we should have based
	# on their abundances.
	# TODO(cmhill): Inefficient, but works fine for 100 million reads.
	total_read_set = []

	# We have to process the mates together in order.
	first_mate_files = options.first_mates.split(',')
	second_mate_files = options.second_mates.split(',')

	if len(first_mate_files) != len(second_mate_files):
		print "Error: Mate files need to have the same number."
		sys.exit(0)
	
	# Handle the option of multiple samples.
	for samples in options.samples.split(','):
		samples = int(samples)

		output_dir = options.output_dir + '/' + str(samples) + '/'
		if not os.path.exists(output_dir):
			os.makedirs(output_dir)

		# Re-open all read files.
		first_mate_readers = []
		second_mate_readers = []

		for i in range(len(first_mate_files)):
			first_mate_readers.append(SeqIO.ParseFastQ(first_mate_files[i]))
			second_mate_readers.append(SeqIO.ParseFastQ(second_mate_files[i]))

		sample_reads_dict = {}
		sample_reads = []

		k = samples
		index = 0

		file_index = 0
		while file_index < len(first_mate_readers):
			
			second_mate = second_mate_readers[file_index].next()
			for first_mate in first_mate_readers[file_index]:
				index += 1

				# Reserviour sampling algorithm.
				if len(sample_reads) < k:
					sample_reads.append((file_index, (first_mate, second_mate)))
				else:
					r = random.randrange(index)
					if r < k:
						sample_reads[r] = ((file_index, (first_mate, second_mate)))

				try:
					second_mate = second_mate_readers[file_index].next()
				except:
					pass

			if debug_level > 0:
				print 'File Index: ' + str(file_index)
				print 'Reads needed: ' + str(k)
				print sample_reads
			
			file_index += 1

		# TODO(cmhill): Remove, since we print the reads out right away.
		sample_reads_dict[file_index] = sample_reads

		file_index = 0
		# Write out these sample reads to file.
		# Re-open all read files.
		first_mate_writers = []
		second_mate_writers = []

		for i in range(len(first_mate_files)):
			first_mate_writers.append(open(output_dir + '/' + str(file_index) + '_1.fastq', 'w'))
			second_mate_writers.append(open(output_dir + '/' + str(file_index) + '_2.fastq', 'w'))
			file_index += 1
		
		for reads in sample_reads:
			first_mate_writers[reads[0]].write('\n'.join(reads[1][0]) + '\n')
			second_mate_writers[reads[0]].write('\n'.join(reads[1][1]) + '\n')


if __name__ == '__main__':
	main()