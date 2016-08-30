#!/usr/bin/perl
# ---------------------------------------------
# Alex Lomsadze
# Georgia Institute of Technology, -2014
#
# get GT-BP 
# ---------------------------------------------

use warnings;
use strict;
use Getopt::Long;
use Cwd qw(abs_path cwd);
use Data::Dumper;

# ------------------------------------------------
my $v = 0;
my $debug = 0;
# ------------------------------------------------
my $gibbs_in = '';
my $seq_in = '';
my $pos_out = '';

Usage() if ( @ARGV < 1 );
ParseCMD();
# ------------------------------------------------

my %h;
my $w = 0;

LoadMat( $gibbs_in, \%h );

my $current_seq;
my $current_length;
my $current_out;

open( my $IN, $seq_in ) or die "error on open file $0: $seq_in\n$!\n";
open( my $OUT, ">", $pos_out ) or die "error on open file $0: $pos_out\n$!\n"; 
while( my $line = <$IN>)
{	
	# skip comments and empty
	if ( $line =~ /^\#/ )   {next;}
	if ( $line =~ /^\s+$/ ) {next;}
		
	if ( $line =~ /^(\d+)\s+(\S+)\s*$/ )
	{
		$current_length = $1;
		$current_seq = uc($2);
		
		$current_out = $current_length - BestPosition( $current_seq ) - 2;
		
		print $OUT "$current_out\n";
	}
	else { print "error, unexpect format found in input seq $0: $line\n"; exit 1; }
}
close $OUT;
close $IN;

exit 0;

# ------------------------------------------------
sub LoadMat
{
	my( $name, $ref ) = @_;

	my %back;
	my %motif;
	my $size = 0;

	open( my $IN, $name ) or die "error on open file $0: $name\n$!\n"; 
	while( my $line = <$IN>)
	{
		# skip comments and empty
		if ( $line =~ /^\#/ )   {next;}
		if ( $line =~ /^>/ )    {next;}
		if ( $line =~ /^\s+$/ ) {next;}

		if ( $line =~ /^Pos\.\s+\#\s+a\s+t\s+c\s+g\s*$/ )
		{
			$line = <$IN>;
			
			while( ($line = <$IN>) =~ /^\s+(\d+)\s+\|\s+(0\.\d+)\s+(0\.\d+)\s+(0\.\d+)\s+(0\.\d+)\s*$/ )
			{	
				$size = $1;
				
				$motif{$1}{'a'} = $2;
				$motif{$1}{'t'} = $3;
				$motif{$1}{'c'} = $4;
				$motif{$1}{'g'} = $5;
			}
		}
		
		if ( $line =~ /^Background\s+probability\s+model\s*$/ )
		{
			$line = <$IN>;
			if ( $line =~ /^\s+(0\.\d+)\s+(0\.\d+)\s+(0\.\d+)\s+(0\.\d+)\s*$/ )
			{
				$back{'a'} = $1;
				$back{'t'} = $2;
				$back{'c'} = $3;
				$back{'g'} = $4;
			}
			else { print "error, unexpected format$0\n"; exit 1; }
		}
	}
	
	print Dumper(\%back) if $debug;
	print Dumper(\%motif) if $debug;
	print "size $size\n" if $debug;
	
	$w = $size;
	
	for( my $i = 1; $i <= $size; ++$i )
	{
		$ref->{$i}{'A'} = $motif{$i}{'a'} / $back{'a'};
		$ref->{$i}{'T'} = $motif{$i}{'t'} / $back{'t'};
		$ref->{$i}{'C'} = $motif{$i}{'c'} / $back{'c'};
		$ref->{$i}{'G'} = $motif{$i}{'g'} / $back{'g'};
		$ref->{$i}{'N'} = 1;
	}
	
	print Dumper($ref) if $debug;
}
# ------------------------------------------------
sub BestPosition
{
	my $s = shift;
	
	my $best_score = 0;
	my $score = 0;
	my $pos = 0;
	
	my $size = length($s) - $w;
	my @arr = split( '', $s );
	
	for( my $i = 0; $i <= $size; ++$i )
	{
		$score = 0;
		
		for( my $j = 1; $j <= $w; ++$j )
		{
			$score += $h{$j}{ $arr[$i + $j - 1] };
		}
		
		if ( $score > $best_score )
		{
			$best_score = $score;
			$pos = $i;
		}
	}

	return length($s) - $pos;
}
# ------------------------------------------------
sub ParseCMD
{
	my $cmd = $0;
	foreach my $str (@ARGV) { $cmd .= ( ' '. $str ); }
	
	my $opt_result = GetOptions
	(
		'seq_in=s'   => \$seq_in,
		'pos_out=s'  => \$pos_out,
		'gibbs_in=s' => \$gibbs_in,
		'verbose'  => \$v,
		'debug'    => \$debug
	);
	
	if( !$opt_result ) { print "error on command line\n"; exit 1; }
	if( @ARGV > 0 ) { print "error, unexpected argument found on command line: @ARGV\n"; exit 1; }
	$v = 1 if $debug;
}
# ------------------------------------------------
sub Usage
{
	print qq(# -------------------
Usage: $0   parameters

required:
  --seq_in    [filename]  input sequence
  --pos_out   [filename]  output sequence
  --gibss_in  [filename]  from gibss sampler

optional:
  --verbose
  --debug
# -------------------
);
	exit 1;
}
# ------------------------------------------------


