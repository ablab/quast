import shutil
import os
import quast_libs.viralquast.quast_functions as quast_functions
from quast_libs import qconfig


class QuastRanger:
    def __init__(self):
        if qconfig.output_dirpath is None:
            self.output_dir = 'viralquast_results'
        else:
            self.output_dir = qconfig.output_dirpath

    def run_quast(self, contigs_fpath, samples_paths):
        tmp_dirname = '{}/quast_all_reports'.format(self.output_dir)
        threads = qconfig.max_threads if qconfig.max_threads is not None else 1
        try:
            os.mkdir(tmp_dirname)
        except:
            shutil.rmtree(tmp_dirname)
            os.mkdir(tmp_dirname)
        names = []

        quast_options = qconfig.quast_options
        quast_args = []
        if quast_options is not None:
            quast_args = quast_options.split()
            quast_args = self.fix_args(quast_args)

        for sample_path in samples_paths:
            sample_name = sample_path.split('/')[-1].split('.fasta')[0]
            names.append(sample_name)
            args = ['-R', sample_path, contigs_fpath, '-o', '{}/quast_all_reports/{}'.format(self.output_dir, sample_name),
                    '-t', str(threads)]
            args += quast_args
            quast_functions.my_main(args)
        return names

    def fix_args(self, args):
        R_pos = -1
        o_pos = -1
        for i, arg in enumerate(args):
            if arg == '-R' or arg == '-r' or arg == '--reference':
                R_pos = i
        if R_pos != -1:
            args = args[:R_pos] + args[R_pos + 1:]
        for i, arg in enumerate(args):
            if arg == '-0' or arg == '--output-dir':
                o_pos = i
        if o_pos != -1:
            args = args[:o_pos] + args[o_pos + 1:]
        while '--test' in args:
            args.remove('--test')
        return args


    def parse_reports(self, names):
        results = {}
        for sample_name in names:
            results[sample_name] = {}
            with open('{}/quast_all_reports/{}/report.txt'.format(self.output_dir, sample_name)) as f:
                for line in f:
                    if len(line.split()) > 0 and line.split()[0] == 'Genome':
                        results[sample_name]['Genome fraction'] = line.split()[-1]
                    elif len(line.split()) > 1 and line.split()[1] == 'misassemblies':
                        results[sample_name]['misassemblies'] = line.split()[-1]
                    elif len(line.split()) > 1 and line.split()[1] == 'mismatches':
                        results[sample_name]['mismatches per 100 kbp'] = line.split()[-1]
                    elif len(line.split()) > 1 and line.split()[0] == 'Reference' and line.split()[1] == 'length':
                        results[sample_name]['Reference length'] = line.split()[-1]
                    elif len(line.split()) > 1 and line.split()[1] == 'indels':
                        results[sample_name]['indels per 100 kbp'] = line.split()[-1]
                try:
                    results[sample_name]['score'] = int(results[sample_name]['Reference length']) * \
                                                    float(results[sample_name]['Genome fraction']) * \
                                                    (100000 - float(results[sample_name]['mismatches per 100 kbp'])
                                                     - float(results[sample_name]['indels per 100 kbp'])) / 100000 / 100
                except Exception as ex:
                    pass
        sorted_res = list(sorted(results.items(), key=lambda x: -x[1]['score'] if 'score' in x[1] else 0))
        return sorted_res[0]