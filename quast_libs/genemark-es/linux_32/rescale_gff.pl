#!/usr/bin/perl
# --------------------------------------------
# Alex Lomsadze
# Georgia Institute of Technology, -2014
#
# Find new ID and coordinates for features from input GFF file
# with respect to new contigs
# --------------------------------------------

use strict;
use warnings;
use Getopt::Long;
use Cwd qw( abs_path );
use Data::Dumper;

# --------------------------------------------
my $in = '';
my $trace = '';
my $out = '';
my $v = 0;
my $warnings = 0;

Usage() if ( @ARGV < 1 );
ParseCMD();
CheckBeforeRun();

my %h;

ReadTraceInformation( $trace, \%h );
SortHashOfArrayOfHashes( \%h );

print Dumper(%h) if $v;

my %tmp;

open( IN, $in ) or die "error on open file: $in\n$!\n";
open( OUT, ">$out" ) or die "error on open file: $out\n$!\n";

while( my $line = <IN> )
{
	if ( $line =~ /^#/ )
	{
		print OUT $line;
	}
	elsif( $line =~ /^\s*$/ )
	{
		next;
	}
	elsif( $line =~ /^(\S+)\t(\S+)\t(\S+)\t(\d+)\t(\d+)\t(\S+)\t(\S+)\t(\S+)\t(.+)/ )
	{
		$tmp{'id'} = $1;
		$tmp{'2'} = $2;
		$tmp{'3'} = $3;
		$tmp{'left'}  = $4;
		$tmp{'right'} = $5;
		$tmp{'6'} = $6;
		$tmp{'7'} = $7;
		$tmp{'8'} = $8;
		$tmp{'9'} = $9;
		
		my $new_id = FindInterval( \%tmp, \@{$h{$1}} );
		
		if( $new_id )
		{
			my $new_line =  $new_id ."\t".  $tmp{'2'} ."\t". $tmp{'3'} ."\t". $tmp{'left'} ."\t". $tmp{'right'} ."\t". $tmp{'6'} ."\t". $tmp{'7'} ."\t". $tmp{'8'} ."\t". $tmp{'9'} ."\n";
			
			print OUT $new_line;
		}
		else
		{
			if ($warnings)
			{
				print $line;
			}
		}
	}
	else { print "error, unexpected format found: $0 $line\n"; exit 1; }
}
close OUT;
close IN;

exit 0;

# ================= subs =========================
sub FindInterval
{
	my( $gff_ref, $arr_ref ) = @_;
	
	my $new_id = '';

	foreach my $entry ( @{$arr_ref} )
	{
		if ( $gff_ref->{'left'} > $entry->{'left'} && $gff_ref->{'right'} < $entry->{'right'} )
		{
			$gff_ref->{'left'}  = $gff_ref->{'left'}  - $entry->{'left'} + 1;
			$gff_ref->{'right'} = $gff_ref->{'right'} - $entry->{'left'} + 1;
			$new_id =  $entry->{'new_id'};
			last;
		}
	}
	
	return $new_id;
}
#-------------------------------------------------
sub SortHashOfArrayOfHashes
{
	my $ref = shift;

	foreach my $key ( keys %{$ref} )
	{
		my @sorted = sort {$a->{left} <=> $b->{left}}  @{$ref->{$key}};

		$ref->{$key} = [@sorted];
	}
}
# --------------------------------------------
sub ReadTraceInformation
{
	my( $name, $ref ) = @_;
	
	my %current;
	
	open( TRACE, $name ) or die( "error on opne file: $name\n $!\n");
	while( my $line = <TRACE> )
	{
		if ( $line =~ /^(\S+)\s+(\S+)\s+(\d+)\s+(\d+)\s*/ )
		{
			$current{'new_id'} = $1;
			$current{'left'}   = $3;
			$current{'right'}  = $4;
			
			push @{$ref->{$2}}, { %current };
		}
		elsif ( $line =~ /^#/ ) { next; }
		else { print "error, unexpected format found: $line\n"; exit 1; }
	}
	close TRACE;
}
# --------------------------------------------
sub CheckBeforeRun
{
	if ( !$out )   { print "error, output file name is missing\n"; exit 1; }
	if ( !$in )    { print "error, input file name is missing\n"; exit 1; }
	if ( ! -e $in )    { print "error, file not found: $in\n"; exit 1; }
	if ( !$trace )    { print "error, trace file name is missing\n"; exit 1; }
	if ( ! -e $trace )    { print "error, file not found: $trace\n"; exit 1; }
	$in = abs_path($in);
	$trace = abs_path($trace);
};
# --------------------------------------------
sub ParseCMD
{
	my $opt_result = GetOptions
	(
	  'in=s'    => \$in,
	  'trace=s' => \$trace,
	  'out=s'   => \$out,
	  'v'       => \$v,
	  'warnings'=> \$warnings 
	);

	if ( !$opt_result ) { print "error on command line: $0\n"; exit 1; }
	if( @ARGV > 0 ) { print "error, unexpected argument found on command line: $0 @ARGV\n"; exit 1; }
}
# --------------------------------------------
sub Usage
{
	print qq(
Usage: $0  --out [filename]  --in [filename] 
  --in      [filename] input
  --trace   [filename] 
  --out     [filename] output
  --v       verbose
  --warnings

);
	exit 1;
}
# --------------------------------------------
