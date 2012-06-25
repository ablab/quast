sub process_misassembled_contig  # input: OUTPUT_FILE, $i_start, $i_finish, $contig, \@prev, @sorted, $is_1st_chimeric_half
{          
    # reading input params

    local *COORDS_FILT = *{$_[0]};
    $i_start = $_[1];
    $i_finish = $_[2];
    
    $contig = $_[3];

    $prev = \@{$_[4]};
    $sorted = \@{$_[5]};

    $is_1st_chimeric_half = $_[6];

    for ($i = $i_start; $i < $i_finish; $i++){                        
        if ($verbose) {print "\t\t\tReal Alignment ",$i+1,": @{${$sorted}[$i]}\n";}

        #Calculate the distance on the reference between the end of the first alignment and the start of the second
        $gap = ${$sorted}[$i+1][0]-${$sorted}[$i][1];   

        #Check strands 
        $strand1 = 0;                
        if (${$sorted}[$i][3] > ${$sorted}[$i][4]) { $strand1 = 1; }
        $strand2 = 0;                
        if (${$sorted}[$i+1][3] > ${$sorted}[$i+1][4]) { $strand2 = 1; }

        if ( ${$sorted}[$i][11] ne ${$sorted}[$i+1][11] || $gap > $ns+$smgap ||     # different chromosomes or large gap
            ($rcinem == 0 && $strand1 != $strand2) ) {                            # or different strands 
          
            #Contig spans chromosomes or there is a gap larger than 1kb
            #MY: output in coords.filtered
            print COORDS_FILT "@{$prev}\n";
            @{$prev} = @{${$sorted}[$i+1]};

            if ($verbose) {print "\t\t\tExtensive misassembly between these two alignments: [${$sorted}[$i][11]] @ ${$sorted}[$i][1] and ${$sorted}[$i+1][0]\n";}
            push (@{$ref_aligns{${$sorted}[$i][11]}}, [${$sorted}[$i][0], ${$sorted}[$i][1], $contig, ${$sorted}[$i][3], ${$sorted}[$i][4]]);
            $ref_features{${$sorted}[$i][11]}[${$sorted}[$i][1]] = "M";
            $ref_features{${$sorted}[$i+1][11]}[${$sorted}[$i+1][1]] = "M";

            $region_misassemblies++;
			$misassembled_contigs{$contig} = length($assembly{$contig});                        

        } elsif ($gap < 0) {
            #There is overlap between the two alignments, a local misassembly                        
            if ($verbose) {print "\t\tOverlap between these two alignments (local misassembly): [${$sorted}[$i][11]] ${$sorted}[$i][1] to ${$sorted}[$i+1][0]\n";}
            push (@{$ref_aligns{${$sorted}[$i][11]}}, [${$sorted}[$i][0], ${$sorted}[$i][1], $contig, ${$sorted}[$i][3], ${$sorted}[$i][4]]);

            #MY:
            ${$prev}[1] = ${$sorted}[$i+1][1];            # [E1]
            ${$prev}[3] = 0;                              # [S2]
            ${$prev}[4] = 0;                              # [E2]
            ${$prev}[6] = ${$prev}[1] - ${$prev}[0];             # [LEN1]
            ${$prev}[7] = ${$prev}[7] + ${$sorted}[$i+1][7] + $gap; # [LEN2]
        } else {
            #There is a small gap between the two alignments, a local misassembly                        
            if ($verbose) {print "\t\tGap in alignment between these two alignments (local misassembly): [${$sorted}[$i][11]] ${$sorted}[$i][0]\n";}
            push (@{$ref_aligns{${$sorted}[$i][11]}}, [${$sorted}[$i][0], ${$sorted}[$i][1], $contig, ${$sorted}[$i][3], ${$sorted}[$i][4]]);

            #MY:
            ${$prev}[1] = ${$sorted}[$i+1][1];            # [E1]
            ${$prev}[3] = 0;                              # [S2]
            ${$prev}[4] = 0;                              # [E2]
            ${$prev}[6] = ${$prev}[1] - ${$prev}[0];      # [LEN1]
            ${$prev}[7] = ${$prev}[7] + ${$sorted}[$i+1][7]; # [LEN2]
        }	
    }

    #MY: output in coords.filtered
    if (! $is_1st_chimeric_half) {   
        print COORDS_FILT "@{$prev}\n";
    }

    #Record the very last alignment
	if ($verbose){print "\t\t\tReal Alignment ",$i+1,": @{${$sorted}[$i]}\n";}
	push (@{$ref_aligns{${$sorted}[$i][11]}}, [${$sorted}[$i][0], ${$sorted}[$i][1], $contig, ${$sorted}[$i][3], ${$sorted}[$i][4]]); 
}

__END__
