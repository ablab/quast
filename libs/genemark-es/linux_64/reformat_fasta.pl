#!/usr/bin/perl
# --------------------------------------------
# Alex Lomsadze
# Georgia Institute of Technology, -2014
#
# Format sequence according to local eukaryotic
# standard on sequence and definition line style
# --------------------------------------------

use strict;
use warnings;
use Getopt::Long;
use Cwd qw( abs_path );

my $Version = "2.11";
# --------------------------------------------
my @in = ();
my $out = '';
my $trace = '';
my $label = '';
my $up = 0;
my $soft_mask = 0;
my $v = 0;
my $first;
my $lcl;
my $external;
my $native;

Usage( $Version ) if ( @ARGV < 1 );
ParseCMD();
CheckBeforeRun();

my %extr_hash;  # this is used only if $extr is defined

ReadNewDeflinesFromFile( $external, \%extr_hash ) if defined $external;
	
my $trace_data;  # this is used only if $trace is defined
my $index = 0;   # counter in unique index

open( OUT, ">$out" ) or die "Error on open file: $out, $!\n";
foreach my $file ( @in )
{
	$file = abs_path( $file );
	
	print "$file\n" if $v;
	
	$index = ProcessFile( $file, $index );
}
close OUT;

SaveTraceInfoToFile( $trace, $trace_data ) if $trace;

exit 0;

# ================== sub =====================
sub PrintDefline
{
	my ( $line, $id, $file ) = @_;

	my $defline;
	
	
	if ( defined $native )  # use original defline
	{
		$defline = $line;
	}
	elsif ( defined $first )  # use first word in original defline
	{
		if ( $line =~ /^>\s*(\S+)\s*/ )
		{
			$defline = ">" . $1;
		}
		else { print "error, unexpected empty defline found: $line\n"; exit 1; }
	}
	elsif ( defined $external ) # use defline loaded from external file
	{
		$line =~ /^>\s*(\S.*?)\s*$/;
		
		if ( exists $extr_hash{ $1 } )
		{
			$defline = ">". $extr_hash{ $1 };
		}
		else { print "error, defline in file is missing matching defilne in $external for: $1\n"; exit 1; }
	}
	else  # build new defline - default method
	{
		my $prefix = ">";
		
  		if ( defined $lcl )  # ncbi style defline
  		{
  			$prefix = ">lcl|";
  		}
  		
  		$label = '' if !$label;
  		
		$defline = $prefix . $id . $label;
 	}

	print OUT "$defline\n";
	
	if( $trace )
	{
		$trace_data .= $defline ."\t". $file ."\t". $line ."\n";
	}
	
	print $id ."\n" if $v;
}
# --------------------------------------------
sub ProcessFile
{
	my ( $file, $id ) = @_;

	my $defline_found = 0;
	my $line_count = 0;
	my $repl_count = 0;

	open( IN, $file ) or die "error on open file $0: $file, $!\n";
	while( my $line = <IN> )
	{
		++$line_count;

		# skip empty
		if ( $line =~ /^\s*$/ ) { next; }

		# if defline was found
		if ( $line =~ /^>/ )
		{
			++$id;
			$defline_found = 1;
			PrintDefline( $line, $id, $file );
		}
		else
		{
			# if sequence started and defline is missing
			if ( !$defline_found )
			{	
				++$id;
				$defline_found = 1;
				PrintDefline( "", $id, $file );
			}
			
			# parse sequence
			
			# move to dna characters
			# $line =~ tr/Uu/Tt/;
			
			
			# low case 'n' not a repeat in soft masking
			
			if( $soft_mask )
			{
				$line =~ tr/atcg/x/;
				$repl_count += ( $line =~ tr/rykmswbdhv/x/ );
			}

			if( $up )
			{
				$line = uc( $line );
			}

			# remove white spaces, etc
			$line =~ tr/0123456789\n\r\t //d;
			
			# replace allowed nucleic acid code (non A T C G) by N
			$repl_count += ( $line =~ tr/RYKMSWBDHVrykmswbdhv/N/ );

			if ( $line =~ m/[^ATCGatcgNnXx]/ )
				{ print "error, unexpected letter found in file: $file on line: $line_count\n"; exit 1; }
				
			print OUT "$line\n";
		}
	}
	close IN;
	
	print "# substitutions to N in file $file: $repl_count\n" if $v;

	if ( !$defline_found ) { print "error, empty file on input: $file\n"; exit 1; }

	return $id;
}
# --------------------------------------------
sub ReadNewDeflinesFromFile
{
	my ( $name, $ref ) = @_;
	
	open( IN, $name ) or die "error on open file $0: $name $!\n";
	while( my $line = <IN> )
	{
		if ( $line =~ /^#/ ) {next;}
		if ( $line =~ /^\s+$/ ) {next;}
		
		$line =~ s/\s+$//;
		
		if ( $line =~ /^\s*(\S+)\s+(\S.*)/ )
		{
			if ( exists $ref->{$2} ) { print "error: unique label is expected $0: $line\n"; exit 1; }

			$ref->{$2} = $1;
		}
		else { print "error: unexpected format found $0: $line\n"; exit 1; }
	}
	close IN ; 	
}
# --------------------------------------------
sub SaveTraceInfoToFile
{
	my ( $name, $data ) = @_;
	open( OUT, ">$name" ) or die "Error on open file: $name, $!\n";
	print OUT "#new_defline\told_file_name\told_defline\n";
	print OUT $data;
	close OUT;
}
# --------------------------------------------
sub CheckBeforeRun
{
	if ( !$out )    { print "error, output file name is missing: $0\n"; exit 1; }
	if ( $#in < 0 ) { print "error, input file name is missing: $0\n"; exit 1; }
	foreach my $file ( @in )
	{
		if ( !-e $file )
			{ print "error, file not found $0: $file\n"; exit 1; }
	}
}
# --------------------------------------------
sub ParseCMD
{
	my $opt_result = GetOptions
	(
	  'in=s{,}'     => \@in,
	  'out=s'       => \$out,
	  'trace=s'     => \$trace,
	  'label=s'     => \$label,
	  'up'          => \$up,
	  'soft_mask'   => \$soft_mask,
	  'v'           => \$v,
	  'first'       => \$first,
	  'lcl'         => \$lcl,
	  'external=s'  => \$external,
	  'native'      => \$native
	);

	if ( !$opt_result ) { print "error on command line: $0\n"; exit 1; }
}
# --------------------------------------------
sub Usage
{
	my $version = shift;

	print qq(
Usage: $0 --out [output file] --trace [filename] --label [string] --dna [input dna_file/s]
Input is file/s with FASTA like formatted DNA sequence 
version $version

  This program:
  * Creates one file with FASTA formatted sequence and with unique definition lines.
  * Replaces all non [ATCGNXUatcgunx] allowed letters letters by 'N'
  * Saves old-new relation of definition lines in trace file
  * Stops if unexpected letter found

  --in     [dna input file/s] input data
  --out    [filename] output sequence here

  optional
  --trace  [filename] put information about the new - old defline relationships here
  --label  [string] label for forming new unique defline:  '>\$counter_\$label'
  --up     uppercase all letters
  --soft_mask  mask lowercase letters
  --v      verbose
  --first  use first word in original defline as fasta id
  --lcl    use genbank defline format for local databases '>lcl|id'
  --external [filename] read new label from this file
           two column format: 'defline_in_input' 'deline_for_output'
  --native keep original definition line; 
);
	exit 1;
}
# --------------------------------------------
