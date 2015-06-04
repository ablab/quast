#!/usr/bin/perl
# ------------------------------------------------
# Alex Lomsadze
# Georgia Institute of Technology, -2014
#
# Check and remove contradictions in evidence data and Genemark.hmm model
# ------------------------------------------------

use strict;
use warnings;
use Getopt::Long;
use Cwd qw( abs_path );
use Data::Dumper;

my $in_gff  = '';
my $out_gff = '';
my $mod     = '';
my $v;

Usage() if ( @ARGV < 1 );
ParseCMD();
CheckBeforeRun();

# %h holds info from GeneMark.hmm model
my %h;
LoadMod( \%h, $mod );

my @gff;
ParseGFF( $in_gff, \@gff );

CleanSet( \@gff, \%h );

PrintToFile( $out_gff, \@gff );

exit 0;

# ------------------------------------------------
sub CleanSet
{
	my( $gref, $modref ) = @_;
	
	# check for maximum duration of states

	foreach my $r ( @{$gref} )
	{
		if( $r->{'type'} eq 'Intron' )
		{
			
			if( $r->{'end'} - $r->{'start'} + 1 > $modref->{'INTRON_MAX'} )
			{
				$r->{'status'} = 0;
				print $r->{'att'} ."ignored\n" if $v;
			}
			else
			{
				$r->{'status'} = 1;
			}
		}
	} 
}
# ------------------------------------------------
sub LoadMod
{
	my( $ref, $name ) = @_;
	
	if(!$name) { print "error, file name is missing in LoadMod $0,\n"; exit 1;}
	print "load parameters from file: $name\n" if $v;
	
	my $data = '';
	
	open( my $IN, $name ) or die "error on open file $0: $name\n";
	while( my $line = <$IN> )
	{
		# remove comments
		$line =~ s/\#.*$/\n/;
		
		# ignore empty lines
		if ( $line =~ /^\s*$/) {next;}
		
		$data .= $line;
	}
	close $IN;
	
	my @tmp = split( '\$', $data );
	my $size = scalar  @tmp;
	
	for( my $i = 1; $i < $size; ++$i )
	{
		if ( $tmp[$i] =~ /^(\S+)\s*(\S+)\s*$/s )
		{
			$ref->{$1} = $2;
		}
		elsif ( $tmp[$i] =~ /^(\S+)\s*(\S+.*\S)\s*$/s )
		{
			$ref->{$1} = $2;
		}
		else { print "error, unexpected format found in file $name\n$tmp[$i]\n"; exit 1; }
	}
}
# ------------------------------------------------
sub ParseGFF
{
	my( $name, $ref ) = @_;

	my %d;
	my $count_in_gff = 0;
	
	open( my $IN, $name ) || die "$! on open $name\n";
	while( my $line = <$IN> )
	{
		if( $line =~ /^(\S+)\t(\S+)\t(\S+)\t(\d+)\t(\d+)\t(\S+)\t([-+])\t(\S+)\t(.*)/ )
		{
			$d{'id'} = $1;
			$d{'info'} = $2;
			$d{'type'} = $3;
			$d{'start'} = $4;
			$d{'end'} = $5;
			$d{'score'} = $6;
			$d{'strand'} = $7;
			$d{'ph'} = $8;
			$d{'att'} = $9;
			
			push @{$ref}, ({%d});
			
			++$count_in_gff;
		}
	}
	close $IN;
	
	print "in: $count_in_gff\n" if $v;	
}
# ------------------------------------------------
sub PrintToFile
{
	my( $name, $ref ) = @_;
	
	my $line;
	
	open( my $OUT, ">$name" ) or die( "$!, error on open file $name" ); 
	foreach my $r ( @{$ref} )
	{
		if( $r->{'status'} )
		{
			$line = $r->{'id'} ."\t". $r->{'info'} ."\t". $r->{'type'} ."\t". $r->{'start'} ."\t". $r->{'end'} ."\t". $r->{'score'} ."\t". $r->{'strand'}."\t". $r->{'ph'} ."\t". $r->{'att'} ."\n";
			print $OUT $line;
		}
	} 
	close $OUT;;
}
# --------------------------------------------
sub CheckBeforeRun
{
	if ( !$out_gff )                { print "error, output file name is missing\n"; exit 1; }
	if ( !$in_gff or ! -e $in_gff ) { print "error, input file not found\n"; exit 1; }
	if ( !$mod or ! -e $mod )       { print "error, model file not found\n"; exit 1; }
	
	$in_gff = abs_path($in_gff);
	$mod    = abs_path($mod);
};
# --------------------------------------------
sub ParseCMD
{
	my $opt_result = GetOptions
	(
	  'in=s'  => \$in_gff,
	  'out=s' => \$out_gff,
	  'mod=s' => \$mod,
	  'v'     => \$v
	);

	if ( !$opt_result ) { print "error on command line: $0\n"; exit 1; }
	if( @ARGV > 0 ) { print "error, unexpected argument found on command line: $0 @ARGV\n"; exit 1; }
}
# --------------------------------------------
sub Usage
{
	print qq(# -------------------
Usage: $0 --in [filename] --out [filename]  --mod [filename] 

  --in    [filename] input
  --out   [filename] output
  --mode  [filename] file with GeneMark.hmm parameters
  --v     verbose
# -------------------
);
	exit 1;
}
# --------------------------------------------
