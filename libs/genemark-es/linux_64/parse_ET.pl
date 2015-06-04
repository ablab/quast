#!/usr/bin/perl
# --------------------------------------------
# Alex Lomsadze
# Georgia Institute of Technology, -2014
#
# Parse training set using ET introns
# --------------------------------------------

use strict;
use warnings;
use Getopt::Long;
use Cwd qw(abs_path cwd);
use Data::Dumper;
use YAML;
use Hash::Merge qw( merge );
use File::Spec;

use FindBin qw( $RealBin );
use lib $FindBin::Bin ."/lib";
use LineFit;

# ------------------------------------------------
my $v = 0;
my $debug = 0;
# system control
#   $cfg - reference on hash with parameters
#   $log - reference on logger
my $cfg;
my $section = 'ET_A';
# ------------------------------------------------

Usage() if ( @ARGV < 1 );
ParseCMD();
CheckInput();
print Dumper( $cfg ) if $debug;

# ------------------------------------------------
# temp solution, time constrain
my $BP = 0;
$BP = 1 if $cfg->{'Parameters'}->{'fungus'};


# for debug
my %tha;

# tmp for phase
my %phase;

# for single vs multiple
my %s_vs_m;
my $min_s_vs_m = 900;

# hash %h - keys - seq_id
# hash of hashes -> second hash key is unique string and value is hash of intron features

my %h;
LoadFromGFF( $cfg->{$section}->{'introns_in'}, \%h, uc 'intron', $cfg->{$section}->{'et_score'} );

my %sh;
LoadFromHMM( $cfg->{$section}->{'set_in'}, \%sh );

MatchIntrons( \%h, \%sh );

my $out = File::Spec->catfile( $cfg->{$section}->{'parse_dir'}, $cfg->{$section}->{'parse_out'} );
PrintOutput( $out );

print Dumper(\%tha) if $debug;

SplitSet($out);

if( $cfg->{$section}->{'single_len'} )
{
	SingleDuration( $cfg->{$section}->{'set_in'}, $cfg->{$section}->{'single_len'} );
}

if( $cfg->{$section}->{'phase'} )
{
	ExonPhase( $cfg->{$section}->{'phase'}  );
}

if( $cfg->{$section}->{'tr_s_vs_m'} )
{
	Transition_S_vs_M( $cfg->{$section}->{'tr_s_vs_m'}  );
}

if( $cfg->{$section}->{'tr_i_vs_t'} )
{
	Transition_I_vs_T( $cfg->{$section}->{'tr_i_vs_t'}, $cfg->{$section}->{'set_in'}  );
}

exit 0;

# ================= subs =========================
sub ExonPhase
{
	my( $name_out ) = @_;
	
	my $ph0 = $phase{'1'};
	my $ph1 = $phase{'2'};
	my $ph2 = $phase{'3'};
	my $ph = $ph0 + $ph1 + $ph2;

	my @arr;
	$arr[0] = sprintf("%.3f", ($ph0/$ph)*($ph0/$ph) );
	$arr[1] = sprintf("%.3f", ($ph0/$ph)*($ph1/$ph) );
	$arr[2] = sprintf("%.3f", ($ph0/$ph)*($ph2/$ph) );
	$arr[3] = sprintf("%.3f", ($ph1/$ph)*($ph0/$ph) );
	$arr[4] = sprintf("%.3f", ($ph1/$ph)*($ph1/$ph) );
	$arr[5] = sprintf("%.3f", ($ph1/$ph)*($ph2/$ph) );
	$arr[6] = sprintf("%.3f", ($ph2/$ph)*($ph0/$ph) );
	$arr[7] = sprintf("%.3f", ($ph2/$ph)*($ph1/$ph) );
	$arr[8] = sprintf("%.3f", ($ph2/$ph)*($ph2/$ph) );

	$ph0 = sprintf("%.3f", $ph0/$ph );
	$ph1 = sprintf("%.3f", $ph1/$ph );
	$ph2 = sprintf("%.3f", $ph2/$ph );
	
	open( my $PHASE, ">", $name_out ) or die "error on open file $0: $name_out\n$!\n";	
	
	print $PHASE  '$INITIAL_EXON_PHASE' ."\n";
	print $PHASE  "1 $ph0\n";
	print $PHASE  "2 $ph1\n";
	print $PHASE  "3 $ph2\n";
	
	print $PHASE  '$TERMINAL_EXON_PHASE' ."\n";
	print $PHASE  "1 $ph0\n";
	print $PHASE  "2 $ph1\n";
	print $PHASE  "3 $ph2\n";
	
	print $PHASE  '$INTERNAL_EXON_PHASE' ."\n";
	print $PHASE  "1 $arr[0]\n";
	print $PHASE  "2 $arr[1]\n";
	print $PHASE  "3 $arr[2]\n";
	print $PHASE  "4 $arr[3]\n";
	print $PHASE  "5 $arr[4]\n";
	print $PHASE  "6 $arr[5]\n";
	print $PHASE  "7 $arr[6]\n";
	print $PHASE  "8 $arr[7]\n";
	print $PHASE  "9 $arr[8]\n";
	
	close $PHASE;
}
# ------------------------------------------------
sub Transition_I_vs_T
{
	my( $name_out, $name_in) = @_;
	
	my %hash_introns_genes;
	my $min_gene_length = 700;
	
	open( my $PARSE, $name_in ) or die "error on open file $0: $name_in\n$!\n";
	while( my $line = <$PARSE>)
	{
		if( $line =~ /^##\s+(\d+)\s+(\d+)\s+(\d+)\s*$/  )
		{
			if( $2 > $min_gene_length )
			{
				$hash_introns_genes{$3 - 1} += 1;
			}
		}
	}
	close $PARSE;
	
	# count genes
	
	my $sum = 0;
	
	foreach my $key ( keys %hash_introns_genes)
	{
		$sum += $hash_introns_genes{$key};
	}

	print "genes in IvsT: $sum\n" if $v;
	
	# remove noize regions
	foreach my $key ( keys %hash_introns_genes)
	{
		if( $key == 0 )
		{
			delete $hash_introns_genes{$key};
		}
		elsif(  $hash_introns_genes{$key} < 10 )
		{
			delete $hash_introns_genes{$key};
		}
	}
	
	# normalize number of genes and move to log scale
	
	foreach my $key ( keys %hash_introns_genes)
	{
		$hash_introns_genes{$key} /= $sum;
		$hash_introns_genes{$key} = log($hash_introns_genes{$key});
	}
	
	# craete arrays for fit
	
	my @x;
	my @y;
	
	foreach my $key ( sort{$a<=>$b} keys %hash_introns_genes )
	{
		
		push @x, $key;
		push @y, $hash_introns_genes{$key};
		
		print "$key, $hash_introns_genes{$key}\n" if $v;
	}

	my $slope;

 	my $lineFit = Statistics::LineFit->new();
 	$lineFit->setData(\@x, \@y) or die "Invalid regression data\n";
 	if (defined $lineFit->rSquared()) 
	{
     	$slope = $lineFit->coefficients();
 	}

	my $p_to_i = exp($slope);
	my $p_to_t = 1 - $p_to_i;
		
	open( my $TR, ">", $name_out ) or die "error on open file $0: $name_out\n$!\n";
	print $TR  '$ToInternalExon '. sprintf("%.3f", $p_to_i) ."\n";
	print $TR  '$ToTerminalExon '. sprintf("%.3f", $p_to_t) ."\n";
	close $TR;
	
	print "IvsT  $p_to_i vs $p_to_t\n" if $v;
}
# ------------------------------------------------
sub Transition_S_vs_M
{
	my( $name_out ) = @_;
	
	my $ref;
	my $size;
	
	foreach my $seq_id ( keys %sh )
	{
		$ref = $sh{$seq_id};
		$size = scalar(@{$ref});
		
		for( my $i = 0; $i < $size; ++$i )
		{
			if( !defined $ref->[$i]->{'type'} )
			{
				print "$seq_id  $i \n";
				print Dumper($ref->[$i]);
			}
			
			if( ($ref->[$i]->{'type'} eq 'Single') and ($ref->[$i]->{'base_length'} > $min_s_vs_m) )
			{
				$s_vs_m{'s'} += 2;
			}
			elsif( ($ref->[$i]->{'type'} eq 'Initial') and ($ref->[$i]->{'base_length'} > $min_s_vs_m) )
			{
				$s_vs_m{'m'} += 1;
			}
			elsif( ($ref->[$i]->{'type'} eq 'Terminal') and ($ref->[$i]->{'base_length'} > $min_s_vs_m) )
			{
				$s_vs_m{'m'} += 1;
			}
		}
	}
	
	$s_vs_m{'mp'} = $s_vs_m{'m'} /( $s_vs_m{'m'} + $s_vs_m{'s'} );
	$s_vs_m{'sp'} = $s_vs_m{'s'} /( $s_vs_m{'m'} + $s_vs_m{'s'} );
	
	open( my $TR, ">", $name_out ) or die "error on open file $0: $name_out\n$!\n";

	print $TR  '$ToSingleGene '. sprintf("%.3f", $s_vs_m{'sp'}) ."\n";
	print $TR  '$ToMultiGene '. sprintf("%.3f", $s_vs_m{'mp'}) ."\n";
	
	close $TR;
	
	print "SvsM  $s_vs_m{'sp'} vs $s_vs_m{'mp'}\n" if $v;
}
# ------------------------------------------------
sub SingleDuration
{
	my ( $name_in, $name_out ) = @_;
	
	open( my $HMM, $name_in ) or die "error on open file $0: $name_in\n$!\n";
	open( my $SDUR, ">", $name_out ) or die "error on open file $0: $name_out\n$!\n";
	while( my $line = <$HMM> )
	{
		# if( $line =~ /^##\s+\d+\s+(\d+)\s*$/ )
		# {
		# 	print $SDUR "$1\n";
		# }
		
		if( $line =~ /^\#\s*Single\s+\d+\s+\d+\s+\d+\s+\d+\s+(\d+)\s+[.+-]\s+\d+\s+\d+\s+\S+\s*$/ )
		{
			print $SDUR "$1\n";
		}
	}
	close $SDUR;
	close $HMM;
}
# ------------------------------------------------
sub SplitSet
{
	my $name = shift;
	
	my $PARSE;
	open( $PARSE, $name )or die"error on open file $0: $name\n";
	
	my $DON;
	my $ACC;
	my $START;
	my $STOP_TAA;
	my $STOP_TAG;
	my $STOP_TGA;
	my $COD;
	my $NON;
	
	my $INTRON_LEN;
	my $INITIAL_LEN;
	my $INTERNAL_LEN;
	my $TERMINAL_LEN;

	my $ACC_SHORT;
	
	if( $cfg->{$section}->{'don'} )
	{
		$name = File::Spec->catfile( $cfg->{$section}->{'parse_dir'}, $cfg->{$section}->{'don'} );
		open( $DON, ">", $name ) or die"error on open file $0: $name\n";
	}

	if( $cfg->{$section}->{'acc'} )
	{
		$name = File::Spec->catfile( $cfg->{$section}->{'parse_dir'}, $cfg->{$section}->{'acc'} );
		open( $ACC, ">", $name ) or die"error on open file $0: $name\n";
	}

	if( $cfg->{$section}->{'intron_len'} )
	{
		$name = File::Spec->catfile( $cfg->{$section}->{'parse_dir'}, $cfg->{$section}->{'intron_len'} );
		open( $INTRON_LEN, ">", $name ) or die"error on open file $0: $name\n";
	}

	if( $cfg->{$section}->{'start'} )
	{
		$name = File::Spec->catfile( $cfg->{$section}->{'parse_dir'}, $cfg->{$section}->{'start'} );
		open( $START, ">", $name ) or die"error on open file $0: $name\n";
	}
	
	if( $cfg->{$section}->{'stop_taa'} )
	{
		$name = File::Spec->catfile( $cfg->{$section}->{'parse_dir'}, $cfg->{$section}->{'stop_taa'} );
		open( $STOP_TAA, ">", $name ) or die"error on open file $0: $name\n";
	}

	if( $cfg->{$section}->{'stop_tag'} )
	{
		$name = File::Spec->catfile( $cfg->{$section}->{'parse_dir'}, $cfg->{$section}->{'stop_tag'} );
		open( $STOP_TAG, ">", $name ) or die"error on open file $0: $name\n";
	}
	
	if( $cfg->{$section}->{'stop_tga'} )
	{
		$name = File::Spec->catfile( $cfg->{$section}->{'parse_dir'}, $cfg->{$section}->{'stop_tga'} );
		open( $STOP_TGA, ">", $name ) or die"error on open file $0: $name\n";
	}	

	if( $cfg->{$section}->{'cod'} )
	{
		$name = File::Spec->catfile( $cfg->{$section}->{'parse_dir'}, $cfg->{$section}->{'cod'} );
		open( $COD, ">", $name ) or die"error on open file $0: $name\n";
	}
	
	if( $cfg->{$section}->{'non'} )
	{
		$name = File::Spec->catfile( $cfg->{$section}->{'parse_dir'}, $cfg->{$section}->{'non'} );
		open( $NON, ">", $name ) or die"error on open file $0: $name\n";
	}

	if( $cfg->{$section}->{'initial_len'} )
	{
		$name = File::Spec->catfile( $cfg->{$section}->{'parse_dir'}, $cfg->{$section}->{'initial_len'} );
		open( $INITIAL_LEN, ">", $name ) or die"error on open file $0: $name\n";
	}
	
	if( $cfg->{$section}->{'internal_len'} )
	{
		$name = File::Spec->catfile( $cfg->{$section}->{'parse_dir'}, $cfg->{$section}->{'internal_len'} );
		open( $INTERNAL_LEN, ">", $name ) or die"error on open file $0: $name\n";
	}	

	if( $cfg->{$section}->{'terminal_len'} )
	{
		$name = File::Spec->catfile( $cfg->{$section}->{'parse_dir'}, $cfg->{$section}->{'terminal_len'} );
		open( $TERMINAL_LEN, ">", $name ) or die"error on open file $0: $name\n";
	}
	
	if( $BP and $cfg->{$section}->{'acc_short'} )
	{	
		$name = File::Spec->catfile( $cfg->{$section}->{'parse_dir'}, $cfg->{$section}->{'acc_short'} );
		open( $ACC_SHORT, ">", $name ) or die"error on open file $0: $name\n";
	}	

	my $label;
	my $value;
	my $len;
	my $ph_L;
	my $ph_R;
	
	while( my $line = <$PARSE> )
	{
		if( $line =~ /^\#\s*(\S+)\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+([.+-])\s+(\d+)\s+(\d+)\s+(\S+)\s*$/ )
		{
			$label = $1;
			$value = $10;
			$len = $6;
			$ph_L = $8;
			$ph_R = $9;

			if ( $DON and  $label eq 'DON' )
			{
				print $DON "$value\t$ph_L\n";
			}

			if ( $ACC and  $label eq 'ACC' )
			{
				print $ACC "$value\t$ph_L\n";
			}
			
			if ( $START and  $label eq 'START' )
			{
				print $START "$value\n";
			}

			if ( $STOP_TAA and  $label eq 'STOP_TAA' )
			{
				print $STOP_TAA "$value\n";
			}
			
			if ( $STOP_TAG and  $label eq 'STOP_TAG' )
			{
				print $STOP_TAG "$value\n";
			}
			
			if ( $STOP_TGA and  $label eq 'STOP_TGA' )
			{
				print $STOP_TGA "$value\n";
			}

			if( $BP and $ACC_SHORT and ($label eq 'ACC') )
			{
				print $ACC_SHORT ( substr($value, $cfg->{'acceptor_AG'}->{'width'} - $cfg->{'acceptor_short_AG'}->{'width'} ) ."\t$ph_L\n" );
			}
			
			if ( $COD and  ( $label =~ /Internal|Terminal|Initial|Single|CDS/ ) )
			{
				if( $label eq 'Terminal' )
				{
					$value =~ s/...$/nnn/;
					print $COD "$value\n";
				}
				elsif( $label eq 'Initial' )
				{
					$value =~ s/^.../nnn/;
					print $COD "$value";
					print $COD " nnn\n"
				}
				elsif( $label eq 'Internal' )
				{
					print $COD "$value";
					print $COD "  nnn\n"
				}
				elsif( $label eq 'CDS' )
				{
					$value = substr( $value, 60, -60 );					
					print $COD "$value";
					print $COD "  nnn\n"
				}
			}
			
			if ( $NON and  ( $label =~ /Intron|Intergenic/ ) )
			{
				if( $label =~ /Intron/ )
				{
					$value = substr( $value, 6, -20 );
					$value =~ s/.$/n/;
					$value =~ s/^./n/;
				}
				elsif( $label =~ /Intergenic/ )
				{
					$value = substr( $value, 20, -20 );
					$value =~ s/.$/n/;
					$value =~ s/^./n/;
				}
				
				print $NON "$value\n";
			}
			
			if( $INITIAL_LEN and ( $label eq 'Initial') )
			{
				print $INITIAL_LEN  "$len\n";
			}
				
			if( $INTERNAL_LEN and ( $label eq 'Internal') )
			{
				print $INTERNAL_LEN  "$len\n";
			}
			
			if( $TERMINAL_LEN and ( $label eq 'Terminal') )
			{
				print $TERMINAL_LEN  "$len\n";
			}
		}
	}

	close $DON if $DON;
	close $ACC if $ACC;
	close $INTRON_LEN if $INTRON_LEN;
	close $START if $START;
	close $STOP_TAA if $STOP_TAA;
	close $STOP_TAG if $STOP_TAG;
	close $STOP_TGA if $STOP_TGA;
	close $COD if $COD;
	close $NON if $NON;
	
	close $INITIAL_LEN  if $INITIAL_LEN;
	close $INTERNAL_LEN if $INTERNAL_LEN;
	close $TERMINAL_LEN if $TERMINAL_LEN;

	close $ACC_SHORT if $ACC_SHORT;
	
	close $PARSE;
}
# ------------------------------------------------
sub PrintOutput
{
	my $name = shift;
	
	open( OUT, '>', $name ) or die "error on open file $0: $name\n $!\n";
	
	my $ref;
	my $size;
	
	my %stat;
	
	$stat{'internal_match_one'} = 0;
	$stat{'internal_match_two'} = 0;
	$stat{'internal_no_match'}  = 0;
	$stat{'terminal_match_one'} = 0;
	$stat{'terminal_match_two'} = 0;
	$stat{'terminal_no_match'}  = 0;
	$stat{'initial_match_one'}  = 0;
	$stat{'initial_match_two'}  = 0;
	$stat{'initial_no_match'}   = 0;
	$stat{'single_match_one'}   = 0;
	$stat{'single_match_two'}   = 0;
	$stat{'single_no_match'}    = 0;
	$stat{'count_intergenic_all'}   = 0;
	$stat{'count_intergenic_match'} = 0;
	$stat{'seq_intergenic'}         = 0;
	$stat{'CDS_all'}       = 0;
	$stat{'CDS_short'}     = 0;
	$stat{'CDS_long'}      = 0;
	$stat{'CDS_seq_long'}  = 0;
	$stat{'CDS_seq_short'} = 0;
	
	
	foreach my $seq_id ( keys %sh )
	{
		$ref = $sh{$seq_id};
		$size = scalar(@{$ref});
		
		print OUT "##seq_id $seq_id\n";
		
		for( my $i = 0; $i < $size; ++$i )
		{
			if( ($ref->[$i]->{'type'} eq 'Intron') and $ref->[$i]->{'match'} )
			{
				$tha{'intron_match'} +=1;
				
				if( $ref->[$i-2]->{'type'} eq 'DON' )      { $ref->[$i-2]->{'match'} += 1; }
				else { print  "warnnig, DON label expected: $i\n"; }
				if( $ref->[$i-1]->{'type'} eq 'ACC' )      { $ref->[$i-1]->{'match'} += 1; }
				else { print  "warnnig, ACC label expected: $i\n"; }

				# befor intron label
				if( $ref->[$i-3]->{'type'} eq 'Internal' ) { $ref->[$i-3]->{'match'} += 1; }
				else
				{
					if( $ref->[$i-3]->{'type'} eq 'Terminal' )
					{
						$ref->[$i-3]->{'match'} += 1;
						
						if( $ref->[$i-4]->{'type'} eq 'STOP_TAA' )    { $ref->[$i-4]->{'match'} += 1; }
						elsif( $ref->[$i-4]->{'type'} eq 'STOP_TAG' ) { $ref->[$i-4]->{'match'} += 1; }
						elsif( $ref->[$i-4]->{'type'} eq 'STOP_TGA' ) { $ref->[$i-4]->{'match'} += 1; }					
						else { print "warning, STOP label expected: $i  -4\n"; }
					}
					elsif( $ref->[$i-3]->{'type'} eq 'Initial' )
					{
						$ref->[$i-3]->{'match'} += 1;
						
						if( $ref->[$i-4]->{'type'} eq 'START' )    { $ref->[$i-4]->{'match'} += 1; }
						else { print "warning, START label expected: $i\n"; }
					}
					else { print "warning, unexpected label found: $ref->[$i]->{'type'}\n"; }
				}

				# after intron label
				if( $ref->[$i+1]->{'type'} eq 'Internal' ) { $ref->[$i+1]->{'match'} += 1; }
				else
				{
					if( $ref->[$i+2]->{'type'} eq 'Terminal' )
					{
						$ref->[$i+2]->{'match'} += 1;
						
						if( $ref->[$i+1]->{'type'} eq 'STOP_TAA' ) { $ref->[$i+1]->{'match'} += 1; }
						elsif( $ref->[$i+1]->{'type'} eq 'STOP_TAG' ) { $ref->[$i+1]->{'match'} += 1; }
						elsif( $ref->[$i+1]->{'type'} eq 'STOP_TGA' ) { $ref->[$i+1]->{'match'} += 1; }					
						else { print "warning, STOP label expected: $i  +1\n"; }
					}					
					elsif( $ref->[$i+2]->{'type'} eq 'Initial' )
					{
						$ref->[$i+2]->{'match'} += 1;
						
						if( $ref->[$i+1]->{'type'} eq 'START' )    { $ref->[$i+1]->{'match'} += 1; }
						else { print "warning, START label expected: $i\n"; }
					}
					else { print "warning, unexpected label found: $ref->[$i]->{'type'}\n"; }
				}
			}
			elsif ( ($ref->[$i]->{'type'} eq 'Intron') and  !$ref->[$i]->{'match'} )
			{
				$tha{'intron_no_match'} += 1;
			}
		}
		
		# -- stat
		for( my $i = 0; $i < $size; ++$i )
		{
			if( $ref->[$i]->{'type'} eq 'Internal'  )
			{
				if( $ref->[$i]->{'match'} and $ref->[$i]->{'match'} == 1 )
				{
					++$stat{'internal_match_one'};
				}
				elsif( $ref->[$i]->{'match'} and $ref->[$i]->{'match'} == 2 )
				{
					++$stat{'internal_match_two'};
				}
				else
				{
					++$stat{'internal_no_match'};
				}
			}
			
			if( $ref->[$i]->{'type'} eq 'Terminal'  )
			{
				if( $ref->[$i]->{'match'} and $ref->[$i]->{'match'} == 1 )
				{
					++$stat{'terminal_match_one'};
				}
				elsif( $ref->[$i]->{'match'} and $ref->[$i]->{'match'} == 2 )
				{
					++$stat{'terminal_match_two'};
				}
				else
				{
					++$stat{'terminal_no_match'};
				}
			}
			
			if( $ref->[$i]->{'type'} eq 'Initial'  )
			{
				if( $ref->[$i]->{'match'} and $ref->[$i]->{'match'} == 1 )
				{
					++$stat{'initial_match_one'};
				}
				elsif( $ref->[$i]->{'match'} and $ref->[$i]->{'match'} == 2 )
				{
					++$stat{'initial_match_two'};
				}
				else
				{
					++$stat{'initial_no_match'};
				}
			}
			
			if( $ref->[$i]->{'type'} eq 'Single'  )
			{
				if( $ref->[$i]->{'match'} and $ref->[$i]->{'match'} == 1 )
				{
					++$stat{'single_match_one'};
				}
				elsif( $ref->[$i]->{'match'} and $ref->[$i]->{'match'} == 2 )
				{
					++$stat{'single_match_two'};
				}
				else
				{
					++$stat{'single_no_match'};
				}
			}
		}
		
		# end stat
		
		for( my $i = 0; $i < $size; ++$i )
		{
			if( ($ref->[$i]->{'type'} eq 'Intergenic') )
			{
				++$stat{'count_intergenic_all'};
				
				if ( $i > 1 and $i < $size - 1 )
				{
					if( $ref->[$i-1]->{'match'} and $ref->[$i+1]->{'match'} )
					{
						$ref->[$i]->{'match'} += 1;
						++$stat{'count_intergenic_match'};
						$stat{'seq_intergenic'} += ($ref->[$i]->{'right'} - $ref->[$i]->{'left'} + 1);
					}
				}
			}
		}


		for( my $i = 0; $i < $size; ++$i )
		{
			if( $ref->[$i]->{'type'} =~ /Initial|Internal|Terminal|Single/ )
			{
				if(  !$ref->[$i]->{'match'}  )
				{
					++$stat{'CDS_all'};
						
					if ( $ref->[$i]->{'right'} - $ref->[$i]->{'left'} + 1 > 800 )
					{
						my $sub_str = $ref->[$i]->{'line'};
						
						$sub_str =~ s/Initial|Internal|Terminal|Single/CDS/;
						
						$ref->[$i]->{'line'} = $sub_str;
						
						$ref->[$i]->{'match'} = 1;
						
						++$stat{'CDS_long'};
						$stat{'CDS_seq_long'} += ( $ref->[$i]->{'right'} - $ref->[$i]->{'left'} + 1  );
					}
					else
					{
						++$stat{'CDS_short'};
						$stat{'CDS_seq_short'} += ( $ref->[$i]->{'right'} - $ref->[$i]->{'left'} + 1  );
					}
				}
			}	
		}
		
		for( my $i = 0; $i < $size; ++$i )
		{
			if( $ref->[$i]->{'match'} )
			{
				print OUT $ref->[$i]->{'line'};
			}
		}
	}
	
	if($v)
	{
		print "exon\tno_match\tmatch_one\tmatch_two\n";
		print "Initial\t$stat{'initial_no_match'}\t$stat{'initial_match_one'}\t$stat{'initial_match_two'}\n";
		print "Internal\t$stat{'internal_no_match'}\t$stat{'internal_match_one'}\t$stat{'internal_match_two'}\n";
		print "Terminal\t$stat{'terminal_no_match'}\t$stat{'terminal_match_one'}\t$stat{'terminal_match_two'}\n";
		print "Single\t$stat{'single_no_match'}\t$stat{'single_match_one'}\t$stat{'single_match_two'}\n";
		print "CDS_no_match\tall\tshort\tlong\tseq_short\tseq_long\n";
		print "CDS_no_match\t$stat{'CDS_all'}\t$stat{'CDS_short'}\t$stat{'CDS_long'}\t$stat{'CDS_seq_short'}\t$stat{'CDS_seq_long'}\n";
		print "Intergenic\tall\tbetween_match\tseq_match\n";
		print "Intergenic:\t$stat{'count_intergenic_all'}\t$stat{'count_intergenic_match'}\t$stat{'seq_intergenic'}\n";
	}
	
	close OUT;
}
# ------------------------------------------------
sub MatchIntrons
{
	my ( $source, $target ) = @_;
	
	my $ref;
	my $size;

	foreach my $seq_id ( keys %{$target} )
	{
		if( ! exists $source->{$seq_id} ) { next; }
	
		$ref = $target->{$seq_id};
		$size = scalar(@{$ref});
		
		my $uniq_id;
		
		my $intron_source = scalar(  keys %{$source->{$seq_id}} );
		my $intron_target = 0;
		my $intron_match = 0;

		for( my $i = 0; $i < $size; ++$i )
		{
			if( $ref->[$i]->{'type'} eq 'Intron' )
			{
				++$intron_target;
				$uniq_id = $ref->[$i]->{'left'} .'_'. $ref->[$i]->{'right'} .'_'. $ref->[$i]->{'strand'};
				
				if ( exists $source->{$seq_id}{$uniq_id} )
				{
					++$intron_match;
					$ref->[$i]->{'match'} = 1;
					
					$phase{ $ref->[$i]->{'phase'} } += 1;
				}
			}
		}
	
		print "$seq_id $intron_source $intron_target $intron_match\n" if $debug;
	}
}
# ------------------------------------------------
sub LoadFromHMM
{
	my( $name, $ref ) = @_;

	my %tmp;
	my $id;

	open( my $HMM, $name ) or die "error on open file $0: $name\n$!\n";
	while( my $line = <$HMM> )
	{
		if( $line =~ /^\#* seq_id:\s+(\S+)/ )
		{
			$id = $1;
			
			print "$id\n" if $debug;
			
			next;
		}
		
		if( $line =~ /^\#\s*(\S+)\s+\d+\s+(\d+)\s+(\d+)\s+(\d+)\s+\d+\s+([.+-])\s+(\d)\s+/ )
		{
			$tmp{'type'} = $1;
			$tmp{'base_length'} = $2;
			$tmp{'left'} = $3;
			$tmp{'right'} = $4;
			$tmp{'strand'} = $5;
			
			$tmp{'phase'}  = $6 + 1;
			
			$tmp{'line'} = $line;
			$tmp{'match'} = 0;
			
			push @{$ref->{$id}}, { %tmp };

			$tha{$1} += 1;
		}
		elsif( $line =~ /^##/ or $line =~ /^# seq_id_end/ or $line =~ /^---$/ )
		{
			;
		}
		else
		{
			print $line;
		}
	}
	close $HMM;	
}
# ------------------------------------------------
sub LoadFromGFF
{
	my( $name, $ref, $label, $score ) = @_;
	
	my $id;       # $ref hash key
	my $uniq_id;  # form unique id for each record in key
	my %tmp;      # hash with info
	
	# $id->$uniq_id->%tmp
	
	# count for verbose
	my $count_all = 0;  # all informative GFF lines
	my $count_in  = 0;  # unique GFF lines selected with type - $label
	my $count_dub = 0;  # number of dublications in input - the same intron from alternative spliced genes	
	
	open( my $GFF, $name ) or die "error on open file $0: $name\n$!\n";
	while( my $line = <$GFF> )
	{
		if( $line =~ /^\#/ ) {next;}
		if( $line =~ /^\s*$/ ) {next;}
		
		if ( $line =~ /^(\S+)\t(\S+)\t(\S+)\t(\d+)\t(\d+)\t(\S+)\t([+-.])\t(\S+)\t(.+)/ )
		{
			++$count_all;
			
			$id = $1;
			$tmp{'id'} = $1;
			$tmp{'type'} = uc $3;
			$tmp{'left'}   = $4;
			$tmp{'right'}  = $5;
			$tmp{'score'}  = $6;
			$tmp{'strand'} = $7;
		
			if( $tmp{'type'} ne $label ) {next;}
			
			if( $tmp{'score'} ne '.' and  $tmp{'score'} < $score ) { next; }
		
			$uniq_id = $tmp{'left'} .'_'. $tmp{'right'}  .'_'. $tmp{'strand'};
			
			if( ! exists $ref->{$id}->{$uniq_id} )
			{
				$ref->{$id}->{$uniq_id} = { %tmp };
				
				++$count_in;
			}
			else
			{
				++$count_dub;
			}
		}
		else { print "error, unexpected format found on line $0: $line\n"; exit 1; }
	}
	close $GFF;
	
	print "From $count_all loaded $count_in and ignored dublications $count_dub\n" if $v;
}
# ------------------------------------------------
sub CheckInput
{
	if( ! $cfg->{$section}->{'set_in'} )     { print "error, input  file is not specified $0\n"; exit 1; }
	if( ! $cfg->{$section}->{'introns_in'} ) { print "error, input  file is not specified $0\n"; exit 1; }
	if( ! $cfg->{$section}->{'parse_out'} )  { print "error, output file is not specified $0\n"; exit 1; }
	if( ! $cfg->{$section}->{'parse_dir'} )  { print "error, output directory is not specified $0\n"; exit 1; }

	$cfg->{$section}->{'set_in'}     = ResolvePath( $cfg->{$section}->{'set_in'} );
	$cfg->{$section}->{'introns_in'} = ResolvePath( $cfg->{$section}->{'introns_in'} );
	$cfg->{$section}->{'parse_dir'}  = ResolvePath( $cfg->{$section}->{'parse_dir'} );
}
# ------------------------------------------------
sub ParseCMD
{
	my $cmd = $0;
	foreach my $str (@ARGV) { $cmd .= ( ' '. $str ); }
	
	my %h;
	my $opt_result = GetOptions
	(
		\%h,
		'set_in=s',
		'introns_in=s',
		'parse_out=s',
		'parse_dir=s',
		'cfg=s',
		'section=s' => \$section,
		'verbose'   => \$v,
		'debug'     => \$debug
	);
	
	if( !$opt_result ) { print "error on command line\n"; exit 1; }
	if( @ARGV > 0 ) { print "error, unexpected argument found on command line: @ARGV\n"; exit 1; }
	$v = 1 if $debug;

	SetDefaultConfiguration( $section );
	
	if( $h{'cfg'} )
	{
		$cfg->{$section}->{'cfg'} = ReadCfgFile( $h{'cfg'} );
	}

	foreach my $key ( keys %h )
	{
		$cfg->{$section}->{$key} = $h{$key};
	}
	
	$cfg->{$section}->{'cmd'} = $cmd;
}
# ------------------------------------------------
sub ReadCfgFile
{
	my( $name, $path ) = @_;
	return '' if !$name;

	print "read config file: $name\n" if $debug;
	
	$name = ResolvePath( $name, $path );
	
	Hash::Merge::set_behavior( 'RIGHT_PRECEDENT' );
	%$cfg = %{ merge( $cfg, YAML::LoadFile( $name )) };
	
	return $name;
}
# ------------------------------------------------
sub ResolvePath
{
	my ( $name, $path ) = @_;
	return '' if !$name;
	$name = File::Spec->catfile( $path, $name ) if ( defined $path and $path );
	if( ! -e $name ) { print "error, file not found $0: $name\n"; exit 1; }
	return abs_path( $name );
}
# ------------------------------------------------
sub SetDefaultConfiguration
{
	my $label = shift;
	
	# file names
	$cfg->{$label}->{'set_in'}     = ''; # required
	$cfg->{$label}->{'introns_in'} = ''; # required
	$cfg->{$label}->{'parse_dir'}  = '.';
	$cfg->{$label}->{'parse_out'}  = ''; # required
	$cfg->{$label}->{'don'}        = '';
	$cfg->{$label}->{'acc'}        = '';
	$cfg->{$label}->{'start'}      = '';
	$cfg->{$label}->{'stop_taa'}   = '';
	$cfg->{$label}->{'stop_tga'}   = '';
	$cfg->{$label}->{'stop_tag'}   = '';
	$cfg->{$label}->{'cod'}        = '';
	$cfg->{$label}->{'non'}        = '';
	$cfg->{$label}->{'intron_len'} = '';
	
	# number
	$cfg->{$label}->{'order'} = 5;
	$cfg->{$label}->{'auto_order'} = 0;
	$cfg->{$label}->{'et_score'} = 0;
	# label
	$cfg->{$label}->{'source'} = 'TopHat2';
	# file name
	$cfg->{$label}->{'cfg'} = '';
}
# ------------------------------------------------
sub Usage
{
	print qq(# -------------------
Usage: $0   parameters

required:
  --set_in     [name]
  --introns_in [name]
 optional:
  --parse_dir  [name]   put data here
  --parse_out  [name]   file with parse
  --score      [number] filter out introns with scores below threshold
  --source     [label]  intros were generated by this tool: UnSplicer, TrueSight,TopHat2 etc...

  --cfg     [name] read parameters from this file
  --section [label] use this section from configuration file
  --verbose
  --debug
# -------------------
);
	exit 1;
}
# ------------------------------------------------
