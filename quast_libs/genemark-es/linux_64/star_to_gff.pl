#!/usr/bin/perl
# ------------------------------------------------
# Alex Lomsadze
# Georgia Institute of Technology, -2015
#
# parse intons coordinates in GFF format from STAR junction file
# ------------------------------------------------

use strict;
use warnings;
use Getopt::Long;

my @star_files;

my $star = '';
my $gff = '';
my $label = "RNA_seq_junction";
my $seq_file = '';
my $trace = '';
my $v = '';

Usage() if ( @ARGV < 1 );
ParseCMD();

my %seqh;
if ( $seq_file )
{
	LoadSeqToHash( $seq_file );
}

my %traceNames;
if( $trace )
{
	TraceSeqId($trace);
}

my @arr;
my %h;
my $key;

my %splice_site;
$splice_site{'0'} = "non-canonical";
$splice_site{'1'} = "GT_AG";
$splice_site{'2'} = "GT_AG";
$splice_site{'3'} = "GC_AG";
$splice_site{'4'} = "GC_AG";
$splice_site{'5'} = "AT_AC";
$splice_site{'6'} = "AT_AC";

my %sites_from_seq;

my $attr;
my $seq_id;

foreach $star (@star_files)
{
	open( my $STAR, $star ) or die "error on open file $0: $star\n$!\n";
	
	print "$star\n" if $v;
	
	while(<$STAR>)
	{
		if ( /^\s*$/ ) { next; }

		@arr = split( /\s+/ );

		$seq_id = $arr[0];
		if ($trace)
		{
			$seq_id = $traceNames{ $seq_id };
		}

		if ( $arr[3] eq '1' )
		{
			$key =  $seq_id ."\t". $label ."\tintron\t". $arr[1] ."\t". $arr[2] ."\t". "__SCORE__" ."\t+\t.\t". $splice_site{$arr[4]} ."\n";
			$h{$key} += ($arr[6] + $arr[7]); 
		}
		elsif ( $arr[3] eq '2' ) 
		{
			$key =  $seq_id ."\t". $label ."\tintron\t". $arr[1] ."\t". $arr[2] ."\t". "__SCORE__" ."\t-\t.\t". $splice_site{$arr[4]} ."\n";
			$h{$key} += ($arr[6] + $arr[7]);
		}
		elsif ( $arr[3] eq '0' )
		{
			$key =  $seq_id ."\t". $label ."\tintron\t". $arr[1] ."\t". $arr[2] ."\t". "__SCORE__" ."\t.\t.\t". $splice_site{$arr[4]} ."\n";
			$h{$key} += ($arr[6] + $arr[7]);
		}
		else
		{
			print "enexpected format: $_\n";
		}
		
		if ( $seq_file )
		{
			my $strand = '.';
			
			if ( $arr[3] eq '1' )
			{
				$strand = '+';
			}
			elsif ( $arr[3] eq '2' )
			{
				$strand = '-';
			}
			else
			{
				$strand = '.';
			}
			
			$attr = GetSpliceSites( $seq_id, $arr[1], $arr[2], $strand );
		}
		
		$sites_from_seq{ $key } = $attr;
	}
	close $STAR;
}

open( my $GFF, '>', $gff ) or die "error on open file $0: $gff\n$!\n";
my $value;
foreach $key ( keys %h)
{
	print $sites_from_seq{ $key } ."\tsites\n" if $v;
	
	$value = $h{$key};
	$key =~ s/__SCORE__/$value/;
	print $GFF $key;
}
close $GFF;

exit 0;

# ==================== sub ======================
sub TraceSeqId
{
	my $name = shift;
	
	open( my $TRACE, $name ) or die "error on open file $0: $name\n$!\n";
	while( <$TRACE> )
	{
		if ( /^\s*$/ ) { next; }
		if ( /^\s*#/ ) { next; }
		
		if( />(\S+)\s+\S+\s+>(\S+)\s*/ )
		{
			$traceNames{ $2 } = $1;
		}
		else
		{
			print "unexpected format found: $_\n";
		}
	}
	close $TRACE;
}
# ------------------------------------------------
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
		'star=s{,}' => \@star_files,
		'gff=s'    => \$gff,
		'label=s'  => \$label,
		'seq=s'    => \$seq_file,
		'trace=s'  => \$trace,
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
usage $0  --star  [name]  --gff [name]  --label [label]
  --star   input name/s of junctions.bed from RNA-Seq alignment
  --gff    output intron coordinates in GFF format
  --label  [$label] use this label in GFF to preserve name of the alignment tool
# -----
);
	exit 1;
}
# ------------------------------------------------
