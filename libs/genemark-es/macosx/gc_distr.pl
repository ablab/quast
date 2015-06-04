#!/usr/bin/perl
# --------------------------------------------
# Alex Lomsadze
# Georgia Institute of Technology, -2014
# --------------------------------------------

use strict;
use warnings;
use Getopt::Long;
use File::Spec;

my $Version = "2.1";
my $MIN_ATCG = 0.5;

# --------------------------------------------
my $in;
my $out;
my $w;
my $org = '';

Usage( $Version ) if ( @ARGV < 1 );
ParseCMD();
CheckBeforeRun();

my @windows = ParseWindowSizes( $w );

# --------------------------------------------
# keep GC counts in hash with key - GC rounded to 1%
# --w 1000,2000,3000   @window 1000   2000  8000
# GC{30}{1000} - counts    GC{30}{2000} - counts
# GC{31}{1000} - counts    GC{31}{2000} - counts
# --------------------------------------------

my %distr;

my $seq = "";
my $line_count = 0;

open( IN, $in ) or die "error on open $in: $!\n";
while( my $line = <IN> )
{
	++$line_count;

	# skip empty
	if ( $line =~ /^\s*$/ ) { next; }

	# if defline was found
	if ( $line =~ /^\s*>/ )
	{
		if ( length ($line ) > 1000 ) { print "warning: long definition line found on line: $line_count\n"; }

		if ( $seq )
		{
			CountGC();
		}

    	$seq = "";
	}
	else
	{
		# parse sequence

		# remove white spaces, etc
		$line =~ tr/0123456789\n\r\t //d;

		$line = uc( $line );
		
		# move to dna characters
		# $line =~ tr/Uu/Tt/;

		# replace allowed nucleic acid code (non A T C G and X) by N
		$line =~ tr/RYKMSWBDHVXrykmswbdhvx/N/;

		if ( $line =~ m/[^ATCGatcgNn]/ )
			{ die "error, unexpected letter found on line $line_count\n"; }
			
		$seq .= $line;
	}
}
close IN;

if ( $seq )
{
	CountGC();
}

NormDistr();
PrintOut();

exit 0;

# ================== sub =====================
sub CountGC
{
	# sequence of current window
	my $str;
	# counts
	my $gc_count;
	my $at_count;
	my $atgc_count;
	my $gc;

	my $max = length($seq);

	foreach my $size ( @windows )
	{
		if ( $size == 0 )
		{
			$size = $max;
		}

		for( my $i = 0; $size * $i < $max ; ++$i  )
		{
			$str = substr( $seq, $size * $i, $size );

			$gc_count = ( $str =~ tr/CG/z/ );
			$at_count = ( $str =~ tr/AT/z/ );
			$atgc_count = $at_count + $gc_count;
			
			if ( $atgc_count  < $size * $MIN_ATCG ) { next; }
			if ( !$atgc_count ) { next; }
			
			$gc = int( 100.0*$gc_count/$atgc_count + 0.5 );
			
			$distr{$size}{$gc} += 1;
		}
 	}
}
# --------------------------------------------
sub NormDistr
{
	# number of counted windows for each window size, the same order as in @window
	my %sum;

	foreach my $win ( @windows )
	{
		$sum{$win} = 0;
		
		foreach my $i (0..100)
		{
			if ( defined $distr{$win}{$i} )
			{
				$sum{$win} += $distr{$win}{$i};
			}
		}
	}
	
	foreach my $win ( @windows )
	{
		foreach my $i (0..100)
		{
			if ( defined $distr{$win}{$i} )
			{
				$distr{$win}{$i} = $distr{$win}{$i} / $sum{$win};
			}
		}
	}
	
	foreach my $win ( @windows )
	{
		$distr{$win}{'sum'}= $sum{$win};
	}
}
# --------------------------------------------
sub PrintOut
{
	open(OUT, ">$out") or die "error on open $out: $!\n";

	# print species name
	print OUT "#Species name, $org\n";
	
	# print column names
	print OUT "#GC";
	foreach my $current (@windows)
	{
		print OUT ", $current";
	}
	print OUT "\n";

	# print number of counted windows
	print OUT "#Window_counts";
	foreach my $win ( @windows )
	{
		print OUT ", $distr{$win}{'sum'}";
	}
	print OUT "\n";


	# print GC
	foreach my $i (0..100)
	{
		print OUT "$i";
    	foreach my $win (@windows)
		{
			if ( defined $distr{$win}{$i} )
			{
				print OUT  sprintf( ",%.3f", $distr{$win}{$i} );
			}
			else
			{
				print OUT ",";
			}
		}
		print OUT "\n";
	}

	close OUT;
}
# --------------------------------------------
sub ParseWindowSizes
{
	my $str = shift;
	
	# set window size to 0, if not defined on cmd
	my @arr = ( 0 );
	
	if ( defined $str )
	{
		@arr = split( /,/ , $w );

		# check values
		foreach my $current (@arr)
		{
			if ( ! $current =~ /^\d+$/ ) { print "error on command line in value of option --w: $str\n"; exit 1; }
		}
	}

	return @arr;
}
# --------------------------------------------
sub CheckBeforeRun
{
	if ( !defined $out ) { print "error, output file name is not provided\n"; exit 1; }
	if ( !defined $in )  { print "error, input file name is missing\n"; exit 1; }
	if ( $in eq $out )   { print "error in cmd: the same names for input and output\n"; exit 1; }
};
# --------------------------------------------
sub ParseCMD
{
	my $opt_results = GetOptions
	( 
		'in=s'  => \$in,
		'out=s' => \$out,
		'w=s'   => \$w,
		'org=s' => \$org
	);
	
	if ( !$opt_results ) { print "error on command line: $0\n"; exit 1; }	
}
# --------------------------------------------
sub Usage
{
	my $version = shift;

	print qq( 
Usage: $0  --in [filename]  --out [filename]  --w [size]

This program calculates GC histogram of sliding non-overlaping windows
Input: DNA sequence file in FASTA like format and the window size/s
Bin size is 1% ; bin is centered on whole number; 101 bins
Bin 0 is set as range 0.0-0.5 and bin 100 as range 99.5-100.0
Ignores windows with less than 50% allowed letters
Output is CSV formatted (the Comma Separated Value File Format)

  --w   [window size]  number/s separated by comma
        if no value, get GC of each FASTA record in file
  --org [species name]

version $version;
);
	exit 1;
};
# --------------------------------------------
