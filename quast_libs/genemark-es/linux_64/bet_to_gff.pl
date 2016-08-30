#!/usr/bin/perl
# ------------------------------------------------
# Alex Lomsadze
# Georgia Institute of Technology, -2014
#
# parse intons coordinates in GFF format from bet junction file
# ------------------------------------------------

use strict;
use warnings;
use Getopt::Long;

my @bed_files;

my $bed = '';
my $gff = '';
my $label = "RNA_seq_junction";
my $seq_file = '';
my $v = '';

Usage() if ( @ARGV < 1 );
ParseCMD();

my %seqh;
if ( $seq_file )
{
	LoadSeqToHash( $seq_file );
}

my @arr;
my @sh;

my $left;
my $right;
my $score;

my $attr = ".";

open( my $GFF, '>', $gff ) or die "error on open file $0: $gff\n$!\n";

foreach $bed (@bed_files)
{
	open( my $BED, $bed ) or die "error on open file $0: $bed\n$!\n";
	while(<$BED>)
	{
		# skip header
		if ( /^track name/ ) { next; }
		if ( /^\s*$/ ) { next; }

		@arr = split;
		@sh = split( ',' , $arr[10] );

		$left  = $arr[1] + $sh[0] + 1;
		$right = $arr[2] - $sh[1];
		$score = $arr[4];

		if ( $seq_file )
		{
			$attr = GetSpliceSites( $arr[0], $left, $right, $arr[5] );
		}

		if ( $arr[5] eq '+' )
		{
			print $GFF   $arr[0] ."\t". $label ."\tintron\t". $left ."\t". $right ."\t". $score ."\t+\t.\t". $attr ."\n";
		}
		elsif ( $arr[5] eq '-' ) 
		{
			print $GFF   $arr[0] ."\t". $label ."\tintron\t". $left ."\t". $right ."\t". $score ."\t-\t.\t". $attr ."\n";
		}
	}
	close $BED;
}
close $GFF;

exit 0;

# ==================== sub ======================
sub GetSpliceSites
{
	my ( $key, $L, $R, $strand ) = @_;
	
	my $L_di = substr( $seqh{$key}, $L - 1, 2 );
	my $R_di = substr( $seqh{$key}, $R - 2, 2 );
	
	print "$L_di $R_di\n" if $v;
	
	my $str ='';
	
	if ( $strand eq '+' )
	{
		if ( $L_di eq "GT"  and $R_di eq "AG" )
		{
			$str = "GT_AG";
		}
		elsif ( $L_di eq "GC"  and $R_di eq "AG" )
		{
			$str = "GC_AG";
		}
		elsif ( $L_di eq "AT"  and $R_di eq "AC" )
		{
			$str = "AT_AC";
		}
		else
		{
			$str = "non-canonical";
		}
	}
	elsif ( $strand eq '-' )
	{
		if ( $L_di eq "CT"  and $R_di eq "AC" )
		{
			$str = "GT_AG";
		}
		elsif ( $L_di eq "CT"  and $R_di eq "GC" )
		{
			$str = "GC_AG";
		}
		elsif ( $L_di eq "GT"  and $R_di eq "AT" )
		{
			$str = "AT_AC";
		}
		else
		{
			$str = "non-canonical";
		}
	}
	else { $str = "." };
	

	return $str;	
}
# ------------------------------------------------
sub LoadSeqToHash
{
	my $name = shift;

	my $defline = "";
	my $seq = "";
	my $key = "";

	open( my $IN, $name ) or die "error on open file: $name\n$!";
	while( my $line = <$IN>)
	{
		if ( $line =~ /^#/ ) { next; }
		if ( $line =~ /^\s*$/ ) { next; } 

		if ( $line =~ /^\s*>/ )
		{
			if ( $line =~ /^>\s*(\S+)\s+/ )
			{
				$defline = $1;
				print "$defline : defline\n" if $v;
			}
			else { die "unexpected format found: $line\n"; }

			if ( $key eq "" and $seq eq "" )
			{
				$key = $defline;
			}
			elsif ( $key ne "" and $seq ne "" )
			{
				$seqh{ $key } = $seq;
				
				print "$key : saved record\n" if $v;
				$key = $defline;
				$seq = "";
			}
			else
				{ die "error in sequence format: $defline\n"; }
		}
		else
		{
			$line =~ s/\s|[0-9]//g;
			$line = uc($line);
			$seq .= $line;
		}
	}

	close $IN;

	if ( $key ne "" and $seq ne "" )
	{
		$seqh{ $key } = $seq;

		print "$key : saved record\n" if $v;
	}
	else
		{ die "error in sequence format last record: $defline\n"; }
}
# ------------------------------------------------
sub ParseCMD
{
	my $opt_result = GetOptions
	(
		'bed=s{,}' => \@bed_files,
		'gff=s'    => \$gff,
		'label=s'  => \$label,
		'seq=s'    => \$seq_file,
		'v'        => \$v,
	);
	
	if( !$opt_result ) { print "error on command line\n"; exit 1; }
	if( @ARGV > 0 ) { print "error, unexpected argument found on command line: @ARGV\n"; exit 1; }
}
# ------------------------------------------------
sub Usage
{
	print qq(
# -----
usage $0  --bed  [name]  --gff [name]  --label [label]
  --bed    input name/s of junctions.bed from RNA-Seq alignment
  --gff    output intron coordinates in GFF format
  --label  [$label] use this label in GFF to preserve name of the alignment tool
  --seq    [name] name of file with sequence (optional)
  --v      verbose
# -----
);
	exit 1;
}
# ------------------------------------------------
