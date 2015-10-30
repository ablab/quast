import shutil
import copy
from libs import reporting, qconfig, qutils, fastaparser, contigs_analyzer

from libs.log import get_logger

logger = get_logger(qconfig.LOGGER_DEFAULT_NAME)
import shlex
import os

bowtie_dirpath = os.path.join(qconfig.LIBS_LOCATION, 'bowtie2')
samtools_dirpath = os.path.join(qconfig.LIBS_LOCATION, 'samtools')
manta_dirpath = os.path.join(qconfig.LIBS_LOCATION, 'manta')
manta_bin_dirpath = os.path.join(qconfig.LIBS_LOCATION, 'manta', 'build/bin')


class QuastDeletion(object):
    CONFIDENCE_INTERVAL = 0
    MERGE_GAP = 500  # for merging neighbouring deletions into superdeletions

    def __init__(self, ref, start, end):
        self.ref, self.start, self.end = ref, start, end
        self.id = 'QuastDEL'

    def __str__(self):
        return '\t'.join([self.ref, self.start - self.CONFIDENCE_INTERVAL, self.start + self.CONFIDENCE_INTERVAL,
                          self.ref, self.end - self.CONFIDENCE_INTERVAL, self.end + self.CONFIDENCE_INTERVAL,
                          self.id] + ['-'] * 4)


def process_one_ref(cur_ref_fpath, output_dirpath, err_path):
    ref = qutils.name_from_fpath(cur_ref_fpath)
    ref_sam_fpath = os.path.join(output_dirpath, ref + '.sam')
    ref_bam_fpath = os.path.join(output_dirpath, ref + '.bam')
    ref_bamsorted_fpath = os.path.join(output_dirpath, ref + '.sorted')
    if os.path.getsize(ref_sam_fpath) < 1024 * 1024:
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
    ref_bed_fpath = os.path.join(output_dirpath, ref + '.bed')
    vcfToBedpe.vcfToBedpe(open(unpacked_fpath), open(ref_bed_fpath, 'w'))
    return ref_bed_fpath


def create_bed_files(main_ref_fpath, meta_ref_fpaths, ref_labels, deletions, output_dirpath, bed_fpath, err_path):
    logger.info('  Searching structural variations...')
    if meta_ref_fpaths:
        from joblib import Parallel, delayed
        n_jobs = min(len(meta_ref_fpaths), qconfig.max_threads)
        bed_fpaths = Parallel(n_jobs=n_jobs)(delayed(process_one_ref)(cur_ref_fpath, output_dirpath, err_path) for cur_ref_fpath in meta_ref_fpaths)
        bed_fpaths = [f for f in bed_fpaths if f]
        qutils.call_subprocess(['cat'] + bed_fpaths, stdout=open(bed_fpath, 'w'), stderr=open(err_path, 'a'))
    else:
        bed_fpath = process_one_ref(main_ref_fpath, output_dirpath, err_path)
    bed_file = open(bed_fpath, 'a')
    for deletion in deletions:
        bed_file.write(str(deletion) + '\n')
    return bed_fpath


def run_processing_reads(main_ref_fpath, meta_ref_fpaths, ref_labels, reads_fpaths, output_dirpath, res_path, log_path, err_path):
    ref_name = qutils.name_from_fpath(main_ref_fpath)
    sam_fpath = os.path.join(output_dirpath, ref_name + '.sam')
    bam_fpath = os.path.join(output_dirpath, ref_name + '.bam')
    bam_sorted_fpath = os.path.join(output_dirpath, ref_name + '.sorted')
    sam_sorted_fpath = os.path.join(output_dirpath, ref_name + '.sorted.sam')
    bed_fpath = os.path.join(res_path, ref_name + '.bed')

    if os.path.isfile(bed_fpath):
        logger.info('  Using existing BED-file')
        return bed_fpath

    logger.info('  ' + 'Pre-processing for searching structural variations...')
    logger.info('  ' + 'Logging to %s...' % err_path)
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
    qutils.call_subprocess([samtools_fpath('samtools'), 'view', '-@', str(qconfig.max_threads), '-bS', sam_fpath], stdout=open(bam_fpath, 'w'),
                           stderr=open(err_path, 'a'))
    qutils.call_subprocess([samtools_fpath('samtools'), 'sort', '-@', str(qconfig.max_threads), bam_fpath, bam_sorted_fpath],
                           stderr=open(err_path, 'a'))
    qutils.call_subprocess([samtools_fpath('samtools'), 'view', '-@', str(qconfig.max_threads), bam_sorted_fpath + '.bam'], stdout=open(sam_sorted_fpath, 'w'),
                           stderr=open(err_path, 'a'))
    logger.info('  Splitting SAM-file by references...')
    headers = []
    with open(sam_fpath) as sam_file:
        for line in sam_file:
            l = line.split('\t')
            if len(l) > 5:
                break
            headers.append(line.strip())
    if meta_ref_fpaths:
        ref_files = {}
        for cur_ref_fpath in meta_ref_fpaths:
            ref = qutils.name_from_fpath(cur_ref_fpath)
            new_ref_sam_file = open(os.path.join(output_dirpath, ref + '.sam'), 'w')
            new_ref_sam_file.write(headers[0] + '\n')
            chrs = []
            for h in headers:
                l = h.split('\t')
                sn = l[1].split(':')
                if sn[0] == 'SN' and sn[1] in ref_labels and ref_labels[sn[1]] == ref:
                    new_ref_sam_file.write(h+'\n')
                    chrs.append(sn[1])
            new_ref_sam_file.write(headers[-1]+'\n')
            ref_files[ref] = new_ref_sam_file
    deletions = []
    with open(sam_sorted_fpath) as sam_file:
        prev_pos = 0
        prev_ref = ''
        for line in sam_file:
            if line:
                line = line.split('\t')
                if len(line) > 5:
                    pos = int(line[3])
                    if pos - prev_pos > qconfig.extensive_misassembly_threshold and prev_ref == line[2]:
                        deletions.append(QuastDeletion(line[2], prev_pos + len(line[9]), pos))
                    prev_pos = pos
                    prev_ref = line[2]
                    if meta_ref_fpaths:
                        cur_ref = ref_labels[line[2]]
                        if line[6].strip() == '=' or cur_ref in line[6]:
                            ref_files[cur_ref].write('\t'.join(line))
    if meta_ref_fpaths:
        for ref in ref_files:
            ref_files[ref].close()
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

    log_path = os.path.join(temp_output_dir, 'align_reads.log')
    err_path = os.path.join(temp_output_dir, 'align_reads.err')
    logger.info('  ' + 'Logging to files %s and %s...' % (log_path, err_path))
    bed_fpath = run_processing_reads(ref_fpath, meta_ref_fpaths, contigs_analyzer.ref_labels_by_chromosomes, reads_fpaths, temp_output_dir, output_dir, log_path, err_path)
    if not qconfig.debug:
        shutil.rmtree(temp_output_dir, ignore_errors=True)

    logger.info('Done.')
    return bed_fpath
