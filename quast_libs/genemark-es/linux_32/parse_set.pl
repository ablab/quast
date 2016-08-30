#!/usr/bin/perl
# ==============================================================
# Alex Lomsadze
# Georgia Institute of Technology, -2014
#
# Parse training set ES mode
# ==============================================================

use strict;
use warnings;
use Getopt::Long;
use Cwd qw(abs_path cwd);
use Data::Dumper;
use FindBin;
use lib "$FindBin::Bin/../lib";
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
my $section = 'z';
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

my %sh;
LoadFromHMM( $cfg->{$section}->{'set_in'}, \%sh );

SelectSet( $cfg->{$section}->{'min_cds'} );

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
	Transition_I_vs_T( $cfg->{$section}->{'tr_i_vs_t'}, $cfg->{$section}->{'set_in'},  $cfg->{$section}->{'min_cds'} );
}

exit 0;

# ================= subs =========================
sub SelectSet
{
	my( $min ) = @_;
	
	my $ref;
	my $size;

	foreach my $seq_id ( keys %sh )
	{
		$ref = $sh{$seq_id};
		$size = scalar(@{$ref});
		
		for( my $i = 0; $i < $size; ++$i )
		{
			if( $ref->[$i]->{'type'} ne "Intergenic" and $ref->[$i]->{'base_length'} >= $min )
			{
				$ref->[$i]->{'match'} = 1;
				
				if( $ref->[$i]->{'type'} eq "Intron" )
				{
					$phase{ $ref->[$i]->{'phase'} } += 1;
				}
			}
		}
	}
}
# ------------------------------------------------
sub PrintOutput
{
	my( $name ) = @_;
	
	open( OUT, '>', $name ) or die "error on open file $0: $name\n $!\n";
	
	my $ref;
	my $size;
	
	my %stat;

	$stat{'initial'}  = 0;	
	$stat{'internal'} = 0;
	$stat{'terminal'} = 0;
	$stat{'single'}   = 0;
	$stat{'count_intergenic'} = 0;
	$stat{'seq_intergenic'}   = 0;
	$stat{'count_intron'} = 0;
	$stat{'seq_intron'}   = 0;	
	
	foreach my $seq_id ( keys %sh )
	{
		$ref = $sh{$seq_id};
		$size = scalar(@{$ref});

		print OUT "##seq_id $seq_id\n" if $debug;
		
		# -- stat
		for( my $i = 0; $i < $size; ++$i )
		{
			++$stat{'initial'}  if( $ref->[$i]->{'type'} eq 'Initial'  and $ref->[$i]->{'match'} );
			++$stat{'internal'} if( $ref->[$i]->{'type'} eq 'Internal' and $ref->[$i]->{'match'} );
			++$stat{'terminal'} if( $ref->[$i]->{'type'} eq 'Terminal' and $ref->[$i]->{'match'} );
			++$stat{'single'}   if( $ref->[$i]->{'type'} eq 'Single'   and $ref->[$i]->{'match'} );
			
			if( $ref->[$i]->{'type'} eq 'Intron' and $ref->[$i]->{'match'} )
			{
				++$stat{'count_intron'};
				$stat{'seq_intron'} += ($ref->[$i]->{'right'} - $ref->[$i]->{'left'} + 1);
			}
		}
		# end stat
		
		for( my $i = 0; $i < $size; ++$i )
		{
			if( ($ref->[$i]->{'type'} eq 'Intergenic') )
			{	
				if ( $i > 1 and $i < $size - 1 )
				{
					if( $ref->[$i-1]->{'match'} and $ref->[$i+1]->{'match'} )
					{
						$ref->[$i]->{'match'} += 1;
						++$stat{'count_intergenic'};
						$stat{'seq_intergenic'} += ($ref->[$i]->{'right'} - $ref->[$i]->{'left'} + 1);
					}
				}
			}
		}

		# output
		
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
		print "Initial\t$stat{'initial'}\n";
		print "Internal\t$stat{'internal'}\n";
		print "Terminal\t$stat{'terminal'}\n";
		print "Single\t$stat{'single'}\n";
		print "Intron:\t$stat{'count_intron'}\t$stat{'seq_intron'}\n";
		print "Intergenic:\t$stat{'count_intergenic'}\t$stat{'seq_intergenic'}\n";
	}
	
	close OUT;
}
# ------------------------------------------------
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
	my( $name_out, $name_in, $min_gene_length ) = @_;
	
	my %hash_introns_genes;
	
	open( my $PARSE, $name_in ) or die "error on open file $0: $name_in\n$!\n";
	while( my $line = <$PARSE>)
	{
		## gene_id      cds_length      exons_in_gene
		
		if( $line =~ /^##\s+(\d+)\s+(\d+)\s+(\d+)\s*$/  )
		{
			if( $2 > $min_gene_length )
			{
				$hash_introns_genes{$3 - 1} += 1;
			}
		}
	}
	close $PARSE;
	
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
	
	# count genes
	
	my $sum = 0;
	
	foreach my $key ( keys %hash_introns_genes)
	{
		$sum += $hash_introns_genes{$key};
	}
	
	print "genes in IvsT: $sum\n" if $v;
	
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
	
	my $BP_REGION;
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
	
	if( $BP and $cfg->{$section}->{'bp_region'} )
	{
		$name = File::Spec->catfile( $cfg->{$section}->{'parse_dir'}, $cfg->{$section}->{'bp_region'} );
		open( $BP_REGION, ">", $name ) or die"error on open file $0: $name\n";
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

			if( $BP and $BP_REGION and ($label eq 'Intron') )
			{
				if ( $len > $cfg->{'ES_C'}->{'bp_region_length'} + 2 )
				{
					print $BP_REGION ( "$len\t". substr( $value, ( -1 * $cfg->{'ES_C'}->{'bp_region_length'} - 2 ), $cfg->{'ES_C'}->{'bp_region_length'} ) ."\n");
				}
				else
				{
					print $BP_REGION ( "$len\t". substr( $value, 0, $len - 2 ) ."\n");
				}
			}

			if( $BP and $ACC_SHORT and ($label eq 'ACC') )
			{
				print $ACC_SHORT ( substr($value, $cfg->{'acceptor_AG'}->{'width'} - $cfg->{'acceptor_short_AG'}->{'width'} ) ."\t$ph_L\n" );
			}
			
			if ( $COD and ( $label =~ /Internal|Terminal|Initial|Single/ ) )
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
					print $COD " nnn\n";
				}
				elsif( $label eq 'Internal' )
				{
					print $COD "$value";
					print $COD "  nnn\n";
				}
				elsif( $label eq 'Single' )
				{
					# $value = substr( $value, 60, -60 );
					$value =~ s/...$/nnn/;
					$value =~ s/^.../nnn/;
					
					print $COD "$value";
					print $COD "   nnn\n";
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
			
			if( $INTRON_LEN and ( $label eq 'Intron') )
			{
				print $INTRON_LEN  "$len\n";
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
	
	close $INTRON_LEN  if $INTRON_LEN;
	close $INITIAL_LEN  if $INITIAL_LEN;
	close $INTERNAL_LEN if $INTERNAL_LEN;
	close $TERMINAL_LEN if $TERMINAL_LEN;

	close $BP_REGION if $BP_REGION;
	close $ACC_SHORT if $ACC_SHORT;
	
	close $PARSE;
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
sub CheckInput
{
	if( ! $cfg->{$section}->{'set_in'} )     { print "error, input  file is not specified $0\n"; exit 1; }
	if( ! $cfg->{$section}->{'parse_out'} )  { print "error, output file is not specified $0\n"; exit 1; }
	if( ! $cfg->{$section}->{'parse_dir'} )  { print "error, output directory is not specified $0\n"; exit 1; }

	$cfg->{$section}->{'set_in'}    = ResolvePath( $cfg->{$section}->{'set_in'} );
	$cfg->{$section}->{'parse_dir'} = ResolvePath( $cfg->{$section}->{'parse_dir'} );
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
		'min_cds=i',
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
	
	$cfg->{$label}->{'min_cds'}     = 800;
	
	# file names
	$cfg->{$label}->{'set_in'}     = ''; # required
	$cfg->{$label}->{'parse_dir'}  = '.';
	$cfg->{$label}->{'parse_out'}  = ''; # required
	
	# file name
	$cfg->{$label}->{'cfg'} = '';
	
	$cfg->{$label}->{'don'}        = '';
	$cfg->{$label}->{'acc'}        = '';
	$cfg->{$label}->{'start'}      = '';
	$cfg->{$label}->{'stop_taa'}   = '';
	$cfg->{$label}->{'stop_tga'}   = '';
	$cfg->{$label}->{'stop_tag'}   = '';
	$cfg->{$label}->{'cod'}        = '';
	$cfg->{$label}->{'non'}        = '';
	$cfg->{$label}->{'intron_len'} = '';
	}
# ------------------------------------------------
sub Usage
{
	print qq(# -------------------
Usage: $0   parameters

required:
  --set_in     [name]
  --min_cds    [number]
 optional:
  --parse_dir  [name]   put data here
  --parse_out  [name]   file with parse

  --cfg     [name] read parameters from this file
  --section [label] use this section from configuration file
  --verbose
  --debug
# -------------------
);
	exit 1;
}
# ------------------------------------------------
