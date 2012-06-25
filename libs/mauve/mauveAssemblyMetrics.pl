#!/usr/bin/env perl
# (c) Aaron Darling 2011
# Licensed under the GPL
# Program to generate plots on a directory containing assemblies scored by Mauve
# Usage: <scored assembly directory>
#

use strict;
use warnings;
use File::Basename;
use File::Spec;

if(@ARGV != 2){
	print "Usage: mauveAssemblyMetrics.pl <directory containing scored assemblies> <output directory>\n";
	exit(-1);
}

# first check that the given directory has data
my $files = get_summary_files();
if(length($files)<2){
	print STDERR "Error: no assembly scoring files found in \"$ARGV[0]\"\n";
	exit(-2);
}
# then create the plots
get_chromosomes();
get_summary();
make_plots();
exit(0);



#
# find all the scoring summary files
#
sub get_summary_files
{
	my $finder_cl = "find $ARGV[0] -name \"*__sum.txt\" \| sort |";
	open(FINDER, $finder_cl);
	my $alldat="";
	while(my $line=<FINDER>){
		chomp($line);
		$alldat .= "$line ";
	}
	return $alldat;
}

#
# run the R script to plot assembly metrics
#
sub make_plots
{
	my $alldat = get_summary_files();
	$alldat =~ s/__sum.txt//g;
	
	# find the plotting script
	my $prefix = "";
	my $script_fullpath = File::Spec->rel2abs( $0 );
	my $script_dir = dirname($script_fullpath);
	my $plotscript = "mauveAssemblyMetrics.R";
	my $testout = `$plotscript 2> /dev/null`;
	unless($testout =~ /Error/){
		$plotscript = "$script_dir/$plotscript" if( -e "$script_dir/$plotscript" );
		$plotscript = "./$plotscript" if( -e "./$plotscript" );
		die "Unable to find $plotscript\n" if($plotscript eq "mauveAssemblyMetrics.R");
	}
	
	# run the plots
	my $plot_cl = "$plotscript $ARGV[1]/ $alldat";
	# print "$plot_cl\n";
	`$plot_cl`;
}

#
# create a summaries.txt file
#
sub get_summary
{
	my $alldat = get_summary_files();
	my @files = split( /\s+/, $alldat );
	`rm -f $ARGV[1]/summaries.txt`;
	my $firstfile = $files[0];
	`head -n 1 $firstfile > $ARGV[1]/summaries.txt`;
	foreach my $file( @files ){
		`tail -n 1 $file >> $ARGV[1]/summaries.txt`;
	};
}

sub get_chromosomes
{
	my $finder_cl = "find $ARGV[0] -name chromosomes.txt |";
	open(FINDER, $finder_cl);
	my $chr = <FINDER>;
	chomp $chr;
	`cp $chr $ARGV[1]/`;
}

