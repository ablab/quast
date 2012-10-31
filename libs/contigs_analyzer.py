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
import sys
import fastaparser
import shutil
from libs import reporting, qconfig
from qutils import id_to_str

required_binaries = ['nucmer', 'delta-filter', 'show-coords', 'dnadiff']

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


def sympalign(id, out_filename, in_filename):
    print '  ' + id_to_str(id) + 'Running SympAlign...'
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
            print '    Error: lc != int(arr[12]) ' + str(lc) + ' ' + str(int(arr[12]))
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

    print '  ' + id_to_str(id) + '  Cleaned ' + str(counter[0]) + ' down to ' + str(counter[1])
    all, add_counter = additional_cleaning(all)
    print '  ' + id_to_str(id) + '  Additionally cleaned ' + str(counter[1]) + ' down to ' + str(add_counter)

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
    print '  ' + id_to_str(id) + '  Sympaligning is finished.'


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


def process_misassembled_contig(plantafile, output_file, i_start, i_finish, contig, prev, sorted_aligns, is_1st_chimeric_half, ns, smgap, assembly, misassembled_contigs, extensive_misassembled_contigs, ref_aligns, ref_features):
    region_misassemblies = []
    for i in xrange(i_start, i_finish):
        print >> plantafile, '\t\t\tReal Alignment %d: %s' % (i+1, str(sorted_aligns[i]))
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

        ref_aligns.setdefault(sorted_aligns[i].ref, []).append(sorted_aligns[i])

        if sorted_aligns[i].ref != sorted_aligns[i+1].ref or gap > ns + smgap or (strand1 != strand2): # different chromosomes or large gap or different strands
            #Contig spans chromosomes or there is a gap larger than 1kb
            #MY: output in coords.filtered
            print >> output_file, str(prev)
            prev = sorted_aligns[i+1].clone()
            print >> plantafile, '\t\t\tExtensive misassembly (',

            extensive_misassembled_contigs.add(sorted_aligns[i].contig)
            ref_features.setdefault(sorted_aligns[i].ref, {})[sorted_aligns[i].e1] = 'M'
            ref_features.setdefault(sorted_aligns[i+1].ref, {})[sorted_aligns[i+1].e1] = 'M'

            if sorted_aligns[i].ref != sorted_aligns[i+1].ref:
                region_misassemblies += [Misassembly.TRANSLOCATION]
                print >> plantafile, 'translocation',
            elif gap > ns + smgap:
                region_misassemblies += [Misassembly.RELOCATION]
                print >> plantafile, 'relocation',
            elif strand1 != strand2:
                region_misassemblies += [Misassembly.INVERSION]
                print >> plantafile, 'inversion',
            misassembled_contigs[contig] = len(assembly[contig])

            print >> plantafile, ') between these two alignments: [%s] @ %d and %d' % (sorted_aligns[i].ref, sorted_aligns[i].e1, sorted_aligns[i+1].s1)

        else:
            if gap < 0:
                #There is overlap between the two alignments, a local misassembly
                print >> plantafile, '\t\tOverlap between these two alignments (local misassembly): [%s] %d to %d' % (sorted_aligns[i].ref, sorted_aligns[i].e1, sorted_aligns[i+1].s1)
            else:
                #There is a small gap between the two alignments, a local misassembly
                print >> plantafile, '\t\tGap in alignment between these two alignments (local misassembly): [%s] %d' % (sorted_aligns[i].ref, sorted_aligns[i].s1)

            region_misassemblies += [Misassembly.LOCAL]

            #MY:
            prev.e1 = sorted_aligns[i+1].e1 # [E1]
            prev.s2 = 0 # [S2]
            prev.e2 = 0 # [E2]
            prev.len1 = prev.e1 - prev.s1 # [LEN1]
            prev.len2 = prev.len2 + sorted_aligns[i+1].len2 - (overlap_in_contig if overlap_in_contig > 0 else 0) # [LEN2]

    #MY: output in coords.filtered
    if not is_1st_chimeric_half:
        print >> output_file, str(prev)

    #Record the very last alignment
    i = i_finish
    print >> plantafile, '\t\t\tReal Alignment %d: %s' % (i+1, str(sorted_aligns[i]))
    ref_aligns.setdefault(sorted_aligns[i].ref, []).append(sorted_aligns[i])

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

class NucmerStatus:
    FAILED=0
    OK=1
    NOT_ALIGNED=2

def plantakolya(cyclic, draw_plots, id, filename, nucmerfilename, myenv, output_dir, reference):
    # run plantakolya tool
    logfilename_out = output_dir + '/contigs_report_' + os.path.basename(filename) + '.stdout'
    logfilename_err = output_dir + '/contigs_report_' + os.path.basename(filename) + '.stderr'
    logfile_err = open(logfilename_err, 'a')
    print '  ' + id_to_str(id) + 'Logging to files ' + logfilename_out + ' and ' + os.path.basename(logfilename_err) + '...'
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
    snps_filename = nucmerfilename + '.snps'
    nucmer_report_filename = nucmerfilename + '.report'
    plantafile = open(logfilename_out, 'a')

    print >> plantafile, 'Aligning contigs to reference...'

    # Checking if there are existing previous nucmer alignments.
    # If they exist, using them to save time.
    using_existing_alignments = False
    if (os.path.isfile(nucmer_successful_check_filename) and os.path.isfile(coords_filename)
        and os.path.isfile(nucmer_report_filename)):

        if open(nucmer_successful_check_filename).read().split('\n')[1].strip() == str(qconfig.min_contig):
            print >> plantafile, '\tUsing existing Nucmer alignments...'
            print '  ' + id_to_str(id) + 'Using existing Nucmer alignments... '
            using_existing_alignments = True

    if not using_existing_alignments:
        print >> plantafile, '\tRunning Nucmer...'
        print '  ' + id_to_str(id) + 'Running Nucmer... '
        # GAGE params of Nucmer
        #subprocess.call(['nucmer', '--maxmatch', '-p', nucmerfilename, '-l', '30', '-banded', reference, filename],
        #    stdout=open(logfilename_out, 'a'), stderr=logfile_err, env=myenv)
        subprocess.call(['nucmer', '--maxmatch', '-p', nucmerfilename, reference, filename],
             stdout=open(logfilename_out, 'a'), stderr=logfile_err, env=myenv)

        # Filtering by IDY% = 95 (as GAGE did)
        subprocess.call(['delta-filter', '-i', '95', delta_filename],
            stdout=open(filtered_delta_filename, 'w'), stderr=logfile_err, env=myenv)
        shutil.move(filtered_delta_filename, delta_filename)

        # disabling sympalign: part1
        #subprocess.call(['show-coords', '-B', delta_filename],
        #    stdout=open(coords_btab_filename, 'w'), stderr=logfile_err, env=myenv)
        tmp_coords_filename = coords_filename + '_tmp'
        subprocess.call(['show-coords', delta_filename],
            stdout=open(tmp_coords_filename, 'w'), stderr=logfile_err, env=myenv)
        subprocess.call(['dnadiff', '-d', delta_filename, '-p', nucmerfilename],
            stdout=open(logfilename_out, 'a'), stderr=logfile_err, env=myenv)

        # removing waste lines from coords file
        coords_file = open(coords_filename, 'w')
        header = []
        tmp_coords_file = open(tmp_coords_filename)
        for line in tmp_coords_file:
            header.append(line)
            if line.startswith('====='):
                break
        coords_file.write(header[-2])
        coords_file.write(header[-1])
        for line in tmp_coords_file:
            coords_file.write(line)
        coords_file.close()
        tmp_coords_file.close()

        # disabling sympalign: part2
        #sympalign(id, coords_filename, coords_btab_filename)

        if not os.path.isfile(coords_filename):
            print >> logfile_err, id_to_str(id) + 'Nucmer failed for', filename + ':', coords_filename, 'doesn\'t exist.'
            print '  ' + id_to_str(id) + 'Nucmer failed for ' + '\'' + os.path.basename(filename) + '\'.'
            return NucmerStatus.FAILED, {}
        if not os.path.isfile(nucmer_report_filename):
            print >> logfile_err, id_to_str(id) + 'Nucmer failed for', filename + ':', nucmer_report_filename, 'doesn\'t exist.'
            print '  ' + id_to_str(id) + 'Nucmer failed for ' + '\'' + os.path.basename(filename) + '\'.'
            return NucmerStatus.FAILED, {}
        if len(open(coords_filename).readlines()[-1].split()) < 13:
            print >> logfile_err, id_to_str(id) + 'Nucmer: nothing aligned for', filename
            print '  ' + id_to_str(id) + 'Nucmer: nothing aligned for ' + '\'' + os.path.basename(filename) + '\'.'
            return NucmerStatus.NOT_ALIGNED, {}
        nucmer_successful_check_file = open(nucmer_successful_check_filename, 'w')
        nucmer_successful_check_file.write("Min contig size:\n")
        nucmer_successful_check_file.write(str(qconfig.min_contig) + '\n')
        nucmer_successful_check_file.write("Successfully finished on " + datetime.datetime.now().strftime('%Y/%m/%d %H:%M:%S') + '\n')
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
        assembly[name] = seq
        if 'N' in seq:
            assembly_ns[name] = [pos for pos in xrange(len(seq)) if seq[pos] == 'N']

    # Loading the reference sequences
    print >> plantafile, 'Loading reference...' # TODO: move up
    references = {}
    ref_aligns = {}
    ref_features = {}
    for name, seq in fastaparser.read_fasta(reference):
        name = name.split()[0] # no spaces in reference header
        references[name] = seq
        print >> plantafile, '\tLoaded [%s]' % name

    #Loading the SNP calls
    print >> plantafile, 'Loading SNPs...'
    snps = {}
    snp_locs = {}
    for line in open(snps_filename):
        #print "$line";
        line = line.split()
        if not line[0].isdigit():
            continue
        ref = line[10]
        ctg = line[11]

        # if (! exists $line[11]) { die "Malformed line in SNP file.  Please check that show-snps has completed succesfully.\n$line\n[$line[9]][$line[10]][$line[11]]\n"; }

        snps.setdefault(ref, {}).setdefault(ctg, {})[line[0]] = 'I' if line[1] == '.' else ('D' if line[2] == '.' else 'S')
        snp_locs.setdefault(ref, {}).setdefault(ctg, {})[line[0]] = line[3]

    # Loading the regions (if any)
    regions = {}
    total_reg_len = 0
    total_regions = 0
    print >> plantafile, 'Loading regions...'
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
    total_redundant = 0
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
            #if top_len > ctg_len * peral or ctg_len - top_len < maxun:
            if ctg_len - top_len <= qconfig.min_contig:
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
                    # Kolya: removed redundant code about $ref (for gff AFAIU)

                print >> coords_filtered_file, str(top_aligns[0])

                if len(top_aligns) == 1:
                    #There is only one top align, life is good
                    print >> plantafile, '\t\tOne align captures most of this contig: %s' % str(top_aligns[0])
                    ref_aligns.setdefault(top_aligns[0].ref, []).append(top_aligns[0])
                else:
                    #There is more than one top align
                    print >> plantafile, '\t\tThis contig has %d significant alignments. [ambiguous]' % len(
                        top_aligns)
                    #Record these alignments as ambiguous on the reference
                    for align in top_aligns:
                        print >> plantafile, '\t\t\tAmbiguous Alignment: %s' % str(align)
                        ref = align.ref
                        for i in xrange(align.s1, align.e1+1):
                            if (ref not in ref_features) or (i not in ref_features[ref]):
                                ref_features.setdefault(ref, {})[i] = 'A'

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
                        # Kolya: removed redundant code about $ref (for gff AFAIU)

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
                    ref_aligns.setdefault(sorted_aligns[0].ref, []).append(sorted_aligns[0])
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
                                assembly, misassembled_contigs, extensive_misassembled_contigs, ref_aligns, ref_features)
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
                                misassembled_contigs, extensive_misassembled_contigs, ref_aligns, ref_features)
                            region_misassemblies += x

                    if not chimeric_found:                        
                        #MY: for merging local misassemblies
                        prev = sorted_aligns[0].clone()
                        prev, x = process_misassembled_contig(plantafile, coords_filtered_file, 0, sorted_num,
                            contig, prev, sorted_aligns, False, ns, smgap, assembly, misassembled_contigs,
                            extensive_misassembled_contigs, ref_aligns, ref_features)
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

    print >> plantafile, 'Analyzing coverage...'

    region_covered = 0
    region_ambig = 0
    region_snp = 0
    region_insertion = 0
    region_deletion = 0
    gaps = []
    neg_gaps = []
    redundant = []
    snip_left = 0
    snip_right = 0

    #Go through each header in reference file
    for ref, value in regions.iteritems():
        #Check to make sure this reference ID contains aligns.
        if ref not in ref_aligns:
            print >> plantafile, 'ERROR: Reference [$ref] does not have any alignments!  Check that this is the same file used for alignment.'
            print >> plantafile, 'ERROR: Alignment Reference Headers: %s' % ref_aligns.keys()
            continue

        #Sort all alignments in this reference by start location
        sorted_aligns = sorted(ref_aligns[ref], key=lambda x: x.s1)
        total_aligns = len(sorted_aligns)
        print >> plantafile, '\tReference %s: %d total alignments. %d total regions.' % (ref, total_aligns, len(regions[ref]))

        #Walk through each region on this reference sequence
        for region in regions[ref]:
            end = 0
            reg_length = region[1] - region[0]
            print >> plantafile, '\t\tRegion: %d to %d (%d bp)\n' % (region[0], region[1], reg_length)

            #Skipping alignments not in the next region
            while sorted_aligns and sorted_aligns[0].e1 < region[0]:
                skipped = sorted_aligns[0]
                sorted_aligns = sorted_aligns[1:] # Kolya: slooow, but should never happens without gff :)
                print >> plantafile, '\t\t\tThis align occurs before our region of interest, skipping: %s' % skipped

            if not sorted_aligns:
                print >> plantafile, '\t\t\tThere are no more aligns.  Skipping this region.'
                continue

            #If region starts in a contig, ignore portion of contig prior to region start
            if sorted_aligns and region and sorted_aligns[0].s1 < region[0]:
                print >> plantafile, '\t\t\tSTART within alignment : %s' % sorted_aligns[0]
                #Track number of bases ignored at the start of the alignment
                snip_left = region[0] - sorted_aligns[0].s1
                #Modify to account for any insertions or deletions that are present
                for z in xrange(sorted_aligns[0].s1, region[0] + 1):
                    if (ref in snps) and (sorted_aligns[0].contig in snps[ref]) and (z in snps[ref][sorted_aligns[0].contig]) and \
                       (ref in ref_features) and (z in ref_features[ref]) and (ref_features[ref][z] != 'A'): # Kolya: never happened before because of bug: z -> i
                        if snps[ref][sorted_aligns[0].contig][z] == 'I':
                            snip_left += 1
                        if snps[ref][sorted_aligns[0].contig][z] == 'D':
                            snip_left -= 1

                #Modify alignment to start at region
                print >> plantafile, '\t\t\t\tMoving reference start from %d to %d' % (sorted_aligns[0].s1, region[0])
                sorted_aligns[0].s1 = region[0]

                #Modify start position in contig
                if sorted_aligns[0].s2 < sorted_aligns[0].e2:
                    print >> plantafile, '\t\t\t\tMoving contig start from %d to %d.' % (sorted_aligns[0].s2, sorted_aligns[0].s2 + snip_left)
                    sorted_aligns[0].s2 += snip_left
                else:
                    print >> plantafile, '\t\t\t\tMoving contig start from %d to %d.' % (sorted_aligns[0].s2, sorted_aligns[0].s2 - snip_left)
                    sorted_aligns[0].s2 -= snip_left

            #No aligns in this region
            if sorted_aligns[0].s1 > region[1]:
                print >> plantafile, '\t\t\tThere are no aligns within this region.'
                gaps.append([reg_length, 'START', 'END'])
                #Increment uncovered region count and bases
                uncovered_regions += 1
                uncovered_region_bases += reg_length
                continue

            #Record first gap, and first ambiguous bases within it
            if sorted_aligns[0].s1 > region[0]:
                size = sorted_aligns[0].s1 - region[0]
                print >> plantafile, '\t\t\tSTART in gap: %d to %d (%d bp)' % (region[0], sorted_aligns[0].s1, size)
                gaps.append([size, 'START', sorted_aligns[0].contig])
                #Increment any ambiguously covered bases in this first gap
                for i in xrange(region[0], sorted_aligns[0].e1):
                    if (ref in ref_features) and (i in ref_features[ref]) and (ref_features[ref][i] == 'A'):
                        region_ambig += 1

            #For counting number of alignments
            counter = 0
            negative = False
            current = None
            while sorted_aligns and sorted_aligns[0].s1 < region[1] and not end:
                #Increment alignment count
                counter += 1
                if counter % 1000 == 0:
                    print >> plantafile, '\t...%d of %d' % (counter, total_aligns)
                end = False
                #Check to see if previous gap was negative
                if negative:
                    print >> plantafile, '\t\t\tPrevious gap was negative, modifying coordinates to ignore overlap'
                    #Ignoring OL part of next contig, no SNPs or N's will be recorded
                    snip_left = current.e1 + 1 - sorted_aligns[0].s1
                    #Account for any indels that may be present
                    for z in xrange(sorted_aligns[0].s1, current.e1 + 2):
                        if (ref in snps) and (sorted_aligns[0].contig in snps[ref]) and (z in snps[ref][sorted_aligns[0].contig]):
                            if snps[ref][sorted_aligns[0].contig][z] == 'I':
                                snip_left += 1
                            if snps[ref][sorted_aligns[0].contig][z] == 'D':
                                snip_left -= 1
                    #Modifying position in contig of next alignment
                    sorted_aligns[0].s1 = current.e1 + 1
                    if sorted_aligns[0].s2 < sorted_aligns[0].e2:
                        print >> plantafile, '\t\t\t\tMoving contig start from %d to %d.' % (sorted_aligns[0].s2, sorted_aligns[0].s2 + snip_left)
                        sorted_aligns[0].s2 += snip_left
                    else:
                        print >> plantafile, '\t\t\t\tMoving contig start from %d to %d.' % (sorted_aligns[0].s2, sorted_aligns[0].s2 - snip_left)
                        sorted_aligns[0].s2 -= snip_left
                    negative = False

                #Pull top alignment
                current = sorted_aligns[0]
                sorted_aligns = sorted_aligns[1:]
                #print >>plantafile, '\t\t\tAlign %d: %s' % (counter, current)  #(self, s1, e1, s2, e2, len1, len2, idy, ref, contig):
                print >>plantafile, '\t\t\tAlign %d: %s' % (counter, '%d %d %s %d %d' % (current.s1, current.e1, current.contig, current.s2, current.e2))

                #Check if:
                # A) We have no more aligns to this reference
                # B) The current alignment extends to or past the end of the region
                # C) The next alignment starts after the end of the region

                if not sorted_aligns or current.e1 >= region[1] or sorted_aligns[0].s1 > region[1]:
                    #Check if last alignment ends before the regions does (gap at end of the region)
                    if current.e1 >= region[1]:
                        #print "Ends inside current alignment.\n";
                        print >> plantafile, '\t\t\tEND in current alignment.  Modifying %d to %d.' % (current.e1, region[1])
                        #Pushing the rest of the alignment back on the stack
                        sorted_aligns = [current] + sorted_aligns
                        #Flag to end loop through alignment
                        end = True
                        #Clip off right side of contig alignment
                        snip_right = current.e1 - region[1]
                        #End current alignment in region
                        current.e1 = region[1]
                    else:
                        #Region ends in a gap
                        size = region[1] - current.e1
                        print >> plantafile, '\t\t\tEND in gap: %d to %d (%d bp)' % (current.e1, region[1], size)

                        #Record gap
                        if not sorted_aligns:
                            #No more alignments, region ends in gap.
                            gaps.append([size, current.contig, 'END'])
                        else:
                            #Gap between end of current and beginning of next alignment.
                            gaps.append([size, current.contig, sorted_aligns[0].contig])
                        #Increment any ambiguous bases within this gap
                        for i in xrange(current.e1, region[1]):
                            if (ref in ref_features) and (i in ref_features[ref]) and (ref_features[ref][i] == 'A'):
                                region_ambig += 1
                else:
                    #Grab next alignment
                    next = sorted_aligns[0]
                    #print >>plantafile, '\t\t\t\tNext Alignment: %s' % next
                    print >> plantafile, '\t\t\t\tNext Alignment: %d %d %s %d %d' % (next.s1, next.e1, next.contig, next.s2, next.e2)

                    if next.s1 >= current.e1:
                        #There is a gap beetween this and the next alignment
                        size = next.s1 - current.e1 - 1
                        gaps.append([size, current.contig, next.contig])
                        print >> plantafile, '\t\t\t\tGap between this and next alignment: %d to %d (%d bp)' % (current.e1, next.s1, size)
                        #Record ambiguous bases in current gap
                        for i in xrange(current.e1, next.s1):
                            if (ref in ref_features) and (i in ref_features[ref]) and (ref_features[ref][i] == 'A'):
                                region_ambig += 1
                    elif next.e1 <= current.e1:
                        #The next alignment is redundant to the current alignmentt
                        while next.e1 <= current.e1 and sorted_aligns:
                            total_redundant += next.e1 - next.s1
                            print >> plantafile, '\t\t\t\tThe next contig is redundant. Skipping.'
                            redundant.append(current.contig)
                            next = sorted_aligns[0]
                            if next.e1 <= current.e1:
                                sorted_aligns = sorted_aligns[1:]
                            counter += 1
                    else:
                        #This alignment overlaps with the next alignment, negative gap
                        #If contig extends past the region, clip
                        if current.e1 > region[1]:
                            current.e1 = region[1]
                        #Record gap
                        size = next.s1 - current.e1
                        neg_gaps.append([size, current.contig, next.contig])
                        print >>plantafile, '\t\t\t\tNegative gap between this and next alignment: %dbp %s to %s'  % (size, current.contig, next.contig)

                        #Mark this alignment as negative so overlap region can be ignored
                        negative = True

                #Initiate location of SNP on assembly to be first or last base of contig alignment
                contig_estimate = current.s2
                print >> plantafile, '\t\t\t\tContig start coord: %d' % contig_estimate

                #Assess each reference base of the current alignment
                for i in xrange(current.s1, current.e1 + 1):
                    #Mark as covered
                    region_covered += 1

                    #If there is a misassembly, increment count and contig length
                    #if (exists $ref_features{$ref}[$i] && $ref_features{$ref}[$i] eq "M") {
                    #	$region_misassemblies++;
                    #	$misassembled_contigs{$current[2]} = length($assembly{$current[2]});
                    #}

                    #If there is a SNP, and no alternative alignments over this base, record SNPs
                    if (ref in snps) and (current.contig in snps[ref]) and (i in snps[ref][current.contig]):
                        print >> plantafile, '\t\t\t\tSNP: %s, %s, %d, %s, %d, %d' % (ref, current.contig, i, snps[ref][current.contig][i], contig_estimate, snp_locs[ref][current.contig][i])

                        #Capture SNP base
                        snp = snps[ref][current.contig][i]

                        #Check that there are not multiple alignments at this location
                        if (ref in ref_features) and (i in ref_features[ref]):
                            print >> plantafile, '\t\t\t\t\tERROR: SNP at a postion where there are multiple alignments (%s).  Skipping.\n' % ref_features[ref][i]
                            if current.s2 < current.e2: contig_estimate += 1
                            else: contig_estimate -= 1
                            continue
                        #Check that the position of the SNP in the contig is close to the position of this SNP
                        elif abs(contig_estimate - snp_locs[ref][current.contig][i]) > 50:
                            print >> plantafile, '\t\t\t\t\tERROR: SNP position in contig was off by %dbp! (%d vs %d)' % (abs(contig_estimate - snp_locs[ref][current.contig][i]), contig_estimate, snp_locs[ref][current.contig][i])
                            if current.s2 < current.e2: contig_estimate += 1
                            else: contig_estimate -= 1
                            continue

                        #If SNP is an insertion, record
                        if snp == 'I':
                            region_insertion += 1
                            if current.s2 < current.e2: contig_estimate += 1
                            else: contig_estimate -= 1
                        #If SNP is a deletion, record
                        if snp == 'D':
                            region_deletion += 1
                            if current.s2 < current.e2: contig_estimate += 1
                            else: contig_estimate -= 1
                        #If SNP is a mismatch, record
                        if snp == 'S':
                            region_snp += 1

                    if current.s2 < current.e2: contig_estimate += 1
                    else: contig_estimate -= 1

                #Record Ns in current alignment
                if current.s2 < current.e2:
                    #print "\t\t(forward)Recording Ns from $current[3]+$snip_left to $current[4]-$snip_right...\n";
                    for i in (current.s2 + snip_left, current.e2 - snip_right + 1):
                        if (current.contig in assembly_ns) and (i in assembly_ns[current.contig]):
                            region_ambig += 1
                else:
                    #print "\t\t(reverse)Recording Ns from $current[4]+$snip_right to $current[3]-$snip_left...\n";
                    for i in (current.e2 + snip_left, current.s2 - snip_right + 1):
                        if (current.contig in assembly_ns) and (i in assembly_ns[current.contig]):
                            region_ambig += 1
                snip_left = 0
                snip_right = 0

    # calulating SNPs and Subs. error (per 100 kbp)
    indels = 0
    total_aligned_bases = 0
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

    print >> plantafile, '\tSNPs: %d' % region_snp
    print >> plantafile, '\tInsertions: %d' % region_insertion
    print >> plantafile, '\tDeletions: %d' % region_deletion
    print >> plantafile, '\tPositive Gaps: %d' % len(gaps)
    internal = 0
    external = 0
    summ = 0
    for gap in gaps:
        if gap[1] == gap[2]:
            internal += 1
        else:
            external += 1
            summ += gap[0]
    print >> plantafile, '\t\tInternal Gaps: % d' % internal
    print >> plantafile, '\t\tExternal Gaps: % d' % external
    print >> plantafile, '\t\tExternal Gap Total: % d' % summ
    if external:
        avg = summ * 1.0 / external
    else:
        avg = 0.0
    print >> plantafile, '\t\tExternal Gap Average: %.0f' % avg

    print >> plantafile, '\tNegative Gaps: %d' % len(neg_gaps)
    internal = 0
    external = 0
    summ = 0
    for gap in neg_gaps:
        if gap[1] == gap[2]:
            internal += 1
        else:
            external += 1
            summ += gap[0]
    print >> plantafile, '\t\tInternal Overlaps: % d' % internal
    print >> plantafile, '\t\tExternal Overlaps: % d' % external
    print >> plantafile, '\t\tExternal Overlaps Total: % d' % summ
    if external:
        avg = summ * 1.0 / external
    else:
        avg = 0.0
    print >> plantafile, '\t\tExternal Overlaps Average: %.0f' % avg

    print >> plantafile, '\tRedundant Contigs: %d (%d)' % (len(redundant), total_redundant)

    result = {'avg_idy': avg_idy, 'region_misassemblies': region_misassemblies,
              'misassembled_contigs': misassembled_contigs, 'misassembled_bases': misassembled_bases,
              'unaligned': unaligned, 'partially_unaligned': partially_unaligned,
              'partially_unaligned_bases': partially_unaligned_bases, 'fully_unaligned_bases': fully_unaligned_bases,
              'ambiguous': ambiguous, 'total_ambiguous': total_ambiguous, 'SNPs': SNPs, 'indels': indels,
              'total_aligned_bases': total_aligned_bases,
              'partially_unaligned_with_misassembly': partially_unaligned_with_misassembly,
              'partially_unaligned_with_significant_parts': partially_unaligned_with_significant_parts}

    ## outputting misassembled contigs to separate file
    fasta = [(name, seq) for name, seq in fastaparser.read_fasta(filename) if
                         name in extensive_misassembled_contigs]
    fastaparser.write_fasta_to_file(output_dir + '/' + os.path.basename(filename) + '.mis_contigs', fasta)

    plantafile.close()
    logfile_err.close()
    print '  ' + id_to_str(id) + 'Analysis is finished.'
    return NucmerStatus.OK, result

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



def plantakolya_process(cyclic, draw_plots, nucmer_output_dir, filename, id, myenv, output_dir, reference):
    print '  ' + id_to_str(id) + os.path.basename(filename) + '...'
    nucmer_fname = os.path.join(nucmer_output_dir, os.path.basename(filename))
    nucmer_is_ok, result = plantakolya(cyclic, draw_plots, id, filename, nucmer_fname, myenv, output_dir, reference)
    clear_files(filename, nucmer_fname)

    return nucmer_is_ok, result


def all_required_binaries_exist(mummer_path):
    for required_binary in required_binaries:
        if not os.path.isfile(os.path.join(mummer_path, required_binary)):
            return False
    return True


def do(reference, filenames, cyclic, output_dir, lib_dir, draw_plots):
    if not os.path.isdir(output_dir):
        os.mkdir(output_dir)

    ########################################################################
    if platform.system() == 'Darwin':
        mummer_path = os.path.join(lib_dir, 'MUMmer3.23-osx')
    else:
        mummer_path = os.path.join(lib_dir, 'MUMmer3.23-linux')

    ########################################################################
#    report_dict = {'header' : []}
#    for filename in filenames:
#        report_dict[os.path.basename(filename)] = []

    # for running our MUMmer
    myenv = os.environ.copy()
    myenv['PATH'] = mummer_path + ':' + myenv['PATH']

    if not all_required_binaries_exist(mummer_path):
        # making
        print ("Compiling MUMmer...")
        subprocess.call(
            ['make', '-C', mummer_path],
            stdout=open(os.path.join(mummer_path, 'make.log'), 'w'), stderr=open(os.path.join(mummer_path, 'make.err'), 'w'))
        if not all_required_binaries_exist(mummer_path):
            print >>sys.stderr, "Error occurred during MUMmer compilation (", mummer_path, ")! Try to compile it manually!"
            print >>sys.stderr, "Exiting"
            sys.exit(1)

    print 'Running contigs analyzer...'
    nucmer_output_dir = os.path.join(output_dir, 'nucmer_output')
    if not os.path.isdir(nucmer_output_dir):
        os.mkdir(nucmer_output_dir)

    from joblib import Parallel, delayed
    statuses_results_pairs = Parallel(n_jobs=len(filenames))(delayed(plantakolya_process)(
        cyclic, draw_plots, nucmer_output_dir, fname, id, myenv, output_dir, reference)
          for id, fname in enumerate(filenames))
    # unzipping
    statuses, results = [x[0] for x in statuses_results_pairs], [x[1] for x in statuses_results_pairs]

    def save_result(result):
        report = reporting.get(fname)

        avg_idy = result['avg_idy']
        region_misassemblies = result['region_misassemblies']
        misassembled_contigs = result['misassembled_contigs']
        misassembled_bases = result['misassembled_bases']
        unaligned = result['unaligned']
        partially_unaligned = result['partially_unaligned']
        partially_unaligned_bases = result['partially_unaligned_bases']
        fully_unaligned_bases = result['fully_unaligned_bases']
        ambiguous = result['ambiguous']
        total_ambiguous = result['total_ambiguous']
        SNPs = result['SNPs']
        indels = result['indels']
        total_aligned_bases = result['total_aligned_bases']
        partially_unaligned_with_misassembly = result['partially_unaligned_with_misassembly']
        partially_unaligned_with_significant_parts = result['partially_unaligned_with_significant_parts']

        report.add_field(reporting.Fields.AVGIDY, '%.3f' % avg_idy)
        report.add_field(reporting.Fields.MISLOCAL, region_misassemblies.count(Misassembly.LOCAL))
        report.add_field(reporting.Fields.MISASSEMBL, len(region_misassemblies) - region_misassemblies.count(Misassembly.LOCAL))
        report.add_field(reporting.Fields.MISCONTIGS, len(misassembled_contigs))
        report.add_field(reporting.Fields.MISCONTIGSBASES, misassembled_bases)
        report.add_field(reporting.Fields.UNALIGNED, '%d + %d part' % (unaligned, partially_unaligned))
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

    for id, fname in enumerate(filenames):
        if statuses[id] == NucmerStatus.OK:
            save_result(results[id])

    nucmer_statuses = dict(zip(filenames, statuses))

#    nucmer_statuses = {}
#
#    for id, filename in enumerate(filenames):
#        nucmer_status = plantakolya_process(cyclic, draw_plots, filename, id, myenv, output_dir, reference)
#        nucmer_statuses[filename] = nucmer_status

    if NucmerStatus.OK in nucmer_statuses.values():
        reporting.save_misassemblies(output_dir)
        reporting.save_unaligned(output_dir)

    oks = nucmer_statuses.values().count(NucmerStatus.OK)
    not_aligned = nucmer_statuses.values().count(NucmerStatus.NOT_ALIGNED)
    failed = nucmer_statuses.values().count(NucmerStatus.FAILED)
    problems = not_aligned + failed
    all = len(nucmer_statuses)

    if oks == all:
        print '  Done.'
    if oks < all and problems < all:
        print '  Done for', str(all - problems), 'out of', str(all) + '. For the rest, only basic stats are going to be evaluated.'
    if problems == all:
        print '  Failed aligning the contigs for all the assemblies. Only basic stats are going to be evaluated.'

#    if NucmerStatus.FAILED in nucmer_statuses.values():
#        print '  ' + str(failed),      'file' + (' ' if failed == 1 else 's ')      + 'failed to align to the reference. Only basic stats have been evaluated.'
#    if NucmerStatus.NOT_ALIGNED in nucmer_statuses.values():
#        print '  ' + str(not_aligned), 'file' + (' was' if not_aligned == 1 else 's were') + ' not aligned to the reference. Only basic stats have been evaluated.'

#    if problems == all:
#        print '  Nucmer failed.'

    return nucmer_statuses
