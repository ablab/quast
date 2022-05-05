import sys
import gzip
import os
import subprocess
import multiprocessing
import shutil

from Bio import SeqIO
from quast_libs import qconfig

def run_mash(task):
    if qconfig.mash_path is None:
        mash_path = mash_fpath()
    else:
        mash_path = qconfig.mash_path
    if mash_path is None:
        raise Exception('Can not find mash executable')
    idx = task[0]
    tasks = task[1]
    for t in tasks:
        subprocess.run([mash_path, "sketch", "-o", "ref{}".format(idx), "ref{}.msh".format(idx), t])

def cut_sample(task):
    k, v = task
    v2 = sorted(v, key=lambda x: x.name.replace('|', '_').replace('/', '_'))
    filename = "cutted/{}.fasta".format(v2[0].name.replace('|', '_').replace('/', '_'))
    with open(filename, "w") as f:
        SeqIO.write(v2, f, "fasta")
    return filename

def preprocess(option, opt_str, value, logger, threads=4):

    parts_count = 1 if threads is None else threads

    reference_path = value

    logger.info("Starting preprocessing\n")

    d = {}
    fasta_sequences = SeqIO.parse(gzip.open(reference_path, 'rt'), 'fasta')
    for seq in fasta_sequences:
        try:
            name = seq.description.split(" (")[1].split(") ")[0]
        except:
            continue
        if name not in d:
            d[name] = [seq]
        else:
            d[name].append(seq)

    try:
        os.mkdir("cutted")
    except:
        pass

    with multiprocessing.Pool(parts_count) as p:
        filenames = p.map(cut_sample, d.items())

    if qconfig.mash_path is None:
        mash_path = mash_fpath()
    else:
        mash_path = qconfig.mash_path

    subprocess.run([mash_path, "sketch", "-o", "ref", filenames[0]])

    for i in range(1, min(len(filenames), parts_count + 1)):
        subprocess.run([mash_path, "sketch", "-o", "ref{}".format(i), filenames[i]])

    tasks = {i: [] for i in range(1, parts_count + 1)}
    for i, filename in enumerate(filenames[parts_count + 1:]):
        tasks[i % parts_count + 1].append(filename)

    with multiprocessing.Pool(parts_count) as p:
        # TODO: change
        p.map(run_mash, tasks.items())

    for i in range(parts_count):
        subprocess.run([mash_path, "sketch", "-o", "ref", "ref.msh", "ref{}.msh".format(i + 1)])
        os.remove("ref{}.msh".format(i + 1))

    shutil.rmtree("cutted")
    new_path = '/'.join(reference_path.split('/')[:-1]) + '/' + reference_path.split('/')[-1].split('.')[0] + '.msh'
    shutil.move('ref.msh', new_path)
    logger.info("Preprocessing done! New reference path: {}".format(new_path))
    return new_path
