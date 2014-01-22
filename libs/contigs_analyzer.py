############################################################################
# Copyright (c) 2011-2014 Saint-Petersburg Academic University
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
import os
import platform
import datetime
import fastaparser
import shutil
from libs import reporting, qconfig, qutils

from libs.log import get_logger
logger = get_logger(qconfig.LOGGER_DEFAULT_NAME)


required_binaries = ['nucmer', 'delta-filter', 'show-coords', 'show-snps', 'mummer', 'mgaps']

if platform.system() == 'Darwin':
    mummer_dirpath = os.path.join(qconfig.LIBS_LOCATION, 'MUMmer3.23-osx')
else:
    mummer_dirpath = os.path.join(qconfig.LIBS_LOCATION, 'MUMmer3.23-linux')


def bin_fpath(fname):
    return os.path.join(mummer_dirpath, fname)


class Misassembly:
    LOCAL = 0
    RELOCATION = 1
    TRANSLOCATION = 2
    INVERSION = 3


class Mapping(object):
    def __init__(self, s1, e1, s2, e2, len1, len2, idy, ref, contig):
        self.s1, self.e1, self.s2, self.e2, self.len1, self.len2, self.idy, self.ref, self.contig = s1, e1, s2, e2, len1, len2, idy, ref, contig

    @classmethod
    def from_line(cls, line):
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
    def from_coords(cls, fpath):
        f = open(fpath, 'w')
        f.close()


def clear_files(fpath, nucmer_fpath):
    if qconfig.debug:
        return

    # delete temporary files
    for ext in ['.delta', '.coords_tmp', '.coords.headless']:
        if os.path.isfile(nucmer_fpath + ext):
            os.remove(nucmer_fpath + ext)
    if os.path.isfile('nucmer.error'):
        os.remove('nucmer.error')
    if os.path.isfile(fpath + '.clean'):
        os.remove(fpath + '.clean')


class NucmerStatus:
    FAILED = 0
    OK = 1
    NOT_ALIGNED = 2


def run_nucmer(prefix, ref_fpath, contigs_fpath, log_out_fpath, log_err_fpath, index, planta_err_f):
    # additional GAGE params of Nucmer: '-l', '30', '-banded'
    return_code = qutils.call_subprocess(
        [bin_fpath('nucmer'),
         '-c', str(qconfig.mincluster),
         '-l', str(qconfig.mincluster),
         '--maxmatch',
         '-p', prefix,
         ref_fpath,
         contigs_fpath],
        stdout=open(log_out_fpath, 'a'),
        stderr=open(log_err_fpath, 'a'),
        indent='  ' + qutils.index_to_str(index))

    if return_code != 0:
        print >> planta_err_f, qutils.index_to_str(index) + 'Nucmer failed for', contigs_fpath, '\n'

    return return_code


def __fail(contigs_fpath, index):
    logger.error('  ' + qutils.index_to_str(index) +
                 'Failed aligning contigs ' + qutils.label_from_fpath(contigs_fpath) + ' to the reference. ' +
                 ('Run with the --debug flag to see additional information.' if not qconfig.debug else ''))
    return NucmerStatus.FAILED, {}, []


def create_nucmer_successful_check(fpath, contigs_fpath, ref_fpath):
    nucmer_successful_check_file = open(fpath, 'w')
    nucmer_successful_check_file.write("Assembly file size in bytes: %d\n" % os.path.getsize(contigs_fpath))
    nucmer_successful_check_file.write("Reference file size in bytes: %d\n" % os.path.getsize(ref_fpath))
    nucmer_successful_check_file.write("Successfully finished on " +
                                       datetime.datetime.now().strftime('%Y/%m/%d %H:%M:%S') + '\n')
    nucmer_successful_check_file.close()


def check_nucmer_successful_check(fpath, contigs_fpath, ref_fpath):
    successful_check_content = open(fpath).read().split('\n')
    if len(successful_check_content) < 2:
        return False
    if not successful_check_content[0].strip().endswith(str(os.path.getsize(contigs_fpath))):
        return False
    if not successful_check_content[1].strip().endswith(str(os.path.getsize(ref_fpath))):
        return False
    return True


def plantakolya(cyclic, index, contigs_fpath, nucmer_fpath, output_dirpath, ref_fpath):
    assembly_name = qutils.name_from_fpath(contigs_fpath)
    assembly_label = qutils.label_from_fpath(contigs_fpath)

    logger.info('  ' + qutils.index_to_str(index) + assembly_label)

    # run plantakolya tool
    log_out_fpath = os.path.join(output_dirpath, "contigs_report_" + assembly_name + '.stdout')
    log_err_fpath = os.path.join(output_dirpath, "contigs_report_" + assembly_name + '.stderr')
    planta_out_f = open(log_out_fpath, 'w')
    planta_err_f = open(log_err_fpath, 'w')

    logger.info('  ' + qutils.index_to_str(index) + 'Logging to files ' + log_out_fpath +
                ' and ' + os.path.basename(log_err_fpath) + '...')
    maxun = 10
    epsilon = 0.99
    smgap = 1000
    umt = 0.5  # threshold for misassembled contigs with aligned less than $umt * 100% (Unaligned Missassembled Threshold)
    ort = 0.9  # threshold for skipping aligns that significantly overlaps with adjacent aligns (Overlap Relative Threshold)
    oat = 25   # threshold for skipping aligns that significantly overlaps with adjacent aligns (Overlap Absolute Threshold)
    nucmer_successful_check_fpath = nucmer_fpath + '.sf'
    coords_fpath = nucmer_fpath + '.coords'
    delta_fpath = nucmer_fpath + '.delta'
    filtered_delta_fpath = nucmer_fpath + '.fdelta'
    coords_filtered_fpath = nucmer_fpath + '.coords.filtered'
    unaligned_fpath = nucmer_fpath + '.unaligned'
    show_snps_fpath = nucmer_fpath + '.all_snps'
    used_snps_fpath = nucmer_fpath + '.used_snps'

    print >> planta_out_f, 'Aligning contigs to reference...'

    # Checking if there are existing previous nucmer alignments.
    # If they exist, using them to save time.
    using_existing_alignments = False
    if os.path.isfile(nucmer_successful_check_fpath) and\
       os.path.isfile(coords_fpath) and\
       os.path.isfile(show_snps_fpath):
        if check_nucmer_successful_check(nucmer_successful_check_fpath, contigs_fpath, ref_fpath):
            print >> planta_out_f, '\tUsing existing Nucmer alignments...'
            logger.info('  ' + qutils.index_to_str(index) + 'Using existing Nucmer alignments... ')
            using_existing_alignments = True

    if not using_existing_alignments:
        print >> planta_out_f, '\tRunning Nucmer'
        logger.info('  ' + qutils.index_to_str(index) + 'Running Nucmer')
        nucmer_failed = False

        if not qconfig.splitted_ref:
            nucmer_exit_code = run_nucmer(nucmer_fpath, ref_fpath, contigs_fpath,
                                          log_out_fpath, log_err_fpath, index, planta_err_f)
            if nucmer_exit_code != 0:
                return __fail(contigs_fpath, index)

        else:
            prefixes_and_chr_files = [(nucmer_fpath + "_" + os.path.basename(chr_fname), chr_fname)
                                      for chr_fname in qconfig.splitted_ref]

            # Daemonic processes are not allowed to have children,
            # so if we are already one of parallel processes
            # (i.e. daemonic) we can't start new daemonic processes
            if qconfig.assemblies_num == 1:
                n_jobs = min(qconfig.max_threads, len(prefixes_and_chr_files))
            else:
                n_jobs = 1
            if n_jobs > 1:
                logger.info('    ' + 'Aligning to different chromosomes in parallel'
                                     ' (' + str(n_jobs) + ' threads)')

            # processing each chromosome separately (if we can)
            from joblib import Parallel, delayed
            nucmer_exit_codes = Parallel(n_jobs=n_jobs)(delayed(run_nucmer)(
                prefix, chr_file, contigs_fpath, log_out_fpath, log_err_fpath, index, planta_err_f)
                for (prefix, chr_file) in prefixes_and_chr_files)

            if 0 not in nucmer_exit_codes:
                return __fail(contigs_fpath, index)

            else:
                # filling common delta file
                delta_file = open(delta_fpath, 'w')
                delta_file.write(ref_fpath + " " + contigs_fpath + "\n")
                delta_file.write("NUCMER\n")
                for i, (prefix, chr_fname) in enumerate(prefixes_and_chr_files):
                    if nucmer_exit_codes[i] != 0:
                        continue

                    chr_delta_fpath = prefix + '.delta'
                    if os.path.isfile(chr_delta_fpath):
                        chr_delta_file = open(chr_delta_fpath)
                        chr_delta_file.readline()
                        chr_delta_file.readline()
                        for line in chr_delta_file:
                            delta_file.write(line)
                        chr_delta_file.close()

                delta_file.close()

        # Filtering by IDY% = 95 (as GAGE did)
        return_code = qutils.call_subprocess(
            [bin_fpath('delta-filter'), '-i', '95', delta_fpath],
            stdout=open(filtered_delta_fpath, 'w'),
            stderr=planta_err_f,
            indent='  ' + qutils.index_to_str(index))

        if return_code != 0:
            print >> planta_err_f, qutils.index_to_str(index) + 'Delta filter failed for', contigs_fpath, '\n'
            return __fail(contigs_fpath, index)

        shutil.move(filtered_delta_fpath, delta_fpath)

        tmp_coords_fpath = coords_fpath + '_tmp'

        return_code = qutils.call_subprocess(
            [bin_fpath('show-coords'), delta_fpath],
            stdout=open(tmp_coords_fpath, 'w'),
            stderr=planta_err_f,
            indent='  ' + qutils.index_to_str(index))
        if return_code != 0:
            print >> planta_err_f, qutils.index_to_str(index) + 'Show-coords failed for', contigs_fpath, '\n'
            return __fail(contigs_fpath, index)

        # removing waste lines from coords file
        coords_file = open(coords_fpath, 'w')
        header = []
        tmp_coords_file = open(tmp_coords_fpath)
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

        if not os.path.isfile(coords_fpath):
            print >> planta_err_f, qutils.index_to_str(index) + 'Nucmer failed for', contigs_fpath + ':', coords_fpath, 'doesn\'t exist.'
            logger.info('  ' + qutils.index_to_str(index) + 'Nucmer failed for ' + '\'' + assembly_name + '\'.')
            return NucmerStatus.FAILED, {}, []
        if len(open(coords_fpath).readlines()[-1].split()) < 13:
            print >> planta_err_f, qutils.index_to_str(index) + 'Nucmer: nothing aligned for', contigs_fpath
            logger.info('  ' + qutils.index_to_str(index) + 'Nucmer: nothing aligned for ' + '\'' + assembly_name + '\'.')
            return NucmerStatus.NOT_ALIGNED, {}, []

        with open(coords_fpath) as coords_file:
            headless_coords_fpath = coords_fpath + '.headless'
            headless_coords_f = open(headless_coords_fpath, 'w')
            coords_file.readline()
            coords_file.readline()
            headless_coords_f.write(coords_file.read())
            headless_coords_f.close()
            headless_coords_f = open(headless_coords_fpath)

            return_code = qutils.call_subprocess(
                [bin_fpath('show-snps'), '-S', '-T', '-H', delta_fpath],
                stdin=headless_coords_f,
                stdout=open(show_snps_fpath, 'w'),
                stderr=planta_err_f,
                indent='  ' + qutils.index_to_str(index))
            if return_code != 0:
                print >> planta_err_f, qutils.index_to_str(index) + 'Show-snps failed for', contigs_fpath, '\n'
                return __fail(contigs_fpath, index)

        create_nucmer_successful_check(nucmer_successful_check_fpath, contigs_fpath, ref_fpath)

    # Loading the alignment files
    print >> planta_out_f, 'Parsing coords...'
    aligns = {}
    coords_file = open(coords_fpath)
    coords_filtered_file = open(coords_filtered_fpath, 'w')
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
    def distance_between_alignments(align1, align2, pos_strand, cyclic_ref_len=None):
        '''
        returns distance (in reference) between two alignments
        '''
        if pos_strand:            # alignment 1 should be earlier in reference
            distance = align2.s1 - align1.e1 - 1
        else:                     # alignment 2 should be earlier in reference
            distance = align1.s1 - align2.e1 - 1
        cyclic_moment = False
        if cyclic_ref_len is not None:
            if align1.e1 < align2.e1 and (cyclic_ref_len - align2.e1 + align1.s1 - 1) < smgap:
                distance += cyclic_ref_len * (-1 if pos_strand else 1)
                cyclic_moment = True
            elif align1.e1 >= align2.e1 and (cyclic_ref_len - align1.e1 + align2.s1 - 1) < smgap:
                distance += cyclic_ref_len * (1 if pos_strand else -1)
                cyclic_moment = True
        return distance, cyclic_moment


    def is_misassembly(align1, align2, cyclic_ref_lens=None):
        #Calculate inconsistency between distances on the reference and on the contig
        distance_on_contig = min(align2.e2, align2.s2) - max(align1.e2, align1.s2) - 1
        if cyclic_ref_lens is not None and align1.ref == align2.ref:
            distance_on_reference, cyclic_moment = distance_between_alignments(align1, align2, align1.s2 < align1.e2,
                cyclic_ref_lens[align1.ref])
        else:
            distance_on_reference, cyclic_moment = distance_between_alignments(align1, align2, align1.s2 < align1.e2)

        misassembly_internal_overlap = 0
        if distance_on_contig < 0:
            if distance_on_reference >= 0:
                misassembly_internal_overlap = (-distance_on_contig)
            elif (-distance_on_reference) < (-distance_on_contig):
                misassembly_internal_overlap = (distance_on_reference - distance_on_contig)

        strand1 = (align1.s2 < align1.e2)
        strand2 = (align2.s2 < align2.e2)
        inconsistency = distance_on_reference - distance_on_contig

        aux_data = {"inconsistency": inconsistency, "distance_on_contig": distance_on_contig,
                    "misassembly_internal_overlap": misassembly_internal_overlap, "cyclic_moment": cyclic_moment}
        # different chromosomes or large inconsistency (a gap or an overlap) or different strands
        if align1.ref != align2.ref or abs(inconsistency) > smgap or (strand1 != strand2):
            return True, aux_data
        else:
            return False, aux_data


    def process_misassembled_contig(sorted_aligns, cyclic, aligned_lengths, region_misassemblies, reg_lens, ref_aligns, ref_features):
        misassembly_internal_overlap = 0
        prev = sorted_aligns[0].clone()
        cur_aligned_length = prev.len2
        is_misassembled = False

        for i in range(len(sorted_aligns) - 1):
            print >> planta_out_f, '\t\t\tReal Alignment %d: %s' % (i+1, str(sorted_aligns[i]))
            is_extensive_misassembly, aux_data = is_misassembly(sorted_aligns[i], sorted_aligns[i+1],
                reg_lens if cyclic else None)
            inconsistency = aux_data["inconsistency"]
            distance_on_contig = aux_data["distance_on_contig"]
            misassembly_internal_overlap += aux_data["misassembly_internal_overlap"]
            cyclic_moment = aux_data["cyclic_moment"]

            ref_aligns.setdefault(sorted_aligns[i].ref, []).append(sorted_aligns[i])
            print >> coords_filtered_file, str(prev)

            if is_extensive_misassembly:
                is_misassembled = True
                aligned_lengths.append(cur_aligned_length)
                cur_aligned_length = 0

                print >> planta_out_f, '\t\t\t  Extensive misassembly (',
                if sorted_aligns[i].ref != sorted_aligns[i+1].ref:
                    region_misassemblies.append(Misassembly.TRANSLOCATION)
                    print >> planta_out_f, 'translocation',
                elif abs(inconsistency) > smgap:
                    region_misassemblies.append(Misassembly.RELOCATION)
                    print >> planta_out_f, 'relocation, inconsistency =', inconsistency,
                else: #if strand1 != strand2:
                    region_misassemblies.append(Misassembly.INVERSION)
                    print >> planta_out_f, 'inversion',
                print >> planta_out_f, ') between these two alignments'
                ref_features.setdefault(sorted_aligns[i].ref, {})[sorted_aligns[i].e1] = 'M'
                ref_features.setdefault(sorted_aligns[i+1].ref, {})[sorted_aligns[i+1].e1] = 'M'
            else:
                if cyclic_moment and inconsistency == 0:
                    print >> planta_out_f, '\t\t\t  Fake misassembly (caused by linear representation of circular genome) between these two alignments'
                else:
                    if qconfig.strict_NA:
                        aligned_lengths.append(cur_aligned_length)
                        cur_aligned_length = 0

                    if inconsistency < 0:
                        #There is an overlap between the two alignments, a local misassembly
                        print >> planta_out_f, '\t\t\t  Overlap between these two alignments (local misassembly).',
                    else:
                        #There is a small gap between the two alignments, a local misassembly
                        print >> planta_out_f, '\t\t\t  Gap between these two alignments (local misassembly).',
                        #print >> plantafile_out, 'Distance on contig =', distance_on_contig, ', distance on reference =', distance_on_reference
                    print >> planta_out_f, 'Inconsistency =', inconsistency, "(linear representation of circular genome)" if cyclic_moment else ""
                    region_misassemblies.append(Misassembly.LOCAL)

            prev = sorted_aligns[i+1].clone()
            cur_aligned_length += prev.len2 - (-distance_on_contig if distance_on_contig < 0 else 0)

        #Record the very last alignment
        i = len(sorted_aligns) - 1
        print >> planta_out_f, '\t\t\tReal Alignment %d: %s' % (i + 1, str(sorted_aligns[i]))
        ref_aligns.setdefault(sorted_aligns[i].ref, []).append(sorted_aligns[i])
        print >> coords_filtered_file, str(prev)
        aligned_lengths.append(cur_aligned_length)

        return is_misassembled, misassembly_internal_overlap
    #### end of aux. functions ###

    # Loading the assembly contigs
    print >> planta_out_f, 'Loading Assembly...'
    assembly = {}
    assembly_ns = {}
    for name, seq in fastaparser.read_fasta(contigs_fpath):
        assembly[name] = seq
        if 'N' in seq:
            assembly_ns[name] = [pos for pos in xrange(len(seq)) if seq[pos] == 'N']

    # Loading the reference sequences
    print >> planta_out_f, 'Loading reference...'  # TODO: move up
    references = {}
    ref_aligns = {}
    ref_features = {}
    for name, seq in fastaparser.read_fasta(ref_fpath):
        name = name.split()[0]  # no spaces in reference header
        references[name] = seq
        print >> planta_out_f, '\tLoaded [%s]' % name

    #Loading the SNP calls
    print >> planta_out_f, 'Loading SNPs...'

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
    for line in open(show_snps_fpath):
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
    used_snps_file = open(used_snps_fpath, 'w')

    # Loading the regions (if any)
    regions = {}
    reg_lens = {}
    total_reg_len = 0
    total_regions = 0
    print >> planta_out_f, 'Loading regions...'
    # TODO: gff
    print >> planta_out_f, '\tNo regions given, using whole reference.'
    for name, seq in references.iteritems():
        regions.setdefault(name, []).append([1, len(seq)])
        reg_lens[name] = len(seq)
        total_regions += 1
        total_reg_len += len(seq)
    print >> planta_out_f, '\tTotal Regions: %d' % total_regions
    print >> planta_out_f, '\tTotal Region Length: %d' % total_reg_len

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

    print >> planta_out_f, 'Analyzing contigs...'

    unaligned_file = open(unaligned_fpath, 'w')
    for contig, seq in assembly.iteritems():
        #Recording contig stats
        ctg_len = len(seq)
        print >> planta_out_f, 'CONTIG: %s (%dbp)' % (contig, ctg_len)
        #Check if this contig aligned to the reference
        if contig in aligns:
            #Pull all aligns for this contig
            num_aligns = len(aligns[contig])

            #Sort aligns by length and identity
            sorted_aligns = sorted(aligns[contig], key=lambda x: (x.len2 * x.idy, x.len2), reverse=True)
            top_len = sorted_aligns[0].len2
            top_id = sorted_aligns[0].idy
            top_aligns = []
            print >> planta_out_f, 'Top Length: %s  Top ID: %s' % (top_len, top_id)

            #Check that top hit captures most of the contig
            if top_len > ctg_len * epsilon or ctg_len - top_len < maxun:
                #Reset top aligns: aligns that share the same value of longest and highest identity
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
                    print >> planta_out_f, '\t\tMarking as insignificant: %s' % str(ambig) # former ambiguous
                    # Kolya: removed redundant code about $ref (for gff AFAIU)

                if len(top_aligns) == 1:
                    #There is only one top align, life is good
                    print >> planta_out_f, '\t\tOne align captures most of this contig: %s' % str(top_aligns[0])
                    ref_aligns.setdefault(top_aligns[0].ref, []).append(top_aligns[0])
                    print >> coords_filtered_file, str(top_aligns[0])
                    aligned_lengths.append(top_aligns[0].len2)
                else:
                    #There is more than one top align
                    print >> planta_out_f, '\t\tThis contig has %d significant alignments. [An ambiguously mapped contig]' % len(
                        top_aligns)

                    #Increment count of ambiguously mapped contigs and bases in them
                    ambiguous_contigs += 1
                    # we count only extra bases, so we shouldn't include bases in the first alignment
                    # in case --allow-ambiguity is not set the number of extra bases will be negative!
                    ambiguous_contigs_extra_bases -= top_aligns[0].len2

                    # Alex: skip all alignments or count them as normal (just different aligns of one repeat). Depend on --allow-ambiguity option
                    if qconfig.ambiguity_usage == "none":
                        print >> planta_out_f, '\t\tSkipping these alignments (option --ambiguity-usage is set to "none"):'
                        for align in top_aligns:
                            print >> planta_out_f, '\t\tSkipping alignment ', align
                    elif qconfig.ambiguity_usage == "one":
                        print >> planta_out_f, '\t\tUsing only first of these alignment (option --ambiguity-usage is set to "one"):'
                        print >> planta_out_f, '\t\tAlignment: %s' % str(top_aligns[0])
                        ref_aligns.setdefault(top_aligns[0].ref, []).append(top_aligns[0])
                        aligned_lengths.append(top_aligns[0].len2)
                        print >> coords_filtered_file, str(top_aligns[0])
                        top_aligns = top_aligns[1:]
                        for align in top_aligns:
                            print >> planta_out_f, '\t\tSkipping alignment ', align
                    elif qconfig.ambiguity_usage == "all":
                        print >> planta_out_f, '\t\tUsing all these alignments (option --ambiguity-usage is set to "all"):'
                        # we count only extra bases, so we shouldn't include bases in the first alignment
                        first_alignment = True
                        while len(top_aligns):
                            print >> planta_out_f, '\t\tAlignment: %s' % str(top_aligns[0])
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
                        # check whether previous alignment is almost contained in this extension
                        prev_extension = min(sorted_aligns[i].s2, sorted_aligns[i].e2) - min(last_real.s2, last_real.e2)
                        if (prev_extension <= maxun) or (float(prev_extension) / min(sorted_aligns[i].len2, last_real.len2) <= 1.0 - epsilon):
                            if cur_group in real_groups:
                                for align in real_groups[cur_group]:
                                    print >> planta_out_f, '\t\tSkipping redundant alignment %s' % (str(align))
                                del real_groups[cur_group]
                            else:
                                real_aligns = real_aligns[:-1]
                                print >> planta_out_f, '\t\tSkipping redundant alignment %s' % (str(last_real))

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
                            print >> planta_out_f, '\t\tSkipping redundant alignment %s' % (str(sorted_aligns[i]))
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
                    print >> planta_out_f, '\t\t\tSkipping redundant alignments after choosing the best set of alignments'
                    for align in sorted_aligns:
                        if align not in real_aligns:
                            print >> planta_out_f, '\t\tSkipping redundant alignment %s' % (str(align))

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
                        print >> planta_out_f, '\t\tThis contig is partially unaligned. (%d out of %d)' % (top_len, ctg_len)
                        print >> planta_out_f, '\t\tAlignment: %s' % str(sorted_aligns[0])
                        if begin - 1:
                            print >> planta_out_f, '\t\tUnaligned bases: 1 to %d (%d)' % (begin - 1, begin - 1)
                        if ctg_len - end:
                            print >> planta_out_f, '\t\tUnaligned bases: %d to %d (%d)' % (end + 1, ctg_len, ctg_len - end)
                        # check if both parts (aligned and unaligned) have significant length
                        if (unaligned_bases >= qconfig.min_contig) and (ctg_len - unaligned_bases >= qconfig.min_contig):
                            partially_unaligned_with_significant_parts += 1
                            print >> planta_out_f, '\t\tThis contig has both significant aligned and unaligned parts ' \
                                                   '(of length >= min-contig)!'

                    ref_aligns.setdefault(sorted_aligns[0].ref, []).append(sorted_aligns[0])
                else:
                    #Sort real alignments by position on the contig
                    sorted_aligns = sorted(real_aligns, key=lambda x: (min(x.s2, x.e2), max(x.s2, x.e2)))

                    #Extra skipping of redundant alignments (fully or almost fully covered by adjacent alignments)
                    if len(sorted_aligns) >= 3:
                        was_extra_skip = False
                        prev_end = max(sorted_aligns[0].s2, sorted_aligns[0].e2)
                        for i in range(1, len(sorted_aligns) - 1):
                            succ_start = min(sorted_aligns[i + 1].s2, sorted_aligns[i + 1].e2)
                            gap = succ_start - prev_end - 1
                            if gap > smgap:
                                prev_end = max(sorted_aligns[i].s2, sorted_aligns[i].e2)
                                continue
                            overlap = 0
                            if prev_end - min(sorted_aligns[i].s2, sorted_aligns[i].e2) + 1 > 0:
                                overlap += prev_end - min(sorted_aligns[i].s2, sorted_aligns[i].e2) + 1
                            if max(sorted_aligns[i].s2, sorted_aligns[i].e2) - succ_start + 1 > 0:
                                overlap += max(sorted_aligns[i].s2, sorted_aligns[i].e2) - succ_start + 1
                            if gap < oat or (float(overlap) / sorted_aligns[i].len2) > ort:
                                if not was_extra_skip:
                                    was_extra_skip = True
                                    print >> planta_out_f, '\t\t\tSkipping redundant alignments which significantly overlap with adjacent alignments'
                                print >> planta_out_f, '\t\tSkipping redundant alignment %s' % (str(sorted_aligns[i]))
                                real_aligns.remove(sorted_aligns[i])
                            else:
                                prev_end = max(sorted_aligns[i].s2, sorted_aligns[i].e2)
                        if was_extra_skip:
                            sorted_aligns = sorted(real_aligns, key=lambda x: (min(x.s2, x.e2), max(x.s2, x.e2)))

                    #There is more than one alignment of this contig to the reference
                    print >> planta_out_f, '\t\tThis contig is misassembled. %d total aligns.' % num_aligns

                    # Counting misassembled contigs which are mostly partially unaligned
                    # counting aligned and unaligned bases of a contig
                    aligned_bases_in_contig = 0
                    last_e2 = 0
                    for cur_align in sorted_aligns:
                        if max(cur_align.s2, cur_align.e2) <= last_e2:
                            continue
                        elif min(cur_align.s2, cur_align.e2) > last_e2:
                            aligned_bases_in_contig += (abs(cur_align.e2 - cur_align.s2) + 1)
                        else:
                            aligned_bases_in_contig += (max(cur_align.s2, cur_align.e2) - last_e2)
                        last_e2 = max(cur_align.s2, cur_align.e2)

                    #aligned_bases_in_contig = sum(x.len2 for x in sorted_aligns)
                    if aligned_bases_in_contig < umt * ctg_len:
                        print >> planta_out_f, '\t\t\tWarning! This contig is more unaligned than misassembled. ' + \
                            'Contig length is %d and total length of all aligns is %d' % (ctg_len, aligned_bases_in_contig)
                        partially_unaligned_with_misassembly += 1
                        for align in sorted_aligns:
                            print >> planta_out_f, '\t\tAlignment: %s' % str(align)
                            print >> coords_filtered_file, str(align)
                            aligned_lengths.append(align.len2)

                        #Increment tally of partially unaligned contigs
                        partially_unaligned += 1
                        #Increment tally of partially unaligned bases
                        partially_unaligned_bases += ctg_len - aligned_bases_in_contig
                        print >> planta_out_f, '\t\tUnaligned bases: %d' % (ctg_len - aligned_bases_in_contig)
                        # check if both parts (aligned and unaligned) have significant length
                        if (aligned_bases_in_contig >= qconfig.min_contig) and (ctg_len - aligned_bases_in_contig >= qconfig.min_contig):
                            partially_unaligned_with_significant_parts += 1
                            print >> planta_out_f, '\t\tThis contig has both significant aligned and unaligned parts '\
                                                   '(of length >= min-contig)!'
                        continue

                    ### processing misassemblies
                    is_misassembled, current_mio = process_misassembled_contig(sorted_aligns, cyclic,
                        aligned_lengths, region_misassemblies, reg_lens, ref_aligns, ref_features)
                    misassembly_internal_overlap += current_mio
                    if is_misassembled:
                        misassembled_contigs[contig] = len(assembly[contig])
        else:
            #No aligns to this contig
            print >> planta_out_f, '\t\tThis contig is unaligned. (%d bp)' % ctg_len
            print >> unaligned_file, contig

            #Increment unaligned contig count and bases
            unaligned += 1
            fully_unaligned_bases += ctg_len
            print >> planta_out_f, '\t\tUnaligned bases: %d  total: %d' % (ctg_len, fully_unaligned_bases)

        print >> planta_out_f

    coords_filtered_file.close()
    unaligned_file.close()

    print >> planta_out_f, 'Analyzing coverage...'
    print >> planta_out_f, 'Writing SNPs into', used_snps_fpath

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

    nothing_aligned = True
    #Go through each header in reference file
    for ref, value in regions.iteritems():
        #Check to make sure this reference ID contains aligns.
        if ref not in ref_aligns:
            print >> planta_out_f, 'ERROR: Reference [$ref] does not have any alignments!  Check that this is the same file used for alignment.'
            print >> planta_out_f, 'ERROR: Alignment Reference Headers: %s' % ref_aligns.keys()
            continue
        nothing_aligned = False

        #Sort all alignments in this reference by start location
        sorted_aligns = sorted(ref_aligns[ref], key=lambda x: x.s1)
        total_aligns = len(sorted_aligns)
        print >> planta_out_f, '\tReference %s: %d total alignments. %d total regions.' % (ref, total_aligns, len(regions[ref]))

        #Walk through each region on this reference sequence
        for region in regions[ref]:
            end = 0
            reg_length = region[1] - region[0] + 1
            print >> planta_out_f, '\t\tRegion: %d to %d (%d bp)' % (region[0], region[1], reg_length)

            #Skipping alignments not in the next region
            while sorted_aligns and sorted_aligns[0].e1 < region[0]:
                skipped = sorted_aligns[0]
                sorted_aligns = sorted_aligns[1:] # Kolya: slooow, but should never happens without gff :)
                print >> planta_out_f, '\t\t\tThis align occurs before our region of interest, skipping: %s' % skipped

            if not sorted_aligns:
                print >> planta_out_f, '\t\t\tThere are no more aligns. Skipping this region.'
                continue

            #If region starts in a contig, ignore portion of contig prior to region start
            if sorted_aligns and region and sorted_aligns[0].s1 < region[0]:
                print >> planta_out_f, '\t\t\tSTART within alignment : %s' % sorted_aligns[0]
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
                print >> planta_out_f, '\t\t\t\tMoving reference start from %d to %d' % (sorted_aligns[0].s1, region[0])
                sorted_aligns[0].s1 = region[0]

                #Modify start position in contig
                if sorted_aligns[0].s2 < sorted_aligns[0].e2:
                    print >> planta_out_f, '\t\t\t\tMoving contig start from %d to %d.' % (sorted_aligns[0].s2, sorted_aligns[0].s2 + snip_left)
                    sorted_aligns[0].s2 += snip_left
                else:
                    print >> planta_out_f, '\t\t\t\tMoving contig start from %d to %d.' % (sorted_aligns[0].s2, sorted_aligns[0].s2 - snip_left)
                    sorted_aligns[0].s2 -= snip_left

            #No aligns in this region
            if sorted_aligns[0].s1 > region[1]:
                print >> planta_out_f, '\t\t\tThere are no aligns within this region.'
                gaps.append([reg_length, 'START', 'END'])
                #Increment uncovered region count and bases
                uncovered_regions += 1
                uncovered_region_bases += reg_length
                continue

            #Record first gap, and first ambiguous bases within it
            if sorted_aligns[0].s1 > region[0]:
                size = sorted_aligns[0].s1 - region[0]
                print >> planta_out_f, '\t\t\tSTART in gap: %d to %d (%d bp)' % (region[0], sorted_aligns[0].s1, size)
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
                    print >> planta_out_f, '\t...%d of %d' % (counter, total_aligns)
                end = False
                #Check to see if previous gap was negative
                if negative:
                    print >> planta_out_f, '\t\t\tPrevious gap was negative, modifying coordinates to ignore overlap'
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
                        print >> planta_out_f, '\t\t\t\tMoving contig start from %d to %d.' % (sorted_aligns[0].s2, sorted_aligns[0].s2 + snip_left)
                        sorted_aligns[0].s2 += snip_left
                    else:
                        print >> planta_out_f, '\t\t\t\tMoving contig start from %d to %d.' % (sorted_aligns[0].s2, sorted_aligns[0].s2 - snip_left)
                        sorted_aligns[0].s2 -= snip_left
                    negative = False

                #Pull top alignment
                current = sorted_aligns[0]
                sorted_aligns = sorted_aligns[1:]
                #print >>plantafile_out, '\t\t\tAlign %d: %s' % (counter, current)  #(self, s1, e1, s2, e2, len1, len2, idy, ref, contig):
                print >>planta_out_f, '\t\t\tAlign %d: %s' % (counter, '%d %d %s %d %d' % (current.s1, current.e1, current.contig, current.s2, current.e2))

                #Check if:
                # A) We have no more aligns to this reference
                # B) The current alignment extends to or past the end of the region
                # C) The next alignment starts after the end of the region

                if not sorted_aligns or current.e1 >= region[1] or sorted_aligns[0].s1 > region[1]:
                    #Check if last alignment ends before the regions does (gap at end of the region)
                    if current.e1 >= region[1]:
                        #print "Ends inside current alignment.\n";
                        print >> planta_out_f, '\t\t\tEND in current alignment.  Modifying %d to %d.' % (current.e1, region[1])
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
                        print >> planta_out_f, '\t\t\tEND in gap: %d to %d (%d bp)' % (current.e1, region[1], size)

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
                            print >> planta_out_f, '\t\t\t\tThe next alignment (%d %d %s %d %d) is redundant. Skipping.' \
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
                            print >> planta_out_f, '\t\t\t\tGap between this and next alignment: %d to %d (%d bp)' % (current.e1, next.s1, size)
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
                            print >>planta_out_f, '\t\t\t\tNegative gap (overlap) between this and next alignment: %d to %d (%d bp)' % (current.e1, next.s1, size)

                            #Mark this alignment as negative so overlap region can be ignored
                            negative = True
                        print >> planta_out_f, '\t\t\t\tNext Alignment: %d %d %s %d %d' % (next.s1, next.e1, next.contig, next.s2, next.e2)

                #Initiate location of SNP on assembly to be first or last base of contig alignment
                contig_estimate = current.s2
                enable_SNPs_output = False
                if enable_SNPs_output:
                    print >> planta_out_f, '\t\t\t\tContig start coord: %d' % contig_estimate

                #Assess each reference base of the current alignment
                for i in xrange(current.s1, current.e1 + 1):
                    #Mark as covered
                    region_covered += 1

                    if current.s2 < current.e2:
                        pos_strand = True
                    else:
                        pos_strand = False

                    #If there is a misassembly, increment count and contig length
                    #if (exists $ref_features{$ref}[$i] && $ref_features{$ref}[$i] eq "M") {
                    #	$region_misassemblies++;
                    #	$misassembled_contigs{$current[2]} = length($assembly{$current[2]});
                    #}

                    #If there is a SNP, and no alternative alignments over this base, record SNPs
                    if (ref in snps) and (current.contig in snps[ref]) and (i in snps[ref][current.contig]):
                        cur_snps = snps[ref][current.contig][i]
                        # sorting by pos in contig
                        if pos_strand:
                            cur_snps = sorted(cur_snps, key=lambda x: x.ctg_pos)
                        else: # for reverse complement
                            cur_snps = sorted(cur_snps, key=lambda x: x.ctg_pos, reverse=True)

                        for cur_snp in cur_snps:
                            if enable_SNPs_output:
                                print >> planta_out_f, '\t\t\t\tSNP: %s, reference coord: %d, contig coord: %d, estimated contig coord: %d' % \
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
                                    print >> planta_out_f, '\t\t\t\t\tERROR: SNP position in contig was off by %d bp! (%d vs %d)' \
                                             % (abs(contig_estimate - cur_snp.ctg_pos), contig_estimate, cur_snp.ctg_pos)
                                continue

                            print >> used_snps_file, '%s\t%s\t%d\t%s\t%s\t%d' % (ref, current.contig, cur_snp.ref_pos,
                                                                                 cur_snp.ref_nucl, cur_snp.ctg_nucl, cur_snp.ctg_pos)

                            #If SNP is an insertion, record
                            if snp == 'I':
                                region_insertion += 1
                                if pos_strand: contig_estimate += 1
                                else: contig_estimate -= 1
                            #If SNP is a deletion, record
                            if snp == 'D':
                                region_deletion += 1
                                if pos_strand: contig_estimate -= 1
                                else: contig_estimate += 1
                            #If SNP is a mismatch, record
                            if snp == 'S':
                                region_snp += 1

                            if cur_snp.type == 'D' or cur_snp.type == 'I':
                                if prev_snp and (prev_snp.ref == cur_snp.ref) and (prev_snp.ctg == cur_snp.ctg) and \
                                    ((cur_snp.type == 'D' and (prev_snp.ref_pos == cur_snp.ref_pos - 1) and (prev_snp.ctg_pos == cur_snp.ctg_pos)) or
                                     (cur_snp.type == 'I' and ((pos_strand and (prev_snp.ctg_pos == cur_snp.ctg_pos - 1)) or
                                         (not pos_strand and (prev_snp.ctg_pos == cur_snp.ctg_pos + 1))) and (prev_snp.ref_pos == cur_snp.ref_pos))):
                                    cur_indel += 1
                                else:
                                    if cur_indel:
                                        indels_list.append(cur_indel)
                                    cur_indel = 1
                                prev_snp = cur_snp

                    if pos_strand: contig_estimate += 1
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

                print >> planta_out_f

    ##### getting results from Plantagora's algorithm
    SNPs = region_snp
    indels = region_insertion + region_deletion
    total_aligned_bases = region_covered
    print >> planta_out_f, 'Analysis is finished!'
    print >> planta_out_f, 'Founded SNPs were written into', used_snps_fpath
    print >> planta_out_f, '\nResults:'

    print >> planta_out_f, '\tLocal Misassemblies: %d' % region_misassemblies.count(Misassembly.LOCAL)
    print >> planta_out_f, '\tMisassemblies: %d' % (len(region_misassemblies) - region_misassemblies.count(Misassembly.LOCAL))
    print >> planta_out_f, '\t\tRelocations: %d' % region_misassemblies.count(Misassembly.RELOCATION)
    print >> planta_out_f, '\t\tTranslocations: %d' % region_misassemblies.count(Misassembly.TRANSLOCATION)
    print >> planta_out_f, '\t\tInversions: %d' % region_misassemblies.count(Misassembly.INVERSION)
    print >> planta_out_f, '\tMisassembled Contigs: %d' % len(misassembled_contigs)
    misassembled_bases = sum(misassembled_contigs.itervalues())
    print >> planta_out_f, '\tMisassembled Contig Bases: %d' % misassembled_bases
    print >> planta_out_f, '\tMisassmblies Inter-Contig Overlap: %d' % misassembly_internal_overlap
    print >> planta_out_f, 'Uncovered Regions: %d (%d)' % (uncovered_regions, uncovered_region_bases)
    print >> planta_out_f, 'Unaligned Contigs: %d + %d part' % (unaligned, partially_unaligned)
    print >> planta_out_f, 'Partially Unaligned Contigs with Misassemblies: %d' % partially_unaligned_with_misassembly
    print >> planta_out_f, 'Unaligned Contig Bases: %d' % (fully_unaligned_bases + partially_unaligned_bases)

    print >> planta_out_f, ''
    print >> planta_out_f, 'Ambiguously Mapped Contigs: %d' % ambiguous_contigs
    if qconfig.ambiguity_usage == "all":
        print >> planta_out_f, 'Extra Bases in Ambiguously Mapped Contigs: %d' % ambiguous_contigs_extra_bases
        print >> planta_out_f, 'Note that --allow-ambiguity option was set to "all" and each contig was used several times.'
    else:
        print >> planta_out_f, 'Total Bases in Ambiguously Mapped Contigs: %d' % (-ambiguous_contigs_extra_bases)
        if qconfig.ambiguity_usage == "none":
            print >> planta_out_f, 'Note that --allow-ambiguity option was set to "none" and these contigs were skipped.'
        else:
            print >> planta_out_f, 'Note that --allow-ambiguity option was set to "one" and only first alignment per each contig was used.'
            ambiguous_contigs_extra_bases = 0 # this variable is used in Duplication ratio but we don't need it in this case

    #print >> plantafile_out, 'Mismatches: %d' % SNPs
    #print >> plantafile_out, 'Single Nucleotide Indels: %d' % indels

    print >> planta_out_f, ''
    print >> planta_out_f, '\tCovered Bases: %d' % region_covered
    #print >> plantafile_out, '\tAmbiguous Bases (e.g. N\'s): %d' % region_ambig
    print >> planta_out_f, ''
    print >> planta_out_f, '\tSNPs: %d' % region_snp
    print >> planta_out_f, '\tInsertions: %d' % region_insertion
    print >> planta_out_f, '\tDeletions: %d' % region_deletion
    #print >> plantafile_out, '\tList of indels lengths:', indels_list
    print >> planta_out_f, ''
    print >> planta_out_f, '\tPositive Gaps: %d' % len(gaps)
    internal = 0
    external = 0
    summ = 0
    for gap in gaps:
        if gap[1] == gap[2]:
            internal += 1
        else:
            external += 1
            summ += gap[0]
    print >> planta_out_f, '\t\tInternal Gaps: % d' % internal
    print >> planta_out_f, '\t\tExternal Gaps: % d' % external
    print >> planta_out_f, '\t\tExternal Gap Total: % d' % summ
    if external:
        avg = summ * 1.0 / external
    else:
        avg = 0.0
    print >> planta_out_f, '\t\tExternal Gap Average: %.0f' % avg

    print >> planta_out_f, '\tNegative Gaps: %d' % len(neg_gaps)
    internal = 0
    external = 0
    summ = 0
    for gap in neg_gaps:
        if gap[1] == gap[2]:
            internal += 1
        else:
            external += 1
            summ += gap[0]
    print >> planta_out_f, '\t\tInternal Overlaps: % d' % internal
    print >> planta_out_f, '\t\tExternal Overlaps: % d' % external
    print >> planta_out_f, '\t\tExternal Overlaps Total: % d' % summ
    if external:
        avg = summ * 1.0 / external
    else:
        avg = 0.0
    print >> planta_out_f, '\t\tExternal Overlaps Average: %.0f' % avg

    redundant = list(set(redundant))
    print >> planta_out_f, '\tContigs with Redundant Alignments: %d (%d)' % (len(redundant), total_redundant)

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
    fasta = [(name, seq) for name, seq in fastaparser.read_fasta(contigs_fpath)
             if name in misassembled_contigs.keys()]
    fastaparser.write_fasta(
        os.path.join(output_dirpath,
                     qutils.name_from_fpath(contigs_fpath) + '.mis_contigs.fa'),
        fasta)

    alignment_tsv_fpath = os.path.join(output_dirpath, "alignments_" + assembly_name + '.tsv')
    logger.debug('  ' + qutils.index_to_str(index) + 'Alignments: ' + qutils.relpath(alignment_tsv_fpath))
    alignment_tsv_f = open(alignment_tsv_fpath, 'w')
    for ref_name, aligns in ref_aligns.iteritems():
        alignment_tsv_f.write(ref_name)
        for align in aligns:
            alignment_tsv_f.write('\t' + align.contig)
        alignment_tsv_f.write('\n')
    alignment_tsv_f.close()

    planta_out_f.close()
    planta_err_f.close()
    used_snps_file.close()
    logger.info('  ' + qutils.index_to_str(index) + 'Analysis is finished.')
    logger.debug('')
    if nothing_aligned:
        return NucmerStatus.NOT_ALIGNED, result, aligned_lengths
    else:
        return NucmerStatus.OK, result, aligned_lengths


def plantakolya_process(cyclic, nucmer_output_dirpath, contigs_fpath, i, output_dirpath, ref_fpath):
    assembly_name = qutils.name_from_fpath(contigs_fpath)

    nucmer_fname = os.path.join(nucmer_output_dirpath, assembly_name)
    nucmer_is_ok, result, aligned_lengths = plantakolya(
        cyclic, i, contigs_fpath, nucmer_fname, output_dirpath, ref_fpath)

    clear_files(contigs_fpath, nucmer_fname)
    return nucmer_is_ok, result, aligned_lengths


def all_required_binaries_exist(mummer_dirpath):
    for required_binary in required_binaries:
        if not os.path.isfile(os.path.join(mummer_dirpath, required_binary)):
            return False
    return True


def do(reference, contigs_fpaths, cyclic, output_dir):
    if not os.path.isdir(output_dir):
        os.mkdir(output_dir)

    logger.print_timestamp()
    logger.info('Running Contig analyzer...')

    if not all_required_binaries_exist(mummer_dirpath):
        # making
        logger.info('Compiling MUMmer (details are in ' + os.path.join(mummer_dirpath, 'make.log') + " and make.err)")
        return_code = qutils.call_subprocess(
            ['make', '-C', mummer_dirpath],
            stdout=open(os.path.join(mummer_dirpath, 'make.log'), 'w'),
            stderr=open(os.path.join(mummer_dirpath, 'make.err'), 'w'),)

        if return_code != 0 or not all_required_binaries_exist(mummer_dirpath):
            logger.error("Failed to compile MUMmer (" + mummer_dirpath + ")! "
                         "Try to compile it manually. " + ("You can restart Quast with the --debug flag "
                         "to see the command line." if not qconfig.debug else ""))
            logger.info('Failed aligning the contigs for all the assemblies. Only basic stats are going to be evaluated.')
            return dict(zip(contigs_fpaths, [NucmerStatus.FAILED] * len(contigs_fpaths))), None

    nucmer_output_dir = os.path.join(output_dir, 'nucmer_output')
    if not os.path.isdir(nucmer_output_dir):
        os.mkdir(nucmer_output_dir)

    n_jobs = min(len(contigs_fpaths), qconfig.max_threads)
    from joblib import Parallel, delayed
    statuses_results_lengths_tuples = Parallel(n_jobs=n_jobs)(delayed(plantakolya_process)(
        cyclic, nucmer_output_dir, fname, i, output_dir, reference)
             for i, fname in enumerate(contigs_fpaths))

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

        unaligned_ctgs = report.get_field(reporting.Fields.CONTIGS)
        unaligned_length = report.get_field(reporting.Fields.TOTALLEN)
        report.add_field(reporting.Fields.UNALIGNED, '%d + %d part' % (unaligned_ctgs, 0))
        report.add_field(reporting.Fields.UNALIGNEDBASES, unaligned_length)

        report.add_field(reporting.Fields.UNALIGNED_FULL_CNTGS, unaligned_ctgs)
        report.add_field(reporting.Fields.UNALIGNED_FULL_LENGTH, unaligned_length)

    for index, fname in enumerate(contigs_fpaths):
        if statuses[index] == NucmerStatus.OK:
            save_result(results[index])
        elif statuses[index] == NucmerStatus.NOT_ALIGNED:
            save_result_for_unaligned(results[index])

    nucmer_statuses = dict(zip(contigs_fpaths, statuses))
    aligned_lengths_per_fpath = dict(zip(contigs_fpaths, aligned_lengths))

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
        logger.info('Done.')
    if oks < all and problems < all:
        logger.info('Done for ' + str(all - problems) + 'out of ' + str(all) + '. For the rest, only basic stats are going to be evaluated.')
    if problems == all:
        logger.info('Failed aligning the contigs for all the assemblies. Only basic stats are going to be evaluated.')

#    if NucmerStatus.FAILED in nucmer_statuses.values():
#        log.info('  ' + str(failed) + 'file' + (' ' if failed == 1 else 's ') + 'failed to align to the reference. Only basic stats have been evaluated.')
#    if NucmerStatus.NOT_ALIGNED in nucmer_statuses.values():
#        log.info('  ' + str(not_aligned) + ' file' + (' was' if not_aligned == 1 else 's were') + ' not aligned to the reference. Only basic stats have been evaluated.')
#    if problems == all:
#        log.info('  Nucmer failed.')

    return nucmer_statuses, aligned_lengths_per_fpath
