############################################################################
# Copyright (c) 2015-2016 Saint Petersburg State University
# Copyright (c) 2011-2015 Saint Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################

from __future__ import with_statement
import os
import shlex
import shutil
import stat
import sys
import re

from os.path import isdir, isfile, join

from quast_libs import qconfig, qutils
from quast_libs.fastaparser import _get_fasta_file_handler
from quast_libs.log import get_logger
from quast_libs.qutils import is_non_empty_file, is_python2, slugify, correct_name

logger = get_logger(qconfig.LOGGER_META_NAME)
try:
    from urllib2 import urlopen
    import urllib
except:
    from urllib.request import urlopen
    import urllib.request as urllib

import xml.etree.ElementTree as ET
import socket
socket.setdefaulttimeout(120)

silva_pattern = re.compile(r'\S+\_(?P<taxons>\S+);(?P<seqname>\S+)', re.I)
ncbi_pattern = re.compile(r'(?P<id>\S+\_[0-9.]+)[_ |](?P<seqname>\S+)', re.I)

silva_db_url = 'http://www.arb-silva.de/fileadmin/silva_databases/release_123/Exports/'
silva_fname = 'SILVA_123_SSURef_Nr99_tax_silva.fasta'

external_tools_dirpath = join(qconfig.QUAST_HOME, 'external_tools')
blast_external_tools_dirpath = join(external_tools_dirpath, 'blast', qconfig.platform_name)
blast_filenames = ['makeblastdb', 'blastn']
blast_dirpath_url = qconfig.GIT_ROOT_URL + qutils.relpath(blast_external_tools_dirpath, qconfig.QUAST_HOME)

blast_dirpath = join(qconfig.LIBS_LOCATION, 'blast')
blastdb_dirpath = join(qconfig.LIBS_LOCATION, 'blast', '16S_RNA_blastdb')
db_fpath = join(blastdb_dirpath, 'silva.db')
db_nsq_fsize = 194318557

is_quast_first_run = False
taxons_for_krona = {}
connection_errors = 0


def get_blast_fpath(fname):
    blast_path = os.path.join(blast_dirpath, fname)
    if os.path.exists(blast_path):
        return blast_path

    blast_path = qutils.get_path_to_program(fname)
    return blast_path


def natural_sort_key(s, _nsre=re.compile('([0-9]+)')):
    return [int(text) if text.isdigit() else text.lower()
            for text in re.split(_nsre, s[0])]


def try_send_request(url):
    attempts = 0
    response = None
    global connection_errors
    while attempts < 3:
        try:
            request = urlopen(url)
            connection_errors = 0
            response = request.read()
            if not isinstance(response, str):
                response = response.decode('utf-8')
            if response is None or 'ERROR' in response:
                request.close()
                raise Exception
            break
        except Exception:
            # _, exc_value, _ = sys.exc_info()
            # logger.exception(exc_value)
            attempts += 1
            if attempts >= 3:
                connection_errors += 1
                if connection_errors >= 3:
                    logger.error('Cannot established internet connection to download reference genomes! '
                         'Check internet connection or run MetaQUAST with option "--max-ref-number 0".', exit_with_code=404)
                return None
    return response


def download_refs(organism, ref_fpath):
    ncbi_url = 'https://eutils.ncbi.nlm.nih.gov/entrez/eutils/'
    quast_fields = '&tool=quast&email=quast.support@bioinf.spbau.ru'
    organism = organism.replace('_', '+')
    response = try_send_request(ncbi_url + 'esearch.fcgi?db=assembly&term=%s+[Organism]&retmax=100' % organism + quast_fields)
    if not response:
        return None
    xml_tree = ET.fromstring(response)

    if xml_tree.find('Count').text == '0':  # Organism is not found
        return None

    ref_id_list = xml_tree.find('IdList').findall('Id')
    best_ref_links = []
    for id in ref_id_list:
        databases = ['assembly_nuccore_refseq', 'assembly_nuccore_insdc']
        for db in databases:
            response = try_send_request(
                ncbi_url + 'elink.fcgi?dbfrom=assembly&db=nuccore&id=%s&linkname="%s"' % (id.text, db) + quast_fields)
            if not response:
                continue
            xml_tree = ET.fromstring(response)

            link_set = xml_tree.find('LinkSet')
            if link_set is None:
                continue
            link_db = xml_tree.find('LinkSet').find('LinkSetDb')
            if link_db is None:
                continue
            ref_links = link_db.findall('Link')
            if best_ref_links and len(ref_links) > len(best_ref_links):
                continue
            best_ref_links = ref_links
            if best_ref_links:
                break
        if best_ref_links and len(best_ref_links) < 3:
            break

    if not best_ref_links:
        return None

    if len(best_ref_links) > 500:
        logger.info('%s has too fragmented reference genome! It will not be downloaded.' % organism.replace('+', ' '))
        return None

    ref_ids = sorted(link.find('Id').text for link in best_ref_links)
    is_first_piece = False
    fasta_files = []
    for ref_id in ref_ids:
        fasta = try_send_request(ncbi_url + 'efetch.fcgi?db=sequences&id=%s&rettype=fasta&retmode=text' % ref_id)
        if fasta and fasta[0] == '>':
            fasta_files.append(fasta)
    fasta_names = [f.split('|')[-1] for f in fasta_files]
    with open(ref_fpath, "w") as fasta_file:
        for name, fasta in sorted(zip(fasta_names, fasta_files), key=natural_sort_key):
            if not is_first_piece:
                is_first_piece = True
            else:
                fasta = '\n' + fasta.rstrip()
            fasta_file.write(fasta.rstrip())

    if not os.path.isfile(ref_fpath):
        return None
    if not is_non_empty_file(ref_fpath):
        os.remove(ref_fpath)
        return None

    return ref_fpath


def show_progress(a, b, c):
    if a > 0 and a % int(c/(b*100)) == 0:
        print("% 3.1f%% of %d bytes" % (min(100, int(float(a * b) / c * 100)), c)),
        sys.stdout.flush()


def download_all_blast_binaries(logger=logger, only_clean=False):
    if only_clean:
        if os.path.isdir(blastdb_dirpath):
            shutil.rmtree(blastdb_dirpath)
        return True

    for i, cmd in enumerate(blast_filenames):
        blast_file = get_blast_fpath(cmd)
        if only_clean:
            if blast_file and isfile(blast_file):
                os.remove(blast_file)
            continue

        if not blast_file:
            return_code = download_blast_binary(cmd, logger=logger)
            logger.info()
            if return_code != 0:
                return False
            blast_file = get_blast_fpath(cmd)
            os.chmod(blast_file, os.stat(blast_file).st_mode | stat.S_IEXEC)
    return True


def download_blast_binary(blast_filename, logger=logger):
    logger.info()
    if not os.path.isdir(blast_dirpath):
        os.makedirs(blast_dirpath)
    if not os.path.isdir(blastdb_dirpath):
        os.makedirs(blastdb_dirpath)

    blast_libs_fpath = os.path.join(blast_dirpath, blast_filename)
    blast_external_fpath = os.path.join(blast_external_tools_dirpath, blast_filename)
    if not os.path.exists(blast_libs_fpath):
        if os.path.isfile(blast_external_fpath):
            logger.info('Copying blast files from ' + blast_external_fpath)
            shutil.copy(blast_external_fpath, blast_dirpath)
        else:
            blast_download = urllib.URLopener()
            blast_webpath = os.path.join(blast_dirpath_url, blast_filename)
            if not os.path.exists(blast_libs_fpath):
                logger.info('Downloading %s...' % blast_filename)
                try:
                    blast_download.retrieve(blast_webpath, blast_libs_fpath + '.download', show_progress)
                except Exception:
                    logger.error(
                        'Failed downloading %s! The search for reference genomes cannot be performed. '
                        'Please install it and ensure it is in your PATH, then restart your command.' % blast_filename)
                    return 1
                shutil.move(blast_libs_fpath + '.download', blast_libs_fpath)
                logger.info('%s successfully downloaded!' % blast_filename)
        return 0


def download_blastdb(logger=logger, only_clean=False):
    if only_clean:
        if os.path.isdir(blastdb_dirpath):
            logger.info('Removing ' + blastdb_dirpath)
            shutil.rmtree(blastdb_dirpath)
        return True

    if os.path.isfile(db_fpath + '.nsq') and os.path.getsize(db_fpath + '.nsq') >= db_nsq_fsize:
        logger.info()
        logger.info('SILVA 16S rRNA database has already been downloaded, unpacked and BLAST database created. '
                    'If not, please remove %s and restart your command.' % (db_fpath + '.nsq'))
        return True
    log_fpath = os.path.join(blastdb_dirpath, 'blastdb.log')
    db_gz_fpath = os.path.join(blastdb_dirpath, silva_fname + '.gz')
    silva_fpath = os.path.join(blastdb_dirpath, silva_fname)

    logger.info()

    if os.path.isfile(db_gz_fpath):
        logger.info('SILVA 16S ribosomal RNA gene database has already been downloaded.')
    else:
        logger.info('Downloading SILVA 16S ribosomal RNA gene database...')
        if not os.path.isdir(blastdb_dirpath):
            os.makedirs(blastdb_dirpath)
        silva_download = urllib.FancyURLopener()
        silva_remote_fpath = silva_db_url + silva_fname + '.gz'
        try:
            silva_download.retrieve(silva_remote_fpath, db_gz_fpath + '.download', show_progress)
        except Exception:
            logger.error(
                'Failed downloading SILVA 16S rRNA gene database (%s)! The search for reference genomes cannot be performed. '
                'Try to download it manually in %s and restart your command.' % (silva_remote_fpath, blastdb_dirpath))
            return False
        shutil.move(db_gz_fpath + '.download', db_gz_fpath)

    logger.info('Processing downloaded file. Logging to %s...' % log_fpath)
    if not os.path.isfile(silva_fpath):
        logger.info('Unpacking and replacing " " with "_"...')

        unpacked_fpath = silva_fpath + ".unpacked"
        cmd = "gunzip -c %s" % db_gz_fpath
        qutils.call_subprocess(shlex.split(cmd), stdout=open(unpacked_fpath, 'w'), stderr=open(log_fpath, 'a'), logger=logger)

        substituted_fpath = silva_fpath + ".substituted"
        with open(unpacked_fpath) as in_file:
            with open(substituted_fpath, 'w') as out_file:
                for line in in_file:
                    out_file.write(line.replace(' ', '_'))
        os.remove(unpacked_fpath)
        shutil.move(substituted_fpath, silva_fpath)

    logger.info('Making BLAST database...')
    cmd = get_blast_fpath('makeblastdb') + (' -in %s -dbtype nucl -out %s' % (silva_fpath, db_fpath))
    qutils.call_subprocess(shlex.split(cmd), stdout=open(log_fpath, 'a'), stderr=open(log_fpath, 'a'), logger=logger)
    if not os.path.exists(db_fpath + '.nsq') or os.path.getsize(db_fpath + '.nsq') < db_nsq_fsize:
        logger.error('Failed to make BLAST database ("' + blastdb_dirpath +
                     '"). See details in log. Try to make it manually: %s' % cmd)
        return False
    elif not qconfig.debug:
        os.remove(db_gz_fpath)
        os.remove(silva_fpath)
    return True


def parallel_blast(contigs_fpath, label, corrected_dirpath, err_fpath, blast_res_fpath, blast_check_fpath, blast_threads):
    logger.info('  ' + 'processing ' + label)
    blast_query_fpath = contigs_fpath
    compress_ext = ['.gz', '.gzip', '.bz2', '.bzip2', '.zip']
    if any(contigs_fpath.endswith(ext) for ext in compress_ext):
        logger.info('  ' + 'unpacking ' + label)
        unpacked_fpath = os.path.join(corrected_dirpath, os.path.basename(contigs_fpath) + '.unpacked')
        with _get_fasta_file_handler(contigs_fpath) as f_in:
            with open(unpacked_fpath, 'w') as f_out:
                for l in f_in:
                    f_out.write(l)
        blast_query_fpath = unpacked_fpath
    res_fpath = get_blast_output_fpath(blast_res_fpath, label)
    check_fpath = get_blast_output_fpath(blast_check_fpath, label)
    cmd = get_blast_fpath('blastn') + (' -query %s -db %s -outfmt 7 -num_threads %s' % (
        blast_query_fpath, db_fpath, blast_threads))
    qutils.call_subprocess(shlex.split(cmd), stdout=open(res_fpath, 'w'), stderr=open(err_fpath, 'a'), logger=logger)
    logger.info('  ' + 'BLAST results for %s are saved to %s...' % (label, res_fpath))
    with open(check_fpath, 'w') as check_file:
        check_file.writelines('Assembly: %s size: %d\n' % (contigs_fpath, os.path.getsize(contigs_fpath)))


def get_blast_output_fpath(blast_output_fpath, label):
    return blast_output_fpath + '_' + slugify(label)


def check_blast(blast_check_fpath, blast_res_fpath, files_sizes, assemblies_fpaths, assemblies, labels):
    downloaded_organisms = []
    not_founded_organisms = []
    blast_assemblies = [assembly for assembly in assemblies]
    for i, assembly_fpath in enumerate(assemblies_fpaths):
        check_fpath = get_blast_output_fpath(blast_check_fpath, labels[i])
        res_fpath = get_blast_output_fpath(blast_res_fpath, labels[i])
        existing_assembly = None
        assembly_info = True
        if os.path.exists(check_fpath) and is_non_empty_file(res_fpath):
            for line in open(check_fpath):
                if '---' in line:
                    assembly_info = False
                if line and assembly_info:
                    assembly, size = line.split()[1], line.split()[3]
                    if assembly in files_sizes.keys() and int(size) == files_sizes[assembly]:
                        existing_assembly = assemblies_fpaths[assembly]
                        logger.main_info('  Using existing BLAST alignments for %s... ' % labels[i])
                        blast_assemblies.remove(existing_assembly)
                elif line and existing_assembly:
                    line = line.split(' ')
                    if len(line) > 1:
                        if line[0] == 'Downloaded:':
                            downloaded_organisms += line[1].rstrip().split(',')
                        elif line[0] == 'Not_founded:':
                            not_founded_organisms += line[1].rstrip().split(',')
    return blast_assemblies, set(downloaded_organisms), set(not_founded_organisms)


def do(assemblies, labels, downloaded_dirpath, corrected_dirpath, ref_txt_fpath=None):
    logger.print_timestamp()
    err_fpath = os.path.join(downloaded_dirpath, 'blast.err')
    blast_check_fpath = os.path.join(downloaded_dirpath, 'blast.check')
    blast_res_fpath = os.path.join(downloaded_dirpath, 'blast.res')
    files_sizes = dict((assembly.fpath, os.path.getsize(assembly.fpath)) for assembly in assemblies)
    assemblies_fpaths = dict((assembly.fpath, assembly) for assembly in assemblies)
    blast_assemblies, downloaded_organisms, not_founded_organisms = \
        check_blast(blast_check_fpath, blast_res_fpath, files_sizes, assemblies_fpaths, assemblies, labels)
    organisms = []

    if ref_txt_fpath:
        organisms = parse_refs_list(ref_txt_fpath)
        organisms_assemblies = None
    else:
        scores_organisms, organisms_assemblies = process_blast(blast_assemblies, downloaded_dirpath, corrected_dirpath,
                                                               labels, blast_check_fpath, err_fpath)
        if scores_organisms:
            scores_organisms = sorted(scores_organisms, reverse=True)
            organisms = [organism for (score, organism) in scores_organisms]

    downloaded_ref_fpaths = [os.path.join(downloaded_dirpath, file) for (path, dirs, files) in os.walk(downloaded_dirpath)
                             for file in files if qutils.check_is_fasta_file(file)]

    ref_fpaths = process_refs(organisms, assemblies, labels, downloaded_dirpath, not_founded_organisms, downloaded_ref_fpaths,
                 blast_check_fpath, err_fpath, organisms_assemblies)

    if not ref_fpaths:
        logger.main_info('Reference genomes are not found.')
    if not qconfig.debug and os.path.exists(err_fpath):
        os.remove(err_fpath)
    ref_fpaths.sort()
    return ref_fpaths


def parse_organism_id(organism_id):
    seqname = None
    taxons = None
    if silva_pattern.match(organism_id):
        m = silva_pattern.match(organism_id)
        if m:
            taxons = m.group('taxons')
            taxons = taxons.replace(';', '\t')
            domain = taxons.split()[0]
            if domain and domain in ['Bacteria',
                                     'Archaea'] and 'Chloroplast' not in taxons and 'mitochondria' not in taxons:
                seqname = m.group('seqname')
                taxons += '\t' + seqname
    elif ncbi_pattern.match(organism_id):
        m = ncbi_pattern.match(organism_id)
        if m:
            seqname = m.group('seqname')
    else:
        seqname = organism_id.replace(' ', '_')
    if seqname:
        seqname = re.sub('[\[,/\]]', ';', seqname)
        seqname = seqname.split(';')[0]
    return seqname, taxons


def process_blast(blast_assemblies, downloaded_dirpath, corrected_dirpath, labels, blast_check_fpath, err_fpath):
    if not os.path.isdir(blastdb_dirpath):
        os.makedirs(blastdb_dirpath)

    if not download_all_blast_binaries():
        return None, None

    if qconfig.custom_blast_db_fpath:
        global db_fpath
        db_fpath = qconfig.custom_blast_db_fpath
        if isdir(db_fpath):
            db_aux_files = [f for f in os.listdir(db_fpath) if f.endswith('.nsq')]
            if db_aux_files:
                db_fpath = join(qconfig.custom_blast_db_fpath, db_aux_files[0].replace('.nsq', ''))
        elif isfile(db_fpath) and db_fpath.endswith('.nsq'):
            db_fpath = db_fpath[:-len('.nsq')]
        if not os.path.isfile(db_fpath + '.nsq'):
            logger.error('You should specify path to BLAST database obtained by running makeblastdb command: '
                         'either path to directory containing <dbname>.nsq file or path to <dbname>.nsq file itself.'
                         ' Also you can rerun MetaQUAST without --blast-db option. MetaQUAST uses SILVA 16S RNA database by default.',
                         exit_with_code=2)

    elif not os.path.isfile(db_fpath + '.nsq') or os.path.getsize(db_fpath + '.nsq') < db_nsq_fsize:
        # if os.path.isdir(blastdb_dirpath):
        #     shutil.rmtree(blastdb_dirpath)
        if not download_blastdb():
            return None, None
        logger.info()

    blast_res_fpath = os.path.join(downloaded_dirpath, 'blast.res')

    if len(blast_assemblies) > 0:
        logger.main_info('Running BlastN..')
        n_jobs = min(qconfig.max_threads, len(blast_assemblies))
        blast_threads = max(1, qconfig.max_threads // n_jobs)
        if is_python2():
            from joblib import Parallel, delayed
        else:
            from joblib3 import Parallel, delayed
        Parallel(n_jobs=n_jobs)(delayed(parallel_blast)(assembly.fpath, assembly.label, corrected_dirpath,
                                                        err_fpath, blast_res_fpath, blast_check_fpath, blast_threads)
                                for i, assembly in enumerate(blast_assemblies))

    logger.main_info('')
    scores_organisms = []
    organisms_assemblies = {}
    for label in labels:
        all_scores = []
        organisms = []
        res_fpath = get_blast_output_fpath(blast_res_fpath, label)
        if os.path.exists(res_fpath):
            refs_for_query = 0
            for line in open(res_fpath):
                if refs_for_query == 0 and not line.startswith('#') and len(line.split()) > 10:
                    # TODO: find and parse "Fields" line to detect each column indexes:
                    # Fields: query id, subject id, % identity, alignment length, mismatches, gap opens, q. start, q. end, s. start, s. end, evalue, bit score
                    # We need: identity, legnth, score, query and subject id.
                    line = line.split()
                    organism_id = line[1]
                    idy = float(line[2])
                    length = int(line[3])
                    score = float(line[11])
                    if idy >= qconfig.identity_threshold and length >= qconfig.min_length and score >= qconfig.min_bitscore:  # and (not scores or min(scores) - score < max_identity_difference):
                        seqname, taxons = parse_organism_id(organism_id)
                        if not seqname:
                            continue
                        specie = seqname.split('_')
                        if len(specie) > 1 and 'uncultured' not in seqname:
                            specie = specie[0] + '_' + specie[1]
                            if specie not in organisms:
                                all_scores.append((score, seqname))
                                if taxons:
                                    taxons_for_krona[correct_name(seqname)] = taxons
                                organisms.append(specie)
                                refs_for_query += 1
                            else:
                                tuple_scores = [x for x in all_scores if specie in x[1]]
                                if tuple_scores and score > tuple_scores[0][0]:
                                    all_scores.remove((tuple_scores[0][0], tuple_scores[0][1]))
                                    all_scores.append((score, seqname))
                                    if taxons:
                                        taxons_for_krona[correct_name(seqname)] = taxons
                                    refs_for_query += 1
                elif line.startswith('#'):
                    refs_for_query = 0
        all_scores = sorted(all_scores, reverse=True)
        all_scores = all_scores[:qconfig.max_references]
        for score in all_scores:
            if not organisms_assemblies or (organisms_assemblies.values() and not [1 for list in organisms_assemblies.values() if score[1] in list]):
                scores_organisms.append(score)
        organisms_assemblies[label] = [score[1] for score in all_scores]
    if not scores_organisms:
        return None, None
    return scores_organisms, organisms_assemblies


def parse_refs_list(ref_txt_fpath):
    organisms = []
    with open(ref_txt_fpath) as f:
        for l in f.read().split('\n'):
            if l:
                organism = l.strip().replace(' ', '_')
                organisms.append(organism)
    return organisms


def process_refs(organisms, assemblies, labels, downloaded_dirpath, not_founded_organisms, downloaded_ref_fpaths,
                 blast_check_fpath, err_fpath, organisms_assemblies=None):
    ref_fpaths = []
    downloaded_organisms = []

    total_downloaded = 0
    total_scored_left = len(organisms)
    if total_scored_left == 0:
        if not qconfig.debug and os.path.exists(err_fpath):
            os.remove(err_fpath)
        return ref_fpaths

    max_organism_name_len = 0
    for organism in organisms:
        max_organism_name_len = max(len(organism), max_organism_name_len)
    for organism in downloaded_organisms:
        max_organism_name_len = max(len(organism), max_organism_name_len)

    logger.print_timestamp()
    logger.main_info('Trying to download found references from NCBI. '
                'Totally ' + str(total_scored_left) + ' organisms to try.')
    if len(downloaded_ref_fpaths) > 0:
        logger.main_info('MetaQUAST will attempt to use previously downloaded references...')

    for organism in organisms:
        ref_fpath = os.path.join(downloaded_dirpath, correct_name(organism) + '.fasta')
        spaces = (max_organism_name_len - len(organism)) * ' '
        new_ref_fpath = None
        was_downloaded = False
        if not os.path.exists(ref_fpath) and organism not in not_founded_organisms:
            new_ref_fpath = download_refs(organism, ref_fpath)
        elif os.path.exists(ref_fpath):
            was_downloaded = True
            new_ref_fpath = ref_fpath
        if new_ref_fpath:
            total_scored_left -= 1
            total_downloaded += 1
            if was_downloaded:
                logger.main_info("  %s%s | was downloaded previously (total %d, %d more to go)" %
                            (organism.replace('+', ' '), spaces, total_downloaded, total_scored_left))
                if new_ref_fpath not in ref_fpaths:
                    ref_fpaths.append(new_ref_fpath)
            else:
                logger.main_info("  %s%s | successfully downloaded (total %d, %d more to go)" %
                        (organism.replace('+', ' '), spaces, total_downloaded, total_scored_left))
                ref_fpaths.append(new_ref_fpath)
            downloaded_organisms.append(organism)
        else:
            total_scored_left -= 1
            logger.main_info("  %s%s | not found in the NCBI database" % (organism.replace('+', ' '), spaces))
            not_founded_organisms.add(organism)
    for assembly, label in zip(assemblies, labels):
        check_fpath = get_blast_output_fpath(blast_check_fpath, label)
        if os.path.exists(check_fpath):
            with open(check_fpath) as check_file:
                text = check_file.read()
                text = text[:text.find('\n')]
        else:
            text = 'Assembly: %s size: %d\n' % (assembly.fpath, os.path.getsize(assembly.fpath))
        with open(check_fpath, 'w') as check_file:
            check_file.writelines(text)
            check_file.writelines('\n---\n')
            cur_downloaded_organisms = [organism for organism in downloaded_organisms] if not organisms_assemblies else \
                [organism for organism in downloaded_organisms if organism in organisms_assemblies[label]]
            cur_not_founded_organisms = [organism for organism in not_founded_organisms] if not organisms_assemblies else \
                [organism for organism in not_founded_organisms if organism in organisms_assemblies[label]]
            check_file.writelines('Downloaded: %s\n' % ','.join(cur_downloaded_organisms))
            check_file.writelines('Not_founded: %s\n' % ','.join(cur_not_founded_organisms))
    return ref_fpaths
