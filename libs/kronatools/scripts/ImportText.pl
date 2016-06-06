#! /usr/bin/perl

# Copyright © 2011, Battelle National Biodefense Institute (BNBI);
# all rights reserved. Authored by: Brian Ondov, Nicholas Bergman, and
# Adam Phillippy
#
# See the LICENSE.txt file included with this software for license information.


use strict;

use lib (`ktGetLibPath`);
use KronaTools;

setOption('out', 'text.krona.html');
setOption('name', 'all');

my @options =
qw(
	out
	name
	noMag
	combine
	url
);

getKronaOptions(@options);

if
(
	@ARGV < 1
)
{
	printUsage
	(
		'Creates a Krona chart from text files listing quantities and
lineages.',
		'text',
		'Tab-delimited text file. Each line should be a number followed by a
list of wedges to contribute to (starting from the highest level). If no wedges
are listed (and just a quantity is given), it will contribute to the top level.
If the same lineage is listed more than once, the values will be added.
Quantities can be omitted if -q is specified. Lines beginning with "#" will be
ignored.',
		0,
		1,
		\@options
	);
	
	exit 0;
}

my $tree = newTree();
my @datasetNames;
my $set = 0;

foreach my $input ( @ARGV )
{
	my ($fileName, $magFile, $name) = parseDataset($input);
	
	if ( ! getOption('combine') )
	{
		push @datasetNames, $name;
	}
	
	open INFILE, "<$fileName" or die $!;
	
	while ( <INFILE> )
	{
		if ( /^#/ )
		{
			next;
		}
		
		chomp;
		
		my @lineage = split /\t/;
		my $magnitude;
		
		if ( getOption('noMag') )
		{
			$magnitude = 1;
		}
		else
		{
			$magnitude = shift @lineage;
		}
		
		addByLineage($tree, $set, \@lineage, undef, $magnitude);
	}
	
	if ( ! getOption('combine') )
	{
		$set++;
	}
	
	close INFILE;
}

my @attributeNames =
(
	'magnitude',
	'magnitudeUnassigned'
);

my @attributeDisplayNames =
(
	'Total',
	'Unassigned'
);

writeTree
(
	$tree,
	\@attributeNames,
	\@attributeDisplayNames,
	\@datasetNames
);
