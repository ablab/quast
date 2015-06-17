############################################################################
# Copyright (c) 2011-2015 Saint-Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################

import os
import shlex
import sys
import platform
import re
from libs import qconfig, qutils
from libs.log import get_logger
logger = get_logger(qconfig.LOGGER_META_NAME)
from urllib2 import urlopen
import xml.etree.ElementTree as ET

silva_db_path = 'http://www.arb-silva.de/fileadmin/silva_databases/release_119/Exports/'
if platform.system() == 'Darwin':
    sed_cmd = "sed -i '' "
else:
    sed_cmd = 'sed -i '

def blast_fpath(fname):
    blast_dirpath = os.path.join(qconfig.LIBS_LOCATION, 'blast', qconfig.platform_name)
    return os.path.join(blast_dirpath, fname)


def download_refs(ref_fpaths, organism, downloaded_dirpath):
    ncbi_url = 'http://eutils.ncbi.nlm.nih.gov/entrez/eutils/'
    ref_fpath = os.path.join(downloaded_dirpath, re.sub('[/=]', '', organism) + '.fasta')
    organism = organism.replace('_', '+')
    request = urlopen(ncbi_url + 'esearch.fcgi?db=assembly&term=%s+[Organism]&retmax=100' % organism)
    response = request.read()
    xml_tree = ET.fromstring(response)
    if xml_tree.find('Count').text == '0': #  Organism is not found
        logger.info("  %s is not found in NCBI's database" % organism.replace('+', ' '))
        return ref_fpaths
    ref_id = xml_tree.find('IdList').find('Id').text
    request = urlopen(ncbi_url + 'elink.fcgi?dbfrom=assembly&db=nuccore&id=%s&linkname="assembly_nuccore_refseq"' % ref_id)
    response = request.read()
    xml_tree = ET.fromstring(response)
    refs_id = sorted([ref_id.find('Id').text for ref_id in xml_tree.find('LinkSet').find('LinkSetDb').findall('Link')])
    for ref_id in sorted(refs_id):
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
    if os.path.exists(ref_fpath):
        ref_fpaths.append(ref_fpath)
        logger.info('  Successfully downloaded %s' % organism.replace('+', ' '))
    else:
        logger.info("  %s is not found in NCBI's database" % organism.replace('+', ' '))
    return ref_fpaths


def show_progress(a,b,c):
    print "% 3.1f%% of %d bytes\r" % (min(100, float(a * b) / c * 100), c),
    sys.stdout.flush()


def do(assemblies, downloaded_dirpath):
    logger.print_timestamp()
    blastdb_dirpath = os.path.join(qconfig.LIBS_LOCATION, 'blast', '16S_RNA_blastdb')
    db_fpath = os.path.join(blastdb_dirpath, 'silva_119.db')
    err_fpath = os.path.join(downloaded_dirpath, 'blast.err')
    if not os.path.isdir(blastdb_dirpath):
        os.mkdir(blastdb_dirpath)

    if not os.path.isfile(db_fpath + '.nsq'):
        log_fpath = os.path.join(downloaded_dirpath, 'blastdb.log')
        logger.info("Downloading SILVA ribosomal RNA gene database...")
        silva_fname = 'SILVA_119_SSURef_Nr99_tax_silva.fasta'
        db_gz_fpath = os.path.join(blastdb_dirpath, silva_fname + '.gz')
        silva_fpath = os.path.join(blastdb_dirpath, silva_fname)
        import urllib, gzip
        silva_download = urllib.URLopener()
        silva_download.retrieve(silva_db_path + silva_fname + '.gz', db_gz_fpath, show_progress)
        with open(silva_fpath, "wb") as db_file:
            f = gzip.open(db_gz_fpath, 'rb')
            db_file.write(f.read())
        cmd = sed_cmd + " 's/ /_/g' %s" % silva_fpath
        qutils.call_subprocess(shlex.split(cmd), stdout=open(log_fpath, 'a'), stderr=open(err_fpath, 'a'))
        cmd = blast_fpath('makeblastdb') + (' -in %s -dbtype nucl -out %s' % (silva_fpath, db_fpath))
        qutils.call_subprocess(shlex.split(cmd), stdout = open(log_fpath, 'w'), stderr=open(err_fpath, 'a'))
        os.remove(db_gz_fpath)
        os.remove(silva_fpath)

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
            if idy >= qconfig.identity_threshold and length >= qconfig.min_length and score >= qconfig.min_bitscore: #  and (not scores or min(scores) - score < max_identity_difference):
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
    logger.info('Trying to download found references from NCBI..')
    scores_organisms = sorted(scores_organisms, reverse=True)
    for (score, organism) in scores_organisms:
        if len(ref_fpaths) == qconfig.max_references:
            break
        ref_fpaths = download_refs(ref_fpaths, organism, downloaded_dirpath)
    if not ref_fpaths:
        logger.info('Reference genomes are not found.')
    if not qconfig.debug:
        os.remove(blast_res_fpath)
        os.remove(err_fpath)
    return ref_fpaths
