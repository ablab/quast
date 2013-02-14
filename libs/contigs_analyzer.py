############################################################################
# Copyright (c) 2011-2013 Saint-Petersburg Academic University
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

from __future__ import with_statement
import logging
import os
import platform
import subprocess
import datetime
import fastaparser
import shutil
from libs import reporting, qconfig
from qutils import id_to_str, error, print_timestamp

required_binaries = ['nucmer', 'delta-filter', 'show-coords', 'show-snps']

class Misassembly:
    LOCAL=0
    RELOCATION=1
    TRANSLOCATION=2
    INVERSION=3

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


def clear_files(filename, nucmerfilename):
    if qconfig.debug:
        return
    # delete temporary files
    for ext in ['.delta', '.coords_tmp', '.coords.headless']:
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


def run_nucmer(prefix, reference, assembly, log_out, log_err, myenv):
    log = open(log_out, 'a')
    err = open(log_err, 'a')
    # additional GAGE params of Nucmer: '-l', '30', '-banded'
    subprocess.call(['nucmer', '-c', str(qconfig.mincluster), '-l', str(qconfig.mincluster),
                     '--maxmatch', '-p', prefix, reference, assembly],
                     stdout=log, stderr=err, env=myenv)


def plantakolya(cyclic, id, filename, nucmerfilename, myenv, output_dir, reference):
    log = logging.getLogger('quast')
    log.info('  ' + id_to_str(id) + os.path.basename(filename))

    # run plantakolya tool
    logfilename_out = os.path.join(output_dir, "contigs_report_" + os.path.basename(filename) + '.stdout')
    logfilename_err = os.path.join(output_dir, "contigs_report_" + os.path.basename(filename) + '.stderr')
    plantafile_out = open(logfilename_out, 'w')
    plantafile_err = open(logfilename_err, 'w')

    log.info('  ' + id_to_str(id) + 'Logging to files ' + logfilename_out + ' and ' + os.path.basename(logfilename_err) + '...')
    maxun = 10
    epsilon = 0.99
    smgap = 1000
    umt = 0.1 # threshold for misassembled contigs with aligned less than $umt * 100% (Unaligned Missassembled Threshold)
    nucmer_successful_check_filename = nucmerfilename + '.sf'
    coords_filename = nucmerfilename + '.coords'
    delta_filename = nucmerfilename + '.delta'
    filtered_delta_filename = nucmerfilename + '.fdelta'
    #coords_btab_filename = nucmerfilename + '.coords.btab'
    coords_filtered_filename = nucmerfilename + '.coords.filtered'
    unaligned_filename = nucmerfilename + '.unaligned'
    show_snps_filename = nucmerfilename + '.all_snps'
    used_snps_filename = nucmerfilename + '.used_snps'
    #nucmer_report_filename = nucmerfilename + '.report'

    print >> plantafile_out, 'Aligning contigs to reference...'

    # Checking if there are existing previous nucmer alignments.
    # If they exist, using them to save time.
    using_existing_alignments = False
    if os.path.isfile(nucmer_successful_check_filename) and os.path.isfile(coords_filename) \
        and os.path.isfile(show_snps_filename):

        successful_check_content = open(nucmer_successful_check_filename).read().split('\n')
        if len(successful_check_content) > 2 and successful_check_content[1].strip() == str(qconfig.min_contig):
            print >> plantafile_out, '\tUsing existing Nucmer alignments...'
            log.info('  ' + id_to_str(id) + 'Using existing Nucmer alignments... ')
            using_existing_alignments = True

    if not using_existing_alignments:
        print >> plantafile_out, '\tRunning Nucmer...'
        log.info('  ' + id_to_str(id) + 'Running Nucmer... ')

        if qconfig.splitted_ref:
            prefixes_and_chr_files = [(nucmerfilename + "_" + os.path.basename(chr_file), chr_file) for chr_file in qconfig.splitted_ref]

            # Daemonic processes are not allowed to have children, so if we are already one of parallel processes
            # (i.e. daemonic) we can't start new daemonic processes
            if qconfig.assemblies_num == 1:
                n_jobs = min(qconfig.max_threads, len(prefixes_and_chr_files))
            else:
                n_jobs = 1
            if n_jobs > 1:
                log.info('    ' + 'Aligning to different chromosomes in parallel (' + str(n_jobs) + ' threads)')

            # processing each chromosome separately (if we can)
            from joblib import Parallel, delayed
            Parallel(n_jobs=n_jobs)(delayed(run_nucmer)(
                prefix, chr_file, filename, logfilename_out, logfilename_err, myenv)
                for (prefix, chr_file) in prefixes_and_chr_files)

            # filling common delta file
            delta_file = open(delta_filename, 'w')
            delta_file.write(reference + " " + filename + "\n")
            delta_file.write("NUCMER\n")

            for (prefix, chr_file) in prefixes_and_chr_files:
                chr_delta_filename = prefix + '.delta'
                if os.path.isfile(chr_delta_filename):
                    chr_delta_file = open(chr_delta_filename)
                    chr_delta_file.readline()
                    chr_delta_file.readline()
                    for line in chr_delta_file:
                        delta_file.write(line)
                    chr_delta_file.close()

            delta_file.close()
        else:
            run_nucmer(nucmerfilename, reference, filename, logfilename_out, logfilename_err, myenv)

        # Filtering by IDY% = 95 (as GAGE did)
        subprocess.call(['delta-filter', '-i', '95', delta_filename],
            stdout=open(filtered_delta_filename, 'w'), stderr=plantafile_err, env=myenv)
        shutil.move(filtered_delta_filename, delta_filename)

        tmp_coords_filename = coords_filename + '_tmp'
        subprocess.call(['show-coords', delta_filename],
            stdout=open(tmp_coords_filename, 'w'), stderr=plantafile_err, env=myenv)
        #subprocess.call(['dnadiff', '-d', delta_filename, '-p', nucmerfilename],
        #    stdout=open(logfilename_out, 'a'), stderr=plantafile_err, env=myenv)

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

        if not os.path.isfile(coords_filename):
            print >> plantafile_err, id_to_str(id) + 'Nucmer failed for', filename + ':', coords_filename, 'doesn\'t exist.'
            log.info('  ' + id_to_str(id) + 'Nucmer failed for ' + '\'' + os.path.basename(filename) + '\'.')
            return NucmerStatus.FAILED, {}, []
        #if not os.path.isfile(nucmer_report_filename):
        #    print >> plantafile_err, id_to_str(id) + 'Nucmer failed for', filename + ':', nucmer_report_filename, 'doesn\'t exist.'
        #    print '  ' + id_to_str(id) + 'Nucmer failed for ' + '\'' + os.path.basename(filename) + '\'.'
        #    return NucmerStatus.FAILED, {}
        if len(open(coords_filename).readlines()[-1].split()) < 13:
            print >> plantafile_err, id_to_str(id) + 'Nucmer: nothing aligned for', filename
            log.info('  ' + id_to_str(id) + 'Nucmer: nothing aligned for ' + '\'' + os.path.basename(filename) + '\'.')
            return NucmerStatus.NOT_ALIGNED, {}, []


        with open(coords_filename) as coords_file:
            headless_coords_filename = coords_filename + '.headless'
            headless_coords_file = open(headless_coords_filename, 'w')
            coords_file.readline()
            coords_file.readline()
            headless_coords_file.write(coords_file.read())
            headless_coords_file.close()
            headless_coords_file = open(headless_coords_filename)
            subprocess.call(['show-snps', '-S', '-T', '-H', delta_filename], stdin=headless_coords_file, stdout=open(show_snps_filename, 'w'), stderr=plantafile_err, env=myenv)

        nucmer_successful_check_file = open(nucmer_successful_check_filename, 'w')
        nucmer_successful_check_file.write("Min contig size:\n")
        nucmer_successful_check_file.write(str(qconfig.min_contig) + '\n')
        nucmer_successful_check_file.write("Successfully finished on " + datetime.datetime.now().strftime('%Y/%m/%d %H:%M:%S') + '\n')
        nucmer_successful_check_file.close()

    # Loading the alignment files
    print >> plantafile_out, 'Parsing coords...'
    aligns = {}
    coords_file = open(coords_filename)
    coords_filtered_file = open(coords_filtered_filename, 'w')
    coords_filtered_file.write(coords_file.readline())
    coords_filtered_file.write(coords_file.readline())
    sum_idy = 0.0
    num_idy = 0
    for line in coords_file:
        if line.strip() == '':
            break
        assert line[0] != '='
        #Clear leading spaces from nucmer output
        #Store nucmer lines in an array
        mapping = Mapping.from_line(line)
        sum_idy += mapping.idy
        num_idy += 1
        aligns.setdefault(mapping.contig, []).append(mapping)
    avg_idy = sum_idy / num_idy if num_idy else 0

    #### auxiliary functions ####
    def distance_between_alignments(align1, align2):
        '''
        returns distance (in contig) between two alignments
        '''
        align1_s = min(align1.e2, align1.s2)
        align1_e = max(align1.e2, align1.s2)
        align2_s = min(align2.e2, align2.s2)
        align2_e = max(align2.e2, align2.s2)
        if align1_s < align2_s: # alignment 1 is earlier in contig
            return align2_s - align1_e - 1
        else:                   # alignment 2 is earlier in contig
            return align1_s - align2_e - 1


    def process_misassembled_contig(aligned_lenths, i_start, i_finish, contig_len, prev, cur_aligned_length, misassembly_internal_overlap,
                                    sorted_aligns, is_1st_chimeric_half, misassembled_contigs, ref_aligns, ref_features):
        region_misassemblies = []
        for i in xrange(i_start, i_finish):
            print >> plantafile_out, '\t\t\tReal Alignment %d: %s' % (i+1, str(sorted_aligns[i]))
            #Calculate inconsistency between distances on the reference and on the contig
            distance_on_contig = distance_between_alignments(sorted_aligns[i], sorted_aligns[i+1])
            distance_on_reference = sorted_aligns[i+1].s1 - sorted_aligns[i].e1 - 1

            # update misassembly_internal_overlap
            misassembly_internal_overlap += (-distance_on_contig if distance_on_contig < 0 else 0)

            #Check strands
            strand1 = (sorted_aligns[i].s2 < sorted_aligns[i].e2)
            strand2 = (sorted_aligns[i+1].s2 < sorted_aligns[i+1].e2)

            # inconsistency of positions on reference and on contig
            if strand1:
                inconsistency = distance_on_reference - (sorted_aligns[i+1].s2 - sorted_aligns[i].e2 - 1)
            else:
                inconsistency = distance_on_reference - (sorted_aligns[i].e2 - sorted_aligns[i+1].s2 - 1)

            ref_aligns.setdefault(sorted_aligns[i].ref, []).append(sorted_aligns[i])

            # different chromosomes or large inconsistency (a gap or an overlap) or different strands
            if sorted_aligns[i].ref != sorted_aligns[i+1].ref or abs(inconsistency) > smgap or (strand1 != strand2):
                print >> coords_filtered_file, str(prev)
                aligned_lenths.append(cur_aligned_length)
                prev = sorted_aligns[i+1].clone()
                cur_aligned_length = prev.len2 - (-distance_on_contig if distance_on_contig < 0 else 0)

                print >> plantafile_out, '\t\t\t  Extensive misassembly (',

                ref_features.setdefault(sorted_aligns[i].ref, {})[sorted_aligns[i].e1] = 'M'
                ref_features.setdefault(sorted_aligns[i+1].ref, {})[sorted_aligns[i+1].e1] = 'M'

                if sorted_aligns[i].ref != sorted_aligns[i+1].ref:
                    region_misassemblies += [Misassembly.TRANSLOCATION]
                    print >> plantafile_out, 'translocation',
                elif abs(inconsistency) > smgap:
                    region_misassemblies += [Misassembly.RELOCATION]
                    print >> plantafile_out, 'relocation, inconsistency =', inconsistency,
                elif strand1 != strand2:
                    region_misassemblies += [Misassembly.INVERSION]
                    print >> plantafile_out, 'inversion',
                misassembled_contigs[sorted_aligns[i].contig] = contig_len

                print >> plantafile_out, ') between these two alignments'
            else:
                if inconsistency < 0:
                    #There is an overlap between the two alignments, a local misassembly
                    print >> plantafile_out, '\t\t\t  Overlap between these two alignments (local misassembly).',
                else:
                    #There is a small gap between the two alignments, a local misassembly
                    print >> plantafile_out, '\t\t\t  Gap between these two alignments (local misassembly).',
                #print >> plantafile_out, 'Distance on contig =', distance_on_contig, ', distance on reference =', distance_on_reference
                print >> plantafile_out, 'Inconsistency =', inconsistency

                region_misassemblies += [Misassembly.LOCAL]

                # output in coords.filtered (separate output for each alignment even if it is just a local misassembly)
                print >> coords_filtered_file, str(prev)
                prev = sorted_aligns[i+1].clone()

                ###          uncomment the following lines to disable breaking by local misassemblies
                #            # output in coords.filtered (merge alignments if it is just a local misassembly)
                #            prev.e1 = sorted_aligns[i+1].e1 # [E1]
                #            prev.s2 = 0 # [S2]
                #            prev.e2 = 0 # [E2]
                #            prev.len1 = prev.e1 - prev.s1 # [LEN1]
                #            prev.len2 += sorted_aligns[i+1].len2 - (overlap_in_contig if overlap_in_contig > 0 else 0) # [LEN2]

                if qconfig.strict_NA:
                    aligned_lenths.append(cur_aligned_length)
                    cur_aligned_length = 0
                cur_aligned_length += prev.len2 - (-distance_on_contig if distance_on_contig < 0 else 0)

        if not is_1st_chimeric_half:
            print >> coords_filtered_file, str(prev)
            aligned_lenths.append(cur_aligned_length)

        #Record the very last alignment
        i = i_finish
        print >> plantafile_out, '\t\t\tReal Alignment %d: %s' % (i+1, str(sorted_aligns[i]))
        ref_aligns.setdefault(sorted_aligns[i].ref, []).append(sorted_aligns[i])

        return cur_aligned_length, misassembly_internal_overlap, prev.clone(), region_misassemblies
    #### end of aux. functions ###

    # Loading the assembly contigs
    print >> plantafile_out, 'Loading Assembly...'
    assembly = {}
    assembly_ns = {}
    for name, seq in fastaparser.read_fasta(filename):
        assembly[name] = seq
        if 'N' in seq:
            assembly_ns[name] = [pos for pos in xrange(len(seq)) if seq[pos] == 'N']

    # Loading the reference sequences
    print >> plantafile_out, 'Loading reference...' # TODO: move up
    references = {}
    ref_aligns = {}
    ref_features = {}
    for name, seq in fastaparser.read_fasta(reference):
        name = name.split()[0] # no spaces in reference header
        references[name] = seq
        print >> plantafile_out, '\tLoaded [%s]' % name

    #Loading the SNP calls
    print >> plantafile_out, 'Loading SNPs...'

    class SNP():
        def __init__(self, ref=None, ctg=None, ref_pos=None, ctg_pos=None, ref_nucl=None, ctg_nucl=None):
            self.ref = ref
            self.ctg = ctg
            self.ref_pos = ref_pos
            self.ctg_pos = ctg_pos
            self.ref_nucl = ref_nucl
            self.ctg_nucl = ctg_nucl
            self.type = 'I' if self.ref_nucl == '.' else ('D' if ctg_nucl == '.' else 'S')

    snps = {}
    prev_line = None
    for line in open(show_snps_filename):
        #print "$line";
        line = line.split()
        if not line[0].isdigit():
            continue
        if prev_line and line == prev_line:
            continue
        ref = line[10]
        ctg = line[11]
        pos = int(line[0]) # Kolya: python don't convert int<->str types automatically
        loc = int(line[3]) # Kolya: same as above

        # if (! exists $line[11]) { die "Malformed line in SNP file.  Please check that show-snps has completed succesfully.\n$line\n[$line[9]][$line[10]][$line[11]]\n"; }
        if pos in snps.setdefault(ref, {}).setdefault(ctg, {}):
            snps.setdefault(ref, {}).setdefault(ctg, {})[pos].append(SNP(ref=ref, ctg=ctg, ref_pos=pos, ctg_pos=loc, ref_nucl=line[1], ctg_nucl=line[2]))
        else:
            snps.setdefault(ref, {}).setdefault(ctg, {})[pos] = [SNP(ref=ref, ctg=ctg, ref_pos=pos, ctg_pos=loc, ref_nucl=line[1], ctg_nucl=line[2])]
        prev_line = line
    used_snps_file = open(used_snps_filename, 'w')

    # Loading the regions (if any)
    regions = {}
    total_reg_len = 0
    total_regions = 0
    print >> plantafile_out, 'Loading regions...'
    # TODO: gff
    print >> plantafile_out, '\tNo regions given, using whole reference.'
    for name, seq in references.iteritems():
        regions.setdefault(name, []).append([1, len(seq)])
        total_regions += 1
        total_reg_len += len(seq)
    print >> plantafile_out, '\tTotal Regions: %d' % total_regions
    print >> plantafile_out, '\tTotal Region Length: %d' % total_reg_len

    unaligned = 0
    partially_unaligned = 0
    fully_unaligned_bases = 0
    partially_unaligned_bases = 0
    ambiguous_contigs = 0
    ambiguous_contigs_extra_bases = 0
    uncovered_regions = 0
    uncovered_region_bases = 0
    total_redundant = 0
    partially_unaligned_with_misassembly = 0
    partially_unaligned_with_significant_parts = 0
    misassembly_internal_overlap = 0

    region_misassemblies = []
    misassembled_contigs = {}

    aligned_lengths = []

    print >> plantafile_out, 'Analyzing contigs...'

    unaligned_file = open(unaligned_filename, 'w')
    for contig, seq in assembly.iteritems():
        #Recording contig stats
        ctg_len = len(seq)
        print >> plantafile_out, '\tCONTIG: %s (%dbp)' % (contig, ctg_len)
        #Check if this contig aligned to the reference
        if contig in aligns:
            #Pull all aligns for this contig
            num_aligns = len(aligns[contig])

            #Sort aligns by length and identity
            sorted_aligns = sorted(aligns[contig], key=lambda x: (x.len2 * x.idy, x.len2), reverse=True)
            top_len = sorted_aligns[0].len2
            top_id = sorted_aligns[0].idy
            top_aligns = []
            print >> plantafile_out, 'Top Length: %s  Top ID: %s' % (top_len, top_id)

            #Check that top hit captures most of the contig
            if top_len > ctg_len * epsilon or ctg_len - top_len < maxun:
                #Reset top aligns: aligns that share the same value of longest and higest identity
                top_aligns.append(sorted_aligns[0])
                sorted_aligns = sorted_aligns[1:]

                #Continue grabbing alignments while length and identity are identical
                #while sorted_aligns and top_len == sorted_aligns[0].len2 and top_id == sorted_aligns[0].idy:
                while sorted_aligns and ((sorted_aligns[0].len2 * sorted_aligns[0].idy) / (top_len * top_id) > epsilon):
                    top_aligns.append(sorted_aligns[0])
                    sorted_aligns = sorted_aligns[1:]

                #Mark other alignments as ambiguous
                while sorted_aligns:
                    ambig = sorted_aligns.pop()
                    print >> plantafile_out, '\t\tMarking as insignificant: %s' % str(ambig) # former ambiguous
                    # Kolya: removed redundant code about $ref (for gff AFAIU)

                if len(top_aligns) == 1:
                    #There is only one top align, life is good
                    print >> plantafile_out, '\t\tOne align captures most of this contig: %s' % str(top_aligns[0])
                    ref_aligns.setdefault(top_aligns[0].ref, []).append(top_aligns[0])
                    print >> coords_filtered_file, str(top_aligns[0])
                    aligned_lengths.append(top_aligns[0].len2)
                else:
                    #There is more than one top align
                    print >> plantafile_out, '\t\tThis contig has %d significant alignments. [An ambiguously mapped contig]' % len(
                        top_aligns)

                    #Increment count of ambiguously mapped contigs and bases in them
                    ambiguous_contigs += 1
                    # we count only extra bases, so we shouldn't include bases in the first alignment
                    # in case --allow-ambiguity is not set the number of extra bases will be negative!
                    ambiguous_contigs_extra_bases -= top_aligns[0].len2

                    # Alex: skip all alignments or count them as normal (just different aligns of one repeat). Depend on --allow-ambiguity option
                    if not qconfig.allow_ambiguity:
                        print >> plantafile_out, '\t\tSkipping these alignments (option --allow-ambiguity is not set):'
                        for align in top_aligns:
                            print >> plantafile_out, '\t\tSkipping alignment ', align
                    else:
                        # we count only extra bases, so we shouldn't include bases in the first alignment
                        first_alignment = True
                        while len(top_aligns):
                            print >> plantafile_out, '\t\tAlignment: %s' % str(top_aligns[0])
                            ref_aligns.setdefault(top_aligns[0].ref, []).append(top_aligns[0])
                            if first_alignment:
                                first_alignment = False
                                aligned_lengths.append(top_aligns[0].len2)
                            ambiguous_contigs_extra_bases += top_aligns[0].len2
                            print >> coords_filtered_file, str(top_aligns[0]), "ambiguous"
                            top_aligns = top_aligns[1:]

                    #Record these alignments as ambiguous on the reference
                    #                    for align in top_aligns:
                    #                        print >> plantafile_out, '\t\t\tAmbiguous Alignment: %s' % str(align)
                    #                        ref = align.ref
                    #                        for i in xrange(align.s1, align.e1+1):
                    #                            if (ref not in ref_features) or (i not in ref_features[ref]):
                    #                                ref_features.setdefault(ref, {})[i] = 'A'

                    #Increment count of ambiguous contigs and bases
                    #ambiguous += 1
                    #total_ambiguous += ctg_len
            else:
                #Sort all aligns by position on contig, then length
                sorted_aligns = sorted(sorted_aligns, key=lambda x: (x.len2, x.idy), reverse=True)
                sorted_aligns = sorted(sorted_aligns, key=lambda x: min(x.s2, x.e2))

                #Push first alignment on to real aligns
                real_aligns = [sorted_aligns[0]]
                last_end = max(sorted_aligns[0].s2, sorted_aligns[0].e2)
                last_real = sorted_aligns[0]

                #Walk through alignments, if not fully contained within previous, record as real
                real_groups = dict()
                for i in xrange(1, num_aligns):
                    cur_group = (last_end - last_real.len2 + 1, last_end)
                    #If this alignment extends past last alignment's endpoint, add to real, else skip
                    extension = max(sorted_aligns[i].s2, sorted_aligns[i].e2) - last_end # negative if no extension
                    if (extension > maxun) and (float(extension) / min(sorted_aligns[i].len2, last_real.len2) > 1.0 - epsilon):
                        real_aligns = real_aligns + [sorted_aligns[i]]
                        last_end = max(sorted_aligns[i].s2, sorted_aligns[i].e2)
                        last_real = sorted_aligns[i]
                    else:
                        if (sorted_aligns[i].len2 * sorted_aligns[i].idy) / (last_real.idy * last_real.len2) > epsilon:
                            if cur_group not in real_groups:
                                real_groups[cur_group] = [ real_aligns[-1] ]
                                real_aligns = real_aligns[:-1]
                            real_groups[cur_group].append(sorted_aligns[i])
                        else:
                            print >> plantafile_out, '\t\tSkipping redundant alignment %d %s' % (i, str(sorted_aligns[i]))
                            # Kolya: removed redundant code about $ref (for gff AFAIU)

                # choose appropriate alignments (to minimize total size of contig alignment and reduce # misassemblies
                if len(real_groups) > 0:
                    # auxiliary function
                    def get_group_id_of_align(align):
                        for k,v in real_groups.items():
                            if align in v:
                                return k
                        return None

                    # adding degenerate groups for single real aligns
                    if len(real_aligns) > 0:
                        for align in real_aligns:
                            cur_group = (min(align.s2, align.e2), max(align.s2, align.e2))
                            real_groups[cur_group] = [align]

                    sorted_aligns = sorted((align for group in real_groups.values() for align in group), key=lambda x: x.s1)
                    min_selection = []
                    min_selection_distance = None
                    cur_selection = []
                    cur_selection_group_ids = []
                    for cur_align in sorted_aligns:
                        cur_align_group_id = get_group_id_of_align(cur_align)
                        if cur_align_group_id not in cur_selection_group_ids:
                            cur_selection.append(cur_align)
                            cur_selection_group_ids.append(cur_align_group_id)
                        else:
                            for align in cur_selection:
                                if get_group_id_of_align(align) == cur_align_group_id:
                                    cur_selection.remove(align)
                                    break
                            cur_selection.append(cur_align)

                        if len(cur_selection_group_ids) == len(real_groups.keys()):
                            cur_selection_distance = cur_selection[-1].e1 - cur_selection[0].s1
                            if (not min_selection) or (cur_selection_distance < min_selection_distance):
                                min_selection = list(cur_selection)
                                min_selection_distance = cur_selection_distance

                    # save min selection to real aligns and skip others (as redundant)
                    real_aligns = list(min_selection)
                    print >> plantafile_out, '\t\tSkipping redundant alignments after choosing the best set of alignments'
                    for align in sorted_aligns:
                        if align not in real_aligns:
                            print >> plantafile_out, '\t\tSkipping [%d][%d] redundant alignment %s' % (
                                align.s1, align.e1, str(align))

                if len(real_aligns) == 1:
                    #There is only one alignment of this contig to the reference
                    print >> coords_filtered_file, str(real_aligns[0])
                    aligned_lengths.append(real_aligns[0].len2)

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
                        print >> plantafile_out, '\t\tThis contig is partially unaligned. (%d out of %d)' % (
                        top_len, ctg_len)
                        print >> plantafile_out, '\t\tAlignment: %s' % str(sorted_aligns[0])
                        if (begin - 1):
                            print >> plantafile_out, '\t\tUnaligned bases: 1 to %d (%d)' % (begin - 1, begin - 1)
                        if (ctg_len - end):
                            print >> plantafile_out, '\t\tUnaligned bases: %d to %d (%d)' % (end + 1, ctg_len, ctg_len - end)
                        # check if both parts (aligned and unaligned) have significant length
                        if (unaligned_bases >= qconfig.min_contig) and (ctg_len - unaligned_bases >= qconfig.min_contig):
                            partially_unaligned_with_significant_parts += 1
                            print >> plantafile_out, '\t\tThis contig has both significant aligned and unaligned parts ' \
                                                 '(of length >= min-contig)!'
                    ref_aligns.setdefault(sorted_aligns[0].ref, []).append(sorted_aligns[0])
                else:
                    #There is more than one alignment of this contig to the reference
                    print >> plantafile_out, '\t\tThis contig is misassembled. %d total aligns.' % num_aligns
                    #Reset real alignments and sum of real alignments
                    #Sort real alignments by position on the reference
                    sorted_aligns = sorted(real_aligns, key=lambda x: (x.ref, x.s1, x.e1))

                    # Counting misassembled contigs which are mostly partially unaligned
                    all_aligns_len = sum(x.len2 for x in sorted_aligns)
                    if all_aligns_len < umt * ctg_len:
                        print >> plantafile_out, '\t\t\tWarning! This contig is more unaligned than misassembled. ' + \
                            'Contig length is %d and total length of all aligns is %d' % (ctg_len, all_aligns_len)
                        partially_unaligned_with_misassembly += 1
                        for align in sorted_aligns:
                            print >> plantafile_out, '\t\tAlignment: %s' % str(align)
                            print >> coords_filtered_file, str(align)
                            aligned_lengths.append(align.len2)

                        #Increment tally of partially unaligned contigs
                        partially_unaligned += 1
                        #Increment tally of partially unaligned bases
                        partially_unaligned_bases += ctg_len - all_aligns_len
                        print >> plantafile_out, '\t\tUnaligned bases: %d' % (ctg_len - all_aligns_len)
                        # check if both parts (aligned and unaligned) have significant length
                        if (all_aligns_len >= qconfig.min_contig) and (ctg_len - all_aligns_len >= qconfig.min_contig):
                            partially_unaligned_with_significant_parts += 1
                            print >> plantafile_out, '\t\tThis contig has both significant aligned and unaligned parts '\
                                                 '(of length >= min-contig)!'
                        continue

                    sorted_num = len(sorted_aligns) - 1

                    # computing cyclic references
                    if cyclic and (sorted_aligns[0].s1 - 1 + total_reg_len - sorted_aligns[sorted_num].e1 -
                                   distance_between_alignments(sorted_aligns[sorted_num], sorted_aligns[0]) <= smgap): # fake misassembly

                        # find fake alignment between "first" blocks and "last" blocks
                        fake_misassembly_index = 0
                        for i in xrange(sorted_num):
                            gap = sorted_aligns[i + 1].s1 - sorted_aligns[i].e1
                            if gap > distance_between_alignments(sorted_aligns[i], sorted_aligns[i + 1]) + smgap:
                                fake_misassembly_index = i + 1
                                break

                        # for merging local misassemblies
                        prev = sorted_aligns[fake_misassembly_index].clone()
                        cur_aligned_length = prev.len2

                        # process "last half" of blocks
                        cur_aligned_length, misassembly_internal_overlap, prev, x = process_misassembled_contig(
                            aligned_lengths, fake_misassembly_index, sorted_num, len(assembly[contig]), prev,
                            cur_aligned_length, misassembly_internal_overlap, sorted_aligns, True, misassembled_contigs,
                            ref_aligns, ref_features)
                        region_misassemblies += x
                        print >> plantafile_out, '\t\t\t  Fake misassembly (caused by linear representation of circular genome) between these two alignments'

                        # connecting parts of fake misassembly in one alignment
                        prev.e1 = sorted_aligns[0].e1 # [E1]
                        prev.s2 = 0 # [S2]
                        prev.e2 = 0 # [E2]
                        prev.len1 += sorted_aligns[0].e1 - sorted_aligns[0].s1 + 1 # [LEN1]
                        prev.len2 += sorted_aligns[0].len2 # [LEN2]
                        cur_aligned_length += sorted_aligns[0].len2

                        # process "first half" of blocks
                        cur_aligned_length, misassembly_internal_overlap, prev, x = process_misassembled_contig(
                            aligned_lengths, 0, fake_misassembly_index - 1, len(assembly[contig]), prev,
                            cur_aligned_length, misassembly_internal_overlap, sorted_aligns, False, misassembled_contigs,
                            ref_aligns, ref_features)
                        region_misassemblies += x

                    else:
                        # for merging local misassemblies
                        prev = sorted_aligns[0].clone()
                        cur_aligned_length = prev.len2
                        cur_aligned_length, misassembly_internal_overlap, prev, x = process_misassembled_contig(
                            aligned_lengths, 0, sorted_num, len(assembly[contig]), prev,
                            cur_aligned_length, misassembly_internal_overlap, sorted_aligns, False, misassembled_contigs,
                            ref_aligns, ref_features)
                        region_misassemblies += x

        else:
            #No aligns to this contig
            print >> plantafile_out, '\t\tThis contig is unaligned. (%d bp)' % ctg_len
            print >> unaligned_file, contig

            #Increment unaligned contig count and bases
            unaligned += 1
            fully_unaligned_bases += ctg_len
            print >> plantafile_out, '\t\tUnaligned bases: %d  total: %d' % (ctg_len, fully_unaligned_bases)

    coords_filtered_file.close()
    unaligned_file.close()

    print >> plantafile_out, 'Analyzing coverage...'
    print >> plantafile_out, 'Writing SNPs into', used_snps_filename

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

    # for counting short and long indels
    indels_list = []
    prev_snp = None
    cur_indel = 0

    #Go through each header in reference file
    for ref, value in regions.iteritems():
        #Check to make sure this reference ID contains aligns.
        if ref not in ref_aligns:
            print >> plantafile_out, 'ERROR: Reference [$ref] does not have any alignments!  Check that this is the same file used for alignment.'
            print >> plantafile_out, 'ERROR: Alignment Reference Headers: %s' % ref_aligns.keys()
            continue

        #Sort all alignments in this reference by start location
        sorted_aligns = sorted(ref_aligns[ref], key=lambda x: x.s1)
        total_aligns = len(sorted_aligns)
        print >> plantafile_out, '\tReference %s: %d total alignments. %d total regions.' % (ref, total_aligns, len(regions[ref]))

        #Walk through each region on this reference sequence
        for region in regions[ref]:
            end = 0
            reg_length = region[1] - region[0] + 1
            print >> plantafile_out, '\t\tRegion: %d to %d (%d bp)\n' % (region[0], region[1], reg_length)

            #Skipping alignments not in the next region
            while sorted_aligns and sorted_aligns[0].e1 < region[0]:
                skipped = sorted_aligns[0]
                sorted_aligns = sorted_aligns[1:] # Kolya: slooow, but should never happens without gff :)
                print >> plantafile_out, '\t\t\tThis align occurs before our region of interest, skipping: %s' % skipped

            if not sorted_aligns:
                print >> plantafile_out, '\t\t\tThere are no more aligns. Skipping this region.'
                continue

            #If region starts in a contig, ignore portion of contig prior to region start
            if sorted_aligns and region and sorted_aligns[0].s1 < region[0]:
                print >> plantafile_out, '\t\t\tSTART within alignment : %s' % sorted_aligns[0]
                #Track number of bases ignored at the start of the alignment
                snip_left = region[0] - sorted_aligns[0].s1
                #Modify to account for any insertions or deletions that are present
                for z in xrange(sorted_aligns[0].s1, region[0] + 1):
                    if (ref in snps) and (sorted_aligns[0].contig in snps[ref]) and (z in snps[ref][sorted_aligns[0].contig]) and \
                       (ref in ref_features) and (z in ref_features[ref]) and (ref_features[ref][z] != 'A'): # Kolya: never happened before because of bug: z -> i
                        for cur_snp in snps[ref][sorted_aligns[0].contig][z]:
                            if cur_snp.type == 'I':
                                snip_left += 1
                            elif cur_snp.type == 'D':
                                snip_left -= 1

                #Modify alignment to start at region
                print >> plantafile_out, '\t\t\t\tMoving reference start from %d to %d' % (sorted_aligns[0].s1, region[0])
                sorted_aligns[0].s1 = region[0]

                #Modify start position in contig
                if sorted_aligns[0].s2 < sorted_aligns[0].e2:
                    print >> plantafile_out, '\t\t\t\tMoving contig start from %d to %d.' % (sorted_aligns[0].s2, sorted_aligns[0].s2 + snip_left)
                    sorted_aligns[0].s2 += snip_left
                else:
                    print >> plantafile_out, '\t\t\t\tMoving contig start from %d to %d.' % (sorted_aligns[0].s2, sorted_aligns[0].s2 - snip_left)
                    sorted_aligns[0].s2 -= snip_left

            #No aligns in this region
            if sorted_aligns[0].s1 > region[1]:
                print >> plantafile_out, '\t\t\tThere are no aligns within this region.'
                gaps.append([reg_length, 'START', 'END'])
                #Increment uncovered region count and bases
                uncovered_regions += 1
                uncovered_region_bases += reg_length
                continue

            #Record first gap, and first ambiguous bases within it
            if sorted_aligns[0].s1 > region[0]:
                size = sorted_aligns[0].s1 - region[0]
                print >> plantafile_out, '\t\t\tSTART in gap: %d to %d (%d bp)' % (region[0], sorted_aligns[0].s1, size)
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
                    print >> plantafile_out, '\t...%d of %d' % (counter, total_aligns)
                end = False
                #Check to see if previous gap was negative
                if negative:
                    print >> plantafile_out, '\t\t\tPrevious gap was negative, modifying coordinates to ignore overlap'
                    #Ignoring OL part of next contig, no SNPs or N's will be recorded
                    snip_left = current.e1 + 1 - sorted_aligns[0].s1
                    #Account for any indels that may be present
                    for z in xrange(sorted_aligns[0].s1, current.e1 + 2):
                        if (ref in snps) and (sorted_aligns[0].contig in snps[ref]) and (z in snps[ref][sorted_aligns[0].contig]):
                            for cur_snp in snps[ref][sorted_aligns[0].contig][z]:
                                if cur_snp.type == 'I':
                                    snip_left += 1
                                elif cur_snp.type == 'D':
                                    snip_left -= 1
                    #Modifying position in contig of next alignment
                    sorted_aligns[0].s1 = current.e1 + 1
                    if sorted_aligns[0].s2 < sorted_aligns[0].e2:
                        print >> plantafile_out, '\t\t\t\tMoving contig start from %d to %d.' % (sorted_aligns[0].s2, sorted_aligns[0].s2 + snip_left)
                        sorted_aligns[0].s2 += snip_left
                    else:
                        print >> plantafile_out, '\t\t\t\tMoving contig start from %d to %d.' % (sorted_aligns[0].s2, sorted_aligns[0].s2 - snip_left)
                        sorted_aligns[0].s2 -= snip_left
                    negative = False

                #Pull top alignment
                current = sorted_aligns[0]
                sorted_aligns = sorted_aligns[1:]
                #print >>plantafile_out, '\t\t\tAlign %d: %s' % (counter, current)  #(self, s1, e1, s2, e2, len1, len2, idy, ref, contig):
                print >>plantafile_out, '\t\t\tAlign %d: %s' % (counter, '%d %d %s %d %d' % (current.s1, current.e1, current.contig, current.s2, current.e2))

                #Check if:
                # A) We have no more aligns to this reference
                # B) The current alignment extends to or past the end of the region
                # C) The next alignment starts after the end of the region

                if not sorted_aligns or current.e1 >= region[1] or sorted_aligns[0].s1 > region[1]:
                    #Check if last alignment ends before the regions does (gap at end of the region)
                    if current.e1 >= region[1]:
                        #print "Ends inside current alignment.\n";
                        print >> plantafile_out, '\t\t\tEND in current alignment.  Modifying %d to %d.' % (current.e1, region[1])
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
                        print >> plantafile_out, '\t\t\tEND in gap: %d to %d (%d bp)' % (current.e1, region[1], size)

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
                    #print >> plantafile_out, '\t\t\t\tNext Alignment: %d %d %s %d %d' % (next.s1, next.e1, next.contig, next.s2, next.e2)

                    if next.e1 <= current.e1:
                        #The next alignment is redundant to the current alignmentt
                        while next.e1 <= current.e1 and sorted_aligns:
                            total_redundant += next.e1 - next.s1 + 1
                            print >> plantafile_out, '\t\t\t\tThe next alignment (%d %d %s %d %d) is redundant. Skipping.' \
                                                     % (next.s1, next.e1, next.contig, next.s2, next.e2)
                            redundant.append(current.contig)
                            sorted_aligns = sorted_aligns[1:]
                            if sorted_aligns:
                                next = sorted_aligns[0]
                                counter += 1
                            else:
                                #Flag to end loop through alignment
                                end = True

                    if not end:
                        if next.s1 > current.e1 + 1:
                            #There is a gap beetween this and the next alignment
                            size = next.s1 - current.e1 - 1
                            gaps.append([size, current.contig, next.contig])
                            print >> plantafile_out, '\t\t\t\tGap between this and next alignment: %d to %d (%d bp)' % (current.e1, next.s1, size)
                            #Record ambiguous bases in current gap
                            for i in xrange(current.e1, next.s1):
                                if (ref in ref_features) and (i in ref_features[ref]) and (ref_features[ref][i] == 'A'):
                                    region_ambig += 1
                        elif next.s1 <= current.e1:
                            #This alignment overlaps with the next alignment, negative gap
                            #If contig extends past the region, clip
                            if current.e1 > region[1]:
                                current.e1 = region[1]
                            #Record gap
                            size = next.s1 - current.e1
                            neg_gaps.append([size, current.contig, next.contig])
                            print >>plantafile_out, '\t\t\t\tNegative gap (overlap) between this and next alignment: %d to %d (%d bp)' % (current.e1, next.s1, size)

                            #Mark this alignment as negative so overlap region can be ignored
                            negative = True
                        print >> plantafile_out, '\t\t\t\tNext Alignment: %d %d %s %d %d' % (next.s1, next.e1, next.contig, next.s2, next.e2)

                #Initiate location of SNP on assembly to be first or last base of contig alignment
                contig_estimate = current.s2
                enable_SNPs_output = False
                if enable_SNPs_output:
                    print >> plantafile_out, '\t\t\t\tContig start coord: %d' % contig_estimate

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
                        cur_snps = snps[ref][current.contig][i]
                        # sorting by pos in contig
                        if current.s2 < current.e2:
                            cur_snps = sorted(cur_snps, key=lambda x: x.ctg_pos)
                        else: # for reverse complement
                            cur_snps = sorted(cur_snps, key=lambda x: x.ctg_pos, reverse=True)

                        for cur_snp in cur_snps:
                            if enable_SNPs_output:
                                print >> plantafile_out, '\t\t\t\tSNP: %s, reference coord: %d, contig coord: %d, estimated contig coord: %d' % \
                                         (cur_snp.type, i, cur_snp.ctg_pos, contig_estimate)

                            #Capture SNP base
                            snp = cur_snp.type

                            #Check that there are not multiple alignments at this location
                            ### Alex: obsolete, we changed algorithm for ambiguous contigs
                            #if (ref in ref_features) and (i in ref_features[ref]):
                            #    print >> plantafile_out, '\t\t\t\t\tERROR: SNP at a position where there are multiple alignments (%s).  Skipping.\n' % ref_features[ref][i]
                            #    if current.s2 < current.e2: contig_estimate += 1
                            #    else: contig_estimate -= 1
                            #    continue
                            #Check that the position of the SNP in the contig is close to the position of this SNP
                            if abs(contig_estimate - cur_snp.ctg_pos) > 2:
                                if enable_SNPs_output:
                                    print >> plantafile_out, '\t\t\t\t\tERROR: SNP position in contig was off by %d bp! (%d vs %d)' \
                                             % (abs(contig_estimate - cur_snp.ctg_pos), contig_estimate, cur_snp.ctg_pos)
                                continue

                            print >> used_snps_file, '%s\t%s\t%d\t%s\t%s\t%d' % (ref, current.contig, cur_snp.ref_pos,
                                                                                 cur_snp.ref_nucl, cur_snp.ctg_nucl, cur_snp.ctg_pos)

                            #If SNP is an insertion, record
                            if snp == 'I':
                                region_insertion += 1
                                if current.s2 < current.e2: contig_estimate += 1
                                else: contig_estimate -= 1
                            #If SNP is a deletion, record
                            if snp == 'D':
                                region_deletion += 1
                                if current.s2 < current.e2: contig_estimate -= 1
                                else: contig_estimate += 1
                            #If SNP is a mismatch, record
                            if snp == 'S':
                                region_snp += 1

                            if cur_snp.type == 'D' or cur_snp.type == 'I':
                                if prev_snp and (prev_snp.ref == cur_snp.ref) and (prev_snp.ctg == cur_snp.ctg) and \
                                    ((cur_snp.type == 'D' and (prev_snp.ref_pos == cur_snp.ref_pos - 1) and (prev_snp.ctg_pos == cur_snp.ctg_pos)) or
                                    (cur_snp.type == 'I' and (prev_snp.ctg_pos == cur_snp.ctg_pos - 1) and (prev_snp.ref_pos == cur_snp.ref_pos))):
                                    cur_indel += 1
                                else:
                                    if cur_indel:
                                        indels_list.append(cur_indel)
                                    cur_indel = 1
                                prev_snp = cur_snp

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

                if cur_indel:
                    indels_list.append(cur_indel)
                prev_snp = None
                cur_indel = 0

    # calulating SNPs and Subs. error (per 100 kbp)
    ##### getting results from Nucmer's dnadiff
#    SNPs = 0
#    indels = 0
#    total_aligned_bases = 0
#    for line in open(nucmer_report_filename):
#        #                           [REF]                [QRY]
#        # AlignedBases         4501335(97.02%)      4513272(90.71%)
#        if line.startswith('AlignedBases'):
#            total_aligned_bases = int(line.split()[2].split('(')[0])
#        # TotalSNPs                  516                  516
#        if line.startswith('TotalSNPs'):
#            SNPs = int(line.split()[2])
#        # TotalIndels                 9                    9
#        if line.startswith('TotalIndels'):
#            indels = int(line.split()[2])
#            break

    ##### getting results from Plantagora's algorithm
    SNPs = region_snp
    indels = region_insertion + region_deletion
    total_aligned_bases = region_covered
    print >> plantafile_out, 'Analysis is finished!'
    print >> plantafile_out, 'Founded SNPs were written into', used_snps_filename
    print >> plantafile_out, '\nResults:'

    print >> plantafile_out, '\tLocal Misassemblies: %d' % region_misassemblies.count(Misassembly.LOCAL)
    print >> plantafile_out, '\tMisassemblies: %d' % (len(region_misassemblies) - region_misassemblies.count(Misassembly.LOCAL))
    print >> plantafile_out, '\t\tRelocations: %d' % region_misassemblies.count(Misassembly.RELOCATION)
    print >> plantafile_out, '\t\tTranslocations: %d' % region_misassemblies.count(Misassembly.TRANSLOCATION)
    print >> plantafile_out, '\t\tInversions: %d' % region_misassemblies.count(Misassembly.INVERSION)
    print >> plantafile_out, '\tMisassembled Contigs: %d' % len(misassembled_contigs)
    misassembled_bases = sum(misassembled_contigs.itervalues())
    print >> plantafile_out, '\tMisassembled Contig Bases: %d' % misassembled_bases
    print >> plantafile_out, '\tMisassmblies Inter-Contig Overlap: %d' % misassembly_internal_overlap
    print >> plantafile_out, 'Uncovered Regions: %d (%d)' % (uncovered_regions, uncovered_region_bases)
    print >> plantafile_out, 'Unaligned Contigs: %d + %d part' % (unaligned, partially_unaligned)
    print >> plantafile_out, 'Partially Unaligned Contigs with Misassemblies: %d' % partially_unaligned_with_misassembly
    print >> plantafile_out, 'Unaligned Contig Bases: %d' % (fully_unaligned_bases + partially_unaligned_bases)

    print >> plantafile_out, ''
    print >> plantafile_out, 'Ambiguously Mapped Contigs: %d' % ambiguous_contigs
    if qconfig.allow_ambiguity:
        print >> plantafile_out, 'Extra Bases in Ambiguously Mapped Contigs: %d' % ambiguous_contigs_extra_bases
    else:
        print >> plantafile_out, 'Total Bases in Ambiguously Mapped Contigs: %d' % (-ambiguous_contigs_extra_bases)
        print >> plantafile_out, 'Note that --allow-ambiguity option was not set and these contigs were skipped.'

    #print >> plantafile_out, 'Mismatches: %d' % SNPs
    #print >> plantafile_out, 'Single Nucleotide Indels: %d' % indels

    print >> plantafile_out, ''
    print >> plantafile_out, '\tCovered Bases: %d' % region_covered    
    #print >> plantafile_out, '\tAmbiguous Bases (e.g. N\'s): %d' % region_ambig
    print >> plantafile_out, ''
    print >> plantafile_out, '\tSNPs: %d' % region_snp
    print >> plantafile_out, '\tInsertions: %d' % region_insertion
    print >> plantafile_out, '\tDeletions: %d' % region_deletion
    #print >> plantafile_out, '\tList of indels lengths:', indels_list
    print >> plantafile_out, ''
    print >> plantafile_out, '\tPositive Gaps: %d' % len(gaps)
    internal = 0
    external = 0
    summ = 0
    for gap in gaps:
        if gap[1] == gap[2]:
            internal += 1
        else:
            external += 1
            summ += gap[0]
    print >> plantafile_out, '\t\tInternal Gaps: % d' % internal
    print >> plantafile_out, '\t\tExternal Gaps: % d' % external
    print >> plantafile_out, '\t\tExternal Gap Total: % d' % summ
    if external:
        avg = summ * 1.0 / external
    else:
        avg = 0.0
    print >> plantafile_out, '\t\tExternal Gap Average: %.0f' % avg

    print >> plantafile_out, '\tNegative Gaps: %d' % len(neg_gaps)
    internal = 0
    external = 0
    summ = 0
    for gap in neg_gaps:
        if gap[1] == gap[2]:
            internal += 1
        else:
            external += 1
            summ += gap[0]
    print >> plantafile_out, '\t\tInternal Overlaps: % d' % internal
    print >> plantafile_out, '\t\tExternal Overlaps: % d' % external
    print >> plantafile_out, '\t\tExternal Overlaps Total: % d' % summ
    if external:
        avg = summ * 1.0 / external
    else:
        avg = 0.0
    print >> plantafile_out, '\t\tExternal Overlaps Average: %.0f' % avg

    redundant = list(set(redundant))
    print >> plantafile_out, '\tContigs with Redundant Alignments: %d (%d)' % (len(redundant), total_redundant)

    result = {'avg_idy': avg_idy, 'region_misassemblies': region_misassemblies,
              'misassembled_contigs': misassembled_contigs, 'misassembled_bases': misassembled_bases,
              'misassembly_internal_overlap': misassembly_internal_overlap,
              'unaligned': unaligned, 'partially_unaligned': partially_unaligned,
              'partially_unaligned_bases': partially_unaligned_bases, 'fully_unaligned_bases': fully_unaligned_bases,
              'ambiguous_contigs': ambiguous_contigs, 'ambiguous_contigs_extra_bases': ambiguous_contigs_extra_bases, 'SNPs': SNPs, 'indels_list': indels_list,
              'total_aligned_bases': total_aligned_bases,
              'partially_unaligned_with_misassembly': partially_unaligned_with_misassembly,
              'partially_unaligned_with_significant_parts': partially_unaligned_with_significant_parts}

    ## outputting misassembled contigs to separate file
    fasta = [(name, seq) for name, seq in fastaparser.read_fasta(filename) if
                         name in misassembled_contigs.keys()]
    fastaparser.write_fasta(os.path.join(output_dir, os.path.basename(filename) + '.mis_contigs'), fasta)

    plantafile_out.close()
    plantafile_err.close()
    used_snps_file.close()
    log.info('  ' + id_to_str(id) + 'Analysis is finished.')
    return NucmerStatus.OK, result, aligned_lengths


def plantakolya_process(cyclic, nucmer_output_dir, filename, id, myenv, output_dir, reference):
    nucmer_fname = os.path.join(nucmer_output_dir, os.path.basename(filename))
    nucmer_is_ok, result, aligned_lengths = plantakolya(cyclic, id, filename, nucmer_fname, myenv, output_dir, reference)
    clear_files(filename, nucmer_fname)
    return nucmer_is_ok, result, aligned_lengths


def all_required_binaries_exist(mummer_path):
    for required_binary in required_binaries:
        if not os.path.isfile(os.path.join(mummer_path, required_binary)):
            return False
    return True


def do(reference, filenames, cyclic, output_dir):
    if not os.path.isdir(output_dir):
        os.mkdir(output_dir)

    log = logging.getLogger('quast')

    print_timestamp()
    log.info('Running Contigs analyzer...')

    ########################################################################
    if platform.system() == 'Darwin':
        mummer_path = os.path.join(qconfig.LIBS_LOCATION, 'MUMmer3.23-osx')
    else:
        mummer_path = os.path.join(qconfig.LIBS_LOCATION, 'MUMmer3.23-linux')

    ########################################################################
#    report_dict = {'header' : []}
#    for filename in filenames:
#        report_dict[os.path.basename(filename)] = []

    # for running our MUMmer
    myenv = os.environ.copy()
    myenv['PATH'] = mummer_path + ':' + myenv['PATH']

    if not all_required_binaries_exist(mummer_path):
        # making
        log.info("Compiling MUMmer...")
        try:
            subprocess.call(
                ['make', '-C', mummer_path],
                stdout=open(os.path.join(mummer_path, 'make.log'), 'w'), stderr=open(os.path.join(mummer_path, 'make.err'), 'w'))
            if not all_required_binaries_exist(mummer_path):
                raise
        except:
            error("Failed to compile MUMmer (" + mummer_path + ")! Try to compile it manually!")

    nucmer_output_dir = os.path.join(output_dir, 'nucmer_output')
    if not os.path.isdir(nucmer_output_dir):
        os.mkdir(nucmer_output_dir)

    n_jobs = min(len(filenames), qconfig.max_threads)
    from joblib import Parallel, delayed
    statuses_results_lengths_tuples = Parallel(n_jobs=n_jobs)(delayed(plantakolya_process)(
        cyclic, nucmer_output_dir, fname, id, myenv, output_dir, reference)
          for id, fname in enumerate(filenames))
    # unzipping
    statuses, results, aligned_lengths = [x[0] for x in statuses_results_lengths_tuples], \
                                         [x[1] for x in statuses_results_lengths_tuples], \
                                         [x[2] for x in statuses_results_lengths_tuples]

    def save_result(result):
        report = reporting.get(fname)

        avg_idy = result['avg_idy']
        region_misassemblies = result['region_misassemblies']
        misassembled_contigs = result['misassembled_contigs']
        misassembled_bases = result['misassembled_bases']
        misassembly_internal_overlap = result['misassembly_internal_overlap']
        unaligned = result['unaligned']
        partially_unaligned = result['partially_unaligned']
        partially_unaligned_bases = result['partially_unaligned_bases']
        fully_unaligned_bases = result['fully_unaligned_bases']
        ambiguous_contigs = result['ambiguous_contigs']
        ambiguous_contigs_extra_bases = result['ambiguous_contigs_extra_bases']
        SNPs = result['SNPs']
        indels_list = result['indels_list']
        total_aligned_bases = result['total_aligned_bases']
        partially_unaligned_with_misassembly = result['partially_unaligned_with_misassembly']
        partially_unaligned_with_significant_parts = result['partially_unaligned_with_significant_parts']

        report.add_field(reporting.Fields.AVGIDY, '%.3f' % avg_idy)
        report.add_field(reporting.Fields.MISLOCAL, region_misassemblies.count(Misassembly.LOCAL))
        report.add_field(reporting.Fields.MISASSEMBL, len(region_misassemblies) - region_misassemblies.count(Misassembly.LOCAL))
        report.add_field(reporting.Fields.MISCONTIGS, len(misassembled_contigs))
        report.add_field(reporting.Fields.MISCONTIGSBASES, misassembled_bases)
        report.add_field(reporting.Fields.MISINTERNALOVERLAP, misassembly_internal_overlap)
        report.add_field(reporting.Fields.UNALIGNED, '%d + %d part' % (unaligned, partially_unaligned))
        report.add_field(reporting.Fields.UNALIGNEDBASES, (fully_unaligned_bases + partially_unaligned_bases))
        report.add_field(reporting.Fields.AMBIGUOUS, ambiguous_contigs)
        report.add_field(reporting.Fields.AMBIGUOUSEXTRABASES, ambiguous_contigs_extra_bases)
        report.add_field(reporting.Fields.MISMATCHES, SNPs)
        # different types of indels:
        report.add_field(reporting.Fields.INDELS, len(indels_list))
        report.add_field(reporting.Fields.INDELSBASES, sum(indels_list))
        report.add_field(reporting.Fields.MIS_SHORT_INDELS, len([i for i in indels_list if i <= qconfig.SHORT_INDEL_THRESHOLD]))
        report.add_field(reporting.Fields.MIS_LONG_INDELS, len([i for i in indels_list if i > qconfig.SHORT_INDEL_THRESHOLD]))

        if total_aligned_bases:
            report.add_field(reporting.Fields.SUBSERROR, "%.2f" % (float(SNPs) * 100000.0 / float(total_aligned_bases)))
            report.add_field(reporting.Fields.INDELSERROR, "%.2f" % (float(report.get_field(reporting.Fields.INDELS))
                                                                     * 100000.0 / float(total_aligned_bases)))

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


    def save_result_for_unaligned(result):
        report = reporting.get(fname)

        unaligned_ctgs = report.get_field(reporting.Fields.NUMCONTIGS)
        unaligned_length = report.get_field(reporting.Fields.TOTALLEN)
        report.add_field(reporting.Fields.UNALIGNED, '%d + %d part' % (unaligned_ctgs, 0))
        report.add_field(reporting.Fields.UNALIGNEDBASES, unaligned_length)

        report.add_field(reporting.Fields.UNALIGNED_FULL_CNTGS, unaligned_ctgs)
        report.add_field(reporting.Fields.UNALIGNED_FULL_LENGTH, unaligned_length)


    for id, fname in enumerate(filenames):
        if statuses[id] == NucmerStatus.OK:
            save_result(results[id])
        elif statuses[id] == NucmerStatus.NOT_ALIGNED:
            save_result_for_unaligned(results[id])

    nucmer_statuses = dict(zip(filenames, statuses))
    aligned_lengths_per_fpath = dict(zip(filenames, aligned_lengths))

#    nucmer_statuses = {}
#
#    for id, filename in enumerate(filenames):
#        nucmer_status = plantakolya_process(cyclic, filename, id, myenv, output_dir, reference)
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
        log.info('  Done.')
    if oks < all and problems < all:
        log.info('  Done for ' + str(all - problems) + 'out of ' + str(all) + '. For the rest, only basic stats are going to be evaluated.')
    if problems == all:
        log.info('  Failed aligning the contigs for all the assemblies. Only basic stats are going to be evaluated.')

#    if NucmerStatus.FAILED in nucmer_statuses.values():
#        log.info('  ' + str(failed) + 'file' + (' ' if failed == 1 else 's ') + 'failed to align to the reference. Only basic stats have been evaluated.')
#    if NucmerStatus.NOT_ALIGNED in nucmer_statuses.values():
#        log.info('  ' + str(not_aligned) + ' file' + (' was' if not_aligned == 1 else 's were') + ' not aligned to the reference. Only basic stats have been evaluated.')
#    if problems == all:
#        log.info('  Nucmer failed.')

    return nucmer_statuses, aligned_lengths_per_fpath
