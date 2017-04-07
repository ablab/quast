#! /usr/bin/env python

"""
calc_prob.py : Takes a set of reads and an assembly and generates
a list of probabilities representing the probability that a read
came from the assembly.
"""

import math, glob, os, random, re, shlex, subprocess, sys, time
import SeqIO
from collections import defaultdict
from optparse import OptionParser, Option
from tempfile import mkstemp

threads = "1"
debug_level = 0
match_prob = .98
mismatch_prob = .005
max_alignments = 10000
temp_files = []
outfile = None
contig_abundance = defaultdict(lambda : 1.0)

def getInsertProbability(mu, sigma, x):
    """
    Return the probability we'd encounter a matepair insert of this size,
    assuming insert sizes follow a normal distribution.
    Taken from: http://telliott99.blogspot.com/2010/02/plotting-normal-distribution-with.html
    """
    z = 1.0 * (x - mu) / sigma
    e = math.e ** (-0.5 * z ** 2)
    C = math.sqrt(2 * math.pi) * sigma
    return 1.0 * e / C

def setupContigAbundance(abundance_filename):
    """
    Build the dictionary to store the 'contig -> nominal abundance' mapping.
    """
    global contig_abundance
    contig_abundance = defaultdict(lambda : 0.0)
    abundance_file = open(abundance_filename, 'r')
    for entry in abundance_file.readlines():
        abundance_tuple = entry.strip().split()
        contig_abundance[abundance_tuple[0]] = float(abundance_tuple[1])
        #if debug_level > 0:
        #    sys.stderr.write('\t'.join(abundance_tuple) + '\n')

def getAssemblyLength(assembly_fasta):
    """
    Return the total length of the assembly of the given assembly fasta file.
    If an abundance file (for metagenomic analysis) is specified, we have to 
    multiple each contig by its nominal abundance.
    """
    assembly_length = 0.0
    pf = SeqIO.ParseFasta(assembly_fasta)
    tuple = pf.getRecord()
    while tuple is not None:
        #print contig_abundance[tuple[0].split(' ')[0]]
        assembly_length += contig_abundance[tuple[0].split(' ')[0]] * len(tuple[1])
        if debug_level > 0:
            sys.stderr.write('Contig ' + tuple[0].split(' ')[0] + 'length: ' + str(assembly_length) + '\n')
        tuple = pf.getRecord()

    """ DEPRECIATED: uses biopython
    handle = open(assembly_fasta, "rU")
    for record in SeqIO.parse(handle, "fasta") :
        assembly_length += len(record.seq)

    handle.close()
    """
    return assembly_length

def buildBowtie2Index(index_name, reads_file):
    """
    Build a Bowtie2 index.
    """
    command = "$BT2_HOME/bowtie2-build " + os.path.abspath(reads_file) + " " + os.path.abspath(index_name)

    if debug_level > 0:
        sys.stderr.write(command + '\n')

    FNULL = open('/dev/null', 'w')
    #args = shlex.split(command)
    bowtie2_build_proc = subprocess.Popen(command, shell = True, stdout = FNULL, stderr = FNULL)
    bowtie_output, err = bowtie2_build_proc.communicate()

    return index_name

def runBowtie2(options = None, output_sam = 'temp.sam'):
    """
    Run Bowtie2 with the given options and save the SAM file.
    """

    if not options:
        sys.stderr.write("[ERROR] No Bowtie2 options specified" + '\n')
        return

    # Run bowtie on the fastq file.
    ignore = open('/dev/null', 'w')
    bowtie_stderr_name = 'bt'+ str(random.random()) # Replace with date/time.
    bowtie_stderr = open(bowtie_stderr_name, 'w')
    
    # Using bowtie 2.
    command = "$BT2_HOME/bowtie2 " + options + " -S " + output_sam
    
    if debug_level > 0:
        sys.stderr.write(command + '\n')

    args = shlex.split(command) 
    bowtie_proc = subprocess.Popen(command, shell=True, stdout=subprocess.PIPE,
            stderr=bowtie_stderr)
    bowtie_output, err = bowtie_proc.communicate()

    # Hack to print out bowtie output, since piping stderr causes issues.
    bowtie_stderr.close()
    bowtie_stderr = open(bowtie_stderr_name, 'r')
    bowtie_stats = bowtie_stderr.readlines()
    bowtie_stats = ''.join(bowtie_stats)

    # Clean up the temp file.
    bowtie_stderr.close()
    os.remove(bowtie_stderr_name)

    return bowtie_stats

def calcScoreFromReads(reads_filenames, assembly_fasta, fastq_file = False,
            assembly_index = None, assembly_length = -1, input_sam_file = None,
            output_sam_file = "output_sam_file"):
    """
    Calculate the probability the reads came from an assembly using bowtie2.
    """

    # If a SAM file is given, use that instead of re-running bowtie.
    alignments = None
    bowtie_stats = None
    if input_sam_file:
        if debug_level > 0:
            sys.stderr.write('Reading from SAM file: ' + input_sam_file + '\n')
        output_sam_file = input_sam_file
    else:
        # Using bowtie2.
        # Create the bowtie2 index if it wasn't given as input.
        if not assembly_index:
            if not os.path.exists(os.path.abspath('.')+'/indexes'):
                os.makedirs(os.path.abspath('.')+'/indexes')
            fd, index_path = mkstemp(prefix='temp_', dir=(os.path.abspath('.')+'/indexes/'))
            #index_path = "./indexes/temp_" + time.strftime("%y_%m_%d_%I_%M_%S", time.localtime())
            try:
                os.mkdir(os.path.dirname(index_path))
            except:
                pass
            
            buildBowtie2Index(os.path.abspath(index_path), os.path.abspath(assembly_fasta))
            assembly_index = os.path.abspath(index_path)

        input_sam_file = output_sam_file
        read_type = " -f "
        if fastq_file:
            read_type = " -q "
        
        bowtie2_args = "-a -x " + assembly_index + read_type + " -U " + reads_filenames + \
                " --very-sensitive -k " + str(max_alignments) + " --reorder -p " + threads
        # --sam-no-hd

        bowtie_stats = runBowtie2(bowtie2_args, output_sam_file)
        if debug_level > 0:
            sys.stderr.write('Bowtie alignment results:\n' +  bowtie_stats + '\n')

    # Running total of the probability values for a query.
    prev_query_seq = None
    total_query_score = 0.0

    # Create a hash dictionary to store alignment hits.
    read_scores = defaultdict(float)

    alignments = open(output_sam_file, 'r')

    for alignment in alignments:
        # Trim the header file
        if alignment.startswith('@'):
            continue

        query_align_tuple = alignment.split('\t')
        query_seq = query_align_tuple[0]
        if prev_query_seq is None:
            prev_query_seq = query_seq

        # The SAM alignments are in order based on query sequences,
        # so we can just keep a running total of the probability values.
        if prev_query_seq != query_seq:
            total_query_score = total_query_score / (2 * assembly_length)
            outfile.write(str(prev_query_seq) + '\t' + str(total_query_score) + '\n')

            total_query_score = 0.0
            prev_query_seq = query_seq
            
            # # TODO(cmhill): Matepairs, ignore for now.
            # The SAM record's bitfield should be set for read-paired (0x1)
            # and read in proper pair (0x2).
            """if "/" not in query_seq and int(query_align_tuple[1]) & 0x1:
                if int(query_align_tuple[1]) & 0x40:
                if int(query_align_tuple[1]) & 0x80:
            """

        try:
            # Bowtie2 alignments will have "NM:i:<N>" in the optional fields of valid alignments.
            edit_dist = re.search('NM:i:(\d+)', ''.join(query_align_tuple[11:]))
            if edit_dist:
                edit_dist = int(edit_dist.group(1))
                seq_len = len(query_align_tuple[9])
                score = math.pow(mismatch_prob, edit_dist) * math.pow(match_prob, seq_len - edit_dist)
                total_query_score += score * contig_abundance[query_align_tuple[2]]
            #else:
            #     bowtie_alignments[query_seq] = 0
        except:
            sys.stderr.write(query_align_tuple + '\n')

    # Print out the last score
    total_query_score = total_query_score / (2 * assembly_length)
    outfile.write(str(prev_query_seq) + '\t' + str(total_query_score) + '\n')

    # TODO(cmhill): Re-add the alignment statistics tracking.
    no_alignments = 0
    exactly_one_alignment = 0
    more_than_one_alignment = 0
    overall_alignment_rate = 0
    unaligned_seq_count = 0

    return

def calcScoreFromMates(first_mate_filenames, second_mate_filenames, assembly_fasta,
        assembly_index, assembly_length, orientations = "fr", 
        min_insert_sizes = '0', max_insert_sizes = '500', mu = '180', sigma = '18',
        input_sam_files = None, output_sam_file = 'tmp.SAM', fastq_file = False):
    """
    Calculate the probabilistic coverage statistics given a fasta file,
    a bowtie index of an assembly, and the length of said assembly.
    """

    reported_assembly_length = assembly_length

    # We have to process the mates together in order.
    first_mate_files = first_mate_filenames.split(',')
    second_mate_files = second_mate_filenames.split(',')

    # Along with their insert lengths.
    min_insert_list = min_insert_sizes.split(',')
    max_insert_list = max_insert_sizes.split(',')

    # Along with their orientations.
    orientation_list = orientations.split(',')

    # Along with their insert size distributions.
    insert_size_avg_list = mu.split(',')
    insert_size_std_dev_list = sigma.split(',')

    # Use given SAM files?
    sam_file_list = None
    if input_sam_files:
        sam_file_list = input_sam_files.split(',')
    

    if len(first_mate_files) != len(second_mate_files):
        sys.stderr.write("Error: Mate files need to have the same number." + '\n')
        sys.exit(0)

    """TODO(cmhill): Old statistics we will want to re-implement.
    summation = 0
    total_reads = 0
    no_alignments = 0
    exactly_one_alignment = 0
    more_than_one_alignment = 0
    overall_alignment_rate = 0
    """

    # Some error correction methods produce reads with id's starting at 0.
    # The problem is that these sequences may interfere with previously
    # processed sequences.
    for i in range(len(first_mate_files)):
        min_insert_size = min_insert_sizes[0]
        max_insert_size = max_insert_sizes[0]
        orientation = orientation_list[0]
        insert_size_avg = int(insert_size_avg_list[0])
        insert_size_std_dev = int(insert_size_std_dev_list[0])
        input_sam_file = None

        # Get the insert length used for the mate pairs.
        if len(min_insert_list) > i and len(max_insert_list) > i:
            min_insert_size = min_insert_list[i]
            max_insert_size = max_insert_list[i]
        
        # Get the insert avg, std_dev used for the mate pairs.
        if len(insert_size_avg_list) > i and len(insert_size_std_dev_list) > i:
            insert_size_avg = int(insert_size_avg_list[i])
            insert_size_std_dev = int(insert_size_std_dev_list[i])

        if len(orientation_list) > i:
            orientation = orientation_list[i]

        if sam_file_list:
            if len(sam_file_list) > i:
                input_sam_file = sam_file_list[i]
            else:
                input_sam_file = sam_file_list[0]

        # Calculate score from this collection of mate pairs.
        getScoreFromMatePairs(first_mate_files[i], second_mate_files[i], assembly_fasta, assembly_index, assembly_length,
                min_insert_size, max_insert_size, orientation, insert_size_avg, insert_size_std_dev, input_sam_file,
                output_sam_file + '_' + str(i), fastq_file)


def getScoreFromMatePairs(first_mate_filename, second_mate_filename, assembly_fasta, assembly_index,
        assembly_length, min_insert = "0", max_insert = "500", orientation = "fr",
        insert_size_avg = 180, insert_size_std_dev = 18, input_sam_file = None,
        output_sam_file = "output_sam_file", fastq_file = False):
    """
    Calculate the probabilistic coverage statistics given a fasta file,
    a bowtie index of an assembly, and the length of said assembly.
    """

    # If a SAM file is given, use that instead of re-running bowtie.
    alignments = None
    bowtie_stats = None
    if input_sam_file:
        if debug_level > 0:
            sys.stderr.write('Reading from SAM file: ' + input_sam_file + '\n')
        output_sam_file = input_sam_file
    else:
        # Using bowtie2.
        # Create the bowtie2 index if it wasn't given as input.
        if not assembly_index:
            if not os.path.exists(os.path.abspath('.')+'/indexes'):
                os.makedirs(os.path.abspath('.')+'/indexes')
            fd, index_path = mkstemp(prefix='temp_', dir=(os.path.abspath('.')+'/indexes/'))
            #index_path = "indexes/temp_" + time.strftime("%y_%m_%d_%I_%M_%S", time.localtime())
            try:
                os.mkdir(os.path.dirname(index_path))
            except:
                pass
            
            buildBowtie2Index(os.path.abspath(index_path), os.path.abspath(assembly_fasta))
            assembly_index = os.path.abspath(index_path)

        input_sam_file = output_sam_file

        read_type = " -f "
        if fastq_file:
            read_type = " -q "

        # Using bowtie 2. 
        input_sam_file = output_sam_file
        bowtie2_args = "-a -x " + assembly_index + read_type + " -1 " + first_mate_filename + " -2 " + second_mate_filename + \
                " -p " + threads + " --very-sensitive -k " + str(max_alignments) + " --reorder --no-mixed --" + orientation + " -I " + min_insert + \
                " -X " + max_insert
        #--sam-no-hd
        bowtie_stats = runBowtie2(bowtie2_args, output_sam_file)
        if debug_level > 0:
            sys.stderr.write('Bowtie alignment results:\n' +  bowtie_stats + '\n')
    
    # Used to calculate coverage statistic.
    #num_reads = 0
    #summation = 0
    #diff_alignments = 0

    #no_alignments_count = 0
    #tmp_count = 0
    
    # Running total of the probability values for a query.
    prev_query_seq = None
    total_query_score = 0.0

    alignments = open(output_sam_file, 'r')
    alignment = alignments.readline()
    
    # Trim the header file
    while alignment.startswith('@'):
        alignment = alignments.readline()

    while alignment:
        query_align_tuple = alignment.split('\t')
        query_seq = query_align_tuple[0]

        if prev_query_seq is None:
            prev_query_seq = query_seq

        score = 0.0
        get_mate_probability = False

        # print query_seq
        try:
            # The SAM record's bitfield should be set for read-paired (0x1)
            # and read in proper pair (0x2).
            """if (int(query_align_tuple[1]) & 0x3) == 0x3:
                #tmp_count += 1
                pass
            if query_align_tuple[2] == '*':
                # no_alignments_count += 1
                pass
            else:
                #if prev_query_seq is None:
                #    prev_query_seq = query_seq
            """
            # The SAM alignments are in order based on query sequences,
            # so we can just keep a running total of the probability values.
            if prev_query_seq != query_seq:
                outfile.write(str(prev_query_seq) + '\t' + str(total_query_score) + '\n')

                total_query_score = 0.0
                prev_query_seq = query_seq
            
            # Bowtie2 alignments will have "NM:i:<N>" in the optional fields of valid alignments.
            edit_dist = re.search('NM:i:(\d+)', ''.join(query_align_tuple[11:]))
            if edit_dist:
                edit_dist = int(edit_dist.group(1))
                seq_len = len(query_align_tuple[9])
                
                score = math.pow(mismatch_prob, edit_dist) * math.pow(match_prob, seq_len - edit_dist)

                # Multiple probability by the insert size probability.
                mate_pair_len = math.fabs(int(query_align_tuple[8]))

                if debug_level > 1:
                    sys.stderr.write(query_align_tuple[8] + '\t' + str(getInsertProbability(insert_size_avg, insert_size_std_dev, mate_pair_len)) + '\n')
                
                score *= getInsertProbability(insert_size_avg, insert_size_std_dev, mate_pair_len)
                get_mate_probability = True
        except:
            sys.stderr.write(query_align_tuple + '\n')

        # If the mate matches, get the probability of the mate, otherwise skip the
        # mate and find the next pair.
        alignment = alignments.readline()
        if get_mate_probability:
            query_align_tuple = alignment.split('\t')
            try:
                # Bowtie2 alignments will have "NM:i:<N>" in the optional fields of valid alignments.
                edit_dist = re.search('NM:i:(\d+)', ''.join(query_align_tuple[11:]))
                if edit_dist:
                    edit_dist = int(edit_dist.group(1))
                    seq_len = len(query_align_tuple[9])
                    score *= math.pow(mismatch_prob, edit_dist) * math.pow(match_prob, seq_len - edit_dist)

                    total_query_score += (score * contig_abundance[query_align_tuple[2]]) / (2 * assembly_length)
                else:
                    sys.stderr.write('ERROR: MATE DOES NOT MATCH' + '\n')
            except:
                sys.stderr.write(query_align_tuple + '\n')

        alignment = alignments.readline()

    # Print out the last score
    outfile.write(str(prev_query_seq) + '\t' + str(total_query_score) + '\n')

    """
    print "# Reads: " + str(num_reads)
    # print "# alignments: " + str(len(alignments))
    # print "# reads with at least one alignment: " + str(diff_alignments) 
    print "Summation: " + str(summation)
    print "Assembly length: " + str(assembly_length)
    print "Score: " + str(probability)
    """

    return


def main():
    parser = OptionParser()
    parser.add_option("-i", "--input", dest="reads_filenames", help="filename for input reads separated by commas. Must enter a fasta OR fastq filename.")
    parser.add_option("-q", "--fastq", dest="fastq_file", default=False, action='store_true', help="if set, input reads are fastq format (fasta by default).")
    parser.add_option("-b", "--bowtie2_index", dest="bowtie2_index", help="name of bowtie index for the assembly.")
    parser.add_option("-p", "--threads", dest="threads", default="1", help="number of threads to use for bowtie.")
    parser.add_option("-a", "--assembly_fasta", dest="assembly_fasta", help="name of the fasta file of the assembly. Used to calculate the length of the assembly.")
    parser.add_option("-s", "--input_sam_files", dest="input_sam_files", default = None, help="name of the SAM files for the corresponding reads.")
    parser.add_option("-S", "--output_sam_file", dest="output_sam_file", default = None, help="write bowtie SAM output to files starting with this prefix.")
    parser.add_option("-1", "--1", dest="first_mates", help="Fastq filenames separated by commas that contain the first mates.")
    parser.add_option("-2", "--2", dest="second_mates", help="Fastq filenames separated by commas that contain the second mates.")
    parser.add_option("-I", "--minins", dest="min_insert_sizes", help="Min insert sizes for mate pairs separated by commas.")
    parser.add_option("-X", "--maxins", dest="max_insert_sizes", help="Max insert sizes for mate pairs separated by commas.")
    parser.add_option("-o", "--orientations", dest="orientations", default="fr", help="Orientation of the mates.")
    parser.add_option("-m", "--mu" , dest="mu", default = "180", help="average mate pair insert sizes.")
    parser.add_option("-t", "--sigma" , dest="sigma", default = "18", help="standard deviation of mate pair insert sizes.")
    parser.add_option("-k", "--keep_temp_files" , dest="keep_temp_files", default = False, action='store_true')
    parser.add_option("-d", "--debug_level" , dest="debug_level", default = "0")
    parser.add_option("-c", "--mismatch_prob", dest="mismatch_prob", default = "0.02")
    parser.add_option("-r", "--match_prob", dest="match_prob", default = "0.98")
    parser.add_option("-x", "--max_alignments", dest="max_alignments", default = "10000", help="bowtie2 parameter to set the max number of alignments.")
    parser.add_option("-u", "--output", dest="output", default="-", help="Specifies where to write the probabilities (default is stdout).")
    parser.add_option("-n", "--abundance_file", dest="abundance_file", default="", help="File name for abundance file (metagenomics).")
    (options, args) = parser.parse_args(sys.argv[1:])

    if len(sys.argv) < 3:
        print(Option.help)
        sys.exit()



    global threads
    threads = options.threads

    global debug_level
    debug_level = int(options.debug_level)

    global mismatch_prob
    mismatch_prob = float(options.mismatch_prob) / 4

    global match_prob
    match_prob = 1 - float(options.mismatch_prob)

    global max_alignments
    max_alignments = int(options.max_alignments)

    global outfile
    if options.output == '-':
        outfile = sys.stdout
    else:
        outfile = open(options.output, "w")

    if debug_level > 0:
        sys.stderr.write('Mismatch prob: ' + str(mismatch_prob) + ", match prob: " + str(match_prob) + '\n')

    # If no output SAM is specified, create a temp one.
    temp_files_prefix = None
    if not options.output_sam_file:

        if not os.path.exists(os.path.abspath('.')+'/tmp'):
            os.makedirs(os.path.abspath('.')+'/tmp')

        fd, temp_path = mkstemp(prefix='aligner_sam_', dir=(os.path.abspath('.')+'/tmp'))
        if debug_level > 0:
            sys.stderr.write("Creating temp SAM file:\t" + temp_path + '\n')
        options.output_sam_file = temp_path
        temp_files_prefix = temp_path
        temp_files.append(temp_path)

    if options.abundance_file:
        assembly_length = setupContigAbundance(options.abundance_file)

    assembly_length = -1
    if options.assembly_fasta:
        assembly_length = getAssemblyLength(options.assembly_fasta)
    else:
        print("Please enter an assembly fasta (-a)")
        print(parser.print_usage())
        sys.exit()

    if options.reads_filenames:
        calcScoreFromReads(options.reads_filenames, options.assembly_fasta,
                options.fastq_file, options.bowtie2_index, assembly_length,
                options.input_sam_files, options.output_sam_file)
    
    elif options.first_mates and options.second_mates:
        # Use special insert sizes?
        if options.min_insert_sizes and options.max_insert_sizes:
            calcScoreFromMates(options.first_mates, options.second_mates, options.assembly_fasta,  
                    options.bowtie2_index, assembly_length, options.orientations,
                    options.min_insert_sizes, options.max_insert_sizes, options.mu,
                    options.sigma, options.input_sam_files, options.output_sam_file,
                    options.fastq_file)
        else:
            calcScoreFromMates(options.first_mates, options.second_mates,   
                    options.bowtie2_index, assembly_length, options.orientations,
                    options.fastq_file)

    else:
        print(parser.print_usage())

    # Clean up temporary files?
    if not options.keep_temp_files:
        if temp_files_prefix:
            # Make sure we're deleting a calc_prob.py file.
            # TODO(cmhill): Might cause issues on non-linux machines.
            if 'aligner_sam_' in temp_files_prefix:
                for filename in glob.glob(temp_files_prefix + "*"):
                    os.remove(filename)


if __name__ == '__main__':
    main()
