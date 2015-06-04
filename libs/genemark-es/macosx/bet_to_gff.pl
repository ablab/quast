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

my $bet = '';
my $gff = '';
my $label = "TopHat2";

Usage() if ( @ARGV < 1 );
ParseCMD();

my @arr;
my @sh;

my $left;
my $right;
my $score;

open( my $BET, $bet ) or die "error on open file $0: $bet\n$!\n";
open( my $GFF, '>', $gff ) or die "error on open file $0: $gff\n$!\n";

while(<$BET>)
{
	# skip header
	if ( /^track name/ ) { next; }
	if ( /^\s*$/ ) { next; }

	@arr = split;
	@sh = split( ',' , $arr[10] );

	$left  = $arr[1] + $sh[0] + 1;
	$right = $arr[2] - $sh[1];
	$score = $arr[4];

	if ( $arr[5] eq '+' )
	{
		print $GFF   $arr[0] ."\t". $label ."\tintron\t". $left ."\t". $right ."\t". $score ."\t+\t.\t.\n";
	}
	elsif ( $arr[5] eq '-' ) 
	{
		print $GFF   $arr[0] ."\t". $label ."\tintron\t". $left ."\t". $right ."\t". $score ."\t-\t.\t.\n";
	}
}
close $GFF;
close $BET;

exit 0;

# ==================== sub ======================
sub ParseCMD
{
	my $opt_result = GetOptions
	(
		'bet=s'   => \$bet,
		'gff=s'   => \$gff,
		'label=s' => \$label
	);
	
	if( !$opt_result ) { print "error on command line\n"; exit 1; }
	if( @ARGV > 0 ) { print "error, unexpected argument found on command line: @ARGV\n"; exit 1; }
}
# ------------------------------------------------
sub Usage
{
	print qq(
# -----
usage $0  --bet  [name]  --gff [name]  --label [label]
  --bet    input junctions.bed from RNA-Seq alignment
  --gff    output intron coordinates in GFF format
  --label  [$label] use this label in GFF to preserve name of the alignment tool
# -----
);
	
	exit 1;
}
# ------------------------------------------------
