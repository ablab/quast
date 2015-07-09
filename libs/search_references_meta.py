############################################################################
# Copyright (c) 2011-2015 Saint-Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################

from __future__ import with_statement
import os
import shlex
import shutil
import sys
import platform
import re
import gzip
from libs import qconfig, qutils
from libs.log import get_logger

logger = get_logger(qconfig.LOGGER_META_NAME)
from urllib2 import urlopen
import xml.etree.ElementTree as ET
import urllib

silva_db_path = 'http://www.arb-silva.de/fileadmin/silva_databases/release_119/Exports/'
silva_fname = 'SILVA_119_SSURef_Nr99_tax_silva.fasta'
blastdb_dirpath = os.path.join(qconfig.LIBS_LOCATION, 'blast', '16S_RNA_blastdb')
db_fpath = os.path.join(blastdb_dirpath, 'silva_119.db')
db_nsq_fsize = 194318557

if platform.system() == 'Darwin':
    sed_cmd = "sed -i '' "
else:
    sed_cmd = 'sed -i '
is_quast_first_run = False

def blast_fpath(fname):
    blast_dirpath = os.path.join(qconfig.LIBS_LOCATION, 'blast', qconfig.platform_name)
    return os.path.join(blast_dirpath, fname)


def try_send_request(url):
    try:
        request = urlopen(url)
    except Exception:
        logger.error('Cannot established internet connection to download reference genomes! '
                     'Check internet connection or run MetaQUAST with option "--max-ref-number 0".', exit_with_code=404)
    return request.read()


def download_refs(organism, ref_fpath):
    ncbi_url = 'http://eutils.ncbi.nlm.nih.gov/entrez/eutils/'
    organism = organism.replace('_', '+')
    response = try_send_request(ncbi_url + 'esearch.fcgi?db=assembly&term=%s+[Organism]&retmax=100' % organism)
    xml_tree = ET.fromstring(response)

    if xml_tree.find('Count').text == '0':  # Organism is not found
        return None

    ref_id = xml_tree.find('IdList').find('Id').text
    response = try_send_request(
        ncbi_url + 'elink.fcgi?dbfrom=assembly&db=nuccore&id=%s&linkname="assembly_nuccore_refseq"' % ref_id)
    xml_tree = ET.fromstring(response)

    link_set = xml_tree.find('LinkSet')
    if link_set is None:
        return None

    link_db = xml_tree.find('LinkSet').find('LinkSetDb')
    if link_db is None:
        return None

    is_first_piece = False
    for ref_id in sorted(ref_id.find('Id').text for ref_id in link_db.findall('Link')):
        fasta = try_send_request(ncbi_url + 'efetch.fcgi?db=sequences&id=%s&rettype=fasta&retmode=text' % ref_id)
        if fasta:
            if 'complete genome' in fasta[:150]:
                with open(ref_fpath, "w") as fasta_file:
                    fasta_file.write(fasta)
                break
            else:
                if 'shotgun sequence' in fasta[:150]:
                    if not is_first_piece:
                        is_first_piece = True
                        if ',' in fasta[:150]:
                            first_line = fasta[:fasta.find(',')]
                            fasta = first_line + '\n' + fasta[fasta.find('\n')+1:]
                    else:
                        fasta = fasta[fasta.find('\n')+1:]
                with open(ref_fpath, "a") as fasta_file:
                    fasta_file.write(fasta.rstrip())

    if not os.path.isfile(ref_fpath):
        return None
    if os.path.getsize(ref_fpath) < 0:
        os.remove(ref_fpath)
        return None

    return ref_fpath


def show_progress(a, b, c):
    print "% 3.1f%% of %d bytes\r" % (min(100, float(a * b) / c * 100), c),
    sys.stdout.flush()


def download_blastdb():
    if os.path.isfile(db_fpath + '.nsq'):
        logger.info()
        logger.info('SILVA rRNA database has already been downloaded, unpacked and BLAST database created. '
                    'If not, please remove %s and rerun MetaQUAST' % db_fpath + '.nsq')
        return 0
    log_fpath = os.path.join(blastdb_dirpath, 'blastdb.log')
    db_gz_fpath = os.path.join(blastdb_dirpath, silva_fname + '.gz')
    silva_fpath = os.path.join(blastdb_dirpath, silva_fname)

    logger.info()
    if os.path.isfile(db_gz_fpath):
        logger.info('SILVA ribosomal RNA gene database has already been downloaded.')
    else:
        logger.info('Downloading SILVA ribosomal RNA gene database...')
        if not os.path.isdir(blastdb_dirpath):
            os.mkdir(blastdb_dirpath)
        silva_download = urllib.URLopener()
        silva_remote_fpath = silva_db_path + silva_fname + '.gz'
        try:
            silva_download.retrieve(silva_remote_fpath, db_gz_fpath + '.download', show_progress)
        except Exception:
            logger.error(
                'Failed downloading SILVA rRNA gene database (%s)! The search for reference genomes cannot be performed. '
                'Try to download it manually in %s and restart MetaQUAST.' % (silva_remote_fpath, blastdb_dirpath))
            return 1
        shutil.move(db_gz_fpath + '.download', db_gz_fpath)

    logger.info('Processing downloaded file. Logging to %s...' % log_fpath)
    if not os.path.isfile(silva_fpath):
        logger.info('Unpacking and replacing " " with "_"...')

        unpacked_fpath = silva_fpath + ".unpacked"
        cmd = "gunzip -c %s" % db_gz_fpath
        qutils.call_subprocess(shlex.split(cmd), stdout=open(unpacked_fpath, 'w'), stderr=open(log_fpath, 'a'), logger=logger)

        cmd = sed_cmd + " 's/ /_/g' %s" % unpacked_fpath
        qutils.call_subprocess(shlex.split(cmd), stdout=open(log_fpath, 'a'), stderr=open(log_fpath, 'a'), logger=logger)
        shutil.move(unpacked_fpath, silva_fpath)

    logger.info('Making BLAST database...')
    cmd = blast_fpath('makeblastdb') + (' -in %s -dbtype nucl -out %s' % (silva_fpath, db_fpath))
    qutils.call_subprocess(shlex.split(cmd), stdout=open(log_fpath, 'a'), stderr=open(log_fpath, 'a'), logger=logger)
    if not os.path.exists(db_fpath + '.nsq') or os.path.getsize(db_fpath + '.nsq') < db_nsq_fsize:
        logger.error('Failed to make BLAST database ("' + blastdb_dirpath +
                     '"). See details in log. Try to make it manually: %s' % cmd)
        return 1
    elif not qconfig.debug:
        os.remove(db_gz_fpath)
        os.remove(silva_fpath)
    return 0


def parallel_blast(contigs_fpath, blast_res_fpath, err_fpath, blast_check_fpath, blast_threads):
    cmd = blast_fpath('blastn') + (' -query %s -db %s -outfmt 7 -num_threads %s' % (
            contigs_fpath, db_fpath, blast_threads))
    assembly_name = qutils.name_from_fpath(contigs_fpath)
    res_fpath = blast_res_fpath + '_' + assembly_name
    check_fpath =  blast_check_fpath + '_' + assembly_name
    logger.info('  ' + 'processing ' + assembly_name)
    qutils.call_subprocess(shlex.split(cmd), stdout=open(res_fpath, 'w'), stderr=open(err_fpath, 'a'), logger=logger)
    logger.info('  ' + 'BLAST results for %s are saved to %s...' % (assembly_name, res_fpath))
    with open(check_fpath, 'w') as check_file:
        check_file.writelines('Assembly: %s size: %d\n' % (contigs_fpath, os.path.getsize(contigs_fpath)))
    return


def check_blast(blast_check_fpath, files_sizes, assemblies_fpaths, assemblies):
    downloaded_organisms = []
    not_founded_organisms = []
    for assembly_fpath in assemblies_fpaths:
        assembly_name = qutils.name_from_fpath(assembly_fpath)
        check_fpath = blast_check_fpath  + '_' + assembly_name
        existing_assembly = None
        assembly_info = True
        if os.path.exists(check_fpath):
            for line in open(check_fpath):
                if '---' in line:
                    assembly_info = False
                if line and assembly_info:
                    assembly, size = line.split()[1], line.split()[3]
                    if assembly in files_sizes.keys() and int(size) == files_sizes[assembly]:
                        existing_assembly = assemblies_fpaths[assembly]
                        assembly_name = qutils.name_from_fpath(existing_assembly.fpath)
                        logger.info('  Using existing BLAST alignments for %s... ' % assembly_name)
                        assemblies.remove(existing_assembly)
                elif line and existing_assembly:
                    line = line.split(' ')
                    if len(line) > 1:
                        if line[0] == 'Downloaded:':
                            downloaded_organisms += line[1].rstrip().split(',')
                        elif line[0] == 'Not_founded:':
                            not_founded_organisms += line[1].rstrip().split(',')
    return assemblies, set(downloaded_organisms), set(not_founded_organisms)


def do(assemblies, downloaded_dirpath):
    logger.print_timestamp()
    err_fpath = os.path.join(downloaded_dirpath, 'blast.err')
    if not os.path.isdir(blastdb_dirpath):
        os.mkdir(blastdb_dirpath)
    if not os.path.isfile(db_fpath + '.nsq') or os.path.getsize(db_fpath + '.nsq') < db_nsq_fsize:
        return_code = download_blastdb()
        logger.info()
        if return_code != 0:
            return None

    blast_assemblies = assemblies[:]
    blast_check_fpath = os.path.join(downloaded_dirpath, 'blast.check')
    blast_res_fpath = os.path.join(downloaded_dirpath, 'blast.res')
    files_sizes = dict((assembly.fpath, os.path.getsize(assembly.fpath)) for assembly in assemblies)
    assemblies_fpaths = dict((assembly.fpath, assembly) for assembly in assemblies)
    contigs_names = [qutils.name_from_fpath(assembly.fpath) for assembly in assemblies]
    blast_assemblies, downloaded_organisms, not_founded_organisms = \
        check_blast(blast_check_fpath, files_sizes, assemblies_fpaths, blast_assemblies)

    if len(blast_assemblies) > 0:
        logger.info('Running BlastN..')
        n_jobs = min(qconfig.max_threads, len(blast_assemblies))
        blast_threads = max(1, qconfig.max_threads // n_jobs)
        from joblib import Parallel, delayed
        Parallel(n_jobs=n_jobs)(delayed(parallel_blast)(
                    assembly.fpath, blast_res_fpath, err_fpath, blast_check_fpath, blast_threads) for assembly in blast_assemblies)

    logger.info('')
    scores_organisms = []
    organisms_assemblies = {}
    for contig_name in contigs_names:
        all_scores = []
        organisms = []
        res_fpath = blast_res_fpath + '_' + contig_name
        if os.path.exists(res_fpath):
            for line in open(res_fpath):
                if not line.startswith('#'):
                    line = line.split()
                    idy = float(line[2])
                    length = int(line[3])
                    score = float(line[11])
                    if idy >= qconfig.identity_threshold and length >= qconfig.min_length and score >= qconfig.min_bitscore:  # and (not scores or min(scores) - score < max_identity_difference):
                        organism = line[1].split(';')[-1]
                        organism = re.sub('[\[\]]', '', organism)
                        specie = organism.split('_')
                        if len(specie) > 1 and 'uncultured' not in organism:
                            specie = specie[0] + '_' + specie[1]
                            if specie not in organisms:
                                all_scores.append((score, organism))
                                organisms.append(specie)
                            else:
                                tuple_scores = [x for x in all_scores if specie in x[1]]
                                if tuple_scores and score > tuple_scores[0][0]:
                                    all_scores.remove((tuple_scores[0][0], tuple_scores[0][1]))
                                    all_scores.append((score, organism))
        all_scores = sorted(all_scores, reverse=True)
        all_scores = all_scores[:qconfig.max_references]
        for score in all_scores:
            if not organisms_assemblies or (organisms_assemblies.values() and score[1] not in organisms_assemblies.values()[0]):
                scores_organisms.append(score)
        organisms_assemblies[contig_name] = [score[1] for score in all_scores]

    ref_fpaths = []
    downloaded_ref_fpaths = [os.path.join(downloaded_dirpath,file) for (path, dirs, files) in os.walk(downloaded_dirpath) for file in files if qutils.check_is_fasta_file(file)]
    if len(downloaded_ref_fpaths) > 0:
        logger.info('Trying to use previously downloaded references...')

    max_organism_name_len = 0
    for (score, organism) in scores_organisms:
        max_organism_name_len = max(len(organism), max_organism_name_len)
    for organism in downloaded_organisms:
        max_organism_name_len = max(len(organism), max_organism_name_len)
    scores_organisms = sorted(scores_organisms, reverse=True)

    total_downloaded = 0
    total_scored_left = len(scores_organisms)
    for organism in downloaded_organisms:
        ref_fpath = os.path.join(downloaded_dirpath, re.sub('[/.=]', '', organism) + '.fasta')
        if os.path.exists(ref_fpath):
            if len(ref_fpaths) == qconfig.max_references:
                break
            total_downloaded += 1
            if organisms_assemblies and organism in organisms_assemblies.values()[0]:
                total_scored_left -= 1
            spaces = (max_organism_name_len - len(organism)) * ' '
            logger.info("  %s%s | was downloaded previously (total %d)" %
                            (organism.replace('+', ' '), spaces, total_downloaded))
            ref_fpaths.append(ref_fpath)
        else:
            scores_organisms.insert(0, (5000, organism))

    if total_scored_left == 0:
        if not ref_fpaths:
            logger.info('Reference genomes are not found.')
        if not qconfig.debug and os.path.exists(err_fpath):
            os.remove(err_fpath)
        return ref_fpaths

    logger.print_timestamp()
    logger.info('Trying to download found references from NCBI. '
                'Totally ' + str(total_scored_left) + ' organisms to try.')

    for (score, organism) in scores_organisms:
        total_scored_left -= 1
        ref_fpath = os.path.join(downloaded_dirpath, re.sub('[/.=]', '', organism) + '.fasta')
        spaces = (max_organism_name_len - len(organism)) * ' '
        new_ref_fpath = None
        was_downloaded = False
        if not os.path.exists(ref_fpath) and organism not in not_founded_organisms:
            new_ref_fpath = download_refs(organism, ref_fpath)
        elif os.path.exists(ref_fpath) and organism not in downloaded_organisms:
            was_downloaded = True
            new_ref_fpath = ref_fpath
        if new_ref_fpath:
            total_downloaded += 1
            if was_downloaded:
                logger.info("  %s%s | was downloaded previously (total %d, %d more to go)" %
                            (organism.replace('+', ' '), spaces, total_downloaded, total_scored_left))
                if new_ref_fpath not in ref_fpaths:
                    ref_fpaths.append(new_ref_fpath)
            else:
                logger.info("  %s%s | successfully downloaded (total %d, %d more to go)" %
                        (organism.replace('+', ' '), spaces, total_downloaded, total_scored_left))
                ref_fpaths.append(new_ref_fpath)
            downloaded_organisms.add(organism)
        elif organism not in downloaded_organisms:
            logger.info("  %s%s | not found in the NCBI database" % (organism.replace('+', ' '), spaces))
            not_founded_organisms.add(organism)
    for contig_name in contigs_names:
        check_fpath = blast_check_fpath + '_' + contig_name
        with open(check_fpath) as check_file:
            text = check_file.read()
            text = text[:text.find('\n')]
        with open(check_fpath, 'w') as check_file:
            check_file.writelines(text)
            check_file.writelines('\n---\n')
            cur_downloaded_organisms = [organism for organism in downloaded_organisms if organism in organisms_assemblies[contig_name]]
            cur_not_founded_organisms = [organism for organism in not_founded_organisms if organism in organisms_assemblies[contig_name]]
            check_file.writelines('Downloaded: %s\n' % ','.join(cur_downloaded_organisms))
            check_file.writelines('Not_founded: %s\n' % ','.join(cur_not_founded_organisms))

    if not ref_fpaths:
        logger.info('Reference genomes are not found.')
    if not qconfig.debug and os.path.exists(err_fpath):
        os.remove(err_fpath)
    return ref_fpaths
