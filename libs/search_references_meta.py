############################################################################
# Copyright (c) 2011-2015 Saint-Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################

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

if platform.system() == 'Darwin':
    sed_cmd = "sed -i '' "
else:
    sed_cmd = 'sed -i '


def blast_fpath(fname):
    blast_dirpath = os.path.join(qconfig.LIBS_LOCATION, 'blast', qconfig.platform_name)
    return os.path.join(blast_dirpath, fname)


def download_refs(organism, downloaded_dirpath):
    ncbi_url = 'http://eutils.ncbi.nlm.nih.gov/entrez/eutils/'
    ref_fpath = os.path.join(downloaded_dirpath, re.sub('[/=]', '', organism) + '.fasta')
    organism = organism.replace('_', '+')
    request = urlopen(ncbi_url + 'esearch.fcgi?db=assembly&term=%s+[Organism]&retmax=100' % organism)
    response = request.read()
    xml_tree = ET.fromstring(response)

    if xml_tree.find('Count').text == '0':  # Organism is not found
        return None

    ref_id = xml_tree.find('IdList').find('Id').text
    request = urlopen(
        ncbi_url + 'elink.fcgi?dbfrom=assembly&db=nuccore&id=%s&linkname="assembly_nuccore_refseq"' % ref_id)
    response = request.read()
    xml_tree = ET.fromstring(response)

    link_set = xml_tree.find('LinkSet')
    if link_set is None:
        return None

    link_db = xml_tree.find('LinkSet').find('LinkSetDb')
    if link_db is None:
        return None

    for ref_id in sorted(ref_id.find('Id').text for ref_id in link_db.findall('Link')):
        request = urlopen(ncbi_url + 'efetch.fcgi?db=sequences&id=%s&rettype=fasta&retmode=text' % ref_id)
        fasta = request.read()
        if fasta:
            if 'complete genome' in fasta[:100]:
                with open(ref_fpath, "w") as fasta_file:
                    fasta_file.write(fasta)
                break
            else:
                with open(ref_fpath, "a") as fasta_file:
                    fasta_file.write(fasta)

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
        with open(silva_fpath + ".unpacked", "wb") as db_file:
            f = gzip.open(db_gz_fpath, 'rb')
            db_file.write(f.read())
        cmd = sed_cmd + " 's/ /_/g' %s" % (silva_fpath + ".unpacked")
        qutils.call_subprocess(shlex.split(cmd), stdout=open(log_fpath, 'a'), stderr=open(log_fpath, 'a'))
        shutil.move(silva_fpath + ".unpacked", silva_fpath)

    logger.info('Making BLAST database...')
    cmd = blast_fpath('makeblastdb') + (' -in %s -dbtype nucl -out %s' % (silva_fpath, db_fpath))
    qutils.call_subprocess(shlex.split(cmd), stdout=open(log_fpath, 'w'), stderr=open(log_fpath, 'a'))
    if not os.path.exists(db_fpath + '.nsq'):
        logger.error('Failed to make BLAST database ("' + blastdb_dirpath +
                     '"). See details in log. Try to make it manually: %s' % cmd)
        return 1
    else:
        os.remove(db_gz_fpath)
        os.remove(silva_fpath)
    return 0


def do(assemblies, downloaded_dirpath):
    logger.print_timestamp()
    err_fpath = os.path.join(downloaded_dirpath, 'blast.err')
    if not os.path.isdir(blastdb_dirpath):
        os.mkdir(blastdb_dirpath)
    if not os.path.isfile(db_fpath + '.nsq'):
        return_code = download_blastdb()
        logger.info()
        if return_code != 0:
            return None

    logger.info('Running BlastN..')
    blast_res_fpath = os.path.join(downloaded_dirpath, 'blast.res')
    for index, assembly in enumerate(assemblies):
        contigs_fpath = assembly.fpath
        cmd = blast_fpath('blastn') + (' -query %s -db %s -outfmt 7 -num_threads %s' % (
            contigs_fpath, db_fpath, qconfig.max_threads))
        assembly_name = qutils.name_from_fpath(contigs_fpath)
        logger.info('  ' + 'processing ' + assembly_name)
        qutils.call_subprocess(shlex.split(cmd), stdout=open(blast_res_fpath, 'a'), stderr=open(err_fpath, 'a'))
    logger.info('')
    organisms = []
    scores_organisms = []
    ref_fpaths = []
    for line in open(blast_res_fpath):
        if not line.startswith('#'):
            line = line.split()
            idy = float(line[2])
            length = int(line[3])
            score = float(line[11])
            if idy >= qconfig.identity_threshold and length >= qconfig.min_length and score >= qconfig.min_bitscore:  # and (not scores or min(scores) - score < max_identity_difference):
                organism = line[1].split(';')[-1]
                specie = organism.split('_')
                if len(specie) > 1 and 'uncultured' not in organism:
                    specie = specie[0] + '_' + specie[1]
                    if specie not in organisms:
                        scores_organisms.append((score, organism))
                        organisms.append(specie)
                    else:
                        tuple_scores = [x for x in scores_organisms if specie in x[1]]
                        if tuple_scores and score > tuple_scores[0][0]:
                            scores_organisms.remove((tuple_scores[0][0], tuple_scores[0][1]))
                            scores_organisms.append((score, organism))

    logger.print_timestamp()
    total_scored_left = len(scores_organisms)
    total_needed = min(total_scored_left, qconfig.max_references)
    logger.info('Trying to download found references from NCBI. '
                'Totally ' + str(total_needed) + ' organisms to try.')
    scores_organisms = sorted(scores_organisms, reverse=True)
    max_organism_name_len = 0
    for (score, organism) in scores_organisms:
        max_organism_name_len = max(len(organism), max_organism_name_len)

    total_downloaded = 0
    for (score, organism) in scores_organisms:
        total_scored_left -= 1
        if len(ref_fpaths) == qconfig.max_references:
            break

        new_ref_fpath = download_refs(organism, downloaded_dirpath)
        spaces = (max_organism_name_len - len(organism)) * ' '
        if new_ref_fpath:
            total_downloaded += 1
            total_needed = min(total_scored_left, qconfig.max_references)
            logger.info("  %s%s | successfully downloaded (total %d, %d more to go)" %
                        (organism.replace('+', ' '), spaces, total_downloaded, total_needed))
            ref_fpaths.append(new_ref_fpath)
        else:
            logger.info("  %s%s | not found in the NCBI database" % (organism.replace('+', ' '), spaces))

    if not ref_fpaths:
        logger.info('Reference genomes are not found.')
    if not qconfig.debug:
        os.remove(blast_res_fpath)
        os.remove(err_fpath)
    return ref_fpaths
