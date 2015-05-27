import os
import shlex
from libs import qconfig, qutils
from libs.log import get_logger
logger = get_logger(qconfig.LOGGER_META_NAME)
from urllib2 import urlopen
import xml.etree.ElementTree as ET
import shutil
import tarfile


def blast_fpath(fname):
    blast_dirpath = os.path.join(qconfig.LIBS_LOCATION, 'blast', qconfig.platform_name)
    return os.path.join(blast_dirpath, fname)


def download_refs(ref_fpaths, organism, downloaded_dirpath):
    ncbi_url = 'http://eutils.ncbi.nlm.nih.gov/entrez/eutils/'
    ref_fpath = os.path.join(downloaded_dirpath, organism.replace('/','') + '.fasta')
    organism = organism.replace('_', '+')
    request = urlopen(ncbi_url + 'esearch.fcgi?db=genome&term=%s' % organism)
    response = request.read()
    xml_tree = ET.fromstring(response)
    if xml_tree.find('Count').text == '0': #  Organism is not found
        return ref_fpaths
    ref_id = xml_tree.find('IdList').find('Id').text
    request = urlopen(ncbi_url + 'elink.fcgi?dbfrom=genome&db=nuccore&id=%s&term='
                                 '(gene+in+chromosome[prop]+OR+gene+in+genomic[prop])\+AND+srcdb+refseq[prop]' % ref_id)
    response = request.read()
    xml_tree = ET.fromstring(response)
    if xml_tree[0][2][2].tag == 'Info': #  No reference genome on NCBI site
        return ref_fpaths
    ref_id = xml_tree.find('LinkSet').find('LinkSetDb').find('Link').find('Id').text
    request = urlopen(ncbi_url + 'efetch.fcgi?db=nuccore&id=%s&rettype=fasta&retmode=text' % ref_id)
    fasta = request.read()
    if fasta:
        with open(ref_fpath, "w") as fasta_file:
            fasta_file.write(fasta)
        ref_fpaths.append(ref_fpath)
        logger.info('  Successfully downloaded %s' % organism.replace('+', ' '))
    return ref_fpaths


def do(contigs_fpaths, downloaded_dirpath):
    logger.print_timestamp()
    blastdb_dirpath = os.path.join(qconfig.LIBS_LOCATION, 'blast', '16S_RNA_blastdb')
    db_fpath = os.path.join(blastdb_dirpath, 'silva_119.db')
    if not os.path.isfile(db_fpath + '.nsq'):
        db_gz_fpath = os.path.join(blastdb_dirpath, 'blastdb.tar.gz')
        db_files = [os.path.join(blastdb_dirpath, 'blastdb_1.tar.gz'),
                    os.path.join(blastdb_dirpath, 'blastdb_2.tar.gz')]
        with open(db_gz_fpath, 'wb') as zipfile:
            for file in db_files:
                with open(file, 'rb') as splitfile:
                    shutil.copyfileobj(splitfile, zipfile)
                os.remove(file)
        tar = tarfile.open(db_gz_fpath, 'r:gz')
        for item in tar:
            tar.extract(item, blastdb_dirpath)
        os.remove(db_gz_fpath)
    logger.info('Running BlastN..')
    err_fpath = os.path.join(downloaded_dirpath, 'blast.err')
    blast_res_fpath = os.path.join(downloaded_dirpath, 'blast.res')
    for index, contigs_fpath in enumerate(contigs_fpaths):
        cmd = blast_fpath('blastn') + (' -query %s -db %s -outfmt 7' % (
            contigs_fpath, db_fpath))
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
                if 'uncultured' not in organism:
                    specie = organism.split('_')
                    if len(specie) > 1:
                        specie = specie[0] + specie[1]
                    else:
                        specie = specie[0]
                    if specie not in organisms:
                        scores_organisms.append((score, organism))
                        organisms.append(specie)
    logger.print_timestamp()
    logger.info('Trying to download found references from NCBI..')
    for (score, organism) in sorted(scores_organisms, reverse=True):
        if len(ref_fpaths) == qconfig.max_references:
            break
        ref_fpaths = download_refs(ref_fpaths, organism, downloaded_dirpath)
    if not ref_fpaths:
        logger.info('Reference genomes are not found.')
    if not qconfig.debug:
        os.remove(blast_res_fpath)
        os.remove(err_fpath)
    return ref_fpaths
