import shutil
import copy
from libs import reporting, qconfig, qutils, contigs_analyzer
from qutils import is_non_empty_file

from libs.log import get_logger

logger = get_logger(qconfig.LOGGER_DEFAULT_NAME)
import shlex
import os

bowtie_dirpath = os.path.join(qconfig.LIBS_LOCATION, 'bowtie2')
samtools_dirpath = os.path.join(qconfig.LIBS_LOCATION, 'samtools')
manta_dirpath = os.path.join(qconfig.LIBS_LOCATION, 'manta')
manta_bin_dirpath = os.path.join(qconfig.LIBS_LOCATION, 'manta', 'build/bin')


class Mapping(object):
    MIN_MAP_QUALITY = 50  # for distiguishing "good" reads and "bad" ones

    def __init__(self, fields):
        self.ref, self.start, self.mapq, self.ref_next, self.len = \
            fields[2], int(fields[3]), int(fields[4]), fields[6], len(fields[9])
        self.end = self.start + self.len - 1  # actually not always true because of indels

    @staticmethod
    def parse(line):
        if line.startswith('@'):  # comment
            return None
        if line.split('\t') < 11:  # not valid line
            return None
        mapping = Mapping(line.split('\t'))
        return mapping


class QuastDeletion(object):
    ''' describes situtations: GGGGBBBBBNNNNNNNNNNNNBBBBBBGGGGGG, where
    G -- "good" read (high mapping quality)
    B -- "bad" read (low mapping quality)
    N -- no mapped reads
    size of Ns fragment -- "deletion" (not less than MIN_GAP)
    size of Bs fragment -- confidence interval (not more than MAX_CONFIDENCE_INTERVAL,
        fixing last/first G position otherwise)
    '''

    MAX_CONFIDENCE_INTERVAL = 300
    MIN_GAP = 1000

    def __init__(self, ref, prev_good=None, prev_bad=None, next_bad=None, next_good=None, next_bad_end=None):
        self.ref, self.prev_good, self.prev_bad, self.next_bad, self.next_good, self.next_bad_end = \
            ref, prev_good, prev_bad, next_bad, next_good, next_bad_end
        self.id = 'QuastDEL'

    def is_valid(self):
        return self.prev_good is not None and self.prev_bad is not None and \
               self.next_bad is not None and self.next_good is not None and \
               (self.next_bad - self.prev_bad > QuastDeletion.MIN_GAP)

    def set_prev_good(self, mapping):
        self.prev_good = mapping.end
        self.prev_bad = self.prev_good  # prev_bad cannot be earlier than prev_good!
        return self  # to use this function like "deletion = QuastDeletion(ref).set_prev_good(coord)"

    def set_prev_bad(self, mapping=None, position=None):
        self.prev_bad = position if position else mapping.end
        if self.prev_good is None or self.prev_good + QuastDeletion.MAX_CONFIDENCE_INTERVAL < self.prev_bad:
            self.prev_good = max(1, self.prev_bad - QuastDeletion.MAX_CONFIDENCE_INTERVAL)
        return self  # to use this function like "deletion = QuastDeletion(ref).set_prev_bad(coord)"

    def set_next_good(self, mapping=None, position=None):
        self.next_good = position if position else mapping.start
        if self.next_bad is None:
            self.next_bad = self.next_good
        elif self.next_good - QuastDeletion.MAX_CONFIDENCE_INTERVAL > self.next_bad:
            self.next_good = self.next_bad + QuastDeletion.MAX_CONFIDENCE_INTERVAL

    def set_next_bad(self, mapping):
        self.next_bad = mapping.start
        self.next_bad_end = mapping.end
        self.next_good = self.next_bad  # next_good is always None at this moment (deletion is complete already otherwise)

    def set_next_bad_end(self, mapping):
        self.next_bad_end = mapping.end

    def __str__(self):
        return '\t'.join(map(str, [self.ref, self.prev_good, self.prev_bad,
                          self.ref, self.next_bad, self.next_good,
                          self.id]) + ['-'] * 4)


def process_one_ref(cur_ref_fpath, output_dirpath, err_path, bed_fpath=None):
    ref = qutils.name_from_fpath(cur_ref_fpath)
    ref_sam_fpath = os.path.join(output_dirpath, ref + '.sam')
    ref_bam_fpath = os.path.join(output_dirpath, ref + '.bam')
    ref_bamsorted_fpath = os.path.join(output_dirpath, ref + '.sorted')
    ref_bed_fpath = bed_fpath if bed_fpath else os.path.join(output_dirpath, ref + '.bed')
    if os.path.getsize(ref_sam_fpath) < 1024 * 1024:  # TODO: make it better (small files will cause Manta crush -- "not enough reads...")
        return None
    if not os.path.exists(ref_bamsorted_fpath + '.bam'):
        qutils.call_subprocess([samtools_fpath('samtools'), 'view', '-bS', ref_sam_fpath], stdout=open(ref_bam_fpath, 'w'),
                               stderr=open(err_path, 'a'))
        qutils.call_subprocess([samtools_fpath('samtools'), 'sort', ref_bam_fpath, ref_bamsorted_fpath],
                               stderr=open(err_path, 'a'))
    qutils.call_subprocess([samtools_fpath('samtools'), 'index', ref_bamsorted_fpath + '.bam'], stderr=open(err_path, 'a'))
    qutils.call_subprocess([samtools_fpath('samtools'), 'faidx', cur_ref_fpath], stderr=open(err_path, 'a'))
    vcfoutput_dirpath = os.path.join(output_dirpath, ref + '_manta')
    if os.path.exists(vcfoutput_dirpath):
        shutil.rmtree(vcfoutput_dirpath, ignore_errors=True)
    os.makedirs(vcfoutput_dirpath)
    qutils.call_subprocess([os.path.join(manta_bin_dirpath, 'configManta.py'), '--normalBam', ref_bamsorted_fpath + '.bam',
                            '--referenceFasta', cur_ref_fpath, '--runDir', vcfoutput_dirpath],
                           stdout=open(err_path, 'a'), stderr=open(err_path, 'a'))
    if not os.path.exists(os.path.join(vcfoutput_dirpath, 'runWorkflow.py')):
        return None
    qutils.call_subprocess([os.path.join(vcfoutput_dirpath, 'runWorkflow.py'), '-m', 'local', '-j', str(qconfig.max_threads)],
                           stderr=open(err_path, 'a'))
    temp_fpath = os.path.join(vcfoutput_dirpath, 'results/variants/diploidSV.vcf.gz')
    unpacked_fpath = temp_fpath + '.unpacked'
    cmd = 'gunzip -c %s' % temp_fpath
    qutils.call_subprocess(shlex.split(cmd), stdout=open(unpacked_fpath, 'w'), stderr=open(err_path, 'a'))
    from manta import vcfToBedpe
    vcfToBedpe.vcfToBedpe(open(unpacked_fpath), open(ref_bed_fpath, 'w'))
    return ref_bed_fpath


def create_bed_files(main_ref_fpath, meta_ref_fpaths, ref_labels, deletions, output_dirpath, bed_fpath, err_path):
    logger.info('  Searching structural variations...')
    if meta_ref_fpaths:
        from joblib import Parallel, delayed
        n_jobs = min(len(meta_ref_fpaths), qconfig.max_threads)
        bed_fpaths = Parallel(n_jobs=n_jobs)(delayed(process_one_ref)(cur_ref_fpath, output_dirpath, err_path) for cur_ref_fpath in meta_ref_fpaths)
        bed_fpaths = [f for f in bed_fpaths if f is not None]
        qutils.call_subprocess(['cat'] + bed_fpaths, stdout=open(bed_fpath, 'w'), stderr=open(err_path, 'a'))
    else:
        process_one_ref(main_ref_fpath, output_dirpath, err_path, bed_fpath=bed_fpath)
    bed_file = open(bed_fpath, 'a')
    for deletion in deletions:
        bed_file.write(str(deletion) + '\n')
    bed_file.close()
    return bed_fpath


def run_processing_reads(main_ref_fpath, meta_ref_fpaths, ref_labels, reads_fpaths, output_dirpath, res_path, log_path, err_path):
    ref_name = qutils.name_from_fpath(main_ref_fpath)
    sam_fpath = os.path.join(output_dirpath, ref_name + '.sam')
    bam_fpath = os.path.join(output_dirpath, ref_name + '.bam')
    bam_sorted_fpath = os.path.join(output_dirpath, ref_name + '.sorted')
    sam_sorted_fpath = os.path.join(output_dirpath, ref_name + '.sorted.sam')
    bed_fpath = os.path.join(res_path, ref_name + '.bed')

    if is_non_empty_file(bed_fpath):
        logger.info('  Using existing BED-file: ' + bed_fpath)
        return bed_fpath

    logger.info('  ' + 'Pre-processing for searching structural variations...')
    logger.info('  ' + 'Logging to %s...' % err_path)
    if is_non_empty_file(sam_fpath):
        logger.info('  Using existing SAM-file: ' + sam_fpath)
    else:
        logger.info('  Running Bowtie2...')
        abs_reads_fpaths = []  # use absolute paths because we will change workdir
        for reads_fpath in reads_fpaths:
            abs_reads_fpaths.append(os.path.abspath(reads_fpath))

        prev_dir = os.getcwd()
        os.chdir(output_dirpath)
        cmd = [bin_fpath('bowtie2-build'), main_ref_fpath, ref_name]
        qutils.call_subprocess(cmd, stdout=open(log_path, 'a'), stderr=open(err_path, 'a'))

        cmd = bin_fpath('bowtie2') + ' -x ' + ref_name + ' -1 ' + abs_reads_fpaths[0] + ' -2 ' + abs_reads_fpaths[1] + ' -S ' + \
              sam_fpath + ' --no-unal -a -p %s' % str(qconfig.max_threads)
        qutils.call_subprocess(shlex.split(cmd), stdout=open(log_path, 'a'), stderr=open(err_path, 'a'))
        logger.info('  Done.')
        os.chdir(prev_dir)
        if not os.path.exists(sam_fpath) or os.path.getsize(sam_fpath) == 0:
            logger.error('  Failed running Bowtie2 for the reference. See ' + log_path + ' for information.')
            logger.info('  Failed searching structural variations.')
            return None
    logger.info('  Sorting SAM-file...')
    if is_non_empty_file(sam_sorted_fpath):
        logger.info('  Using existing sorted SAM-file: ' + sam_fpath)
    else:
        qutils.call_subprocess([samtools_fpath('samtools'), 'view', '-@', str(qconfig.max_threads), '-bS', sam_fpath], stdout=open(bam_fpath, 'w'),
                               stderr=open(err_path, 'a'))
        qutils.call_subprocess([samtools_fpath('samtools'), 'sort', '-@', str(qconfig.max_threads), bam_fpath, bam_sorted_fpath],
                               stderr=open(err_path, 'a'))
        qutils.call_subprocess([samtools_fpath('samtools'), 'view', '-@', str(qconfig.max_threads), bam_sorted_fpath + '.bam'], stdout=open(sam_sorted_fpath, 'w'),
                               stderr=open(err_path, 'a'))
    if meta_ref_fpaths:
        logger.info('  Splitting SAM-file by references...')
    headers = []
    seq_name_length = {}
    with open(sam_fpath) as sam_file:
        for line in sam_file:
            if not line.startswith('@'):
                break
            if line.startswith('@SQ') and 'SN:' in line and 'LN:' in line:
                seq_name = line.split('\tSN:')[1].split('\t')[0]
                seq_length = int(line.split('\tLN:')[1].split('\t')[0])
                seq_name_length[seq_name] = seq_length
            headers.append(line.strip())
    if meta_ref_fpaths:
        ref_files = {}
        for cur_ref_fpath in meta_ref_fpaths:
            ref = qutils.name_from_fpath(cur_ref_fpath)
            new_ref_sam_fpath = os.path.join(output_dirpath, ref + '.sam')
            if is_non_empty_file(new_ref_sam_fpath):
                logger.info('  Using existing split SAM-file for %s: %s' % (ref, new_ref_sam_fpath))
                ref_files[ref] = None
            else:
                new_ref_sam_file = open(new_ref_sam_fpath, 'w')
                new_ref_sam_file.write(headers[0] + '\n')
                chrs = []
                for h in (h for h in headers if h.startswith('@SQ') and 'SN:' in h):
                    seq_name = h.split('\tSN:')[1].split('\t')[0]
                    if seq_name in ref_labels and ref_labels[seq_name] == ref:
                        new_ref_sam_file.write(h + '\n')
                        chrs.append(seq_name)
                new_ref_sam_file.write(headers[-1] + '\n')
                ref_files[ref] = new_ref_sam_file
    deletions = []
    logger.info('  Looking for trivial deletions (long zero-covered fragments)...')
    with open(sam_sorted_fpath) as sam_file:
        cur_deletion = None
        for line in sam_file:
            mapping = Mapping.parse(line)
            if mapping:
                # common case: continue current deletion (potential) on the same reference
                if cur_deletion and cur_deletion.ref == mapping.ref:
                    if cur_deletion.next_bad is None:  # previous mapping was in region BEFORE 0-covered fragment
                        # just passed 0-covered fragment
                        if mapping.start - cur_deletion.prev_bad > QuastDeletion.MIN_GAP:
                            cur_deletion.set_next_bad(mapping)
                            if mapping.mapq >= Mapping.MIN_MAP_QUALITY:
                                cur_deletion.set_next_good(mapping)
                                if cur_deletion.is_valid():
                                    deletions.append(cur_deletion)
                                cur_deletion = QuastDeletion(mapping.ref).set_prev_good(mapping)
                        # continue region BEFORE 0-covered fragment
                        elif mapping.mapq >= Mapping.MIN_MAP_QUALITY:
                            cur_deletion.set_prev_good(mapping)
                        else:
                            cur_deletion.set_prev_bad(mapping)
                    else:  # previous mapping was in region AFTER 0-covered fragment
                        # just passed another 0-cov fragment between end of cur_deletion BBB region and this mapping
                        if mapping.start - cur_deletion.next_bad_end > QuastDeletion.MIN_GAP:
                            if cur_deletion.is_valid():   # add previous fragment's deletion if needed
                                deletions.append(cur_deletion)
                            cur_deletion = QuastDeletion(mapping.ref).set_prev_bad(position=cur_deletion.next_bad)
                        # continue region AFTER 0-covered fragment
                        elif mapping.mapq >= Mapping.MIN_MAP_QUALITY:
                            cur_deletion.set_next_good(mapping)
                            if cur_deletion.is_valid():
                                deletions.append(cur_deletion)
                            cur_deletion = QuastDeletion(mapping.ref).set_prev_good(mapping)
                        else:
                            cur_deletion.set_next_bad_end(mapping)
                # special case: just started or just switched to the next reference
                else:
                    if cur_deletion and cur_deletion.ref in seq_name_length:  # switched to the next ref
                        cur_deletion.set_next_good(position=seq_name_length[cur_deletion.ref])
                        if cur_deletion.is_valid():
                            deletions.append(cur_deletion)
                    cur_deletion = QuastDeletion(mapping.ref).set_prev_good(mapping)

                if meta_ref_fpaths:
                    cur_ref = ref_labels[mapping.ref]
                    if mapping.ref_next.strip() == '=' or cur_ref == ref_labels[mapping.ref_next]:
                        if ref_files[cur_ref] is not None:
                            ref_files[cur_ref].write(line)
        if cur_deletion and cur_deletion.ref in seq_name_length:  # switched to the next ref
            cur_deletion.set_next_good(position=seq_name_length[cur_deletion.ref])
            if cur_deletion.is_valid():
                deletions.append(cur_deletion)
    logger.info('  Looking for trivial deletions: %d found' % len(deletions))
    if meta_ref_fpaths:
        for ref_handler in ref_files.values():
            if ref_handler is not None:
                ref_handler.close()
    create_bed_files(main_ref_fpath, meta_ref_fpaths, ref_labels, deletions, output_dirpath, bed_fpath, err_path)

    if os.path.exists(bed_fpath):
        logger.info('  Structural variations saved to ' + bed_fpath)
        return bed_fpath
    else:
        logger.info('  Failed searching structural variations.')
        return None


def bin_fpath(fname):
    return os.path.join(bowtie_dirpath, fname)


def samtools_fpath(fname):
    return os.path.join(samtools_dirpath, fname)


def all_required_binaries_exist(bin_dirpath, binary):
    if not os.path.isfile(os.path.join(bin_dirpath, binary)):
        return False
    return True


def do(ref_fpath, contigs_fpaths, reads_fpaths, meta_ref_fpaths, output_dir, interleaved=False):
    logger.print_timestamp()
    logger.info('Running Structural Variants caller...')

    if not os.path.isdir(output_dir):
        os.makedirs(output_dir)

    if not all_required_binaries_exist(bowtie_dirpath, 'bowtie2-align-l'):
        # making
        logger.info('Compiling Bowtie2 (details are in ' + os.path.join(bowtie_dirpath, 'make.log') + ' and make.err)')
        return_code = qutils.call_subprocess(
            ['make', '-C', bowtie_dirpath],
            stdout=open(os.path.join(bowtie_dirpath, 'make.log'), 'w'),
            stderr=open(os.path.join(bowtie_dirpath, 'make.err'), 'w'), )

        if return_code != 0 or not all_required_binaries_exist(bowtie_dirpath, 'bowtie2-align-l'):
            logger.error('Failed to compile Bowtie2 (' + bowtie_dirpath + ')! '
                                                                   'Try to compile it manually. ' + (
                             'You can restart QUAST with the --debug flag '
                             'to see the command line.' if not qconfig.debug else ''))
            logger.info('Failed aligning the reads.')
            return None

    if not all_required_binaries_exist(samtools_dirpath, 'samtools'):
        # making
        logger.info(
            'Compiling SAMtools (details are in ' + os.path.join(samtools_dirpath, 'make.log') + ' and make.err)')
        return_code = qutils.call_subprocess(
            ['make', '-C', samtools_dirpath],
            stdout=open(os.path.join(samtools_dirpath, 'make.log'), 'w'),
            stderr=open(os.path.join(samtools_dirpath, 'make.err'), 'w'), )

        if return_code != 0 or not all_required_binaries_exist(samtools_dirpath, 'samtools'):
            logger.error('Failed to compile SAMtools (' + samtools_dirpath + ')! '
                                                                             'Try to compile it manually. ' + (
                             'You can restart QUAST with the --debug flag '
                             'to see the command line.' if not qconfig.debug else ''))
            logger.info('Failed aligning the reads.')
            return None

    if not all_required_binaries_exist(manta_bin_dirpath, 'configManta.py'):
        # making
        logger.info('Compiling Manta (details are in ' + os.path.join(manta_dirpath, 'make.log') + ' and make.err)')
        prev_dir = os.getcwd()
        if not os.path.exists(os.path.join(manta_dirpath, 'build')):
            os.mkdir(os.path.join(manta_dirpath, 'build'))
        os.chdir(os.path.join(manta_dirpath, 'build'))
        return_code = qutils.call_subprocess(
            [os.path.join(manta_dirpath, 'source', 'src', 'configure'), '--prefix=' + os.path.join(manta_dirpath, 'build'),
             '--jobs=' + str(qconfig.max_threads)],
            stdout=open(os.path.join(manta_dirpath, 'make.log'), 'w'),
            stderr=open(os.path.join(manta_dirpath, 'make.err'), 'w'), )
        if return_code == 0:
            return_code = qutils.call_subprocess(
                ['make', '-j' + str(qconfig.max_threads)],
                stdout=open(os.path.join(manta_dirpath, 'make.log'), 'a'),
                stderr=open(os.path.join(manta_dirpath, 'make.err'), 'a'), )
            if return_code == 0:
                return_code = qutils.call_subprocess(
                ['make', 'install'],
                stdout=open(os.path.join(manta_dirpath, 'make.log'), 'a'),
                stderr=open(os.path.join(manta_dirpath, 'make.err'), 'a'), )
        os.chdir(prev_dir)
        if return_code != 0 or not all_required_binaries_exist(manta_bin_dirpath, 'configManta.py'):
            logger.error('Failed to compile Manta (' + manta_dirpath + ')! '
                                                                   'Try to compile it manually. ' + (
                             'You can restart QUAST with the --debug flag '
                             'to see the command line.' if not qconfig.debug else ''))
            logger.info('Failed aligning the reads.')
            return None

    temp_output_dir = os.path.join(output_dir, 'temp_output')

    if not os.path.isdir(temp_output_dir):
        os.mkdir(temp_output_dir)

    log_path = os.path.join(temp_output_dir, 'align_reads.log')  # TODO: don't clear these logs!
    err_path = os.path.join(temp_output_dir, 'align_reads.err')
    logger.info('  ' + 'Logging to files %s and %s...' % (log_path, err_path))
    bed_fpath = run_processing_reads(ref_fpath, meta_ref_fpaths, contigs_analyzer.ref_labels_by_chromosomes, reads_fpaths, temp_output_dir, output_dir, log_path, err_path)
    if not qconfig.debug:
        shutil.rmtree(temp_output_dir, ignore_errors=True)

    logger.info('Done.')
    return bed_fpath
