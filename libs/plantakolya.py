############################################################################
# Copyright (c) 2011-2012 Saint-Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################
import os
import re
import platform
import string
import subprocess
import fastaparser
from qutils import id_to_str

def do(reference, filenames, cyclic, rc, output_dir, lib_dir, draw_plots):
    if not os.path.isdir(output_dir):
        os.mkdir(output_dir)

    ########################################################################
    #assess_assembly_path1 = os.path.join(lib_dir, 'plantagora/assess_assembly1.pl')
    assess_assembly_path2 = os.path.join(lib_dir, 'plantagora/assess_assembly2.pl')
    if platform.system() == 'Darwin':
        mummer_path = os.path.join(lib_dir, 'MUMmer3.23-osx')
    else:
        mummer_path  = os.path.join(lib_dir, 'MUMmer3.23-linux')

    ########################################################################
    report_dict = {'header' : []}
    for filename in filenames:
        report_dict[os.path.basename(filename)] = []

    # for running our MUMmer
    myenv = os.environ.copy()
    myenv['PATH'] = mummer_path + ':' + myenv['PATH']
    # making if needed
    if not os.path.exists(os.path.join(mummer_path, 'nucmer')):
        print ("Making MUMmer...")
        subprocess.call(
            ['make', '-C', mummer_path],
            stdout=open(os.path.join(mummer_path, 'make.log'), 'w'), stderr=open(os.path.join(mummer_path, 'make.err'), 'w'))

    print 'Running plantakolya tool...'
    metrics = ['Average %IDY', 'Local misassemblies', 'Misassemblies', 'Misassembled contigs', 'Misassembled contig bases', 'Misassembled and unaligned', 'SNPs', 'Unaligned contigs', 'Unaligned contig bases', 'Ambiguous contigs']
    report_dict['header'] += metrics

    for id, filename in enumerate(filenames):
        print ' ', id_to_str(id), os.path.basename(filename), '...'
        nucmerfilename = output_dir + '/nucmer_' + os.path.basename(filename)
        # remove old nucmer coords file
        if os.path.isfile(nucmerfilename + '.coords'):
            os.remove(nucmerfilename + '.coords')
        # run plantakolya tool
        logfilename_out = output_dir + '/plantakolya_' + os.path.basename(filename) + '.stdout'
        logfilename_err = output_dir + '/plantakolya_' + os.path.basename(filename) + '.stderr'
        print '    Logging to files', logfilename_out, 'and', os.path.basename(logfilename_err), '...',

        cyclic_option = ''
        if cyclic :
            cyclic_option = '--cyclic'
        rc_option = '' # reverse complementarity is not an extensive misassemble
        if rc :
            rc_option = '--rc'

        peral = 0.99
        rcinem = rc
        maxun = 10
        smgap = 1000

        coords_filename = nucmerfilename + '.coords'
        delta_filename = nucmerfilename + '.delta'
        snps_filename = nucmerfilename + '.snps'
        coords_btab_filename = nucmerfilename + '.coords.btab'
        unaligned_filename = nucmerfilename + '.unaligned'

        if os.path.isfile(coords_filename):
            os.remove(coords_filename)
        if os.path.isfile(snps_filename):
            os.remove(snps_filename)

        # TODO: clean contigs?

        print 'NUCmer...',
        subprocess.call(['nucmer', '--maxmatch', '-p', nucmerfilename, reference, filename],
            stdout=open(logfilename_out, 'a'), stderr=open(logfilename_err, 'a'), env=myenv)
        subprocess.call(['show-coords', '-B', delta_filename],
            stdout=open(coords_btab_filename, 'w'), stderr=open(logfilename_err, 'a'), env=myenv)

        import sympalign
        sympalign.do(1, coords_filename, [coords_btab_filename])

        if not os.path.isfile(coords_filename):
            print 'failed'
        else:
            # TODO: check: Nucmer ended early?
            subprocess.call(['show-snps', '-T', delta_filename], stdout=open(snps_filename, 'w'), stderr=open(logfilename_err, 'a'), env=myenv)
            # TODO: check: Show-snps failed?

            # Loading the alignment files
            print 'Parsing coords...'
            aligns = {}
            coords_file = open(coords_filename)
            coords_file.readline()
            coords_file.readline()
            for line in coords_file:
                line = line.split()
                contig = line[12]
                aligns.setdefault(contig, []).append(line)

            # Loading the assembly contigs
            print 'Loading Assembly...'
            assembly = {}
            assembly_ns = {}
            for name, seq in fastaparser.read_fasta(filename):
                seq = seq.upper()
                assembly[name] = seq
                if 'N' in seq:
                    assembly_ns[name] = [pos for pos in xrange(len(seq)) if seq[pos] == 'N']

            # Loading the reference sequences
            print 'Loading Reference...'
            references = {}
            for name, seq in fastaparser.read_fasta(reference):
                references[name] = seq
                print '\tLoaded [%s]' % name

            # Loading the SNP calls
            print 'Loading SNPs...'
            snps = {}
            snps_locs = {}
            for line in open(snps_filename):
                if line[0] not in string.digits:
                    continue
                line = line.split()
                ref = line[10]
                ctg = line[11]
                # TODO: check: Malformed line in SNP file
                if line[1] == '.':
                    snps.setdefault(ref, {}).setdefault(ctg, {})[line[0]] == 'I'
                elif line[2] == '.':
                    snps.setdefault(ref, {}).setdefault(ctg, {})[line[0]] == 'D'
                else:
                    snps.setdefault(ref, {}).setdefault(ctg, {})[line[0]] == 'S'
                snps_locs.setdefault(ref, {}).setdefault(ctg, {})[line[0]] = line[3]

            # Loading the regions (if any)
            regions = {}
            total_reg_len = 0
            total_regions = 0
            print 'Loading Regions...'
            # TODO: gff
            print '\tNo regions given, using whole reference.';
            for name, seq in references.iteritems():
                regions.setdefault(name, []).append([1, len(seq)])
                total_regions += 1
                total_reg_len += len(seq)
            print '\tTotal Regions: %d' % total_regions
            print '\tTotal Region Length: %d' % total_reg_len

            aligned = 0
            unaligned = 0
            partially_unaligned = 0
            total_unaligned = 0
            ambiguous = 0
            total_ambiguous = 0
            uncovered_regions = 0
            uncovered_region_bases = 0
            total_redundant = 0
            misassembled_partially_unaligned = 0

            region_misassemblies = 0
            region_local_misassemblies = 0
            misassembled_contigs = []

            print 'Analyzing contigs...'

            unaligned_id = open(unaligned_filename, 'w')
            for contig, seq in assembly.iteritems():
                #Recording contig stats
                ctg_len = len(seq)
                # TODO: if ( exists $assembly_ns{$contig} ) { $ns = scalar( keys %{$assembly_ns{$contig}});} else { $ns = 0;}
                print '\tCONTIG: %s (%dbp)' % (contig, ctg_len)
                #Check if this contig aligned to the reference
                if contig in aligns:
                    #Pull all aligns for this contig
                    num_aligns = len(aligns[contig])

                    #Sort aligns by length and identity
                    sorted_aligns = sorted(aligns[contig], cmp=lambda a, b: (a[7]*a[9] < b[7]*b[9]) or (a[7] < b[7]))
                    top_len = sorted_aligns[0][7]
                    top_id = sorted_aligns[0][9]
                    print sorted_aligns
                    top_aligns = []
                    print 'Top Length: %s  Top ID: %s' % (top_len, top_id)

                    #Check that top hit captures most of the contig (>99% or within 10 bases)
                    if top_len > ctg_len * peral or ctg_len - top_len < maxun:
                        #Reset top aligns: aligns that share the same value of longest and higest identity
                        pass
#                        $top_aligns[0] = shift(@sorted);
#
#                        #Continue grabbing alignments while length and identity are identical
#                        while ( @sorted && $top_len == $sorted[0][7] && $top_id == $sorted[0][9]){
#                            push (@top_aligns, shift(@sorted) );
#                        }
#
#                        #Mark other alignments as ambiguous
#                        while (@sorted) {
#                        @ambig = @{pop(@sorted)};
#                        if ($verbose) { print "\t\tMarking as ambiguous: @ambig\n";}
#                        for ($i = $ambig[0]; $i <= $ambig[1]; $i++){
#                        if (defined $ref && ! exists $ref_features{$ref}[$i]) {$ref_features{$ref}[$i] = "A";}
#                        }
#                        }
#
#                        if (@top_aligns < 2){
#                        #There is only one top align, life is good
#                        if ($verbose) {
#                        print "\t\tOne align captures most of this contig: @{$top_aligns[0]}\n";
#                        #MY: output in coords.filtered
#                        print COORDS_FILT "@{$top_aligns[0]}\n";
#                        }
#                        push (@{$ref_aligns{$top_aligns[0][11]}}, [$top_aligns[0][0], $top_aligns[0][1], $contig, $top_aligns[0][3], $top_aligns[0][4]]);
#                        } else {
#                        #There is more than one top align
#                        if ($verbose) {print "\t\tThis contig has ", scalar(@top_aligns)," significant alignments. [ambiguous]\n";}
#                        #MY: output in coords.filtered (for genes - all alignments, and for NA - only one!)
#                        print COORDS_FILT "@{$top_aligns[0]}\n";
#
#                        #Record these alignments as ambiguous on the reference
#                        foreach $align (@top_aligns){
#                        @alignment = @{$align};
#                        $ref = $alignment[11];
#                        if ($verbose) {print "\t\t\tAmbiguous Alignment: @alignment\n";}
#                        for ($i=$alignment[0]; $i <= $alignment[1]; $i++){
#                        if (! exists $ref_features{$ref}[$i]) {$ref_features{$ref}[$i] = "A";}
#                        }
#                        }
#
#                        #Increment count of ambiguous contigs and bases
#                        $ambiguous++;
#                        $total_ambiguous += $ctg_len;
#                        }
#
#                    } else {
#
#                    #Sort  all aligns by position on contig, then length
#                    @sorted = sort {@a = @{$a};
#                    @b = @{$b};
#                    if ($a[3] < $a[4]) {$start_a = $a[3];} else {$start_a = $a[4];}
#                    if ($b[3] < $b[4]) {$start_b = $b[3];} else {$start_b = $b[4];}
#                    $start_a <=> $start_b || $b[7] <=> $a[7] || $b[9] <=> $a[9]} @sorted;
#
#                    #Push first alignment on to real aligns
#                    @real_aligns = ();
#                    push (@real_aligns, [@{$sorted[0]}]);
#                    if ($sorted[0][3] > $sorted[0][4]) { $last_end = $sorted[0][3]; } else { $last_end = $sorted[0][4]; }
#
#                    #Walk through alignments, if not fully contained within previous, record as real
#                    for ($i = 1; $i < $num_aligns; $i++) {
#                    #If this alignment extends past last alignment's endpoint, add to real, else skip
#                    if ($sorted[$i][3] > $last_end || $sorted[$i][4] > $last_end) {
#                        unshift (@real_aligns, [@{$sorted[$i]}]);
#                    if ($sorted[$i][3] > $sorted[$i][4]) { $last_end = $sorted[$i][3]; } else { $last_end = $sorted[$i][4]; }
#                    } else {
#                    if ($verbose) {print "\t\tSkipping [$sorted[$i][0]][$sorted[$i][1]] redundant alignment ",$i," @{$sorted[$i]}\n";}
#                    for ($j = $sorted[$i][0]; $j <= $sorted[$i][1]; $j++){
#                    if (defined $ref && ! exists $ref_features{$ref}[$j]) {$ref_features{$ref}[$j] = "A";}
#                    }
#                    }
#                    }
#
#                    $num_aligns = scalar(@real_aligns);
#
#                    if ($num_aligns < 2){
#                    #There is only one alignment of this contig to the reference
#                    #MY: output in coords.filtered
#                    print COORDS_FILT "@{$real_aligns[0]}\n";
#
#                    #Is the contig aligned in the reverse compliment?
#                    $rc = $sorted[0][3] > $sorted[0][4];
#
#                    #Record beginning and end of alignment in contig
#                    if ($rc) {
#                    $end = $sorted[0][3];
#                    $begin = $sorted[0][4];
#                    } else {
#                    $end = $sorted[0][4];
#                    $begin = $sorted[0][3];
#                    }
#
#
#                    if ($begin-1 || $ctg_len-$end) {
#                    #Increment tally of partially unaligned contigs
#                    $partially_unaligned++;
#                    #Increment tally of partially unaligned bases
#                    $total_unaligned += $begin-1;
#                    $total_unaligned += $ctg_len-$end;
#                    if ($verbose) {print "\t\tThis contig is partially unaligned. ($top_len out of $ctg_len)\n";}
#                    if ($verbose) {print "\t\tUnaligned bases: 1 to $begin (", $begin-1, ")\n";}
#                    if ($verbose) {print "\t\tUnaligned bases: $end to $ctg_len (", $ctg_len-$end, ")\n";}
#                    }
#
#                    push (@{$ref_aligns{$sorted[0][11]}}, [$sorted[0][0], $sorted[0][1], $contig, $sorted[0][3], $sorted[0][4]]);
#
#                    } else {
#                    #There is more than one alignment of this contig to the reference
#                    if ($verbose) {print "\t\tThis contig is misassembled. $num_aligns total aligns.\n";}
#
#                    #Reset real alignments and sum of real alignments
#                    $sum = 0;
#
#                    #Sort real alignments by position on the reference
#                    @sorted = sort {@a = @{$a}; @b = @{$b}; $a[11] cmp $b[11] || $a[0] <=> $b[0]} @real_aligns;
#
#                    # Counting misassembled contigs which are partially unaligned
#                    my $all_aligns_len = 0;
#                    for ($i = 0; $i < @sorted; $i++) { $all_aligns_len += $sorted[$i][7]; }
#
#                    if ( $all_aligns_len/$ctg_len < $umt ) {
#                    if ($verbose) {print "\t\t\tWarning! Contig length is $ctg_len and total length of all aligns is $all_aligns_len\n";}
#                    $misassembled_partially_unaligned += 1;
#                    }
#
#                    $sorted_num = @sorted-1;
#                    $chimeric_found = 0;
#                    #MY: computing cyclic references
#                    if ($cyclic) {
#                    if ($sorted[0][0]-1 + $total_reg_len-$sorted[$sorted_num][1] <= $ns+$smgap) {  # chimerical misassembly
#                    $chimeric_found = 1;
#
#                    # find chimerical alignment between "first" blocks and "last" blocks
#                    $chimeric_index = 0;
#                    for ($i = 0; $i < $sorted_num; $i++){
#                    $gap = $sorted[$i+1][0]-$sorted[$i][1];
#                    if ($gap > $ns+$smgap) {
#                    $chimeric_index = $i+1;
#                    last;
#                    }
#                    }
#
#                    #MY: for merging local misassemlbies
#                    @prev = @{$sorted[$chimeric_index]};
#
#                    # process "last half" of blocks
#                    &process_misassembled_contig(*COORDS_FILT, $chimeric_index, $sorted_num, $contig, \@prev, \@sorted, 1);
#                    if ($verbose) {print "\t\t\tChimerical misassembly between these two alignments: [$sorted[$sorted_num][11]] @ $sorted[$sorted_num][1] and $sorted[0][0]\n";}
#
#                    $prev[1] = $sorted[0][1];                  # [E1]
#                    $prev[3] = 0;                              # [S2]
#                    $prev[4] = 0;                              # [E2]
#                    $prev[6] += $sorted[0][1]-$sorted[0][0]+1; # [LEN1]
#                    $prev[7] += $sorted[0][7];                 # [LEN2]
#
#                    # process "first half" of blocks
#                    &process_misassembled_contig(*COORDS_FILT, 0, ($chimeric_index-1), $contig, \@prev, \@sorted, 0);
#                    }
#                    }
#
#                    if (!$chimeric_found) {
#                    #MY: for merging local misassemlbies
#                    @prev = @{$sorted[0]};
#                    &process_misassembled_contig(*COORDS_FILT, 0, $sorted_num, $contig, \@prev, \@sorted, 0);
#                    }
#                    }
#                    }
#                    } else {
#                    #No aligns to this contig
#                    if ($verbose) {print "\t\tThis contig is unaligned. ($ctg_len bp)\n";}
#                    print UNALIGNED_IDS "$contig\n";
#
#                    #Increment unaligned contig count and bases
#                    $unaligned++;
#                    $total_unaligned += $ctg_len;
#                    if ($verbose) {print "\t\tUnaligned bases: $ctg_len  total: $total_unaligned\n";}
#                    }
#                    }
#                    close (COORDS_FILT);
#                    close (UNALIGNED_IDS);

            # TODO: 'Analyzing coverage...'

            print '\tCovered Bases: %d' % region_covered
            print '\tAmbiguous Bases: %d' % region_ambig
            print '\tLocal Misassemblies: %d' % region_local_misassemblies
            print '\tMisassemblies: %d' % region_misassemblies
            print '\t\tMisassembled Contigs: %d' % len(misassembled_contigs)
            misassembled_bases = sum(len(v) for v in misassembled_contigs.itervalues())
            print '\t\tMisassembled Contig Bases: %d' % misassembled_bases

            print '\t\tMisassembled and Unaligned: %d' % misassembled_partially_unaligned
            print '\tSNPs: %d' % region_snp
            print '\tInsertions: %d' % region_insertion
            print '\tDeletions: %d' % region_deletion
            internal = 0
            external = 0
            sum_gap = 0
            for gap in gaps:
                if gap[1] == gap[2]:
                    internal += 1
                else:
                    external += 1
                sum_gap += gap[0]
            avg_gaps = sum_gaps * 1.0 / external if external else 0.0
            print '\tPositive Gaps: %d' % len(gaps)
            print '\t\tInternal Gaps: %d' % internal
            print '\t\tExternal Gaps: %d' % external
            print '\t\tExternal Gap Total: %d' % sum_gap
            print '\t\tExternal Gap Average: %.0f' % avg_gaps
            internal = 0
            external = 0
            sum_gap = 0
            for gap in neg_gaps:
                if gap[1] == gap[2]:
                    internal += 1
                else:
                    external += 1
                sum_gap += gap[0]
            avg_gaps = sum_gaps * 1.0 / external if external else 0.0
            print '\tNegative Gaps: %d' % len(neg_gaps)
            print '\t\tInternal Overlaps: %d' % internal
            print '\t\tExternal Overlaps: %d' % external
            print '\t\tExternal Overlaps Total: %d' % sum_gap
            print '\t\tExternal Overlaps Average: %.0f' % avg_gaps

            print '\tRedundant Contigs: %d (%d)' % (len(redundant), total_redundant)
            print

            print 'Uncovered Regions: %d (%d)' % (uncovered_regions, uncovered_region_bases)
            print 'Unaligned Contigs: %d (%d)' % (unaligned, partially_unaligned)
            print 'Unaligned Contig Bases: %d' % total_unaligned
            print 'Ambiguous Contigs: %d (%d)' % (ambiguous, total_ambiguous)

            subprocess.call(
                ['perl', assess_assembly_path2, reference, filename, nucmerfilename, '--verbose', cyclic_option, rc_option],
                stdout=open(logfilename_out, 'a'), stderr=open(logfilename_err, 'a'), env=myenv)

        print 'done.'

        if draw_plots:
            # draw reference coverage plot
            print '    Drawing reference coverage plot...',
            plotfilename = output_dir + '/mummerplot_' + os.path.basename(filename)
            plot_logfilename_out = output_dir + '/mummerplot_' + os.path.basename(filename) + '.stdout'
            plot_logfilename_err = output_dir + '/mummerplot_' + os.path.basename(filename) + '.stderr'
            plot_logfile_out = open(plot_logfilename_out, 'w')
            plot_logfile_err = open(plot_logfilename_err, 'w')
            subprocess.call(
                ['mummerplot', '--coverage', '--postscript', '--prefix', plotfilename, nucmerfilename + '.delta'],
                stdout=plot_logfile_out, stderr=plot_logfile_err, env=myenv)
            plot_logfile_out.close()
            plot_logfile_err.close()
            print 'saved to', plotfilename + '.ps'

        # compute nucmer average % IDY
        if os.path.isfile(nucmerfilename + '.coords'):
            file = open(nucmerfilename + '.coords')
            sum = 0.0
            num = 0
            for line in file:
                arr = line.split('|')
                if len(arr) > 4:
                    x = arr[3].strip()
                    if x[0] != '[': # not [% IDY]
                        sum += float(x)
                        num += 1
            if num:
                avg = sum / num
            else:
                avg = 0
            file.close()
        else:
            print '  ERROR: nucmer coord file (' + nucmerfilename + ') not found, skipping...'
            avg = 'N/A'
        print '    Average %IDY = ', avg
        report_dict[os.path.basename(filename)].append('%3.2f' % avg)
        # delete temporary files
        for ext in ['.delta', '.mgaps', '.ntref', '.gp']:
            if os.path.isfile(nucmerfilename + ext):
                os.remove(nucmerfilename + ext)
        if draw_plots:
            for ext in ['.gp', '.rplot', '.fplot']:
                if os.path.isfile(plotfilename + ext):
                    os.remove(plotfilename + ext)
        if os.path.isfile('nucmer.error'):
            os.remove('nucmer.error')
        if os.path.isfile(filename + '.clean'):
            os.remove(filename + '.clean')

        ## find metrics for total report:

        logfile_out = open(logfilename_out, 'r')
        cur_metric_id = 1
        for line in logfile_out:
            if metrics[cur_metric_id].lower() in line.lower():
                report_dict[os.path.basename(filename)].append( line.split(':')[1].strip() )
                cur_metric_id += 1
                if cur_metric_id == len(metrics):
                    break
        logfile_out.close()
        report_dict[os.path.basename(filename)] += ['N/A'] * (len(report_dict['header']) - len(report_dict[os.path.basename(filename)]))

        ## outputting misassembled contigs in separate file

        logfile_out = open(logfilename_out, 'r')
        mis_contigs_ids = []
        # skipping prologue
        for line in logfile_out:
            if line.startswith("Analyzing contigs..."):
                break
            # main part of plantakolya output
        cur_contig_id = ""
        for line in logfile_out:
            if line.startswith("	CONTIG:"):
                cur_contig_id = line.split("	CONTIG:")[1].strip()
            if (line.find("Extensive misassembly") != -1) and (cur_contig_id != ""):
                mis_contigs_ids.append(cur_contig_id.split()[0])
                cur_contig_id = ""
            if line.startswith("Analyzing coverage..."):
                break
        logfile_out.close()

        # outputting misassembled contigs
        input_contigs = fastaparser.read_fasta(filename)
        mis_contigs = open(output_dir + '/' + os.path.basename(filename) + '.mis_contigs', "w")

        for (name, seq) in input_contigs:
            corr_name = re.sub(r'\W', '', re.sub(r'\s', '_', name))
            if mis_contigs_ids.count(corr_name) != 0:
                mis_contigs.write(name + '\n')
                for i in xrange(0, len(seq), 60):
                    mis_contigs.write(seq[i:i+60] + '\n')
        mis_contigs.close()

    print '  Done'

    return report_dict
