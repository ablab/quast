############################################################################
# Copyright (c) 2015-2016 Saint Petersburg State University
# Copyright (c) 2011-2015 Saint Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################
from quast_libs import qconfig


def analyze_coverage(ca_output, regions, ref_aligns, ref_features, snps, total_indels_info):
    uncovered_regions = 0
    uncovered_region_bases = 0
    total_redundant = 0

    region_covered = 0
    region_ambig = 0
    gaps = []
    neg_gaps = []
    redundant = []
    snip_left = 0
    snip_right = 0

    # for counting short and long indels
    # indels_list = []  # -- defined earlier
    prev_snp = None
    cur_indel = 0

    nothing_aligned = True
    #Go through each header in reference file
    for ref, value in regions.items():
        #Check to make sure this reference ID contains aligns.
        if ref not in ref_aligns:
            ca_output.stdout_f.write('ERROR: Reference %s does not have any alignments!  ' \
                                          'Check that this is the same file used for alignment.' % ref + '\n')
            ca_output.stdout_f.write('ERROR: Alignment Reference Headers: %s' % ref_aligns.keys() + '\n')
            continue
        nothing_aligned = False

        #Sort all alignments in this reference by start location
        sorted_aligns = sorted(ref_aligns[ref], key=lambda x: x.s1)
        total_aligns = len(sorted_aligns)
        ca_output.stdout_f.write('\tReference %s: %d total alignments. %d total regions.\n' % (ref, total_aligns, len(regions[ref])))

        # the rest is needed for SNPs stats only
        if not qconfig.show_snps:
            continue

        #Walk through each region on this reference sequence
        for region in regions[ref]:
            end = 0
            reg_length = region[1] - region[0] + 1
            ca_output.stdout_f.write('\t\tRegion: %d to %d (%d bp)\n' % (region[0], region[1], reg_length))

            #Skipping alignments not in the next region
            while sorted_aligns and sorted_aligns[0].e1 < region[0]:
                skipped = sorted_aligns[0]
                sorted_aligns = sorted_aligns[1:] # Kolya: slooow, but should never happens without gff :)
                ca_output.stdout_f.write('\t\t\tThis align occurs before our region of interest, skipping: %s\n' % skipped)

            if not sorted_aligns:
                ca_output.stdout_f.write('\t\t\tThere are no more aligns. Skipping this region.\n')
                continue

            #If region starts in a contig, ignore portion of contig prior to region start
            if sorted_aligns and region and sorted_aligns[0].s1 < region[0]:
                ca_output.stdout_f.write('\t\t\tSTART within alignment : %s\n' % sorted_aligns[0])
                #Track number of bases ignored at the start of the alignment
                snip_left = region[0] - sorted_aligns[0].s1
                #Modify to account for any insertions or deletions that are present
                for z in range(sorted_aligns[0].s1, region[0] + 1):
                    if (ref in snps) and (sorted_aligns[0].contig in snps[ref]) and (z in snps[ref][sorted_aligns[0].contig]) and \
                       (ref in ref_features) and (z in ref_features[ref]) and (ref_features[ref][z] != 'A'): # Kolya: never happened before because of bug: z -> i
                        for cur_snp in snps[ref][sorted_aligns[0].contig][z]:
                            if cur_snp.type == 'I':
                                snip_left += 1
                            elif cur_snp.type == 'D':
                                snip_left -= 1

                #Modify alignment to start at region
                ca_output.stdout_f.write('\t\t\t\tMoving reference start from %d to %d.\n' % (sorted_aligns[0].s1, region[0]))
                sorted_aligns[0].s1 = region[0]

                #Modify start position in contig
                if sorted_aligns[0].s2 < sorted_aligns[0].e2:
                    ca_output.stdout_f.write('\t\t\t\tMoving contig start from %d to %d.\n' % (sorted_aligns[0].s2,
                                                                                                  sorted_aligns[0].s2 + snip_left))
                    sorted_aligns[0].s2 += snip_left
                else:
                    ca_output.stdout_f.write('\t\t\t\tMoving contig start from %d to %d.\n' % (sorted_aligns[0].s2,
                                                                                                  sorted_aligns[0].s2 - snip_left))
                    sorted_aligns[0].s2 -= snip_left

            #No aligns in this region
            if sorted_aligns[0].s1 > region[1]:
                ca_output.stdout_f.write('\t\t\tThere are no aligns within this region.\n')
                gaps.append([reg_length, 'START', 'END'])
                #Increment uncovered region count and bases
                uncovered_regions += 1
                uncovered_region_bases += reg_length
                continue

            #Record first gap, and first ambiguous bases within it
            if sorted_aligns[0].s1 > region[0]:
                size = sorted_aligns[0].s1 - region[0]
                ca_output.stdout_f.write('\t\t\tSTART in gap: %d to %d (%d bp)\n' % (region[0], sorted_aligns[0].s1, size))
                gaps.append([size, 'START', sorted_aligns[0].contig])
                #Increment any ambiguously covered bases in this first gap
                for i in range(region[0], sorted_aligns[0].e1):
                    if (ref in ref_features) and (i in ref_features[ref]) and (ref_features[ref][i] == 'A'):
                        region_ambig += 1

            #For counting number of alignments
            counter = 0
            negative = False
            current = None
            while sorted_aligns and sorted_aligns[0].s1 < region[1] and not end:
                #Increment alignment count
                counter += 1
                if counter % 1000 == 0:
                    ca_output.stdout_f.write('\t...%d of %d\n' % (counter, total_aligns))
                end = False
                #Check to see if previous gap was negative
                if negative:
                    ca_output.stdout_f.write('\t\t\tPrevious gap was negative, modifying coordinates to ignore overlap\n')
                    #Ignoring OL part of next contig, no SNPs or N's will be recorded
                    snip_left = current.e1 + 1 - sorted_aligns[0].s1
                    #Account for any indels that may be present
                    for z in range(sorted_aligns[0].s1, current.e1 + 2):
                        if (ref in snps) and (sorted_aligns[0].contig in snps[ref]) and (z in snps[ref][sorted_aligns[0].contig]):
                            for cur_snp in snps[ref][sorted_aligns[0].contig][z]:
                                if cur_snp.type == 'I':
                                    snip_left += 1
                                elif cur_snp.type == 'D':
                                    snip_left -= 1
                    #Modifying position in contig of next alignment
                    sorted_aligns[0].s1 = current.e1 + 1
                    if sorted_aligns[0].s2 < sorted_aligns[0].e2:
                        ca_output.stdout_f.write('\t\t\t\tMoving contig start from %d to %d.\n' % (sorted_aligns[0].s2,
                                                                                                      sorted_aligns[0].s2 + snip_left))
                        sorted_aligns[0].s2 += snip_left
                    else:
                        ca_output.stdout_f.write('\t\t\t\tMoving contig start from %d to %d.\n' % (sorted_aligns[0].s2,
                                                                                                      sorted_aligns[0].s2 - snip_left))
                        sorted_aligns[0].s2 -= snip_left
                    negative = False

                #Pull top alignment
                current = sorted_aligns[0]
                sorted_aligns = sorted_aligns[1:]
                ca_output.stdout_f.write('\t\t\tAlign %d: %s\n' % (counter, '%d %d %s %d %d' % (current.s1, current.e1,
                                                                                                  current.contig, current.s2, current.e2)))

                #Check if:
                # A) We have no more aligns to this reference
                # B) The current alignment extends to or past the end of the region
                # C) The next alignment starts after the end of the region

                if not sorted_aligns or current.e1 >= region[1] or sorted_aligns[0].s1 > region[1]:
                    #Check if last alignment ends before the regions does (gap at end of the region)
                    if current.e1 >= region[1]:
                        #print "Ends inside current alignment.\n";
                        ca_output.stdout_f.write('\t\t\tEND in current alignment.  Modifying %d to %d.\n' % (current.e1, region[1]))
                        #Pushing the rest of the alignment back on the stack
                        sorted_aligns = [current] + sorted_aligns
                        #Flag to end loop through alignment
                        end = True
                        #Clip off right side of contig alignment
                        snip_right = current.e1 - region[1]
                        #End current alignment in region
                        current.e1 = region[1]
                    else:
                        #Region ends in a gap
                        size = region[1] - current.e1
                        ca_output.stdout_f.write('\t\t\tEND in gap: %d to %d (%d bp).\n' % (current.e1, region[1], size))

                        #Record gap
                        if not sorted_aligns:
                            #No more alignments, region ends in gap.
                            gaps.append([size, current.contig, 'END'])
                        else:
                            #Gap between end of current and beginning of next alignment.
                            gaps.append([size, current.contig, sorted_aligns[0].contig])
                        #Increment any ambiguous bases within this gap
                        for i in range(current.e1, region[1]):
                            if (ref in ref_features) and (i in ref_features[ref]) and (ref_features[ref][i] == 'A'):
                                region_ambig += 1
                else:
                    #Grab next alignment
                    next = sorted_aligns[0]

                    if next.e1 <= current.e1:
                        #The next alignment is redundant to the current alignmentt
                        while next.e1 <= current.e1 and sorted_aligns:
                            total_redundant += next.e1 - next.s1 + 1
                            ca_output.stdout_f.write('\t\t\t\tThe next alignment (%d %d %s %d %d) is redundant. Skipping.\n' \
                                                     % (next.s1, next.e1, next.contig, next.s2, next.e2))
                            redundant.append(current.contig)
                            sorted_aligns = sorted_aligns[1:]
                            if sorted_aligns:
                                next = sorted_aligns[0]
                                counter += 1
                            else:
                                #Flag to end loop through alignment
                                end = True

                    if not end:
                        if next.s1 > current.e1 + 1:
                            #There is a gap beetween this and the next alignment
                            size = next.s1 - current.e1 - 1
                            gaps.append([size, current.contig, next.contig])
                            ca_output.stdout_f.write('\t\t\t\tGap between this and next alignment: %d to %d (%d bp)\n' % \
                                                          (current.e1, next.s1, size))
                            #Record ambiguous bases in current gap
                            for i in range(current.e1, next.s1):
                                if (ref in ref_features) and (i in ref_features[ref]) and (ref_features[ref][i] == 'A'):
                                    region_ambig += 1
                        elif next.s1 <= current.e1:
                            #This alignment overlaps with the next alignment, negative gap
                            #If contig extends past the region, clip
                            if current.e1 > region[1]:
                                current.e1 = region[1]
                            #Record gap
                            size = next.s1 - current.e1
                            neg_gaps.append([size, current.contig, next.contig])
                            ca_output.stdout_f.write('\t\t\t\tNegative gap (overlap) between this and next alignment: ' \
                                                         '%d to %d (%d bp)\n' % (current.e1, next.s1, size))

                            #Mark this alignment as negative so overlap region can be ignored
                            negative = True
                        ca_output.stdout_f.write('\t\t\t\tNext Alignment: %d %d %s %d %d\n' % (next.s1, next.e1,
                                                                                                  next.contig, next.s2, next.e2))

                #Initiate location of SNP on assembly to be first or last base of contig alignment
                contig_estimate = current.s2
                enable_SNPs_output = False
                if enable_SNPs_output:
                    ca_output.stdout_f.write('\t\t\t\tContig start coord: %d\n' % contig_estimate)

                #Assess each reference base of the current alignment
                for i in range(current.s1, current.e1 + 1):
                    #Mark as covered
                    region_covered += 1

                    if current.s2 < current.e2:
                        pos_strand = True
                    else:
                        pos_strand = False

                    #If there is a misassembly, increment count and contig length
                    #if (exists $ref_features{$ref}[$i] && $ref_features{$ref}[$i] eq "M") {
                    #	$region_misassemblies++;
                    #	$misassembled_contigs{$current[2]} = length($assembly{$current[2]});
                    #}

                    #If there is a SNP, and no alternative alignments over this base, record SNPs
                    if (ref in snps) and (current.contig in snps[ref]) and (i in snps[ref][current.contig]):
                        cur_snps = snps[ref][current.contig][i]
                        # sorting by pos in contig
                        if pos_strand:
                            cur_snps = sorted(cur_snps, key=lambda x: x.ctg_pos)
                        else: # for reverse complement
                            cur_snps = sorted(cur_snps, key=lambda x: x.ctg_pos, reverse=True)

                        for cur_snp in cur_snps:
                            if enable_SNPs_output:
                                ca_output.stdout_f.write('\t\t\t\tSNP: %s, reference coord: %d, contig coord: %d, estimated contig coord: %d\n' % \
                                         (cur_snp.type, i, cur_snp.ctg_pos, contig_estimate))

                            #Capture SNP base
                            snp = cur_snp.type

                            #Check that the position of the SNP in the contig is close to the position of this SNP
                            if abs(contig_estimate - cur_snp.ctg_pos) > 2:
                                if enable_SNPs_output:
                                    ca_output.stdout_f.write('\t\t\t\t\tERROR: SNP position in contig was off by %d bp! (%d vs %d)\n' \
                                             % (abs(contig_estimate - cur_snp.ctg_pos), contig_estimate, cur_snp.ctg_pos))
                                continue

                            ca_output.used_snps_f.write('%s\t%s\t%d\t%s\t%s\t%d\n' % (ref, current.contig, cur_snp.ref_pos,
                                                                                      cur_snp.ref_nucl, cur_snp.ctg_nucl, cur_snp.ctg_pos))

                            #If SNP is an insertion, record
                            if snp == 'I':
                                total_indels_info.insertions += 1
                                if pos_strand: contig_estimate += 1
                                else: contig_estimate -= 1
                            #If SNP is a deletion, record
                            if snp == 'D':
                                total_indels_info.deletions += 1
                                if pos_strand: contig_estimate -= 1
                                else: contig_estimate += 1
                            #If SNP is a mismatch, record
                            if snp == 'S':
                                total_indels_info.mismatches += 1

                            if cur_snp.type == 'D' or cur_snp.type == 'I':
                                if prev_snp and ((cur_snp.type == 'D' and (prev_snp.ref_pos == cur_snp.ref_pos - 1) and (prev_snp.ctg_pos == cur_snp.ctg_pos)) or
                                     (cur_snp.type == 'I' and ((pos_strand and (prev_snp.ctg_pos == cur_snp.ctg_pos - 1)) or
                                         (not pos_strand and (prev_snp.ctg_pos == cur_snp.ctg_pos + 1))) and (prev_snp.ref_pos == cur_snp.ref_pos))):
                                    cur_indel += 1
                                else:
                                    if cur_indel:
                                        total_indels_info.indels_list.append(cur_indel)
                                    cur_indel = 1
                                prev_snp = cur_snp

                    if pos_strand: contig_estimate += 1
                    else: contig_estimate -= 1

                #Record Ns in current alignment
                if current.s2 < current.e2:
                    #print "\t\t(forward)Recording Ns from $current[3]+$snip_left to $current[4]-$snip_right...\n";
                    for i in (current.s2 + snip_left, current.e2 - snip_right + 1):
                        if current.ns_pos and (i in current.ns_pos):
                            region_ambig += 1
                else:
                    #print "\t\t(reverse)Recording Ns from $current[4]+$snip_right to $current[3]-$snip_left...\n";
                    for i in (current.e2 + snip_left, current.s2 - snip_right + 1):
                        if current.ns_pos and (i in current.ns_pos):
                            region_ambig += 1
                snip_left = 0
                snip_right = 0

                if cur_indel:
                    total_indels_info.indels_list.append(cur_indel)
                prev_snp = None
                cur_indel = 0

                ca_output.stdout_f.write('\n')

    SNPs = total_indels_info.mismatches
    indels_list = total_indels_info.indels_list
    total_aligned_bases = region_covered
    result = {'SNPs': SNPs, 'indels_list': indels_list, 'total_aligned_bases': total_aligned_bases, 'total_redundant':
              total_redundant, 'redundant': redundant, 'gaps': gaps, 'neg_gaps': neg_gaps, 'uncovered_regions': uncovered_regions,
              'uncovered_region_bases': uncovered_region_bases, 'region_covered': region_covered}

    return result


