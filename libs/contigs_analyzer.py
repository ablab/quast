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
import datetime
import fastaparser
import shutil
from libs import reporting, qconfig
from qutils import id_to_str

class Misassembly:
    LOCAL=0
    RELOCATION=1
    TRANSLOCATION=2
    INVERSION=3

# see bug http://ablab.myjetbrains.com/youtrack/issue/QUAST-74 for more details
def additional_cleaning(all):
    counter = 0
    cleaned = {}
    for contig in all.iterkeys():
        #print contig
        if len(all[contig]) == 1:
            cleaned[contig] = all[contig]
        else:
            # align: 0:start_contig, 1:end_contig, 2:start_reference, 3:end_reference, 4:IDY%, 5:reference_id
            sorted_aligns = sorted(all[contig], key=lambda align: min(align[0], align[1]))
            ids_to_discard = []
            for id1, align1 in enumerate(sorted_aligns[:-1]):
                if id1 in ids_to_discard:
                    continue

                for i, align2 in enumerate(sorted_aligns[id1 + 1:]):
                    id2 = i + id1 + 1
                    if id2 in ids_to_discard:
                        continue

                    # if start of align2 > end of align1 than there is no more overlapped aligns with align1
                    if min(align2[0], align2[1]) > max(align1[0], align1[1]):
                        break

                    # if all second alignment is inside first alignment then discard it
                    if max(align2[0], align2[1]) < max(align1[0], align1[1]):
                        ids_to_discard.append(id2)
                        continue

                    # if overlap is less than half of shortest of align1 and align2 we shouldn't discard anything
                    overlap_size =  max(align1[0], align1[1]) - min(align2[0], align2[1]) + 1
                    threshold = 0.5
                    if float(overlap_size) < threshold * min(abs(align2[0] - align2[1]), abs(align1[0] - align1[1])):
                        continue

                    # align with worse IDY% is under suspicion
                    len_diff =  min(align2[0], align2[1]) - min(align1[0], align1[1])
                    if align1[4] < align2[4]:
                        quality_loss = 100.0 * float(len_diff) / float(abs(align2[0] - align2[1]) + 1)
                        if quality_loss < abs(align1[4] - align2[4]):
                            #print "discard id1: ", id1, ", quality loss", quality_loss, "lendiff", len_diff, "div", float(abs(align2[0] - align2[1]) + 1)
                            ids_to_discard.append(id1)
                    elif align2[4] < align1[4]:
                        quality_loss = 100.0 * float(len_diff) / float(abs(align1[0] - align1[1]) + 1)
                        if quality_loss < abs(align1[4] - align2[4]):
                            #print "discard id2: ", id2, ", quality loss", quality_loss, "lendiff", len_diff, "div", float(abs(align1[0] - align1[1]) + 1)
                            ids_to_discard.append(id2)

            cleaned[contig] = []
            for id, align in enumerate(sorted_aligns):
                if id not in ids_to_discard:
                    cleaned[contig].append(align)
            counter += len(ids_to_discard)
            #print cleaned[contig]
    return cleaned, counter


def sympalign(out_filename, in_filename):
    print 'Running SympAlign...'
    assert in_filename[-5:] == '.btab', in_filename
    counter = [0, 0]
    all = {}
    list_events = []
    aligns = {}
    for line in open(in_filename, 'r'):
        arr = line.split('\t')
        #    Output format will consist of 21 tab-delimited columns. These are as
        #    follows: [0] query sequence ID [1] date of alignment [2] length of the query [3] alignment type
        #    [4] reference file [5] reference sequence ID [6] start of alignment in the query [7] end of alignment
        #    in the query [8] start of alignment in the reference [9] end of
        #    alignment in the reference [10] percent identity [11] percent
        #    similarity [12] length of alignment in the query [13] 0 for
        #    compatibility [14] 0 for compatibility [15] NULL for compatibility
        #    [16] 0 for compatibility [17] strand of the query [18] length of the
        #    reference sequence [19] 0 for compatibility [20] and 0 for
        #    compatibility.
        #
        # NODE_31_length_14785	Aug 23 2011	14785	NUCMER	/home/data/input/E.Coli.K12.MG1655/MG1655-K12.fasta	gi|49175990|ref|NC_000913.2|	14785	14203	3364194	3364776	100.000000	100.000000	583	0	0	NULL	0	Minus	4639675	0	0
        # NODE_31_length_14785	Aug 23 2011	14785	NUCMER	/home/data/input/E.Coli.K12.MG1655/MG1655-K12.fasta	gi|49175990|ref|NC_000913.2|	14785	1	3650675	3665459	99.945892	99.945892	14785	0	0	NULL	0	Minus	4639675	0	0
        contig_id = arr[0]
        contig_len = int(arr[2])
        ref_id = arr[5]
        sc, ec, sr, er = map(int, arr[6:10])
        p = float(arr[10])
        lc = abs(sc - ec) + 1
        # lr = abs(sr - er) + 1
        if lc != int(arr[12]):
            print '  Error: lc != int(arr[12])', lc, int(arr[12])
            return
        align = (sc, ec, sr, er, p, ref_id)
        contig = (contig_id, contig_len)
        if contig not in aligns:
            aligns[contig] = []
        aligns[contig].append(align)
        counter[0] += 1

    for contig in aligns:
        contig_id, contig_len = contig
        superb = False
        output = []
        for a in sorted(aligns[contig], key=lambda align: (-abs(align[0] - align[1]), -align[4])):
            sc, ec, sr, er, p, ref_id = a[0:6]
            lc = abs(sc - ec) + 1
            # lr = abs(sr - er) + 1
            if lc >= 0.99 * contig_len:
                superb = True
                output.append(a)
            else:
                if superb:
                    break
                discard = False
                for b in output:
                    sc2, ec2, sr2, er2, p2 = b[0:5]
                    lc2 = abs(sc2 - ec2) + 1
                    # lr2 = abs(sr2 - er2) + 1
                    if min(sc2, ec2) <= min(sc, ec) and max(sc, ec) <= max(sc2, ec2) and (
                        (lc <= 0.5 * lc2) or (p < p2 * 0.9999)):
                        discard = True
                        break
                if discard:
                    continue
                output.append(a)
        counter[1] += len(output)
        all[contig] = output
        for a in output:
            sc, ec, sr, er, p = a[0:5]
            xr = min(sr, er)
            yr = max(sr, er) + 1
            ev1 = (xr, +1, contig, a)
            ev2 = (yr, -1, contig, a)
            list_events += [ev1, ev2]

    print '  Cleaned', counter[0], 'down to', counter[1]
    all, add_counter = additional_cleaning(all)
    print '  Additionally cleaned', counter[1], 'down to', add_counter

    ouf = open(out_filename, 'w')
    print >> ouf, "    [S1]     [E1]  |     [S2]     [E2]  |  [LEN 1]  [LEN 2]  |  [% IDY]  | [TAGS]"
    print >> ouf, "====================================================================================="
    for contig in sorted(all, key=lambda contig: -contig[1]):
        contig_id, contig_len = contig
        for a in all[contig]:
            sc, ec, sr, er, p, ref_id = a[0:6]
            lc = abs(sc - ec) + 1
            lr = abs(sr - er) + 1
            label = ref_id + '\t' + contig_id
            print >> ouf, '%8d %8d  | %8d %8d  | %8d %8d  | %8.4f  | %s' % (sr, er, sc, ec, lr, lc, p, label)
    ouf.close()
    print '  Done sympaligning.'


class Mapping(object):
    def  __init__(self, s1, e1, s2, e2, len1, len2, idy, ref, contig):
        self.s1, self.e1, self.s2, self.e2, self.len1, self.len2, self.idy, self.ref, self.contig = s1, e1, s2, e2, len1, len2, idy, ref, contig

    @classmethod
    def from_line(self, line):
        # line from coords file,e.g.
        # 4324128  4496883  |   112426   285180  |   172755   172756  |  99.9900  | gi|48994873|gb|U00096.2|	NODE_333_length_285180_cov_221082
        line = line.split()
        assert line[2] == line[5] == line[8] == line[10] == '|', line
        contig = line[12]
        ref = line[11]
        s1, e1, s2, e2, len1, len2 = [int(line[i]) for i in [0, 1, 3, 4, 6, 7]]
        idy = float(line[9])
        return Mapping(s1, e1, s2, e2, len1, len2, idy, ref, contig)

    def __str__(self):
        return ' '.join(str(x) for x in [self.s1, self.e1, '|', self.s2, self.e2, '|', self.len1, self.len2, '|', self.idy, '|', self.ref, self.contig])

    def clone(self):
        return Mapping.from_line(str(self))


class Mappings(object):
    def __init__(self):
        self.aligns = {} # contig -> [mapping]
        self.cnt = 0

    def add(self, mapping):
        self.aligns.setdefault(mapping.contig, []).append(mapping)

    @classmethod
    def from_coords(cls, filename):
        file = open(filename, 'w')

        file.close()


def process_misassembled_contig(plantafile, output_file, i_start, i_finish, contig, prev, sorted_aligns, is_1st_chimeric_half, ns, smgap, assembly, misassembled_contigs, extensive_misassembled_contigs):
    region_misassemblies = []
    for i in xrange(i_start, i_finish):
        print >>plantafile, '\t\t\tReal Alignment %d: %s' % (i+1, str(sorted_aligns[i]))
        #Calculate the distance on the reference between the end of the first alignment and the start of the second
        gap = sorted_aligns[i+1].s1 - sorted_aligns[i].e1
        # overlap between positions of alignments in contig
        overlap_in_contig = 0
        cur_s = min(sorted_aligns[i].e2, sorted_aligns[i].s2)        
        cur_e = max(sorted_aligns[i].e2, sorted_aligns[i].s2)
        next_s = min(sorted_aligns[i+1].e2, sorted_aligns[i+1].s2)        
        next_e = max(sorted_aligns[i+1].e2, sorted_aligns[i+1].s2)
        if cur_s < next_s: # current alignment is earlier in contig
            overlap_in_contig = cur_e - next_s + 1
        else:              # next alignment is earlier in contig  
            overlap_in_contig = next_e - cur_s + 1

        #Check strands
        strand1 = (sorted_aligns[i].s2 > sorted_aligns[i].e2)
        strand2 = (sorted_aligns[i+1].s2 > sorted_aligns[i+1].e2)

        if sorted_aligns[i].ref != sorted_aligns[i+1].ref or gap > ns + smgap or (strand1 != strand2): # different chromosomes or large gap or different strands
            #Contig spans chromosomes or there is a gap larger than 1kb
            #MY: output in coords.filtered
            print >>output_file, str(prev)
            prev = sorted_aligns[i+1].clone()
            print >>plantafile, '\t\t\tExtensive misassembly (',

            extensive_misassembled_contigs.add(sorted_aligns[i].contig)
            # Kolya: removed something about ref_features

            if sorted_aligns[i].ref != sorted_aligns[i+1].ref:
                region_misassemblies += [Misassembly.TRANSLOCATION]
                print >>plantafile, 'translocation',
            elif gap > ns + smgap:
                region_misassemblies += [Misassembly.RELOCATION]
                print >>plantafile, 'relocation',
            elif strand1 != strand2:
                region_misassemblies += [Misassembly.INVERSION]
                print >>plantafile, 'inversion',
            misassembled_contigs[contig] = len(assembly[contig])

            print >>plantafile, ') between these two alignments: [%s] @ %d and %d' % (sorted_aligns[i].ref, sorted_aligns[i].e1, sorted_aligns[i+1].s1)

        else:
            if gap < 0:
                #There is overlap between the two alignments, a local misassembly
                print >>plantafile, '\t\tOverlap between these two alignments (local misassembly): [%s] %d to %d' % (sorted_aligns[i].ref, sorted_aligns[i].e1, sorted_aligns[i+1].s1)
            else:
                #There is a small gap between the two alignments, a local misassembly
                print >>plantafile, '\t\tGap in alignment between these two alignments (local misassembly): [%s] %d' % (sorted_aligns[i].ref, sorted_aligns[i].s1)

            region_misassemblies += [Misassembly.LOCAL]

            #MY:
            prev.e1 = sorted_aligns[i+1].e1 # [E1]
            prev.s2 = 0 # [S2]
            prev.e2 = 0 # [E2]
            prev.len1 = prev.e1 - prev.s1 # [LEN1]
            prev.len2 = prev.len2 + sorted_aligns[i+1].len2 - (overlap_in_contig if overlap_in_contig > 0 else 0) # [LEN2]

    #MY: output in coords.filtered
    if not is_1st_chimeric_half:
        print >>output_file, str(prev)

    #Record the very last alignment
    i = i_finish
    print >>plantafile, '\t\t\tReal Alignment %d: %s' % (i+1, str(sorted_aligns[i]))

    return prev.clone(), region_misassemblies

def clear_files(filename, nucmerfilename):
    if qconfig.debug:
        return
    # delete temporary files
    for ext in ['.delta', '.1delta', '.mdelta', '.unqry', '.qdiff', '.rdiff', '.1coords', '.mcoords', '.mgaps', '.ntref', '.gp', '.coords.btab']:
        if os.path.isfile(nucmerfilename + ext):
            os.remove(nucmerfilename + ext)
    if os.path.isfile('nucmer.error'):
        os.remove('nucmer.error')
    if os.path.isfile(filename + '.clean'):
        os.remove(filename + '.clean')

def plantakolya(cyclic, draw_plots, filename, nucmerfilename, myenv, output_dir, reference):
    # run plantakolya tool
    logfilename_out = output_dir + '/contigs_report_' + os.path.basename(filename) + '.stdout'
    logfilename_err = output_dir + '/contigs_report_' + os.path.basename(filename) + '.stderr'
    logfile_err = open(logfilename_err, 'a')
    print '    Logging to files', logfilename_out, 'and', os.path.basename(logfilename_err), '...',
    # reverse complementarity is not an extensive misassemble
    peral = 0.99
    maxun = 10
    smgap = 1000
    umt = 0.1 # threshold for misassembled contigs with aligned less than $umt * 100% (Unaligned Missassembled Threshold)
    nucmer_successful_check_filename = nucmerfilename + '.sf'
    coords_filename = nucmerfilename + '.coords'
    delta_filename = nucmerfilename + '.delta'
    filtered_delta_filename = nucmerfilename + '.fdelta'
    coords_btab_filename = nucmerfilename + '.coords.btab'
    coords_filtered_filename = nucmerfilename + '.coords.filtered'
    unaligned_filename = nucmerfilename + '.unaligned'
    nucmer_report_filename = nucmerfilename + '.report'
    plantafile = open(logfilename_out, 'a')

    print >> plantafile, 'Aligning contigs to reference...'

    # Checking if there are existing previous nucmer alignments.
    # If they exist, using them to save time.
    if (os.path.isfile(nucmer_successful_check_filename) and os.path.isfile(coords_filename)
        and os.path.isfile(nucmer_report_filename)):

        print >> plantafile, '\tUsing existing Nucmer alignments...'
        print 'Using existing Nucmer alignments... ',
    else:

        print >> plantafile, '\tRunning Nucmer...'
        print 'Running Nucmer... ',
        # GAGE params of Nucmer
        #subprocess.call(['nucmer', '--maxmatch', '-p', nucmerfilename, '-l', '30', '-banded', reference, filename],
        #    stdout=open(logfilename_out, 'a'), stderr=logfile_err, env=myenv)
        subprocess.call(['nucmer', '--maxmatch', '-p', nucmerfilename, reference, filename],
             stdout=open(logfilename_out, 'a'), stderr=logfile_err, env=myenv)

        # Filtering by IDY% = 95 (as GAGE did)
        subprocess.call(['delta-filter', '-i', '95', delta_filename],
            stdout=open(filtered_delta_filename, 'w'), stderr=logfile_err, env=myenv)
        shutil.move(filtered_delta_filename, delta_filename)

        subprocess.call(['show-coords', '-B', delta_filename],
            stdout=open(coords_btab_filename, 'w'), stderr=logfile_err, env=myenv)
        subprocess.call(['dnadiff', '-d', delta_filename, '-p', nucmerfilename],
            stdout=open(logfilename_out, 'a'), stderr=logfile_err, env=myenv)

        sympalign(coords_filename, coords_btab_filename)

        if not os.path.isfile(coords_filename) or\
           not os.path.isfile(nucmer_report_filename):
            print >> logfile_err, 'Nucmer failed.'
            return 'FAILED'
        if len(open(coords_filename).readlines()[-1].split()) < 13:
            print >> logfile_err, 'Nucmer ended early.'
            return 'FAILED'
        nucmer_successful_check_file = open(nucmer_successful_check_filename, 'w')
        nucmer_successful_check_file.write("Successfully finished " + datetime.datetime.now().strftime('%Y_%m_%d_%H_%M_%S'))
        nucmer_successful_check_file.close()

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
        mapping = Mapping.from_line(line)
        sum_idy += mapping.idy
        num_idy += 1
        aligns.setdefault(mapping.contig, []).append(mapping)
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

    SNPs = 0
    unaligned = 0
    partially_unaligned = 0
    fully_unaligned_bases = 0
    partially_unaligned_bases = 0
    ambiguous = 0
    total_ambiguous = 0
    uncovered_regions = 0
    uncovered_region_bases = 0
    partially_unaligned_with_misassembly = 0
    partially_unaligned_with_significant_parts = 0

    region_misassemblies = []
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
            sorted_aligns = sorted(aligns[contig], key=lambda x: (x.len2 * x.idy, x.len2), reverse=True)
            top_len = sorted_aligns[0].len2
            top_id = sorted_aligns[0].idy
            top_aligns = []
            print >> plantafile, 'Top Length: %s  Top ID: %s' % (top_len, top_id)

            #Check that top hit captures most of the contig (>99% or within 10 bases)
            if top_len > ctg_len * peral or ctg_len - top_len < maxun:
                #Reset top aligns: aligns that share the same value of longest and higest identity
                top_aligns.append(sorted_aligns[0])
                sorted_aligns = sorted_aligns[1:]

                #Continue grabbing alignments while length and identity are identical
                while sorted_aligns and top_len == sorted_aligns[0].len2 and top_id == sorted_aligns[0].idy:
                    top_aligns.append(sorted_aligns[0])
                    sorted_aligns = sorted_aligns[1:]

                #Mark other alignments as ambiguous
                while sorted_aligns:
                    ambig = sorted_aligns.pop()
                    print >> plantafile, '\t\tMarking as ambiguous: %s' % str(ambig)
                    # Kolya: removed redundant code about $ref

                print >> coords_filtered_file, str(top_aligns[0])

                if len(top_aligns) == 1:
                    #There is only one top align, life is good
                    print >> plantafile, '\t\tOne align captures most of this contig: %s' % str(top_aligns[0])
                else:
                    #There is more than one top align
                    print >> plantafile, '\t\tThis contig has %d significant alignments. [ambiguous]' % len(
                        top_aligns)
                    #Record these alignments as ambiguous on the reference
                    for align in top_aligns:
                        print >> plantafile, '\t\t\tAmbiguous Alignment: %s' % str(align)
                        # Kolya: removed redundant code about $ref
                    #Increment count of ambiguous contigs and bases
                    ambiguous += 1
                    total_ambiguous += ctg_len
            else:
                #Sort all aligns by position on contig, then length
                sorted_aligns = sorted(sorted_aligns, key=lambda x: (x.len2, x.idy), reverse=True)
                sorted_aligns = sorted(sorted_aligns, key=lambda x: min(x.s2, x.e2))

                #Push first alignment on to real aligns
                real_aligns = [sorted_aligns[0]]
                last_end = max(sorted_aligns[0].s2, sorted_aligns[0].e2)

                #Walk through alignments, if not fully contained within previous, record as real
                for i in xrange(1, num_aligns):
                    #If this alignment extends past last alignment's endpoint, add to real, else skip
                    if sorted_aligns[i].s2 > last_end or sorted_aligns[i].e2 > last_end:
                        real_aligns = [sorted_aligns[i]] + real_aligns
                        last_end = max(sorted_aligns[i].s2, sorted_aligns[i].e2)
                    else:
                        print >> plantafile, '\t\tSkipping [%d][%d] redundant alignment %d %s' % (
                        sorted_aligns[i].s1, sorted_aligns[i].e1, i, str(sorted_aligns[i]))
                        # Kolya: removed redundant code about $ref

                if len(real_aligns) == 1:
                    #There is only one alignment of this contig to the reference
                    #MY: output in coords.filtered
                    print >> coords_filtered_file, str(real_aligns[0])

                    #Is the contig aligned in the reverse compliment?
                    #Record beginning and end of alignment in contig
                    if sorted_aligns[0].s2 > sorted_aligns[0].e2:
                        end, begin = sorted_aligns[0].s2, sorted_aligns[0].e2
                    else:
                        end, begin = sorted_aligns[0].e2, sorted_aligns[0].s2
                    if (begin - 1) or (ctg_len - end):
                        #Increment tally of partially unaligned contigs
                        partially_unaligned += 1
                        #Increment tally of partially unaligned bases
                        unaligned_bases = (begin - 1) + (ctg_len - end)
                        partially_unaligned_bases += unaligned_bases
                        print >> plantafile, '\t\tThis contig is partially unaligned. (%d out of %d)' % (
                        top_len, ctg_len)
                        print >> plantafile, '\t\tAlignment: %s' % str(sorted_aligns[0])
                        print >> plantafile, '\t\tUnaligned bases: 1 to %d (%d)' % (begin, begin)
                        print >> plantafile, '\t\tUnaligned bases: %d to %d (%d)' % (end, ctg_len, ctg_len - end + 1)
                        # check if both parts (aligned and unaligned) have significant length
                        if (unaligned_bases >= qconfig.min_contig) and (ctg_len - unaligned_bases >= qconfig.min_contig):
                            partially_unaligned_with_significant_parts += 1
                            print >> plantafile, '\t\tThis contig has both significant aligned and unaligned parts ' \
                                                 '(of length >= min-contig)!'
                else:
                    #There is more than one alignment of this contig to the reference
                    print >> plantafile, '\t\tThis contig is misassembled. %d total aligns.' % num_aligns
                    #Reset real alignments and sum of real alignments
                    #Sort real alignments by position on the reference
                    sorted_aligns = sorted(real_aligns, key=lambda x: (x.ref, x.s1))

                    # Counting misassembled contigs which are mostly partially unaligned
                    all_aligns_len = sum(x.len2 for x in sorted_aligns)
                    if all_aligns_len < umt * ctg_len:
                        print >> plantafile, '\t\t\tWarning! This contig is more unaligned than misassembled. ' + \
                            'Contig length is %d and total length of all aligns is %d' % (ctg_len, all_aligns_len)
                        partially_unaligned_with_misassembly += 1
                        for align in sorted_aligns:
                            print >> plantafile, '\t\tAlignment: %s' % str(align)
                            print >> coords_filtered_file, str(align)

                        #Increment tally of partially unaligned contigs
                        partially_unaligned += 1
                        #Increment tally of partially unaligned bases
                        partially_unaligned_bases += ctg_len - all_aligns_len
                        print >> plantafile, '\t\tUnaligned bases: %d' % (ctg_len - all_aligns_len)
                        # check if both parts (aligned and unaligned) have significant length
                        if (all_aligns_len >= qconfig.min_contig) and (ctg_len - all_aligns_len >= qconfig.min_contig):
                            partially_unaligned_with_significant_parts += 1
                            print >> plantafile, '\t\tThis contig has both significant aligned and unaligned parts '\
                                                 '(of length >= min-contig)!'
                        continue

                    sorted_num = len(sorted_aligns) - 1
                    chimeric_found = False

                    #MY: computing cyclic references
                    if cyclic:
                        if sorted_aligns[0].s1 - 1 + total_reg_len - sorted_aligns[sorted_num].e1 <= ns + smgap:  # fake misassembly aka chimeric one
                            chimeric_found = True

                            # find fake alignment between "first" blocks and "last" blocks
                            chimeric_index = 0
                            for i in xrange(sorted_num):
                                gap = sorted_aligns[i + 1].s1 - sorted_aligns[i].e1
                                if gap > ns + smgap:
                                    chimeric_index = i + 1
                                    break

                            #MY: for merging local misassemblies
                            prev = sorted_aligns[chimeric_index].clone()

                            # process "last half" of blocks
                            prev, x = process_misassembled_contig(plantafile, coords_filtered_file,
                                chimeric_index, sorted_num, contig, prev, sorted_aligns, True, ns, smgap,
                                assembly, misassembled_contigs, extensive_misassembled_contigs)
                            region_misassemblies += x
                            print >> plantafile, '\t\t\tFake misassembly (caused by circular genome) between these two alignments: [%s] @ %d and %d' % (
                            sorted_aligns[sorted_num].ref, sorted_aligns[sorted_num].e1, sorted_aligns[0].s1)

                            prev.e1 = sorted_aligns[0].e1 # [E1]
                            prev.s2 = 0 # [S2]
                            prev.e2 = 0 # [E2]
                            prev.len1 += sorted_aligns[0].e1 - sorted_aligns[0].s1 + 1 # [LEN1]
                            prev.len2 += sorted_aligns[0].len2 # [LEN2]

                            # process "first half" of blocks
                            prev, x = process_misassembled_contig(plantafile, coords_filtered_file, 0,
                                chimeric_index - 1, contig, prev, sorted_aligns, False, ns, smgap, assembly,
                                misassembled_contigs, extensive_misassembled_contigs)
                            region_misassemblies += x

                    if not chimeric_found:                        
                        #MY: for merging local misassemblies
                        prev = sorted_aligns[0].clone()
                        prev, x = process_misassembled_contig(plantafile, coords_filtered_file, 0, sorted_num,
                            contig, prev, sorted_aligns, False, ns, smgap, assembly, misassembled_contigs,
                            extensive_misassembled_contigs)
                        region_misassemblies += x

        else:
            #No aligns to this contig
            print >> plantafile, '\t\tThis contig is unaligned. (%d bp)' % ctg_len
            print >> unaligned_file, contig

            #Increment unaligned contig count and bases
            unaligned += 1
            fully_unaligned_bases += ctg_len
            print >> plantafile, '\t\tUnaligned bases: %d  total: %d' % (ctg_len, fully_unaligned_bases)

    coords_filtered_file.close()
    unaligned_file.close()

    # TODO: 'Analyzing coverage...'

    # calulating SNPs and Subs. error (per 100 kbp)
    for line in open(nucmer_report_filename):
        #                           [REF]                [QRY]
        # AlignedBases         4501335(97.02%)      4513272(90.71%)    
        if line.startswith('AlignedBases'):
            total_aligned_bases = int(line.split()[2].split('(')[0])
        # TotalSNPs                  516                  516
        if line.startswith('TotalSNPs'):
            SNPs = int(line.split()[2])
        # TotalIndels                 9                    9
        if line.startswith('TotalIndels'):
            indels = int(line.split()[2])
            break

    print >> plantafile, '\tLocal Misassemblies: %d' % region_misassemblies.count(Misassembly.LOCAL)
    print >> plantafile, '\tMisassemblies: %d' % (len(region_misassemblies) - region_misassemblies.count(Misassembly.LOCAL))
    print >> plantafile, '\t\tRelocations: %d' % region_misassemblies.count(Misassembly.RELOCATION)
    print >> plantafile, '\t\tTranslocations: %d' % region_misassemblies.count(Misassembly.TRANSLOCATION)
    print >> plantafile, '\t\tInversions: %d' % region_misassemblies.count(Misassembly.INVERSION)
    print >> plantafile, '\tMisassembled Contigs: %d' % len(misassembled_contigs)
    misassembled_bases = sum(misassembled_contigs.itervalues())
    print >> plantafile, '\tMisassembled Contig Bases: %d' % misassembled_bases
    print >> plantafile, 'Uncovered Regions: %d (%d)' % (uncovered_regions, uncovered_region_bases)
    print >> plantafile, 'Unaligned Contigs: %d + %d part' % (unaligned, partially_unaligned)
    print >> plantafile, 'Partially Unaligned Contigs with Misassemblies: %d' % partially_unaligned_with_misassembly
    print >> plantafile, 'Unaligned Contig Bases: %d' % (fully_unaligned_bases + partially_unaligned_bases)
    print >> plantafile, 'Ambiguous Contigs: %d' % ambiguous
    print >> plantafile, 'Ambiguous Contig Bases: %d' % total_ambiguous
    print >> plantafile, 'Mismatches: %d' % SNPs
    print >> plantafile, 'Single Nucleotide Indels: %d' % indels

    report = reporting.get(filename)
    report.add_field(reporting.Fields.AVGIDY, '%.3f' % avg_idy)
    report.add_field(reporting.Fields.MISLOCAL, region_misassemblies.count(Misassembly.LOCAL))
    report.add_field(reporting.Fields.MISASSEMBL, len(region_misassemblies) - region_misassemblies.count(Misassembly.LOCAL))
    report.add_field(reporting.Fields.MISCONTIGS, len(misassembled_contigs))
    report.add_field(reporting.Fields.MISCONTIGSBASES, misassembled_bases)
    report.add_field(reporting.Fields.UNALIGNEDBASES, (fully_unaligned_bases + partially_unaligned_bases))
    report.add_field(reporting.Fields.AMBIGUOUS, ambiguous)
    report.add_field(reporting.Fields.AMBIGUOUSBASES, total_ambiguous)
    report.add_field(reporting.Fields.MISMATCHES, SNPs)
    report.add_field(reporting.Fields.INDELS, indels)
    report.add_field(reporting.Fields.SUBSERROR, "%.2f" % (float(SNPs) * 100000.0 / float(total_aligned_bases)))
    report.add_field(reporting.Fields.INDELSERROR, "%.2f" % (float(indels) * 100000.0 / float(total_aligned_bases)))

    # for misassemblies report:
    report.add_field(reporting.Fields.MIS_ALL_EXTENSIVE, len(region_misassemblies) - region_misassemblies.count(Misassembly.LOCAL))
    report.add_field(reporting.Fields.MIS_RELOCATION, region_misassemblies.count(Misassembly.RELOCATION))
    report.add_field(reporting.Fields.MIS_TRANSLOCATION, region_misassemblies.count(Misassembly.TRANSLOCATION))
    report.add_field(reporting.Fields.MIS_INVERTION, region_misassemblies.count(Misassembly.INVERSION))
    report.add_field(reporting.Fields.MIS_EXTENSIVE_CONTIGS, len(misassembled_contigs))
    report.add_field(reporting.Fields.MIS_EXTENSIVE_BASES, misassembled_bases)
    report.add_field(reporting.Fields.MIS_LOCAL, region_misassemblies.count(Misassembly.LOCAL))

    # for unaligned report:
    report.add_field(reporting.Fields.UNALIGNED_FULL_CNTGS, unaligned)
    report.add_field(reporting.Fields.UNALIGNED_FULL_LENGTH, fully_unaligned_bases)
    report.add_field(reporting.Fields.UNALIGNED_PART_CNTGS, partially_unaligned)
    report.add_field(reporting.Fields.UNALIGNED_PART_WITH_MISASSEMBLY, partially_unaligned_with_misassembly)
    report.add_field(reporting.Fields.UNALIGNED_PART_SIGNIFICANT_PARTS, partially_unaligned_with_significant_parts)
    report.add_field(reporting.Fields.UNALIGNED_PART_LENGTH, partially_unaligned_bases)

    ## outputting misassembled contigs to separate file
    fasta = [(name, seq) for name, seq in fastaparser.read_fasta(filename) if
                         name in extensive_misassembled_contigs]
    fastaparser.write_fasta_to_file(output_dir + '/' + os.path.basename(filename) + '.mis_contigs', fasta)

    plantafile.close()
    logfile_err.close()
    print '  Done plantakoling.'
    return 'OK'

###  I think we don't need this
#    if draw_plots and os.path.isfile(delta_filename):
#        # draw reference coverage plot
#        print '    Drawing reference coverage plot...',
#        plotfilename = output_dir + '/mummerplot_' + os.path.basename(filename)
#        plot_logfilename_out = output_dir + '/mummerplot_' + os.path.basename(filename) + '.stdout'
#        plot_logfilename_err = output_dir + '/mummerplot_' + os.path.basename(filename) + '.stderr'
#        plot_logfile_out = open(plot_logfilename_out, 'w')
#        plot_logfile_err = open(plot_logfilename_err, 'w')
#        subprocess.call(
#            ['mummerplot', '--coverage', '--postscript', '--prefix', plotfilename, delta_filename],
#            stdout=plot_logfile_out, stderr=plot_logfile_err, env=myenv)
#        plot_logfile_out.close()
#        plot_logfile_err.close()
#        print 'saved to', plotfilename + '.ps'
#        for ext in ['.gp', '.rplot', '.fplot']: # remove redundant files
#            if os.path.isfile(plotfilename + ext):
#                os.remove(plotfilename + ext)


def plantakolya_process(cyclic, draw_plots, filename, id, myenv, output_dir, reference):
    print ' ', id_to_str(id), os.path.basename(filename), '...'
    nucmer_output_dir = os.path.join(output_dir, 'nucmer_output')
    if not os.path.isdir(nucmer_output_dir):
        os.mkdir(nucmer_output_dir)
    nucmerfilename = os.path.join(nucmer_output_dir, os.path.basename(filename))
    nucmer_status = plantakolya(cyclic, draw_plots, filename, nucmerfilename, myenv, output_dir, reference)
    clear_files(filename, nucmerfilename)
    return nucmer_status


def do(reference, filenames, cyclic, output_dir, lib_dir, draw_plots):
    if not os.path.isdir(output_dir):
        os.mkdir(output_dir)

    ########################################################################
    if platform.system() == 'Darwin':
        mummer_path = os.path.join(lib_dir, 'MUMmer3.23-osx')
    else:
        mummer_path  = os.path.join(lib_dir, 'MUMmer3.23-linux')

    ########################################################################
#    report_dict = {'header' : []}
#    for filename in filenames:
#        report_dict[os.path.basename(filename)] = []

    # for running our MUMmer
    myenv = os.environ.copy()
    myenv['PATH'] = mummer_path + ':' + myenv['PATH']
    # making
    print ("Making MUMmer... (it may take several minutes on the first run)")
    subprocess.call(
        ['make', '-C', mummer_path],
        stdout=open(os.path.join(mummer_path, 'make.log'), 'w'), stderr=open(os.path.join(mummer_path, 'make.err'), 'w'))

    print 'Running contigs analyzer...'

    nucmer_statuses = {}
    for id, filename in enumerate(filenames):
        #TODO: use joblib
        nucmer_status = plantakolya_process(cyclic, draw_plots, filename, id, myenv, output_dir, reference)
        nucmer_statuses[filename] = nucmer_status

    if 'OK' in nucmer_statuses.values():
        reporting.save_misassemblies(output_dir)
        reporting.save_unaligned(output_dir)
        if 'FAILED' in nucmer_statuses.values():
            print '  Done for', str(nucmer_statuses.values().count('OK')), 'of', str(len(nucmer_statuses)) + \
                     '. Nucmer failed on the other contigs files. They will be skipped in the report.'
        else:
            print '  Done.'
    else:
        print '  Nucmer failed.'
    return nucmer_statuses #, report_dict
