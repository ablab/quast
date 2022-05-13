import subprocess
import gzip
import os
import shutil

from quast_libs import qconfig
from quast_libs.ca_utils.misc import mash_fpath

from Bio import SeqIO
from quast_libs.viralquast.quast_ranger import QuastRanger
from quast_libs.viralquast.reference_finder import ReferenceFinder
from typing import List


class MashReferenceFinder(ReferenceFinder):

    def __init__(self, *args):
        super().__init__(*args)
        if qconfig.mash_path is None:
            self.mash_path = mash_fpath()
        else:
            # mash_path = 'external_tools/mash/mash-Linux64-v2.3/mash'
            self.mash_path = qconfig.mash_path
        if self.mash_path is None:

            raise Exception('Can not find mash, try to run "sudo apt update & sudo apt install mash" command in terminal,'
                            'or install it manually from https://github.com/marbl/Mash')

    def check_reference(self, contigs_fpath: str, mash_reference_fpath: str) -> bool:
        res = subprocess.check_output([self.mash_path, 'dist', mash_reference_fpath, contigs_fpath], universal_newlines=True)
        res = res.split('\n')[:-1]
        name = res[0].split()[0].split('/')[-1].split('.fasta')[0]
        fasta_sequences = SeqIO.parse(gzip.open(self.ref_path, 'rt'), 'fasta')
        for seq in fasta_sequences:
            name_ = seq.name.replace('|', '_').replace('/', '_')
            if seq.name == name or name_ == name:
                return True
        return False

    def find_reference(self, contigs_fpath: str, mash_reference_fpath: str):
        save_all_reports = qconfig.save_all_reports
        if not self.check_reference(contigs_fpath, mash_reference_fpath):
            raise Exception('Looks like references do not match, exiting')

        mash_output = subprocess.check_output([self.mash_path, 'dist', mash_reference_fpath, contigs_fpath], universal_newlines=True)
        mash_output = mash_output.split('\n')[:-1]
        mash_output = list(sorted(mash_output, key=lambda x: float(x.split('\t')[2])))
        names = []
        for line in mash_output[:30]:
            name = line.split()[0].split('/')[-1].split('.fasta')[0]
            names.append(name)
        samples_paths = self.cut_samples(names)
        ranger = QuastRanger()
        quast_files_names = ranger.run_quast(contigs_fpath, samples_paths)
        mash_output = ranger.parse_reports(quast_files_names)
        shutil.move('{}/cutted_tmp/{}.fasta'.format(self.output_dir, mash_output[0]),
                    '{}/cutted_result.fasta'.format(self.output_dir))
        shutil.move('{}/quast_all_reports/{}'.format(self.output_dir, mash_output[0]),
                    '{}/quast_best_results'.format(self.output_dir))
        shutil.rmtree('{}/cutted_tmp'.format(self.output_dir))
        if save_all_reports is False:
            shutil.rmtree('{}/quast_all_reports'.format(self.output_dir))


    def cut_samples(self, names: List[str]) -> List[str]:
        fasta_sequences = SeqIO.parse(gzip.open(self.ref_path, 'rt'), 'fasta')
        seqs = {n: [] for n in names}
        seqs_descrs = {}
        for seq in fasta_sequences:
            name = seq.name.replace('|', '_').replace('/', '_')
            try:
                descr = seq.description.split(' (')[1].split(') ')[0]
            except:
                continue
            if name in names or seq.name in names:
                if 'mixed' in seq.description or len(seq.seq) > 15000:
                    names.remove(name)
                    continue
                if descr not in seqs_descrs:
                    seqs_descrs[descr] = name
            if descr in seqs_descrs:
                seqs[seqs_descrs[descr]].append((name, seq))
        try:
            os.mkdir('{}/cutted_tmp'.format(self.output_dir))
        except:
            shutil.rmtree('{}/cutted_tmp'.format(self.output_dir))
            os.mkdir('{}/cutted_tmp'.format(self.output_dir))
        cutted_filenames_paths = []
        for name in names[:10]:
            seqs_ = map(lambda x: x[1], sorted(seqs[name], key=lambda x:x[0]))
            fixed_name = name.replace('|', '_')
            SeqIO.write(seqs_, '{}/cutted_tmp/{}.fasta'.format(self.output_dir, fixed_name), 'fasta')
            cutted_filenames_paths.append('{}/cutted_tmp/{}.fasta'.format(self.output_dir, fixed_name))
        return cutted_filenames_paths
