#!/usr/bin/python

############################################################################
# Copyright (c) 2011-2012 Saint-Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################

import fnmatch
import os


def do_mode_one(ouf, all, inf_filenames, ref_id):
    print >> ouf, "    [S1]     [E1]  |     [S2]     [E2]  |  [LEN 1]  [LEN 2]  |  [% IDY]  | [TAGS]"
    print >> ouf, "====================================================================================="
    for contig in sorted(all, key=lambda contig: -contig[1]):
        contig_id, contig_len, contig_i = contig
        for a in all[contig]:
            sc, ec, sr, er, p = a[0:5]
            lc = abs(sc - ec) + 1
            lr = abs(sr - er) + 1
            if len(inf_filenames) > 1:
                label = ref_id + '\t' + str(contig_i) + '_' + contig_id
            else:
                label = ref_id + '\t' + contig_id
            print >> ouf, '%8d %8d  | %8d %8d  | %8d %8d  | %8.4f  | %s' % (sr, er, sc, ec, lc, lr, p, label)


def do_mode_two(ouf, list_events, inf_filenames, SEG):
    events = {}
    for e in list_events:
        x = e[0]
        if x not in events:
            events[x] = []
        events[x].append(e[1:])
    print >> ouf, inf_filenames
    op = [0] * len(inf_filenames)
    x_ = 0
    output = []
    for x in sorted(events):
        op_ = op[:]
        for e in events[x]:
            d, contig, a = e[0:3]
            contig_i = contig[2]
            op[contig_i] += d
        if x - x_ > SEG:
            s = (x_, x, op_)
            s_ = None
            if output:
                s_ = output[-1]
            if s_ and s_[2] == s[2]:
                output[-1] = (s_[0], s[1], s[2])
            else:
                output.append(s)
        x_ = x
    print ' ', len(output), 'segments.'
    for s in output:
        print >> ouf, '%8d\t%8d\t%8d\t%s' % (s[0], s[1], s[1] - s[0], s[2])


def do(mode, out_filename, input_files_or_dirs): #, ouf_filename, inf_filenames):
    print 'Running SympAlign...'
    ouf = open(out_filename, 'w')
    inf_filenames = []
    for input_file_or_dir in input_files_or_dirs:
        if os.path.isdir(input_file_or_dir):
            for filename in os.listdir(input_file_or_dir):
                if fnmatch.fnmatch(filename, '*.btab'):
                    inf_filenames.append(os.path.join(input_file_or_dir, filename))
        else:
            inf_filenames.append(input_file_or_dir)
    counter = [0, 0]
    all = {}
    list_events = []
    ref_id = None
    for i, infilename in enumerate(inf_filenames):
        aligns = {}
        for line in open(infilename, 'r'):
            arr = line.split('\t')
            #       Output format will consist of 21 tab-delimited columns. These are as
            #       follows: [0] query sequence ID [1] date of alignment [2] alignment type [3] reference file  
            #       [4] reference file [5] reference sequence ID [6] start of alignment in the query [7] end of alignment
            #       in the query [8] start of alignment in the reference [9] end of
            #       alignment in the reference [10] percent identity [11] percent
            #       similarity [12] length of alignment in the query [13] 0 for
            #       compatibility [14] 0 for compatibility [15] NULL for compatibility
            #       [16] 0 for compatibility [17] strand of the query [18] length of the
            #       reference sequence [19] 0 for compatibility [20] and 0 for
            #       compatibility.
            #
            # NODE_31_length_14785	Aug 23 2011	14785	NUCMER	/home/dvorkin/algorithmic-biology/assembler/src/tools/quality/../../../data/input/E.Coli.K12.MG1655/MG1655-K12.fasta	gi|49175990|ref|NC_000913.2|	14785	14203	3364194	3364776	100.000000	100.000000	583	0	0	NULL	0	Minus	4639675	0	0
            # NODE_31_length_14785	Aug 23 2011	14785	NUCMER	/home/dvorkin/algorithmic-biology/assembler/src/tools/quality/../../../data/input/E.Coli.K12.MG1655/MG1655-K12.fasta	gi|49175990|ref|NC_000913.2|	14785	1	3650675	3665459	99.945892	99.945892	14785	0	0	NULL	0	Minus	4639675	0	0
            contig_id = arr[0]
            contig_len = int(arr[2])
            ref_id = arr[5]
            sc, ec, sr, er = map(int, arr[6:10])
            p = float(arr[10])
            lc = abs(sc - ec) + 1
            # lr = abs(sr - er) + 1
            if arr[10] != arr[11]:
                print '  Error: arr[10] != arr[11]', arr[10], arr[11]
                return
            if lc != int(arr[12]):
                print '  Error: lc != int(arr[12])', lc, int(arr[12])
                return
            if (ref_id is not None) and (arr[5] != ref_id):
                print '  Error: multiple values of arr[5]', ref_id, arr[5]
                return
            align = (sc, ec, sr, er, p)
            contig = (contig_id, contig_len, i)
            if contig not in aligns:
                aligns[contig] = []
            aligns[contig].append(align)
            counter[0] += 1

        for contig in aligns:
            contig_id, contig_len, contig_i = contig
            superb = False
            output = []
            for a in sorted(aligns[contig], key=lambda align: (-abs(align[0] - align[1]), -align[4])):
                sc, ec, sr, er, p = a[0:5]
                lc = abs(sc - ec) + 1
                # lr = abs(sr - er) + 1
                if lc >= 0.99 * contig_len:
                    superb = True
                    output.append(a)
                else:
                    if superb:
                        break
                    discard = False
                    for b in output:
                        sc2, ec2, sr2, er2, p2 = b[0:5]
                        lc2 = abs(sc2 - ec2) + 1
                        # lr2 = abs(sr2 - er2) + 1
                        if min(sc2, ec2) <= min(sc, ec) and max(sc, ec) <= max(sc2, ec2) and (
                        (lc <= 0.5 * lc2) or (p < p2 * 0.9999)):
                            discard = True
                            break
                    if discard:
                        continue
                    output.append(a)
            counter[1] += len(output)
            all[contig] = output
            for a in output:
                sc, ec, sr, er, p = a[0:5]
                xr = min(sr, er)
                yr = max(sr, er) + 1
                ev1 = (xr, +1, contig, a)
                ev2 = (yr, -1, contig, a)
                list_events += [ev1, ev2]
            del output
    print '  Cleaned', counter[0], 'down to', counter[1]

    if mode == 1:
        do_mode_one(ouf, all, inf_filenames, ref_id)
    if mode == 2:
        do_mode_two(ouf, list_events, inf_filenames, 60)
        for filename in inf_filenames: # removing all *.btabs
            if os.path.isfile(filename):
                os.remove(filename)
    ouf.close()
    print '  Done.'

# if __name__ == '__main__':
#     if len(sys.argv) < 4:
#         print 'Usage:', sys.argv[0], 'MODE OUTPUT_FILE INPUT_FILES_OR_DIR+'
#         exit(1)
#     mode = int(sys.argv[1])
#     output_filename = sys.argv[2]
#     input_files_or_dirs = sys.argv[3:]
#     do(mode, output_filename, input_files_or_dirs)
