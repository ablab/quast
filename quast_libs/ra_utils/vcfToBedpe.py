#!/usr/bin/env python

import argparse, sys
import math, time, re
from argparse import RawTextHelpFormatter

__author__ = "Colby Chiang (cc2qe@virginia.edu)"
__version__ = "$Revision: 0.0.1 $"
__date__ = "$Date: 2014-04-23 14:31 $"

# --------------------------------------
# define functions

def get_args():
    parser = argparse.ArgumentParser(formatter_class=RawTextHelpFormatter, description="\
vcfToBedpe.py\n\
author: " + __author__ + "\n\
version: " + __version__ + "\n\
description: Convert a VCF file to a BEDPE file")
    parser.add_argument('-i', '--input', type=argparse.FileType('r'), default=None, help='VCF input (default: stdin)')
    parser.add_argument('-o', '--output', type=argparse.FileType('w'), default=sys.stdout, help='Output BEDPE to write (default: stdout)')

    # parse the arguments
    args = parser.parse_args()

    # if no input, check if part of pipe and if so, read stdin.
    if args.input == None:
        if sys.stdin.isatty():
            parser.print_help()
            exit(1)
        else:
            args.input = sys.stdin

    # send back the user input
    return args

class Vcf(object):
    def __init__(self):
        self.file_format = 'VCFv4.2'
        # self.fasta = fasta
        self.reference = ''
        self.sample_list = []
        self.info_list = []
        self.format_list = []
        self.alt_list = []
        self.add_format('GT', 1, 'String', 'Genotype')

    def add_header(self, header):
        for line in header:
            if line.split('=')[0] == '##fileformat':
                self.file_format = line.rstrip().split('=')[1]
            elif line.split('=')[0] == '##reference':
                self.reference = line.rstrip().split('=')[1]
            elif line.split('=')[0] == '##INFO':
                a = line[line.find('<')+1:line.find('>')]
                r = re.compile(r'(?:[^,\"]|\"[^\"]*\")+')
                self.add_info(*[b.split('=')[1] for b in r.findall(a)])
            elif line.split('=')[0] == '##ALT':
                a = line[line.find('<')+1:line.find('>')]
                r = re.compile(r'(?:[^,\"]|\"[^\"]*\")+')
                self.add_alt(*[b.split('=')[1] for b in r.findall(a)])
            elif line.split('=')[0] == '##FORMAT':
                a = line[line.find('<')+1:line.find('>')]
                r = re.compile(r'(?:[^,\"]|\"[^\"]*\")+')
                d = [b.split('=') for b in r.findall(a) if len(b.split('=')) > 0]
                self.add_format(*[b.split('=')[1] for b in r.findall(a) if len(b.split('=')) > 1])
            elif line[0] == '#' and line[1] != '#':
                self.sample_list = [' '.join(line.rstrip().split('\t')[9:])]
    # return the VCF header
    def get_header(self):
        header = '\n'.join(['##fileformat=' + self.file_format,
                            '##fileDate=' + time.strftime('%Y%m%d'),
                            '##reference=' + self.reference] + \
                           [i.hstring for i in self.info_list] + \
                           [a.hstring for a in self.alt_list] + \
                           [f.hstring for f in self.format_list] + \
                           ['\t'.join([
                               '#CHROM',
                               'POS',
                               'ID',
                               'REF',
                               'ALT',
                               'QUAL',
                               'FILTER',
                               'INFO',
                               'FORMAT'] + \
                                      self.sample_list
                                  )])
        return header

    def add_info(self, id, number, type, desc):
        if id not in [i.id for i in self.info_list]:
            inf = Info(id, number, type, desc)
            self.info_list.append(inf)

    def add_alt(self, id, desc):
        if id not in [a.id for a in self.alt_list]:
            alt = Alt(id, desc)
            self.alt_list.append(alt)

    def add_format(self, id, number, type, desc):
        if id not in [f.id for f in self.format_list]:
            fmt = Format(id, number, type, desc)
            self.format_list.append(fmt)

    def add_sample(self, name):
        self.sample_list.append(name)

class Info(object):
    def __init__(self, id, number, type, desc):
        self.id = str(id)
        self.number = str(number)
        self.type = str(type)
        self.desc = str(desc)
        # strip the double quotes around the string if present
        if self.desc.startswith('"') and self.desc.endswith('"'):
            self.desc = self.desc[1:-1]
        self.hstring = '##INFO=<ID=' + self.id + ',Number=' + self.number + ',Type=' + self.type + ',Description=\"' + self.desc + '\">'

class Alt(object):
    def __init__(self, id, desc):
        self.id = str(id)
        self.desc = str(desc)
        # strip the double quotes around the string if present
        if self.desc.startswith('"') and self.desc.endswith('"'):
            self.desc = self.desc[1:-1]
        self.hstring = '##ALT=<ID=' + self.id + ',Description=\"' + self.desc + '\">'

class Format(object):
    def __init__(self, id, number, type, desc):
        self.id = str(id)
        self.number = str(number)
        self.type = str(type)
        self.desc = str(desc)
        # strip the double quotes around the string if present
        if self.desc.startswith('"') and self.desc.endswith('"'):
            self.desc = self.desc[1:-1]
        self.hstring = '##FORMAT=<ID=' + self.id + ',Number=' + self.number + ',Type=' + self.type + ',Description=\"' + self.desc + '\">'

class Variant(object):
    def __init__(self, var_list, vcf):
        self.chrom = var_list[0]
        self.pos = int(var_list[1])
        self.var_id = var_list[2]
        self.ref = var_list[3]
        self.alt = var_list[4]
        self.qual = var_list[5]
        self.filter = var_list[6]
        self.sample_list = vcf.sample_list
        self.info_list = vcf.info_list
        self.info = dict()
        self.format_list = vcf.format_list
        self.active_formats = list()
        self.gts = dict()
        # make a genotype for each sample at variant
        for i in range(len(self.sample_list)):
            s_gt = var_list[9+i].split(':')[0]
            s = self.sample_list[i]
            self.gts[s] = Genotype(self, s, s_gt)
        # import the existing fmt fields
        for i in range(len(self.sample_list)):
            s = self.sample_list[i]
            for j in zip(var_list[8].split(':'), var_list[9+i].split(':')):
                self.gts[s].set_format(j[0], j[1])

        self.info = dict()
        i_split = [a.split('=') for a in var_list[7].split(';')] # temp list of split info column
        for i in i_split:
            if len(i) == 1:
                i.append(True)
            self.info[i[0]] = i[1]

    def set_info(self, field, value):
        if field in [i.id for i in self.info_list]:
            self.info[field] = value
        else:
            sys.stderr.write('\nError: invalid INFO field, \"' + field + '\"\n')
            exit(1)

    def get_info(self, field):
        return self.info[field]

    def get_info_string(self):
        i_list = list()
        for info_field in self.info_list:
            if info_field.id in self.info.keys():
                if info_field.type == 'Flag':
                    i_list.append(info_field.id)
                else:
                    i_list.append('%s=%s' % (info_field.id, self.info[info_field.id]))
        return ';'.join(i_list)

    def get_format_string(self):
        f_list = list()
        for f in self.format_list:
            if f.id in self.active_formats:
                f_list.append(f.id)
        return ':'.join(f_list)

    def genotype(self, sample_name):
        if sample_name in self.sample_list:
            return self.gts[sample_name]
        else:
            sys.stderr.write('\nError: invalid sample name, \"' + sample_name + '\"\n')

    def get_var_string(self):
        s = '\t'.join(map(str,[
            self.chrom,
            self.pos,
            self.var_id,
            self.ref,
            self.alt,
            '%0.2f' % self.qual,
            self.filter,
            self.get_info_string(),
            self.get_format_string(),
            '\t'.join(self.genotype(s).get_gt_string() for s in self.sample_list)
        ]))
        return s

class Genotype(object):
    def __init__(self, variant, sample_name, gt):
        self.format = dict()
        self.variant = variant
        self.set_format('GT', gt)

    def set_format(self, field, value):
        if field in [i.id for i in self.variant.format_list]:
            self.format[field] = value
            if field not in self.variant.active_formats:
                self.variant.active_formats.append(field)
                # sort it to be in the same order as the format_list in header
                self.variant.active_formats.sort(key=lambda x: [f.id for f in self.variant.format_list].index(x))
        # else:
        #     sys.stderr.write('\nError: invalid FORMAT field, \"' + field + '\"\n')
        #     exit(1)

    def get_format(self, field):
        return self.format[field]

    def get_gt_string(self):
        g_list = list()
        for f in self.variant.active_formats:
            if f in self.format:
                if type(self.format[f]) == float:
                    g_list.append('%0.2f' % self.format[f])
                else:
                    g_list.append(self.format[f])
            else:
                g_list.append('.')
        return ':'.join(map(str,g_list))

# primary function
def vcfToBedpe(vcf_file, bedpe_out):
    vcf = Vcf()
    in_header = True
    header = []

    for line in vcf_file:
        if in_header:
            if line[0] == '#':
                header.append(line)
                if line[1] != '#':
                    sample_list = [' '.join(line.rstrip().split('\t')[9:])]
                continue
            else:
                # print header
                if len(sample_list) > 0:
                    bedpe_out.write('\t'.join(['#CHROM_A',
                                               'START_A',
                                               'END_A',
                                               'CHROM_B',
                                               'START_B',
                                               'END_B',
                                               'ID',
                                               'QUAL',
                                               'STRAND_A',
                                               'STRAND_B',
                                               'TYPE',
                                               'FILTER',
                                               'INFO',
                                               'FORMAT'] +
                                               sample_list
                                              ) + '\n')
                else:
                    bedpe_out.write('\t'.join(['#CHROM_A',
                                               'START_A',
                                               'END_A',
                                               'CHROM_B',
                                               'START_B',
                                               'END_B',
                                               'ID',
                                               'QUAL',
                                               'STRAND_A',
                                               'STRAND_B',
                                               'TYPE',
                                               'FILTER',
                                               'INFO']
                                              ) + '\n')
                    
                in_header = False
                vcf.add_header(header)

        v = line.rstrip().split('\t')
        var = Variant(v, vcf)

        if var.info['SVTYPE'] != 'BND':
            b1 = var.pos
            b2 = int(var.info['END'])
            name = v[2]
            score = v[5]
            additional_fs = v[7:]

            if 'CIPOS' in var.info:
                span = [int(v) for v in var.info['CIPOS'].split(',')]
                s1 = b1 + span[0] - 1
                e1 = b1 + span[1]
            else:
                s1 = b1
                e1 = b1

            if 'CIEND' in var.info:
                span = [int(v) for v in var.info['CIEND'].split(',')]
                s2 = b2 + span[0] - 1
                e2 = b2 + span[1]
            else:
                s2 = b2
                e2 = b2

            ispan = s2 - e1
            ospan = e2 - s1

            # write bedpe
            bedpe_out.write('\t'.join(map(str,
                                          [var.chrom,
                                           s1,
                                           e1,
                                           var.chrom,
                                           s2,
                                           e2,
                                           name,
                                           var.qual,
                                           var.info['SVTYPE'],
                                           var.filter] +
                                           additional_fs
                                          )) + '\n')
        else:
            if 'SECONDARY' in var.info:
                continue

            b1 = var.pos
            sep = '['
            if sep not in var.alt:
                sep = ']'
            r = re.compile(r'\%s(.+?)\%s' % (sep, sep))
            chrom2, b2 = r.findall(var.alt)[0].split(':')
            b2 = int(b2)

            score = v[5]
            additional_fs = v[7:]
            if 'CIPOS' in var.info:
                span = [int(v) for v in var.info['CIPOS'].split(',')]
                s1 = b1 + span[0] - 1
                e1 = b1 + span[1]
            else:
                s1 = b1
                e1 = b1

            if 'CIEND' in var.info:
                span = [int(v) for v in var.info['CIEND'].split(',')]
                s2 = b2 + span[0] - 1
                e2 = b2 + span[1]
            else:
                s2 = b2
                e2 = b2

            ispan = s2 - e1
            ospan = e2 - s1
            event = ''
            if 'EVENT' in var.info:
                event = var.info['EVENT']
            # write bedpe
            bedpe_out.write('\t'.join(map(str,
                                          [var.chrom,
                                           s1,
                                           e1,
                                           chrom2,
                                           s2,
                                           e2,
                                           event,
                                           var.qual,
                                           var.info['SVTYPE'],
                                           var.filter] +
                                           additional_fs
                                          )) + '\n')
    # close the files
    bedpe_out.close()
    vcf_file.close()

    return

# --------------------------------------
# wrapper function

def main():
    # parse the command line args
    args = get_args()

    # call primary function
    vcfToBedpe(args.input, args.output)

# initialize the script
if __name__ == '__main__':
    try:
        sys.exit(main())
    except IOError:
        if sys.exc_info()[0] != BrokenPipeError:  # ignore SIGPIPE
            raise 
