############################################################################
# Copyright (c) 2011-2012 Saint-Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################
#
# Some comments in this script was kindly provided by Plantagora team and
# modified by QUAST team. It is not licensed under GPL as other parts of QUAST,
# but it can be distributed and used in QUAST pipeline with current remarks and
# citation. For more details about assess_assembly.pl please refer to
# http://www.plantagora.org website and to the following paper:
#
# Barthelson R, McFarlin AJ, Rounsley SD, Young S (2011) Plantagora: Modeling
# Whole Genome Sequencing and Assembly of Plant Genomes. PLoS ONE 6(12):
# e28436. doi:10.1371/journal.pone.0028436
############################################################################

import os
import platform
import subprocess
import fastaparser
from qutils import id_to_str

def spaceline(line):
    return ' '.join(str(x) for x in line)

def process_misassembled_contig(plantafile, output_file, i_start, i_finish, contig, prev, sorted_aligns, is_1st_chimeric_half, ns, smgap, rc, assembly, misassembled_contigs, extensive_misassembled_contigs):
    region_misassemblies = 0
    region_local_misassemblies = 0
    for i in xrange(i_start, i_finish):
        print >>plantafile, '\t\t\tReal Alignment %d: %s' % (i+1, spaceline(sorted_aligns[i]))
        #Calculate the distance on the reference between the end of the first alignment and the start of the second
        gap = sorted_aligns[i+1][0] - sorted_aligns[i][1]

        #Check strands
        strand1 = (sorted_aligns[i][3] > sorted_aligns[i][4])
        strand2 = (sorted_aligns[i+1][3] > sorted_aligns[i+1][4])

        if sorted_aligns[i][11] != sorted_aligns[i+1][11] or gap > ns + smgap or (not rc and strand1 != strand2): # different chromosomes or large gap or different strands
            #Contig spans chromosomes or there is a gap larger than 1kb
            #MY: output in coords.filtered
            print >>output_file, spaceline(prev)
            prev = list(sorted_aligns[i+1])
            print >>plantafile, '\t\t\tExtensive misassembly between these two alignments: [%s] @ %d and %d' % (sorted_aligns[i][11], sorted_aligns[i][1], sorted_aligns[i+1][0])

            extensive_misassembled_contigs.add(sorted_aligns[i][11])
            # Kolya: removed something about ref_features

            region_misassemblies += 1
            misassembled_contigs[contig] = len(assembly[contig])

        else:
            if gap < 0:
                #There is overlap between the two alignments, a local misassembly
                print >>plantafile, '\t\tOverlap between these two alignments (local misassembly): [%s] %d to %d' % (sorted_aligns[i][11], sorted_aligns[i][1], sorted_aligns[i+1][0])
            else:
                #There is a small gap between the two alignments, a local misassembly
                print >>plantafile, '\t\tGap in alignment between these two alignments (local misassembly): [%s] %d' % (sorted_aligns[i][11], sorted_aligns[i][0])

            region_local_misassemblies += 1

            #MY:
            prev[1] = sorted_aligns[i+1][1] # [E1]
            prev[3] = 0 # [S2]
            prev[4] = 0 # [E2]
            prev[6] = prev[1] - prev[0] # [LEN1]
            prev[7] = prev[7] + sorted_aligns[i+1][7] + (gap if gap < 0 else 0) # [LEN2]

        #MY: output in coords.filtered
        if not is_1st_chimeric_half:
            print >>output_file, spaceline(prev)

        #Record the very last alignment
        i = i_finish
        print >>plantafile, '\t\t\tReal Alignment %d: %s' % (i+1, spaceline(sorted_aligns[i]))

    return list(prev), region_misassemblies, region_local_misassemblies

def clear_files(filename, nucmerfilename):
    # delete temporary files
    for ext in ['.delta', '.mgaps', '.ntref', '.gp']:
        if os.path.isfile(nucmerfilename + ext):
            os.remove(nucmerfilename + ext)
    if os.path.isfile('nucmer.error'):
        os.remove('nucmer.error')
    if os.path.isfile(filename + '.clean'):
        os.remove(filename + '.clean')

def plantakolya(cyclic, draw_plots, filename, nucmerfilename, myenv, output_dir, rc, reference, report_dict):
    # remove old nucmer coords file
    if os.path.isfile(nucmerfilename + '.coords'):
        os.remove(nucmerfilename + '.coords')
        # run plantakolya tool
    logfilename_out = output_dir + '/plantakolya_' + os.path.basename(filename) + '.stdout'
    logfilename_err = output_dir + '/plantakolya_' + os.path.basename(filename) + '.stderr'
    logfile_err = open(logfilename_err, 'a')
    print '    Logging to files', logfilename_out, 'and', os.path.basename(logfilename_err), '...',
    # reverse complementarity is not an extensive misassemble
    peral = 0.99
    maxun = 10
    smgap = 1000
    umt = 0.1 # threshold for misassembled contigs with aligned less than $umt * 100% (Unaligned Missassembled Threshold)
    coords_filename = nucmerfilename + '.coords'
    delta_filename = nucmerfilename + '.delta'
    coords_btab_filename = nucmerfilename + '.coords.btab'
    coords_filtered_filename = nucmerfilename + '.coords.filtered'
    unaligned_filename = nucmerfilename + '.unaligned'
    if os.path.isfile(coords_filename):
        os.remove(coords_filename)
    plantafile = open(logfilename_out, 'a')
    print >> plantafile, 'Cleaning up contig headers...'
    # TODO: clean contigs?
    print >> plantafile, 'Aligning contigs to reference...'
    print >> plantafile, '\tRunning nucmer...'
    print 'NUCmer... ',
    subprocess.call(['nucmer', '--maxmatch', '-p', nucmerfilename, reference, filename],
        stdout=open(logfilename_out, 'a'), stderr=logfile_err, env=myenv)
    subprocess.call(['show-coords', '-B', delta_filename],
        stdout=open(coords_btab_filename, 'w'), stderr=logfile_err, env=myenv)
    import sympalign

    sympalign.do(1, coords_filename, [coords_btab_filename])
    if not os.path.isfile(coords_filename):
        print 'failed'
        return
    if len(open(coords_filename).readlines()[-1].split()) < 13:
        print >> logfile_err, 'Nucmer ended early'
        return

    # Loading the alignment files
    print >> plantafile, 'Parsing coords...'
    aligns = {}
    coords_file = open(coords_filename)
    coords_filtered_file = open(coords_filtered_filename, 'w')
    coords_filtered_file.write(coords_file.readline())
    coords_filtered_file.write(coords_file.readline())
    sum_idy = 0.0
    num_idy = 0
    for line in coords_file:
        assert line[0] != '='
        #Clear leading spaces from nucmer output
        #Store nucmer lines in an array
        line = line.split()
        contig = line[12]
        for i in [0, 1, 3, 4, 6, 7]:
            line[i] = int(line[i])
        line[9] = float(line[9])
        sum_idy += line[9]
        num_idy += 1
        aligns.setdefault(contig, []).append(line)
    avg_idy = sum_idy / num_idy if num_idy else 0

    # Loading the assembly contigs
    print >> plantafile, 'Loading Assembly...'
    assembly = {}
    assembly_ns = {}
    for name, seq in fastaparser.read_fasta(filename):
        seq = seq.upper()
        assembly[name] = seq
        if 'N' in seq:
            assembly_ns[name] = [pos for pos in xrange(len(seq)) if seq[pos] == 'N']

    # Loading the reference sequences
    print >> plantafile, 'Loading Reference...' # TODO: move up
    references = {}
    for name, seq in fastaparser.read_fasta(reference):
        name = name.split()[0] # no spaces in reference header
        references[name] = seq
        print >> plantafile, '\tLoaded [%s]' % name

    # Loading the regions (if any)
    regions = {}
    total_reg_len = 0
    total_regions = 0
    print >> plantafile, 'Loading Regions...'
    # TODO: gff
    print >> plantafile, '\tNo regions given, using whole reference.'
    for name, seq in references.iteritems():
        regions.setdefault(name, []).append([1, len(seq)])
        total_regions += 1
        total_reg_len += len(seq)
    print >> plantafile, '\tTotal Regions: %d' % total_regions
    print >> plantafile, '\tTotal Region Length: %d' % total_reg_len

    unaligned = 0
    partially_unaligned = 0
    total_unaligned = 0
    ambiguous = 0
    total_ambiguous = 0
    uncovered_regions = 0
    uncovered_region_bases = 0
    misassembled_partially_unaligned = 0

    region_misassemblies = 0
    region_local_misassemblies = 0
    misassembled_contigs = {}
    extensive_misassembled_contigs = set()

    print >> plantafile, 'Analyzing contigs...'

    unaligned_file = open(unaligned_filename, 'w')
    for contig, seq in assembly.iteritems():
        #Recording contig stats
        ctg_len = len(seq)
        if contig in assembly_ns:
            ns = len(assembly_ns[contig])
        else:
            ns = 0
        print >> plantafile, '\tCONTIG: %s (%dbp)' % (contig, ctg_len)
        #Check if this contig aligned to the reference
        if contig in aligns:
            #Pull all aligns for this contig
            num_aligns = len(aligns[contig])

            #Sort aligns by length and identity
            sorted_aligns = sorted(aligns[contig], key=lambda x: (x[7] * x[9], x[7]), reverse=True)
            top_len = sorted_aligns[0][7]
            top_id = sorted_aligns[0][9]
            top_aligns = []
            print >> plantafile, 'Top Length: %s  Top ID: %s' % (top_len, top_id)

            #Check that top hit captures most of the contig (>99% or within 10 bases)
            if top_len > ctg_len * peral or ctg_len - top_len < maxun:
                #Reset top aligns: aligns that share the same value of longest and higest identity
                top_aligns.append(sorted_aligns[0])
                sorted_aligns = sorted_aligns[1:]

                #Continue grabbing alignments while length and identity are identical
                while sorted_aligns and top_len == sorted_aligns[0][7] and top_id == sorted_aligns[0][9]:
                    top_aligns.append(sorted_aligns[0])
                    sorted_aligns = sorted_aligns[1:]

                #Mark other alignments as ambiguous
                while sorted_aligns:
                    ambig = sorted_aligns.pop()
                    print >> plantafile, '\t\tMarking as ambiguous: %s' % spaceline(ambig)
                    # Kolya: removed redundant code about $ref

                print >> coords_filtered_file, spaceline(top_aligns[0])

                if len(top_aligns) == 1:
                    #There is only one top align, life is good
                    print >> plantafile, '\t\tOne align captures most of this contig: %s' % spaceline(top_aligns[0])
                else:
                    #There is more than one top align
                    print >> plantafile, '\t\tThis contig has %d significant alignments. [ambiguous]' % len(
                        top_aligns)
                    #Record these alignments as ambiguous on the reference
                    for align in top_aligns:
                        print >> plantafile, '\t\t\tAmbiguous Alignment: %s' % spaceline(align)
                        # Kolya: removed redundant code about $ref
                    #Increment count of ambiguous contigs and bases
                    ambiguous += 1
                    total_ambiguous += ctg_len
            else:
                #Sort all aligns by position on contig, then length # TODO: check if bug in plantagora
                sorted_aligns = sorted(sorted_aligns, key=lambda x: (x[7], x[9]), reverse=True)
                sorted_aligns = sorted(sorted_aligns, key=lambda x: min(x[3], x[4]))

                #Push first alignment on to real aligns
                real_aligns = [sorted_aligns[0]]
                last_end = max(sorted_aligns[0][3], sorted_aligns[0][4])

                #Walk through alignments, if not fully contained within previous, record as real
                for i in xrange(1, num_aligns):
                    #If this alignment extends past last alignment's endpoint, add to real, else skip
                    if sorted_aligns[i][3] > last_end or sorted_aligns[i][4] > last_end:
                        real_aligns = [sorted_aligns[i]] + real_aligns
                        last_end = max(sorted_aligns[i][3], sorted_aligns[i][4])
                    else:
                        print >> plantafile, '\t\tSkipping [%d][%d] redundant alignment %d %s' % (
                        sorted_aligns[i][0], sorted_aligns[i][1], i, spaceline(sorted_aligns[i]))
                        # Kolya: removed redundant code about $ref

                if len(real_aligns) == 1:
                    #There is only one alignment of this contig to the reference
                    #MY: output in coords.filtered
                    print >> coords_filtered_file, spaceline(real_aligns[0])

                    #Is the contig aligned in the reverse compliment?
                    #Record beginning and end of alignment in contig
                    if sorted_aligns[0][3] > sorted_aligns[0][4]:
                        end, begin = sorted_aligns[0][3], sorted_aligns[0][4]
                    else:
                        end, begin = sorted_aligns[0][4], sorted_aligns[0][3]
                    if (begin - 1) or (ctg_len - end):
                        #Increment tally of partially unaligned contigs
                        partially_unaligned += 1
                        #Increment tally of partially unaligned bases
                        total_unaligned += begin - 1
                        total_unaligned += ctg_len - end
                        print >> plantafile, '\t\tThis contig is partially unaligned. (%d out of %d)' % (
                        top_len, ctg_len)
                        print >> plantafile, '\t\tUnaligned bases: 1 to %d (%d)' % (begin, begin - 1)
                        print >> plantafile, '\t\tUnaligned bases: %d to %d (%d)' % (end, ctg_len, ctg_len - end)
                else:
                    #There is more than one alignment of this contig to the reference
                    print >> plantafile, '\t\tThis contig is misassembled. %d total aligns.' % num_aligns
                    #Reset real alignments and sum of real alignments
                    #Sort real alignments by position on the reference
                    sorted_aligns = sorted(real_aligns, key=lambda x: (x[11], x[0]))
                    # Counting misassembled contigs which are partially unaligned
                    all_aligns_len = sum(x[7] for x in sorted_aligns)
                    if all_aligns_len < umt * ctg_len:
                        print >> plantafile, '\t\t\tWarning! Contig length is %d and total length of all aligns is %d' % (
                        ctg_len, all_aligns_len)
                        misassembled_partially_unaligned += 1
                    sorted_num = len(sorted_aligns) - 1
                    chimeric_found = False

                    #MY: computing cyclic references
                    if cyclic:
                        if sorted_aligns[0][0] - 1 + total_reg_len - sorted_aligns[sorted_num][
                                                                     1] <= ns + smgap:  # chimerical misassembly
                            chimeric_found = True

                            # find chimerical alignment between "first" blocks and "last" blocks
                            chimeric_index = 0
                            for i in xrange(sorted_num):
                                gap = sorted_aligns[i + 1][0] - sorted_aligns[i][1]
                                if gap > ns + smgap:
                                    chimeric_index = i + 1
                                    break

                            #MY: for merging local misassemlbies
                            prev = list(sorted_aligns[chimeric_index])

                            # process "last half" of blocks
                            prev, x, y = process_misassembled_contig(plantafile, coords_filtered_file,
                                chimeric_index, sorted_num, contig, prev, sorted_aligns, True, ns, smgap, rc,
                                assembly, misassembled_contigs, extensive_misassembled_contigs)
                            region_misassemblies += x
                            region_local_misassemblies += y
                            print >> plantafile, '\t\t\tChimerical misassembly between these two alignments: [%s] @ %d and %d' % (
                            sorted_aligns[sorted_num][11], sorted_aligns[sorted_num][1], sorted_aligns[0][0])

                            prev[1] = sorted_aligns[0][1] # [E1]
                            prev[3] = 0 # [S2]
                            prev[4] = 0 # [E2]
                            prev[6] += sorted_aligns[0][1] - sorted_aligns[0][0] + 1 # [LEN1]
                            prev[7] += sorted_aligns[0][7] # [LEN2]

                            # process "first half" of blocks
                            prev, x, y = process_misassembled_contig(plantafile, coords_filtered_file, 0,
                                chimeric_index - 1, contig, prev, sorted_aligns, False, ns, smgap, rc, assembly,
                                misassembled_contigs, extensive_misassembled_contigs)
                            region_misassemblies += x
                            region_local_misassemblies += y

                    if not chimeric_found:
                        print >> plantafile, "here", sorted_aligns
                        #MY: for merging local misassemlbies
                        prev = list(sorted_aligns[0])
                        prev, x, y = process_misassembled_contig(plantafile, coords_filtered_file, 0, sorted_num,
                            contig, prev, sorted_aligns, False, ns, smgap, rc, assembly, misassembled_contigs,
                            extensive_misassembled_contigs)
                        region_misassemblies += x
                        region_local_misassemblies += y
                        print >> plantafile, "there", sorted_aligns

        else:
            #No aligns to this contig
            print >> plantafile, '\t\tThis contig is unaligned. (%d bp)' % ctg_len
            print >> unaligned_file, contig

            #Increment unaligned contig count and bases
            unaligned += 1
            total_unaligned += ctg_len
            print >> plantafile, '\t\tUnaligned bases: %d  total: %d' % (ctg_len, total_unaligned)

    coords_filtered_file.close()
    unaligned_file.close()

    # TODO: 'Analyzing coverage...'

    print >> plantafile, '\tLocal Misassemblies: %d' % region_local_misassemblies
    print >> plantafile, '\tMisassemblies: %d' % region_misassemblies
    print >> plantafile, '\t\tMisassembled Contigs: %d' % len(misassembled_contigs)
    misassembled_bases = sum(misassembled_contigs.itervalues())
    print >> plantafile, '\t\tMisassembled Contig Bases: %d' % misassembled_bases
    print >> plantafile, '\t\tMisassembled and Unaligned: %d' % misassembled_partially_unaligned
    print >> plantafile, 'Uncovered Regions: %d (%d)' % (uncovered_regions, uncovered_region_bases)
    print >> plantafile, 'Unaligned Contigs: %d (%d)' % (unaligned, partially_unaligned)
    print >> plantafile, 'Unaligned Contig Bases: %d' % total_unaligned
    print >> plantafile, 'Ambiguous Contigs: %d (%d)' % (ambiguous, total_ambiguous)

    report_dict[os.path.basename(filename)].append('%.2f' % avg_idy)
    report_dict[os.path.basename(filename)].append(region_local_misassemblies)
    report_dict[os.path.basename(filename)].append(region_misassemblies)
    report_dict[os.path.basename(filename)].append(len(misassembled_contigs))
    report_dict[os.path.basename(filename)].append(misassembled_bases)
    report_dict[os.path.basename(filename)].append(misassembled_partially_unaligned)
    report_dict[os.path.basename(filename)].append('%d (%d)' % (unaligned, partially_unaligned))
    report_dict[os.path.basename(filename)].append(total_unaligned)
    report_dict[os.path.basename(filename)].append('%d (%d)' % (ambiguous, total_ambiguous))

    ## outputting misassembled contigs to separate file
    fasta = [(name, seq) for name, seq in fastaparser.read_fasta(filename) if
                         name in extensive_misassembled_contigs]
    fastaparser.write_fasta_to_file(output_dir + '/' + os.path.basename(filename) + '.mis_contigs', fasta)

    plantafile.close()
    logfile_err.close()
    print 'done.'

    if draw_plots and os.path.isfile(delta_filename):
        # draw reference coverage plot
        print '    Drawing reference coverage plot...',
        plotfilename = output_dir + '/mummerplot_' + os.path.basename(filename)
        plot_logfilename_out = output_dir + '/mummerplot_' + os.path.basename(filename) + '.stdout'
        plot_logfilename_err = output_dir + '/mummerplot_' + os.path.basename(filename) + '.stderr'
        plot_logfile_out = open(plot_logfilename_out, 'w')
        plot_logfile_err = open(plot_logfilename_err, 'w')
        subprocess.call(
            ['mummerplot', '--coverage', '--postscript', '--prefix', plotfilename, delta_filename],
            stdout=plot_logfile_out, stderr=plot_logfile_err, env=myenv)
        plot_logfile_out.close()
        plot_logfile_err.close()
        print 'saved to', plotfilename + '.ps'
        for ext in ['.gp', '.rplot', '.fplot']: # remove redundant files
            if os.path.isfile(plotfilename + ext):
                os.remove(plotfilename + ext)


def plantakolya_process(cyclic, draw_plots, filename, id, myenv, output_dir, rc, reference, report_dict):
    print ' ', id_to_str(id), os.path.basename(filename), '...'
    nucmerfilename = output_dir + '/nucmer_' + os.path.basename(filename)
    plantakolya(cyclic, draw_plots, filename, nucmerfilename, myenv, output_dir, rc, reference, report_dict)
    clear_files(filename, nucmerfilename)
    ## find metrics for total report:
    report_dict[os.path.basename(filename)] += ['N/A'] * (
    len(report_dict['header']) - len(report_dict[os.path.basename(filename)]))


def do(reference, filenames, cyclic, rc, output_dir, lib_dir, draw_plots):
    if not os.path.isdir(output_dir):
        os.mkdir(output_dir)

    ########################################################################
    if platform.system() == 'Darwin':
        mummer_path = os.path.join(lib_dir, 'MUMmer3.23-osx')
    else:
        mummer_path  = os.path.join(lib_dir, 'MUMmer3.23-linux')

    ########################################################################
    report_dict = {'header' : []}
    for filename in filenames:
        report_dict[os.path.basename(filename)] = []

    # for running our MUMmer
    myenv = os.environ.copy()
    myenv['PATH'] = mummer_path + ':' + myenv['PATH']
    # making if needed
    if not os.path.exists(os.path.join(mummer_path, 'nucmer')):
        print ("Making MUMmer...")
        subprocess.call(
            ['make', '-C', mummer_path],
            stdout=open(os.path.join(mummer_path, 'make.log'), 'w'), stderr=open(os.path.join(mummer_path, 'make.err'), 'w'))

    print 'Running plantakolya tool...'
    metrics = ['Average %IDY', 'Local misassemblies', 'Misassemblies', 'Misassembled contigs', 'Misassembled contig bases', 'Misassembled and unaligned', 'Unaligned contigs', 'Unaligned contig bases', 'Ambiguous contigs']
    report_dict['header'] += metrics

    for id, filename in enumerate(filenames):
        plantakolya_process(cyclic, draw_plots, filename, id, myenv, output_dir, rc, reference, report_dict) # TODO: use joblib

    print '  Done'

    return report_dict
