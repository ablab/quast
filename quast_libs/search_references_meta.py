############################################################################
# Copyright (c) 2015-2022 Saint Petersburg State University
# Copyright (c) 2011-2015 Saint Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################

from __future__ import with_statement
import os
import shlex
import shutil
import re
import subprocess
import time
from collections import defaultdict

from os.path import isdir, isfile, join

from quast_libs import qconfig, qutils
from quast_libs.fastaparser import _get_fasta_file_handler
from quast_libs.log import get_logger
from quast_libs.qutils import is_non_empty_file, slugify, correct_name, get_dir_for_download, show_progress, \
    download_blast_binaries, get_blast_fpath, md5, run_parallel, add_suffix

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

silva_pattern = re.compile(r'[a-zA-Z0-9.]+\_(?P<taxons>[A-Z]\S+)$', re.I)
ncbi_pattern = re.compile(r'(?P<id>\S+\_[0-9.]+)[_ |](?P<seqname>\S+)', re.I)

silva_version = 138.1
silva_db_url = 'http://www.arb-silva.de/fileadmin/silva_databases/release_' + str(silva_version) + '/Exports/'
silva_fname = 'SILVA_' + str(silva_version) + '_SSURef_NR99_tax_silva.fasta'
silva_downloaded_fname = 'silva.' + str(silva_version) + '.db'

blast_filenames = ['makeblastdb', 'blastn']
blastdb_dirpath = None
db_fpath = None
min_db_nsq_fsize = 10^8

ncbi_url = 'https://eutils.ncbi.nlm.nih.gov/entrez/eutils/'
quast_fields = '&tool=quast&email=quast.support@bioinf.spbau.ru'

is_quast_first_run = False
taxons_for_krona = {}
connection_errors = 0


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
                         'Check internet connection or run MetaQUAST with option "--max-ref-number 0" to disable reference search in the NCBI database.', exit_with_code=404)
                return None
            # NCBI recommends users post no more than three URL requests per second, so adding artificial 1-sec delay
            # see more: https://github.com/ablab/quast/issues/8
            time.sleep(1)
    return response


def get_download_links(ref_id_list, db):
    best_ref_links = []
    for id in ref_id_list:
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
        if not best_ref_links or (ref_links and len(ref_links) < len(best_ref_links)):
            best_ref_links = ref_links
            if len(best_ref_links) <= 5:
                break
    return best_ref_links


def download_ref(organism, ref_fpath, max_ref_fragments):
    organism = organism.replace('_', '+')
    isolate = ''
    strain = ''
    if '+isolate+' in organism:
        organism, isolate = organism.split('+isolate+')
    if '+strain+' in organism:
        organism, strain = organism.split('+strain+')

    response = try_send_request(ncbi_url + 'esearch.fcgi?db=assembly&term=%s+[Organism]%s%s&retmax=100%s' %
                                (organism, (isolate + '+[Isolate]') if isolate else '', (strain + '+[Strain]') if strain else '', quast_fields))
    if not response:
        logger.warning('Empty/No response from NCBI. Could be an Internet connection issue!')
        return None
    xml_tree = ET.fromstring(response)

    if xml_tree.find('Count') is None or xml_tree.find('IdList') is None:
        logger.warning('Unexpected/malformed response from NCBI. '
                       'Please try to find out what is going wrong or contact us. '
                       'Response: ' + response)
        return None  # broken response for some undefined reason

    if xml_tree.find('Count').text == '0':  # Organism is not found
        return None

    ref_id_list = xml_tree.find('IdList').findall('Id')
    best_ref_links = get_download_links(ref_id_list, "assembly_nuccore_refseq+OR+assembly_nuccore_insdc")
    used_db = "refseq"
    if not best_ref_links:
        used_db = "wgsmaster"
        best_ref_links = get_download_links(ref_id_list, "assembly_nuccore_wgsmaster")

    if len(best_ref_links) > max_ref_fragments:
        logger.info('%s has too fragmented reference genome! It will not be downloaded.' % organism.replace('+', ' '))
        return None

    if used_db == "refseq" and best_ref_links:
        ref_ids = sorted(link.find('Id').text for link in best_ref_links)
        is_first_piece = False
        fasta_files = []
        chunk_size = 200
        for i in range(0, len(ref_ids), chunk_size):
            fasta = try_send_request(ncbi_url + 'efetch.fcgi?db=sequences&id=%s&rettype=fasta&retmode=text' % ','.join(ref_ids[i:i+chunk_size]))
            if fasta and fasta[0] == '>':
                fasta_files.extend(fasta.rstrip().split('\n\n'))
        fasta_names = [f.split(' ')[0] for f in fasta_files]
        with open(ref_fpath, "w") as fasta_file:
            for name, fasta in sorted(zip(fasta_names, fasta_files), key=natural_sort_key):
                if not is_first_piece:
                    is_first_piece = True
                else:
                    fasta = '\n' + fasta.rstrip()
                fasta_file.write(fasta.rstrip())
    elif best_ref_links:  ## download WGS assembly
        try:
            download_wgsmaster_contigs(best_ref_links[0].find('Id').text, ref_fpath)
        except:
            logger.info('Failed downloading %s!' % organism.replace('+', ' '))

    if not os.path.isfile(ref_fpath):
        return None
    if not is_non_empty_file(ref_fpath):
        os.remove(ref_fpath)
        return None

    return ref_fpath


def download_wgsmaster_contigs(ref_id, ref_fpath):
    temp_fpath = add_suffix(ref_fpath, 'tmp') + '.gz'
    response = try_send_request(ncbi_url + 'esummary.fcgi?db=nuccore&id=%s&rettype=text&validate=false' % ref_id)
    xml_tree = ET.fromstring(response)

    for field in xml_tree[0]:
        if field.get('Name') == 'Extra':
            download_system = field.text.split('|')[-1][:6]
            genome_version = int(field.text.split('|')[3].split('.')[-1])
            break
    fsize = None
    while genome_version != 0 and not fsize:
        try:
            fname = "%s.%s.fsa_nt.gz" % (download_system, genome_version)
            url = "ftp://ftp.ncbi.nlm.nih.gov/sra/wgs_aux/%s/%s/%s/%s" % (download_system[:2], download_system[2:4], download_system, fname)
            response = urlopen(url)
            meta = response.info()
            fsize = int(meta.getheaders("Content-length")[0])
            bsize = 1048576
        except:
            fsize = None
            if genome_version != 0:
                genome_version -= 1
    with open(temp_fpath, 'wb') as f:
        while True:
            buffer = response.read(bsize)
            if not buffer:
                break
            f.write(buffer)

    with open(ref_fpath, 'w') as f:
        subprocess.call(['gunzip', '-c', temp_fpath], stdout=f)
    os.remove(temp_fpath)


def download_blastdb(logger=logger, only_clean=False):
    global blastdb_dirpath
    blastdb_dirpath = get_dir_for_download('silva', 'Silva', [silva_downloaded_fname + '.nsq'], logger, only_clean=only_clean)
    if not blastdb_dirpath:
        return False

    if only_clean:
        if os.path.isdir(blastdb_dirpath):
            logger.info('Removing ' + blastdb_dirpath)
            shutil.rmtree(blastdb_dirpath)
        return True

    global db_fpath
    db_fpath = join(blastdb_dirpath, silva_downloaded_fname)
    if os.path.isfile(db_fpath + '.nsq') and os.path.getsize(db_fpath + '.nsq') >= min_db_nsq_fsize:
        return True
    log_fpath = os.path.join(blastdb_dirpath, 'blastdb.log')
    db_gz_fpath = os.path.join(blastdb_dirpath, silva_fname + '.gz')
    silva_fpath = os.path.join(blastdb_dirpath, silva_fname)

    logger.info()

    if os.path.isfile(db_gz_fpath):
        logger.info('SILVA 16S ribosomal RNA gene database (version %s) has already been downloaded.'
                    % str(silva_version))
    else:
        logger.info('Downloading SILVA 16S ribosomal RNA gene database (version %s)...' % str(silva_version))
        if not os.path.isdir(blastdb_dirpath):
            os.makedirs(blastdb_dirpath)
        silva_download = urllib.FancyURLopener()
        silva_remote_fpath = silva_db_url + silva_fname + '.gz'
        silva_download_in_progress_path = db_gz_fpath + '.download'
        silva_md5_remote_fpath = silva_remote_fpath + '.md5'
        silva_md5_local_fpath = db_gz_fpath + '.md5'
        try:
            silva_download.retrieve(silva_remote_fpath, silva_download_in_progress_path, show_progress)
            silva_download.retrieve(silva_md5_remote_fpath, silva_md5_local_fpath, show_progress)
            if not qutils.verify_md5(silva_download_in_progress_path, silva_md5_local_fpath):
                raise ValueError
        except Exception:
            logger.error(
                'Failed downloading SILVA 16S rRNA gene database (%s)! The search for reference genomes cannot be performed. '
                'Try to download it manually, put under %s/ and restart your command.' % (silva_remote_fpath, blastdb_dirpath))
            return False
        os.remove(silva_md5_local_fpath)
        shutil.move(silva_download_in_progress_path, db_gz_fpath)

    logger.info('Processing downloaded file. Logging to %s...' % log_fpath)
    if not qutils.is_non_empty_file(silva_fpath):
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
    ret_code = qutils.call_subprocess(shlex.split(cmd), stdout=open(log_fpath, 'a'), stderr=open(log_fpath, 'a'), logger=logger)
    if ret_code != 0 or not os.path.exists(db_fpath + '.nsq') or os.path.getsize(db_fpath + '.nsq') < min_db_nsq_fsize:
        if os.path.exists(db_fpath + '.nsq'):
            os.remove(db_fpath + '.nsq')
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
        check_file.writelines('Assembly: %s md5 checksum: %s\n' % (contigs_fpath, md5(contigs_fpath)))


def get_blast_output_fpath(blast_output_fpath, label):
    return blast_output_fpath + '_' + slugify(label)


def check_blast(blast_check_fpath, blast_res_fpath, files_md5, assemblies_fpaths, assemblies, labels):
    downloaded_organisms = []
    not_founded_organisms = []
    blast_assemblies = [assembly for assembly in assemblies]
    for i, assembly_fpath in enumerate(assemblies_fpaths):
        check_fpath = get_blast_output_fpath(blast_check_fpath, labels[i])
        res_fpath = get_blast_output_fpath(blast_res_fpath, labels[i])
        existing_assembly = None
        assembly_info = True
        if os.path.exists(check_fpath) and is_non_empty_file(res_fpath):
            with open(check_fpath) as check_file:
                for line in check_file:
                    if '---' in line:
                        assembly_info = False
                    if line and assembly_info:
                        assembly, md5 = line.split()[1], line.split()[-1]
                        if assembly in files_md5.keys() and md5 == files_md5[assembly]:
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
    files_md5 = dict((assembly.fpath, md5(assembly.fpath)) for assembly in assemblies)
    assemblies_fpaths = dict((assembly.fpath, assembly) for assembly in assemblies)
    blast_assemblies, downloaded_organisms, not_founded_organisms = \
        check_blast(blast_check_fpath, blast_res_fpath, files_md5, assemblies_fpaths, assemblies, labels)

    species_list = []
    replacement_list = None
    max_ref_fragments = qconfig.MAX_REFERENCE_FRAGMENTS
    if ref_txt_fpath:
        max_ref_fragments = 10000
        species_list = parse_refs_list(ref_txt_fpath)
        species_by_assembly = None
    else:
        species_scores, species_by_assembly, replacement_dict = process_blast(blast_assemblies, downloaded_dirpath,
                                                                              corrected_dirpath, labels, blast_check_fpath, err_fpath)
        if species_scores:
            species_scores = sorted(species_scores, reverse=True)
            species_list = [species for (species, query_id, score) in species_scores]
            replacement_list = [replacement_dict[query_id] for (species, query_id, score) in species_scores]

    downloaded_ref_fpaths = [os.path.join(downloaded_dirpath, file) for (path, dirs, files) in os.walk(downloaded_dirpath)
                             for file in files if qutils.check_is_fasta_file(file)]

    ref_fpaths = search_references(species_list, assemblies, labels, max_ref_fragments, downloaded_dirpath, not_founded_organisms, downloaded_ref_fpaths,
                 blast_check_fpath, err_fpath, species_by_assembly, replacement_list)

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
                seqname = taxons.split()[-1]
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


def get_species_name(seqname):
    seqname_fs = seqname.split('_')
    species_name = None
    if len(seqname_fs) > 1:
        species_name = seqname_fs[0] + '_' + seqname_fs[1]
        if seqname_fs[1] == 'sp.':
            species_name += '_' + seqname_fs[2]
    return species_name


def process_blast(blast_assemblies, downloaded_dirpath, corrected_dirpath, labels, blast_check_fpath, err_fpath):
    if len(blast_assemblies) > 0:
        if not download_blast_binaries(filenames=blast_filenames):
            return None, None, None

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

        elif not download_blastdb():
            return None, None, None

    blast_res_fpath = os.path.join(downloaded_dirpath, 'blast.res')

    if len(blast_assemblies) > 0:
        logger.main_info('Running BlastN..')
        n_jobs = min(qconfig.max_threads, len(blast_assemblies))
        blast_threads = max(1, qconfig.max_threads // n_jobs)
        parallel_run_args = [(assembly.fpath, assembly.label, corrected_dirpath,
                              err_fpath, blast_res_fpath, blast_check_fpath, blast_threads)
                             for assembly in blast_assemblies]
        run_parallel(parallel_blast, parallel_run_args, n_jobs, filter_results=True)

    logger.main_info()
    species_scores = []
    species_by_assembly = dict()
    max_entries = 4
    replacement_dict = defaultdict(list)
    for label in labels:
        assembly_scores = []
        assembly_species = []
        res_fpath = get_blast_output_fpath(blast_res_fpath, label)
        if os.path.exists(res_fpath):
            refs_for_query = 0
            with open(res_fpath) as res_file:
                query_id_col, subj_id_col, idy_col, len_col, score_col = None, None, None, None, None
                for line in res_file:
                    fs = line.split()
                    if line.startswith('#'):
                        refs_for_query = 0
                        # Fields: query id, subject id, % identity, alignment length, mismatches, gap opens, q. start, q. end, s. start, s. end, evalue, bit score
                        if 'Fields' in line:
                            fs = line.strip().split('Fields: ')[-1].split(', ')
                            query_id_col = fs.index('query id') if 'query id' in fs else 0
                            subj_id_col = fs.index('subject id') if 'subject id' in fs else 1
                            idy_col = fs.index('% identity') if '% identity' in fs else 2
                            len_col = fs.index('alignment length') if 'alignment length' in fs else 3
                            score_col = fs.index('bit score') if 'bit score' in fs else 11
                    elif refs_for_query < max_entries and len(fs) > score_col:
                        query_id = fs[query_id_col]
                        organism_id = fs[subj_id_col]
                        idy = float(fs[idy_col])
                        length = int(fs[len_col])
                        score = float(fs[score_col])
                        if idy >= qconfig.identity_threshold and length >= qconfig.min_length and score >= qconfig.min_bitscore:  # and (not scores or min(scores) - score < max_identity_difference):
                            seqname, taxons = parse_organism_id(organism_id)
                            if not seqname:
                                continue
                            species_name = get_species_name(seqname)
                            if species_name and 'uncultured' not in seqname and 'gut_metagenome' not in species_name:
                                if refs_for_query == 0:
                                    if species_name not in assembly_species:
                                        assembly_scores.append((seqname, query_id, score))
                                        if taxons:
                                            taxons_for_krona[correct_name(seqname)] = taxons
                                        assembly_species.append(species_name)
                                        refs_for_query += 1
                                    else:
                                        seq_scores = [(query_name, seq_query_id, seq_score) for query_name, seq_query_id, seq_score
                                                      in assembly_scores if get_species_name(query_name) == species_name]
                                        if seq_scores and score > seq_scores[0][2]:
                                            assembly_scores.remove(seq_scores[0])
                                            assembly_scores.append((seqname, query_id, score))
                                            if taxons:
                                                taxons_for_krona[correct_name(seqname)] = taxons
                                            refs_for_query += 1
                                else:
                                    if seqname not in replacement_dict[query_id]:
                                        replacement_dict[query_id].append(seqname)
                                        refs_for_query += 1
        assembly_scores = sorted(assembly_scores, reverse=True)
        assembly_scores = assembly_scores[:qconfig.max_references]
        for seqname, query_id, score in assembly_scores:
            if not species_by_assembly or not any(seqname in species_list for species_list in species_by_assembly.values()):
                species_scores.append((seqname, query_id, score))
        species_by_assembly[label] = [seqname for seqname, query_id, score in assembly_scores]
    if not species_scores:
        return None, None, None
    return species_scores, species_by_assembly, replacement_dict


def process_ref(ref_fpaths, organism, max_ref_fragments, downloaded_dirpath, max_organism_name_len, downloaded_organisms, not_founded_organisms,
                 total_downloaded, total_scored_left):
    ref_fpath = os.path.join(downloaded_dirpath, correct_name(organism) + '.fasta')
    spaces = (max_organism_name_len - len(organism)) * ' '
    new_ref_fpath = None
    was_downloaded = False
    if not os.path.exists(ref_fpath) and organism not in not_founded_organisms:
        new_ref_fpath = download_ref(organism, ref_fpath, max_ref_fragments)
    elif os.path.exists(ref_fpath):
        was_downloaded = True
        new_ref_fpath = ref_fpath
    total_scored_left -= 1
    if new_ref_fpath:
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
        logger.main_info("  %s%s | not found in the NCBI database" % (organism.replace('+', ' '), spaces))
        not_founded_organisms.add(organism)
    return new_ref_fpath, total_downloaded, total_scored_left


def parse_refs_list(ref_txt_fpath):
    organisms = []
    with open(ref_txt_fpath) as f:
        for l in f.read().split('\n'):
            if l:
                organism = l.strip().replace(' ', '_')
                organisms.append(organism)
    return organisms


def search_references(organisms, assemblies, labels, max_ref_fragments, downloaded_dirpath, not_founded_organisms, downloaded_ref_fpaths,
                 blast_check_fpath, err_fpath, organisms_assemblies=None, replacement_list=None):
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
    logger.main_info('Trying to download found references from NCBI. Totally ' + str(total_scored_left) + ' organisms to try.')
    if len(downloaded_ref_fpaths) > 0:
        logger.main_info('MetaQUAST will attempt to use previously downloaded references...')

    for idx, organism in enumerate(organisms):
        ref_fpath, total_downloaded, total_scored_left = process_ref(ref_fpaths, organism, max_ref_fragments, downloaded_dirpath, max_organism_name_len,
                                                                      downloaded_organisms, not_founded_organisms, total_downloaded, total_scored_left)
        if not ref_fpath and replacement_list:
            for next_match in replacement_list[idx]:
                if next_match not in organisms:
                    logger.main_info('  ' + organism.replace('+', ' ') + ' was not found in NCBI database, trying to download the next best match')
                    ref_fpath, total_downloaded, _ = process_ref(ref_fpaths, next_match, max_ref_fragments, downloaded_dirpath,
                                                                 max_organism_name_len, downloaded_organisms, not_founded_organisms,
                                                                 total_downloaded, total_scored_left + 1)
                    organism = next_match
                    if ref_fpath:
                        break

    for assembly, label in zip(assemblies, labels):
        check_fpath = get_blast_output_fpath(blast_check_fpath, label)
        if os.path.exists(check_fpath):
            with open(check_fpath) as check_file:
                text = check_file.read()
                text = text[:text.find('\n')]
        else:
            text = 'Assembly: %s md5 checksum: %s\n' % (assembly.fpath, md5(assembly.fpath))
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
