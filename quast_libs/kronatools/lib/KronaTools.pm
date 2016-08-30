# Copyright © 2011, Battelle National Biodefense Institute (BNBI);
# all rights reserved. Authored by: Brian Ondov, Nicholas Bergman, and
# Adam Phillippy
#
# See the LICENSE.txt file included with this software for license information.

use strict;

package KronaTools;

use Getopt::Long;
use File::Basename;
use File::Path;

use base 'Exporter';
use Cwd 'abs_path';

my $libPath = abs_path($0);
use File::Spec;
use File::Basename;
my $libPath = dirname(File::Spec->rel2abs(__FILE__));
my $taxonomyDir = "$libPath/../taxonomy";
my $ecFile = "$libPath/../data/ec.tsv";

# public subroutines
#
our @EXPORT = qw
(
	addByEC
	addByLineage
	addByTaxID
	addXML
	classifyBlast
	default
	getKronaOptions
	getOption
	getOptionString
	getScoreName
	getScriptName
	getTaxDepth
	getTaxName
	getTaxParent
	getTaxRank
	getTaxIDFromGI
	htmlFooter
	htmlHeader
	ktDie
	ktWarn
	loadEC
	loadMagnitudes
	loadTaxonomy
	newTree
	parseDataset
	printColumns
	printHeader
	printOptions
	printUsage
	setOption
	taxContains
	taxLowestCommonAncestor
	taxIDExists
	writeTree
);


###########
# Options #
###########

# The container for option values, initialized with global defaults.
#
my %options =
(
	'collapse' => 1,
	'color' => 0,
	'ecCol' => 2,
	'hueBad' => 0,
	'hueGood' => 120,
	'key' => 1,
	'queryCol' => 1,
	'scoreCol' => 3,
	'standalone' => 1,
	'taxCol' => 2,
	'taxonomy' => $taxonomyDir,
	'threshold' => 3
);

# Option format codes to pass to GetOptions (and to be parsed for display).
# Multiple options can use the same option letter, as long as they don't
# conflict within any given script.
#
my %optionFormats =
(
	'standalone' =>
		'a',
	'bitScore' =>
		'b',
	'combine' =>
		'c',
	'depth' =>
		'd=i',
	'ecCol' =>
		'e=i',
	'factor' =>
		'e=f',
	'include' =>
		'i',
	'noRank' =>
		'k',
	'local' =>
		'l',
	'magCol' =>
		'm=i',
	'minConfidence' =>
		'm=f',
	'name' =>
		'n=s',
	'out' =>
		'o=s',
	'percentIdentity' =>
		'p',
	'phymm' =>
		'p',
	'postUrl' =>
		'post=s',
	'noMag' =>
		'q',
	'queryCol' =>
		'q=i',
	'postUrl' =>
		'qp=s',
	'random' =>
		'r',
	'scoreCol' =>
		's=i',
	'summarize' =>
		's',
	'taxCol' =>
		't=i',
	'threshold' =>
		't=f',
	'taxonomy' =>
		'tax=s',
	'url' =>
		'u=s',
	'verbose' =>
		'v',
	'hueBad' =>
		'x=i',
	'hueGood' =>
		'y=i'
);

# how option arguments should be displayed based on format codes in %optionFormats
#
my %optionTypes =
(
	's' => 'string',
	'f' => 'number',
	'i' => 'integer'
);

# option descriptions to show in printOptions()
#
my %optionDescriptions =
(
	'bitScore' => 'Use bit score for average scores instead of log[10] e-value.',
	'combine' => 'Combine data from each file, rather than creating separate datasets within the chart.',
	'depth' => 'Maximum depth of wedges to include in the chart.',
	'ecCol' => 'Column of input files to use as EC number.',
	'factor' => 'E-value factor for determining "best" hits. A bit score difference threshold (-t) is recommended instead to avoid comparing e-values that BLAST reports as 0 due to floating point underflow. However, an e-value factor should be used if the input is a concatination of BLASTs against different databases. ',
	'hueBad' => 'Hue (0-360) for "bad" scores.',
	'hueGood' => 'Hue (0-360) for "good" scores.',
	'percentIdentity' => 'Use percent identity for average scores instead of log[10] e-value.',
	'include' => 'Include a wedge for queries with no hits.',
	'local' => 'Use resources from the local KronaTools installation instead of bundling them with charts (charts will be smaller but will only work on this computer).',
	'magCol' => 'Column of input files to use as magnitude. If magnitude files are specified, their magnitudes will override those in this column.',
	'minConfidence' => 'Minimum confidence. Each query sequence will only be added to taxa that were predicted with a confidence score of at least this value.',
	'name' => 'Name of the highest level.',
	'noMag' => 'Files do not have a field for quantity.',
	'noRank' => 'Allow assignments to taxa with ranks labeled "no rank" (instead of moving up to parent).',
	'out' => 'Output file name.',
	'phymm' => 'Input is phymm only (no confidence scores).',
	'postUrl' => 'Url to send query IDs to (instead of listing them) for each wedge. The query IDs will be sent as a comma separated list in the POST variable "queries", with the current dataset index (from 0) in the POST variable "dataset". The url can include additional variables encoded via GET.',
	'queryCol' => 'Column of input files to use as query ID. Required if magnitude files are specified.',
	'random' => 'Pick from the best hits randomly instead of finding the lowest common ancestor.',
	'scoreCol' => 'Column of input files to use as score.',
	'standalone' => 'Create a standalone chart, which includes Krona resources and does not require an Internet connection or KronaTools installation to view.',
	'summarize' => 'Summarize counts and average scores by taxonomy ID.',
	'taxCol' => 'Column of input files to use as taxonomy ID.',
	'taxonomy' => 'Taxonomy database to use.',
	'threshold' => 'Threshold for bit score differences when determining "best" hits. Hits with scores that are within this distance of the highest score will be included when computing the lowest common ancestor (or picking randomly if -r is specified).',
	'url' => 'URL of Krona resources to use instead of bundling them with the chart (e.g. "http://krona.sourceforge.net"). Reduces size of charts and allows updates, though charts will not work without access to this URL.',
	'verbose' => 'Verbose.'
);


#############
# Arguments #
#############

# how common arguments should be displayed
#
our %argumentNames =
(
	'blast' => 'blast_output',
	'magnitude' => 'magnitudes',
	'metarep' => 'metarep_folder',
	'name' => 'name',
);

# how common arguments should be described
#
our %argumentDescriptions =
(
	'blast' =>
'File containing BLAST results in tabular format ("Hit table (text)" when
downloading from NCBI).  If running BLAST locally, subject IDs in the local
database must contain GI numbers in "gi|12345" format.',
	'magnitude' =>
'Optional file listing query IDs with magnitudes, separated by tabs.  This can
be used to account for read length or contig depth to obtain a more accurate
representation of abundance.  By default, query sequences without specified
magnitudes will be assigned a magnitude of 1.  Magnitude files for assemblies in
ACE format can be created with ktGetContigMagnitudes.',
	'metarep' =>
'Unpacked METAREP data folder.',
	'name' =>
'A name to show in the list of datasets in the Krona chart (if multiple input
files are present and ' . getOptionString('combine') . ' is not specified). By
default, the basename of the file will be used.',
);


####################
# Global constants #
####################

my $version = '2.5';
my $javascriptVersion = '2.0';
my $javascript = "src/krona-$javascriptVersion.js";
my $hiddenImage = 'img/hidden.png';
my $favicon = 'img/favicon.ico';
my $loadingImage = 'img/loading.gif';
my $logo = 'img/logo-small.png';
my $taxonomyHrefBase = 'https://www.ncbi.nlm.nih.gov/Taxonomy/Browser/wwwtax.cgi?mode=info&id=';
my $ecHrefBase = 'http://www.chem.qmul.ac.uk/iubmb/enzyme/EC';
my $suppDirSuffix = '.files';
my $suppEnableFile = 'enable.js';
my $memberLimitDataset = 10000;
my $memberLimitTotal = 100000;
my $columns = `tput cols`;
our $minEVal = -450;


#################
# Lookup tables #
#################

my @taxDepths;
my @taxParents;
my @taxRanks;
my @taxNames;
my %taxIDByGI;
my %ecNames;
my %missingGIs;
my %missingTaxIDs;


############
# Exported #
############


sub addByEC
{
	# add based on an EC number string
	#
	# Options used:
	# depth
	my
	(
		$node, # hash ref
		$set, # integer
		$ec, # array ref of ec number components (no dots)
		$queryID, # (optional) string
		$magnitude, # (optional) number
		$score, # (optional) number
		
		# for recursion only
		#
		$depth
	) = @_;
	
	if ( ! %ecNames )
	{
		ktDie('EC data not loaded. "loadEC()" must be called first.');
	}
	
	$magnitude = default($magnitude, 1);
	
	$node->{'magnitude'}[$set] += $magnitude;
	$node->{'count'}[$set]++;
	
	if ( ! @$ec )
	{
		$node->{'children'}{'No hits'}{'magnitude'}[$set] += $magnitude;
		$node->{'children'}{'No hits'}{'count'}[$set]++;
		
		if ( ! defined $node->{'children'}{'No hits'}{'scoreCount'} )
		{
			$node->{'children'}{'No hits'}{'scoreCount'}[0] = 0;
		}
		
		if ( $queryID )
		{
			addMember($node->{'children'}{'No hits'}, $set, $queryID);
		}
		
		return;
	}
	
	if
	(
		$depth < @$ec &&
		( ! $options{'depth'} || $depth < $options{'depth'} )
	)
	{
		my $ecString = join '.', @$ec[0..$depth];
		my $name = $ecNames{$ecString};
		my $child;
		
		if ( defined $node->{'children'}{$name} )
		{
			$child = $node->{'children'}{$name};
		}
		else
		{
			my %newChild = ();
			
			$node->{'children'}{$name} = \%newChild;
			$child = $node->{'children'}{$name};
			
			$child->{'ec'}[0] = $ecString;
		}
		
		if ( defined $score )
		{
			${$child->{'scoreTotal'}}[$set] += $score * $magnitude;
			${$child->{'scoreCount'}}[$set] += $magnitude;
		}
		
		addByEC($child, $set, $ec, $queryID, $magnitude, $score, $depth + 1);
	}
	else
	{
		if ( $queryID )
		{
			addMember($node, $set, $queryID);
		}
		
		$node->{'unassigned'}[$set]++;
		$node->{'magnitudeUnassigned'}[$set] += $magnitude;
	}
}

sub addByLineage
{
	# add based on an explicit lineage
	#
	# Options used:
	# depth, minConfidence, leafAdd
	
	my
	(
		$node, # hash ref
		$dataset, # integer
		$lineage, # array ref
		$queryID, # (optional) string
		$magnitude, # (optional) number
		$scores, # (optional) number or array ref
		$ranks, # (optional) array ref
		
		# for recursion only
		#
		$index, # current index of input arrays
		$depth # our node depth (since input array elements can be skipped)
	) = @_;
	
	$magnitude = default($magnitude, 1);
	
	if ( $options{'leafAdd'} )
	{
		# magnitudes are already summarized; instead of adding magnitude to
		# ancestors, directly set it for the lowest level of the lineage.
		
		if ( $index == @$lineage )
		{
			$node->{'magnitude'}[$dataset] = $magnitude;
		}
	}
	else
	{
		$node->{'magnitude'}[$dataset] += $magnitude;
	}
	
	$node->{'count'}[$dataset]++;
	
	# skip nameless nodes
	#
	while ( $$lineage[$index] eq '' && $index < @$lineage )
	{
		$index++;
	}
	
	my $score;
	
	if ( ref($scores) eq 'ARRAY' )
	{
		$score = $$scores[$index];
	}
	else
	{
		$score = $scores;
	}
	
	if
	(
		$index < @$lineage &&
		( ! $options{'depth'} || $depth < $options{'depth'} ) &&
		(
			! defined $options{'minConfidence'} ||
			! defined $score ||
			$score >= $options{'minConfidence'}
		)
	)
	{
		my $name = $$lineage[$index];
		my $child;
		
		if ( defined ${$node}{'children'}{$name} )
		{
			$child = ${$node}{'children'}{$name};
		}
		else
		{
			my %newHash = ();
			${$node}{'children'}{$name} = \%newHash;
			$child = ${$node}{'children'}{$name};
			
			if ( $ranks )
			{
				$child->{'rank'}[0] = $$ranks[$index];
			}
		}
		
		if ( defined $score )
		{
			if ( $options{'leafAdd'} )
			{
				# instead of averaging score for ancestors, directly set it
				# for the lowest level of the lineage.
				
				if ( $index == @$lineage - 1 )
				{
					$child->{'scoreTotal'}[$dataset] = $score;
					$child->{'scoreCount'}[$dataset] = 1;
				}
			}
			else
			{
				$child->{'scoreTotal'}[$dataset] += $score * $magnitude;
				$child->{'scoreCount'}[$dataset] += $magnitude;
			}
		}
		
		addByLineage
		(
			$child,
			$dataset,
			$lineage,
			$queryID,
			$magnitude,
			$scores,
			$ranks,
			$index + 1,
			$depth + 1
		);
	}
	elsif ( ! $options{'leafAdd'} )
	{
		if ( $queryID )
		{
			addMember($node, $dataset, $queryID);
		}
		
		$node->{'unassigned'}[$dataset]++;
		$node->{'magnitudeUnassigned'}[$dataset] += $magnitude;
	}
}

sub addByTaxID
{
	# add based on NCBI taxonomy ID
	#
	# Options used:
	# depth
	
	my
	(
		$node, # hash ref
		$set, # integer
		$taxID, # integer
		$queryID, # string (optional)
		$magnitude, # number (optional)
		$score, # number (optional)
		
		# recusive only
		#
		$assigned
	) = @_;
	
	$magnitude = default($magnitude, 1);
	
	if ( $taxID != 0 && ! defined $taxDepths[$taxID] )
	{
		$missingTaxIDs{$taxID} = 1;
		$taxID = 1; # unknown tax ID; set to root
	}
	
	if ( $taxID == 0 )
	{
		$node->{'count'}[$set]++;
		$node->{'children'}{'No hits'}{'count'}[$set]++;
		
		my $child = $node->{'children'}{'No hits'};
		
		$node->{'magnitude'}[$set] += $magnitude;
		$child->{'magnitude'}[$set] += $magnitude;
		
		if ( ! defined $child->{'scoreCount'} )
		{
			$child->{'scoreCount'}[0] = 0;
		}
		
		if ( $queryID )
		{
			addMember($child, $set, $queryID);
		}
		
		return;
	}
	
	# move up to depth and skip unranked taxonomy nodes
	#
	while
	(
		! $options{'noRank'} && $taxID > 1 && $taxRanks[$taxID] eq 'no rank' ||
		$options{'depth'} && $taxDepths[$taxID] > $options{'depth'}
	)
	{
		$taxID = $taxParents[$taxID];
	}
	
	# get parent recursively
	#
	my $parent;
	my $parentID = getTaxParent($taxID);
	#
	if ( $parentID != 1 )#$taxID )
	{
		$parent = addByTaxID($node, $set, $parentID, undef, $magnitude, $score, 1);
	}
	else
	{
		$parent = $node;
		$parent->{'count'}[$set]++;
		$parent->{'magnitude'}[$set] += $magnitude;
	}
	
	# add this node to parent
	#
	if ( $taxID == 1 )
	{
		if ( $queryID )
		{
			addMember($parent, $set, $queryID);
		}
		
		$parent->{'unassigned'}[$set]++;
		$parent->{'magnitudeUnassigned'}[$set] += $magnitude;
	}
	else
	{
		my $name = $taxNames[$taxID];
		
		my $child;
		
		if ( defined $parent->{'children'}{$name} )
		{
			$child = $parent->{'children'}{$name};
		}
		else
		{
			my %newChild = ();
			
			$parent->{'children'}{$name} = \%newChild;
			$child = $parent->{'children'}{$name};
			
			$child->{'rank'}[0] = $taxRanks[$taxID];
			$child->{'taxon'}[0] = taxonLink($taxID);
		}
		
		if ( $queryID )
		{
			addMember($child, $set, $queryID);
		}
		
		${$child->{'count'}}[$set]++;
		
		if ( ! $assigned )
		{
			$child->{'unassigned'}[$set]++;
			$child->{'magnitudeUnassigned'}[$set] += $magnitude;
		}
		
		${$child->{'magnitude'}}[$set] += $magnitude;
		
		if ( defined $score )
		{
			$magnitude = default($magnitude, 1);
			${$child->{'scoreTotal'}}[$set] += $score * $magnitude;
			${$child->{'scoreCount'}}[$set] += $magnitude;
		}
		
		return $child;
	}
}

sub addXML
{
	my # parameters
	(
		$node,
		$xml,
		$dataset,
		$file
	) = @_;
	
	while ( (my $line = <$xml>) !~ /<\/node>/ )
	{
		if ( $line =~ /<node name="([^"]+)">/ )
		{
			my $child = $1;
			
			if ( ! defined $node->{'children'}{$child} )
			{
				my %newChild = ();
				$node->{'children'}{$child} = \%newChild;
			}
			
			addXML($node->{'children'}{$child}, $xml, $dataset, $file);
		}
		elsif ( $line =~ /<members>/ )
		{
			if ( $line =~ /<val>(.*)<\/val>/ )
			{
				my @members = split /<\/val><val>/, $1;
				my $offset = 0;
				
				for ( my $i = 0; $i < @members; $i++ )
				{
					if ( $members[$i] eq "" )
					{
						next;
					}
					
					my $fileMembers = "$file.files/$members[$i]";
					
					open MEMBERS, $fileMembers or die "Could not open $fileMembers";
					
					while ( <MEMBERS> )
					{
						if ( /(data\(')?(.+)\\n\\/ )
						{
							push @{$node->{'members'}[$dataset + $i]}, $2;
						}
					}
					
					close MEMBERS;
				}
			}
			
			while ( $line !~ /<\/members>/ )
			{
				my $offset = 0;
				
				if ( $line =~ /<vals><val>(.*)<\/val><\/vals>/ )
				{
					my @members = split /<\/val><val>/, $1;
					
					for ( my $i = 0; $i < @members; $i++ )
					{
						push @{$node->{'members'}[$dataset + $offset]}, $members[$i];
					}
					
					$offset++;
				}
				
				$line = <$xml>;
			}
		}
		elsif ( $line =~ /<(rank|taxon)><val>(.*)<\/val><\/\1>/ )
		{
			$node->{$1}[0] = $2;
		}
		elsif ( $line =~ /<(count|score|magnitude)><val>(.*)<\/val><\/\1>/ )
		{
			my @vals = split /<\/val><val>/, $2;
			
			for ( my $i = 0; $i < @vals; $i++ )
			{
				if ( $1 eq 'score' )
				{
					$node->{'scoreTotal'}[$dataset + $i] = $vals[$i];
					$node->{'scoreCount'}[$dataset + $i] = 1;
				}
				else
				{
					$node->{$1}[$dataset + $i] = $vals[$i];
				}
			}
		}
		elsif ( $line =~ /<\/node>/ )
		{
			return;
		}
	}
}

sub classifyBlast
{
	# taxonomically classifies BLAST results based on LCA (or random selection)
	# of 'best' hits.
	#
	# Options used: bitScore, factor, include, percentIdentity, random, score
	
	my # parameters
	(
		$fileName, # file with tabular BLAST results
		
		# hash refs to be populated with results (keyed by query ID)
		#
		$taxIDs,
		$scores
	) = @_;
	
	open BLAST, "<$fileName" or ktDie("Could not open $fileName\n");
	
	my $lastQueryID;
	my $topScore;
	my $topEVal;
	my $ties;
	my $taxID;
	my %lcaSet;
	my $totalScore;
	my $zeroEVal;
	
	while ( 1 )
	{
		my $line = <BLAST>;
		
		chomp $line;
		
		if ( $line =~ /^#/ )
		{
			if ( $line =~ /Query: ([\S]+)/ )
			{
				# Initialize taxID and score in case this query has no hits
				
				$taxIDs->{$1} = -1;
				$scores->{$1} = 0;
			}
			
			next;
		}
		
		my
		(
			$queryID,
			$hitID,
			$identity,
			$length,
			$mismatches,
			$gaps,
			$queryStart,
			$queryEnd,
			$subjectStart,
			$subjectEnd,
			$eVal,
			$bitScore
		) = split /\t/, $line;
		
		if ( $queryID ne $lastQueryID )
		{
			if
			(
				! defined $lastQueryID &&
				! defined $taxIDs->{$queryID} &&
				$options{'include'}
			)
			{
				ktWarn("-i specified but $fileName does not contain comment lines. Queries with no hits will not be included for this file."); 
			}
			
			if (  $ties )
			{
				# add the chosen hit from the last queryID
				
				if ( ! $options{'random'} )
				{
					$taxID = taxLowestCommonAncestor(keys %lcaSet)
				}
				
				$taxIDs->{$lastQueryID} = $taxID;
				$scores->{$lastQueryID} = $totalScore / $ties;
			}
			
			$ties = 0;
			$totalScore = 0;
			%lcaSet = ();
		}
		
		if ( ! defined $hitID )
		{
			last; # EOF
		}
		
		$hitID =~ /gi\|(\d+)/;
		
		my $gi = $1;
		
		if # this is a 'best' hit if...
		(
			$queryID ne $lastQueryID || # new query ID (including null at EOF)
			$bitScore > $topScore - $options{'threshold'} || # within score threshold
			$options{'factor'} && $eVal <= $options{'factor'} * $topEVal # within e-val factor
		)
		{
			# add score for average
			#
			if ( $options{'percentIdentity'} )
			{
				$totalScore += $identity;
			}
			elsif ( $options{'bitScore'} )
			{
				$totalScore += $bitScore;
			}
			else
			{
				if ( $eVal > 0 )
				{
					$totalScore += (log $eVal) / log 10;
				}
				else
				{
					$totalScore += $minEVal;
					$zeroEVal = 1;
				}
			}
			#
			$ties++;
			
			if # use this hit if...
			(
				! $options{'random'} || # using LCA
				$queryID ne $lastQueryID || # new query ID
				int(rand($ties)) == 0 # randomly chosen to replace other hit
			)
			{
				my $newTaxID = getTaxIDFromGI($gi);
				
				if ( ! $newTaxID || ! defined $taxParents[$newTaxID] )
				{
					$missingGIs{$gi} = 1;
					$newTaxID = 1;
				}
				
				if ( $options{'random'} )
				{
					$taxID = $newTaxID;
				}
				else
				{
					$lcaSet{$newTaxID} = 1;
				}
			}
		}
		
		if ( $queryID ne $lastQueryID )
		{
			$topScore = $bitScore;
			$topEVal = $eVal;
		}
		
		$lastQueryID = $queryID;
	}
	
	if ( $zeroEVal )
	{
		ktWarn("\"$fileName\" had e-values of 0. Approximated log[10] of 0 as $minEVal.");
	}
}	

sub default
{
	# Use a variable if it is defined or return a default value if it is not.
	
	my ($value, $default) = @_;
	
	if ( defined $value )
	{
		return $value;
	}
	else
	{
		return $default;
	}
}

sub getKronaOptions
{
	# Parse command line arguments and set options using Getopt::Long
	
	my @options = @_;
	
	my %params;
	
	foreach my $option ( @options )
	{
		$params{$optionFormats{$option}} = \$options{$option};
	}
	
	if ( ! GetOptions(%params) )
	{
		exit;
	}
	
	validateOptions();
}

sub getOption
{
	my ($option) = @_;
	
	return $options{$option};
}

sub getOptionString
{
	# Make a string from the option format to show as the command line option
	
	my ($option) = @_;
	
	my ($short, $type) = split /=/, $optionFormats{$option};
	my $string = "[-$short";
	
	if ( defined $type )
	{
		$string .= " <$optionTypes{$type}>";
	}
	
	$string .= ']';
	
	return $string;
}

sub getScoreName
{
	if ( getOption('bitScore') )
	{
		return 'Avg. bit score';
	}
	elsif ( getOption('percentIdentity') )
	{
		return 'Avg. % identity';
	}
	else
	{
		return 'Avg. log e-value';
	}
}

sub getScriptName
{
	return fileparse($0);
}

sub getTaxDepth
{
	my ($taxID) = @_;
	checkTaxonomy();
	return $taxDepths[$taxID];
}

sub getTaxName
{
	my ($taxID) = @_;
	checkTaxonomy();
	return $taxNames[$taxID]
}

sub getTaxParent
{
	my ($taxID) = @_;
	
	checkTaxonomy();
	
	do
	{
		$taxID = $taxParents[$taxID];
	}
	while (! $options{'noRank'} && $taxID > 1 && $taxRanks[$taxID] eq 'no rank');
	
	return $taxID;
}

sub getTaxRank
{
	my ($taxID) = @_;
	checkTaxonomy();
	return $taxRanks[$taxID];
}

sub getTaxIDFromGI
{
	my ($gi) = @_;
	
	if ( ! defined $taxIDByGI{$gi} )
	{
		if ( ! open GI, "<$options{'taxonomy'}/gi_taxid.dat" )
		{
			print "ERROR: GI to TaxID data not found.  Was updateTaxonomy.sh run?\n";
			exit 1;
		}
		
		seek GI, $gi * 4, 0;
		
		my $data;
		
		read GI, $data, 4;
		
		my $taxID = unpack "L", $data;
		
		close GI;
		
		if ( 0 && $taxID == 0 )
		{
			$taxIDByGI{$gi} = 1;
		}
		else
		{
			$taxIDByGI{$gi} = $taxID;
		}
	}
	
	return $taxIDByGI{$gi};
}

sub htmlFooter
{
	return "</div></body></html>\n";
}

sub htmlHeader
{
	my $path;
	my $notFound;
	my $script;
	
	if ( $options{'standalone'} && ! $options{'local'} &&  ! $options{'url'} )
	{
		$script =
			indent(2) . "<script language=\"javascript\" type=\"text/javascript\">\n" .
			slurp("$libPath/../$javascript") . "\n" .
			indent(2) . "</script>\n";
		
		$hiddenImage = slurp("$libPath/../img/hidden.uri");
		$loadingImage = slurp("$libPath/../img/loading.uri");
		$favicon = slurp("$libPath/../img/favicon.uri");
		$logo = slurp("$libPath/../img/logo-small.uri");
	}
	else
	{
		if ( $options{'local'} )
		{
			$path = "$libPath/../";
			$notFound = "This is a local chart and must be viewed on the computer it was created with.";
		}
		else
		{
			$path = "$options{'url'}/";
			$notFound = "Could not get resources from \\\"$options{'url'}\\\".";
		}
		
		$script = indent(2) . "<script src=\"$path$javascript\"></script>\n";
	}
	
	return
		'<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Strict//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd">' . "\n" .
		'<html xmlns="http://www.w3.org/1999/xhtml" xml:lang="en" lang="en">' . "\n" .
		indent(1) . "<head>\n" .
			indent(2) . "<meta charset=\"utf-8\"/>\n" .
#			indent(2) . "<base href=\"$path\" target=\"_blank\"/>\n" .
			indent(2) . "<link rel=\"shortcut icon\" href=\"$path$favicon\"/>\n" .
			indent(2) . "<script id=\"notfound\">window.onload=function(){document.body.innerHTML=\"$notFound\"}</script>\n" .
			$script .
		indent(1) . "</head>\n" .
		indent(1) . "<body>\n" .
			indent(2) . "<img id=\"hiddenImage\" src=\"$path$hiddenImage\" style=\"display:none\"/>\n" .
			indent(2) . "<img id=\"loadingImage\" src=\"$path$loadingImage\" style=\"display:none\"/>\n" .
			indent(2) . "<img id=\"logo\" src=\"$path$logo\" style=\"display:none\"/>\n" .
			indent(2) . "<noscript>Javascript must be enabled to view this page.</noscript>\n" .
			indent(2) . "<div style=\"display:none\">\n";
}

sub ktDie
{
	my ($error) = @_;
	
	*STDOUT = *STDERR;
	printColumns('[ ERROR ]', $error);
	exit 1;
}

sub ktWarn
{
	my ($warning) = @_;
	
	printColumns('   [ WARNING ]', $warning);
}

sub loadEC
{
	open EC, "<$ecFile" or die "$ecFile not found.";
	
	<EC>; # eat header
	
	while ( <EC> )
	{
		chomp;
		my ($ec, $name) = split /\t/;
		$ecNames{$ec} = $name;
	}
	
	close EC;
}

sub loadMagnitudes
{
	# load magnitudes from a tab-delimited file listing query IDs and magnitudes
	
	my
	(
		$magFile, # file name
		$magnitudes # hash ref (to be populated)
	) = @_;
	
	open MAG, "<$magFile" or die "Couldn't load $magFile";
	
	while ( my $line = <MAG> )
	{
		chomp $line;
		my ( $id, $mag ) = split /\t/, $line;
		$magnitudes->{$id} = $mag;
	}
	
	close MAG;
}

sub loadTaxonomy
{
	open INFO, "<$options{'taxonomy'}/taxonomy.tab" or die
		"Taxonomy not found in $options{'taxonomy'}. Was updateTaxonomy.sh run?";
	
	while ( my $line = <INFO> )
	{
		chomp $line;
		my ($id, $depth, $parent, $rank, $name) = split /\t/, $line;
		
		$taxParents[$id] = $parent;
		$taxDepths[$id] = $depth;
		$taxRanks[$id] = $rank;
		$taxNames[$id] = $name;
	}
	
	if ( $taxParents[2] == 1 && $options{'noRank'} )
	{
		ktDie
		(
"Local taxonomy database is out of date and does not support the
-$optionFormats{'noRank'} option. Update using updateTaxonomy.sh."
		);
	}
	close INFO;
}

sub newTree
{
	my %tree = ();
	return \%tree;
}

sub parseDataset
{
	my ($input) = @_;
	
	$input =~ /([^:,]+)(:([^,]+))?(,"?([^"]+)"?)?/;
	
	my ($file, $mag, $name) = ($1, $3, $5);
	
	if ( ! defined $name )
	{
		$name = fileparse($file, qr/\.[^.]*/); # get dataset name from file
	}
	
	$name =~ s/</&amp;lt;/g;
	$name =~ s/>/&amp;gt;/g;
	
	return ($file, $mag, $name);
}

sub printColumns
{
	# Prints headers and descriptions in two columns. Even indeces of the
	# parameters should be headers (left column); odd indeces should be
	# descriptions (right column).
	
	my @columns = @_;
	
	my $maxHeaderLength;
	
	for ( my $i = 0; $i < @columns; $i += 2 )
	{
		if ( length $columns[$i] > $maxHeaderLength )
		{
			$maxHeaderLength = length $columns[$i];
		}
	}
	
	for ( my $i = 0; $i < @columns; $i += 2 )
	{
		if ( $i > 0 )
		{
			print "\n";
		}
		
		printHangingIndent
		(
			$columns[$i],
			$columns[$i + 1],
			$maxHeaderLength + 2
		);
	}
}

sub printHeader
{
	# Prints a string with decoration
	
	my ($header) = @_;
	
	my $width = length($header) + 2;
	
	#print ' ', '_' x $width, "\n";
	#print '/ ', $header, " \\\n";
	#print '\\', '_' x $width, "/\n\n";
	my $prefix = '/ ';
	my $fill = '_';
	my $suffix = ' \\___';
	my $fillLength = $columns - length($header) - length($prefix) - length($suffix);
	print ' ' x ($fillLength + 1);
	print '_' x $width;
	print "\n";
	print $fill x $fillLength;
	print "$prefix$header$suffix";
	print "\n\n";
#	print "\n [=== $header ===]]]\n\n";
}

sub printOptions
{
	# Takes a list of standard KronaTools options (defined in
	# %optionDescriptions) and prints them with their descriptions in columns.
	
	my @options = @_;
	
	my @optionColumns;
	
	foreach my $option ( @options )
	{
		my $header = '   ' . getOptionString($option);
		my $description = $optionDescriptions{$option};
		
		if ( defined $options{$option} )
		{
			$description .= " [Default: '$options{$option}']";
		}
		
		push @optionColumns, $header, $description;
	}
	
	printHeader('Options');
	printColumns(@optionColumns);
	print "\n";
}

sub printUsage
{
	my
	(
		$description, # script description
		$argumentName,
		$argumentDescription,
		$useMagnitude, # show optional magnitude argument
		$useName, # show optional name argument
		$options # array ref of option names, defined in %option*
	) = @_;
	
	my $scriptName = getScriptName();
	
	printHeader("KronaTools $version - $scriptName");
	printHangingIndent('', $description);
	printHeader('Usage');
	print "$scriptName \\\n";
	print "   [options] \\\n";
	print
		'   ', 
		argumentString($argumentName, $useMagnitude, $useName, 1),
		" \\\n";
	print
		'   ',
		argumentString($argumentName, $useMagnitude, $useName, 2),
		" \\\n";
	print "   ...\n\n";
	
	my $combineString;
	
	foreach my $option ( @$options )
	{
		if ( $option eq 'combine' )
		{
			$combineString =
' By default, separate datasets will be created for each input (see ' .
getOptionString('combine') . ').'
		}
	}
	
	my @columns =
	(
		"   $argumentName",
		$argumentDescription . $combineString
	);
	
	if ( $useMagnitude )
	{
		push @columns,
			"   $argumentNames{'magnitude'}",
			$argumentDescriptions{'magnitude'};
	}
	
	if ( $useName )
	{
		push @columns,
			"   $argumentNames{'name'}",
			$argumentDescriptions{'name'};
	}
	
	printColumns(@columns);
	printOptions(@$options);
}

sub setOption
{
	my ($option, $value) = @_;
	
	$options{$option} = $value;
}

sub taxContains
{
	# determines if $parent is an ancestor of (or equal to) $child
	
	my ($parent, $child) = @_;
	
	my $depthParent = $taxDepths[$parent];
	
	while ( $taxDepths[$child] > $taxDepths[$parent] )
	{
		$child = $taxParents[$child];
	}
	
	return $parent == $child;
}

sub taxLowestCommonAncestor
{
	my @nodes = @_;
	
	checkTaxonomy();
	
	# walk the nodes up to an equal depth
	#
	my $minDepth;
	#
	foreach my $node ( @nodes )
	{
		if ( ! defined $minDepth || $taxDepths[$node] < $minDepth )
		{
			$minDepth = $taxDepths[$node];
		}
	}
	#
	foreach my $node ( @nodes )
	{
		while ( $taxDepths[$node] > $minDepth )
		{
			$node = $taxParents[$node];
		}
	}
	
	my $done = 0;
	
	while ( ! $done )
	{
		$done = 1;
		
		my $prevNode;
		
		foreach my $node ( @nodes )
		{
			if ( defined $prevNode && $prevNode != $node )
			{
				$done = 0;
				last;
			}
			
			$prevNode = $node;
		}
		
		if ( ! $done )
		{
			for ( my $i = 0; $i < @nodes; $i++ )
			{
				if ( ! defined $taxParents[$nodes[$i]] )
				{
					ktDie("Undefined parent for taxID $nodes[$i]");
					return;
				}
				
				$nodes[$i] = $taxParents[$nodes[$i]];
			}
		}
	}
	
	return $nodes[0];
}

sub taxIDExists
{
	my ($taxID) = @_;
	checkTaxonomy();
	return defined $taxDepths[$taxID];
}

sub writeTree
{
	# Writes a Krona chart from a tree created with "addBy..." functions.
	#
	# Uses options: collapse, color, local, name, out, showKey, url
	
	my
	(
		$tree, # hash ref to head node of tree
		$attributes, # array ref with names of attributes to display
		$attributeDisplayNames, # array ref with display names for $attributes
		$datasetNames, # array ref with names of datasets
		$hueStart, # (optional) hue at the start of the gradient for score
		$hueEnd # (optional) hue at the end of the gradient for score
	) = @_;
	
	if ( %missingGIs )
	{
		ktWarn
		(
			"The following GIs were not found in the local taxonomy database and were set to root (if they were recently added to NCBI, use updateTaxonomy.sh to update the local database):\n" .
			join ',', (keys %missingGIs)
		);
		
		%missingGIs = ();
	}
	
	if ( %missingTaxIDs )
	{
		ktWarn
		(
			"The following taxonomy IDs were not found in the local database and were set to root (if they were recently added to NCBI, use updateTaxonomy.sh to update the local database):\n" .
			join ',', (keys %missingTaxIDs)
		);
		
		%missingTaxIDs = ();
	}
	
	my %attributeHash;
	
	for ( my $i = 0; $i < @$attributes; $i++ )
	{
		$attributeHash{$$attributes[$i]} = $$attributeDisplayNames[$i];
	}
	
	if ( $options{'leafAdd'} )
	{
		setInternalValues($tree);
	}
	
	my ($valueStart, $valueEnd);
	
	if ( defined $hueStart && defined $hueEnd )
	{
		($valueStart, $valueEnd) = setScores($tree);
	}
	
	# check if members should be stored in supplemental files
	#
	my $totalCount;
	my $supp;
	#
	foreach my $count ( @{$tree->{'count'}} )
	{
		$totalCount += $count;
		
		if ( $count > $memberLimitDataset || $totalCount > $memberLimitTotal )
		{
			$supp = 1;
			last;
		}
	}
	
	print "Writing $options{'out'}...\n";
	
	if ( $supp )
	{
		my $suppDir = $options{'out'} . $suppDirSuffix;
		
		ktWarn("Too many query IDs to store in chart; storing supplemental files in '$suppDir'.");
		
		if ( -e $suppDir )
		{
			ktWarn("Overwriting files in '$suppDir'.");
			rmtree $suppDir or ktDie("Could not remove '$suppDir'.");
		}
		
		mkdir $suppDir or ktDie("Could not create $suppDir");
		
		open SUPP, ">$suppDir/$suppEnableFile" or ktDie("Could not write file to '$suppDir'");
		print SUPP "enableData();";
		close SUPP;
	}
	
	open OUT, ">$options{'out'}";
	print OUT htmlHeader();
	print OUT dataHeader
	(
		defined $attributeHash{'magnitude'} ? 'magnitude' : 'count',
		$attributes,
		$attributeDisplayNames,
		$datasetNames,
		'unassigned',
		'count',
		defined $hueStart ? 'score' : undef,
		$hueStart,
		$hueEnd,
		$valueStart,
		$valueEnd,
		$supp
	);
	
	my $nodeID = 0;
	print OUT toStringXML($tree, $options{'name'}, 0, \%attributeHash, \$nodeID, $supp);
	print OUT dataFooter();
	print OUT htmlFooter();
	close OUT;
}


################
# Not exported #
################


sub addMember
{
	my ($node, $set, $member) = @_;
	
#	$member =~ s/,/\\,/g;
#	$member =~ s/ /&#32;/g;
#	$member =~ s/"/&quot;/g;
	
	push @{$node->{'members'}[$set]}, $member;
}

sub argumentString
{
	my
	(
		$argumentName,
		$useMagnitude,
		$useName,
		$number
	) = @_;
	
	my $return;
	
	my $numberString;
	
	if ( $number > 0 )
	{
		$numberString = "_$number";
	}
	
	if ( $number > 1 )
	{
		$return .= '[';
	}
	
	$return .= "$argumentName$numberString";
	
	if ( $useMagnitude )
	{
		$return .= "[:$argumentNames{'magnitude'}$numberString]";
	}
	
	if ( $useName )
	{
		$return .= "[,$argumentNames{'name'}$numberString]";
	}
	
	if ( $number > 1 )
	{
		$return .= ']';
	}
	
	return $return;
}

sub checkTaxonomy
{
	if ( ! @taxParents )
	{
		die 'Taxonomy not loaded. "loadTaxonomy()" must be called first.';
	}
}

sub dataFooter
{
	return indent(2) . "</krona>\n";
}

sub dataHeader
{
	my
	(
		$magName,
		$attributes,
		$attributeDisplayNames,
		$datasetNames,
		$assignedName,
		$summaryName,
		$hueName,
		$hueStart,
		$hueEnd,
		$valueStart,
		$valueEnd,
		$supp
	) = @_;
	
	my $header =
	indent(2) . '<krona collapse="' . ($options{'collapse'} ? 'true' : 'false') .
	'" key="' . ($options{'key'} ? 'true' : 'false') . "\">\n" .
	indent(3) . "<attributes magnitude=\"$magName\">\n";
	
	# members
	#
	my $assignedText;
	my $summaryText;
	#
	if ( $assignedName && $summaryName )
	{
		my $memberTag = $supp ? 'data' : 'list';
		my $suppDir = basename($options{'out'}) . $suppDirSuffix;
		my $enableText = $supp ? " enable=\"$suppDir/$suppEnableFile\"" : '';
		$header .= indent(4) . "<$memberTag$enableText>members</$memberTag>\n";
		$assignedText = " ${memberTag}Node=\"members\"";
		$summaryText = " ${memberTag}\All=\"members\"";
		
		if ( $options{'postUrl'} )
		{
			$assignedText .= " postUrl=\"$options{'postUrl'}\" postVar=\"queries\"";
			$summaryText .= " postUrl=\"$options{'postUrl'}\" postVar=\"queries\"";
		}
	}
	
	# attributes
	#
	for ( my $i = 0; $i < @$attributes; $i++ )
	{
		my $attributeText;
		my $name = $$attributes[$i];
		
		if ( $$attributeDisplayNames[$i] )
		{
			$attributeText .= " display=\"$$attributeDisplayNames[$i]\"";
		}
		
		if ( $name eq 'count' )
		{
			# attach to list of members as summary of children
			
			$attributeText .= $summaryText;
		}
		elsif ( $name eq 'unassigned' )
		{
			# attach to list of members as node list
			
			$attributeText .= $assignedText;
		}
		elsif ( $name eq 'taxon' )
		{
			$attributeText .= " hrefBase=\"$taxonomyHrefBase\" target=\"taxonomy\"";
		}
		elsif ( $name eq 'ec' )
		{
			$attributeText .= " hrefBase=\"$ecHrefBase\" target=\"ec\"";
		}
		
		if
		(
			$name eq 'taxon' ||
			$name eq 'ec' ||
			$name eq 'rank'
		)
		{
			$attributeText .= ' mono="true"';
		}
		
		$header .= indent(4) . "<attribute$attributeText>$$attributes[$i]</attribute>\n";
	}
	
	$header .= indent(3) . "</attributes>\n";
	
	if ( @$datasetNames )
	{
		$header .= indent(3) . "<datasets>\n";
		
		foreach my $dataset ( @$datasetNames )
		{
			$dataset =~ s/</&lt;/g;
			$dataset =~ s/>/&gt;/g;
			
			$header .= indent(4) . "<dataset>$dataset</dataset>\n";
		}
		
		$header .= indent(3) . "</datasets>\n";
	}
	
	# hue
	#
	if ( defined $hueName )
	{
		my $colorDefault = $options{'color'} ? 'true' : 'false';
		
		$header .=
			indent(3) . "<color attribute=\"$hueName\" " .
			"hueStart=\"$hueStart\" hueEnd=\"$hueEnd\" " .
			"valueStart=\"$valueStart\" valueEnd=\"$valueEnd\" " .
			"default=\"$colorDefault\" " .
			"></color>\n";
	}
	
	return $header;
}

sub ecLink
{
	my ($ec) = @_;
	
	my $path = $ec;
	
	my $count = ($path =~ s/\./\//g);
	
	if ( $count == 3 )
	{
		$path .= ".html";
	}
	
	return $path;
}

sub ecText
{
	my ($ec) = @_;
	
	return "EC $ec";
}

sub indent
{
	my ($depth) = @_;
	
	return ' ' x $depth;
}

sub printHangingIndent
{
	my ($header, $text, $tab) = @_;
	
	my @words = split /\s+/, $text;
	
	my $col;
	
	if ( $header )
	{
		print $header, ' ' x ($tab - (length $header) - 1);
		$col = $tab;
	}
	else
	{
		my $word = shift @words;
		print $word;
		$col = length $word;
	}
	
	foreach my $word ( @words )
	{
		my $wordLength = length $word;
		
		if ( $col + $wordLength + 1 >= $columns )
		{
			print "\n", ' ' x $tab, $word;
			$col = $tab + $wordLength;
		}
		else
		{
			print " $word";
			$col += $wordLength + 1;
		}
	}
	
	print "\n";
}

sub setInternalValues
{
	my ($node) = @_;
	
	my @magnitudes;
	my @scoreTotals;
	my @scoreCounts;
	
	if ( defined $node->{'children'} )
	{
		foreach my $child (values %{$node->{'children'}})
		{
			setInternalValues($child);
			
			for ( my $i = 0; $i < @{$child->{'magnitude'}}; $i++ )
			{
				if ( ! defined $node->{'magnitude'}[$i] )
				{
					$magnitudes[$i] += $child->{'magnitude'}[$i];
				}
				
				if ( ! defined $node->{'scoreTotal'}[$i] )
				{
					$scoreTotals[$i] += $child->{'scoreTotal'}[$i];
				}
				
				if ( ! defined $node->{'scoreCount'}[$i] )
				{
					$scoreCounts[$i] += $child->{'scoreCount'}[$i];
				}
			}
		}
	}
	
	for ( my $i = 0; $i < @{$node->{'count'}}; $i++ )
	{
		if ( ! defined $node->{'magnitude'}[$i] )
		{
			$node->{'magnitude'}[$i] = $magnitudes[$i];
		}
		
		if ( ! defined $node->{'scoreTotal'}[$i] )
		{
			$node->{'scoreTotal'}[$i] = $scoreTotals[$i];
		}
		
		if ( ! defined $node->{'scoreCount'}[$i] )
		{
			$node->{'scoreCount'}[$i] = $scoreCounts[$i];
		}
	}
}

sub setScores
{
	my ($node) = @_;
	
	my $min;
	my $max;
	
	if ( defined $node->{'scoreCount'} )
	{
		$node->{'score'} = ();
		
		for ( my $i = 0; $i < @{$node->{'scoreCount'}}; $i++ )
		{
			my $score;
			
			if ( ${$node->{'scoreCount'}}[$i] )
			{
				$score =
					${$node->{'scoreTotal'}}[$i] /
					${$node->{'scoreCount'}}[$i];
				
				if ( $options{'logScore'} )
				{
					if ( $score > 0 )
					{
						$score = (log $score) / log 10;
					}
					else
					{
						$score = $minEVal;
					}
				}
				
				if ( ! defined $min || $score < $min )
				{
					$min = $score;
				}
				
				if ( ! defined $max || $score > $max )
				{
					$max = $score;
				}
			}
			else
			{
				$score = 0;
			}
			
			${$node->{'score'}}[$i] = sprintf("%g", $score);
		}
	}
	
	if ( defined $node->{'children'} )
	{
		foreach my $child (keys %{$node->{'children'}})
		{
			my ($childMin, $childMax) =
				setScores($node->{'children'}{$child});
			
			if ( ! defined $min || $childMin < $min )
			{
				$min = $childMin;
			}
			
			if ( ! defined $max || $childMax > $max )
			{
				$max = $childMax;
			}
		}
	}
	
	return ($min, $max);
}

sub slurp
{
	my ($fileName) = @_;
	
	local $/;
	open FILE, $fileName or die "Can't read file '$fileName' [$!]\n";
	my $file = <FILE>;
	close (FILE);
	return $file;
}

sub taxonLink
{
	my ($taxID) = @_;
	
	return $taxID;
}

sub toStringXML
{
	my ($node, $name, $depth, $attributeHash, $nodeIDRef, $supp) = @_;
	
	my $string;
	my $href;
	
	if ( $node->{'href'} )
	{
		$href = " href=\"$node->{'href'}\"";
	}
	
	$string = indent($depth) . "<node name=\"$name\"$href>\n";
	
	foreach my $key ( keys %$node )
	{
		if
		(
			$key ne 'children' &&
			$key ne 'scoreCount' &&
			$key ne 'scoreTotal' &&
			$key ne 'href' &&
			( keys %{$node->{'children'}} || ($key ne 'unassigned' && $key ne 'magnitudeUnassigned') ) &&
			( $key eq 'members' || defined $$attributeHash{$key} )
		)
		{
			$string .= indent($depth + 1) . "<$key>";
			
			my $i = 0;
			
			foreach my $value ( @{$node->{$key}} )
			{
				if ( $key eq 'members' )
				{
					if ( $supp )
					{
						if ( defined $value && @$value > 0 )
						{
							my $file = "node$$nodeIDRef.members.$i.js";
							
							$string .= "<val>$file</val>";
							
							open SUPP, ">$options{'out'}$suppDirSuffix/$file" or die;
							
							print SUPP "data('";
							foreach my $member ( @$value )
							{
								print SUPP "$member\\n\\\n";
							}
							print SUPP "')";
							
							close SUPP;
						}
						else
						{
							$string .= "<val></val>";
						}
					}
					else
					{
						$string .= "\n" . indent($depth + 2) . "<vals>";
						
						foreach my $member ( @$value )
						{
							$member =~ s/</&lt;/g;
							$member =~ s/>/&gt;/g;
							
							$string .= "<val>$member</val>";
						}
						
						$string .= "</vals>";
					}
				}
				else
				{
					my $href;
					
					if ( $key eq 'taxon' )
					{
						$href = ' href="' . taxonLink($value) . '"';
					}
					elsif ( $key eq 'ec' )
					{
						$href = ' href="' . ecLink($value) . '"';
						$value = ecText($value);
					}
					
					$value =~ s/</&lt;/g;
					$value =~ s/>/&gt;/g;
					
					$string .= "<val$href>$value</val>";
				}
				
				$i++;
			}
			
			if ( $key eq 'members' )
			{
				$string .= "\n" . indent($depth + 1);
			}
			
			$string .= "</$key>\n";
		}
	}
	
	$$nodeIDRef++;
	
	if ( defined $node->{'children'} && ( ! $options{'depth'} || $depth < $options{'depth'} ) )
	{
		foreach my $child (keys %{$node->{'children'}})
		{
			$string .= toStringXML($node->{'children'}{$child}, $child, $depth + 1, $attributeHash, $nodeIDRef, $supp);
		}
	}
	
#	print "$string\n";
	return $string . indent($depth) . "</node>\n";
}

sub validateOptions
{
	if ( defined $options{'factor'} && $options{'factor'} < 1 )
	{
		my $factor = getOptionString('factor');
		ktDie("E-value factor ($factor) must be at least 1.");
	}
	
	if ( $options{'percentIdentity'} && $options{'bitScore'} )
	{
		my $pi = getOptionString('percentIdentity');
		my $bs = getOptionString('bitScore');
		
		ktDie("Cannot use $bs and $pi together.");
	}
}


1;
