#!/usr/bin/perl
# --------------------------------------------
# Alex Lomsadze
# Georgia Institute of Technology, -2014
#
# Move RNA-Seq data to gff format
# --------------------------------------------

use strict;
use warnings;
use Getopt::Long;
use Cwd qw(abs_path cwd);
use Data::Dumper;
use File::Spec;

# ------------------------------------------------

my $in = '';
my $out = '';
my $trace = '';
my $left_to_right = 0;
my $right_to_left = 0;
my $label ='UnSplicer';
my $v = 0;
my $debug = 0;

# ------------------------------------------------
Usage() if ( @ARGV < 1 );
ParseCMD();
CheckInput();
# ------------------------------------------------

# sequence ID's for GFF
my %trace_hash;
if( $trace )
{
	ReadSeqID( $trace, \%trace_hash );
}

open( IN, $in ) or die "error on open file $in :  $!\n";
open( OUT, ">$out" ) or die "error on open file $out : $!\n";

my @arr;
my $str;

while( my $line = <IN> )
{
	chomp($line);
	@arr = split( '\t', $line );

	if( !$trace )
	{
		$str = $arr[0];
	}
	else
	{
		if( exists $trace_hash{$arr[0]} )
		{
			$str = $trace_hash{$arr[0]};
		}
		else
			{ print "error, new sequence ID not found $0: $arr[0]\n"; exit 1; }
	}
	
	$str .= "\t$label\tintron\t".  $arr[1] ."\t". ($arr[2]-1) ."\t".  $arr[4] ."\t". $arr[3] ."\t.\t.\n";
	
	print OUT $str;
}
close IN;
close OUT;

exit 0;

# ================= subs =========================
sub ReadSeqID
{
	my ( $name, $ref ) = @_;

	my $from_id;
	my $to_id;

	open( my $TRACE, $name ) or die "error on open file $0: $name\n$!\n";
	while( my $line = <$TRACE> )
	{
		if( $line =~ /^#/ ) {next;}
		if( $line =~ /^\s*$/ ) {next;}
		
		if( $line =~ /^\>(\S+)\s+\S+\s+\>(\S+)\s*/ )
		{
			if ( $left_to_right  )
			{
				$from_id = $1;
				$to_id = $2;
			}
			elsif( $right_to_left )
			{
				$from_id = $2;
				$to_id = $1;				
			}
			else
				{ print "error, direction of seq-id change not specified\n"; exit 1; }
                        
			if( exists $ref->{$from_id} )
				{ print "error, unique label is expected $0: $from_id\n"; exit 1; }

			$ref->{$from_id} = $to_id;
			
			print "$from_id\t$to_id\n" if $v;
		}
		else
			{ print "error, unexpected line format found $0: $line\n"; exit 1; }
	}
	close $TRACE; 
}
# ------------------------------------------------
sub CheckInput
{
	$in    = ResolvePath( $in );
	$trace = ResolvePath( $trace );
	if( !$out ) {print "error, output file name is not specified\n"; exit 1;}
}
# ------------------------------------------------
sub ResolvePath
{
	my ( $name, $path ) = @_;
	return '' if !$name;
	$name = File::Spec->catfile( $path, $name ) if ( defined $path and $path );
	if( ! -e $name ) { print "error, file not found $0: $name\n"; exit 1; }
	return abs_path( $name );
}
# ------------------------------------------------
sub ParseCMD
{
	my $cmd = $0;
	foreach my $str (@ARGV) { $cmd .= ( ' '. $str ); }
	
	my $opt_result = GetOptions
	(
		'in=s'    => \$in,
		'out=s'   => \$out,
		'trace=s' => \$trace,
		'label=s' => \$label,
		'left_to_right' => \$left_to_right,
		'right_to_left' => \$right_to_left,
		'verbose' => \$v,
		'debug'   => \$debug
	);
	
	if( !$opt_result ) { print "error on command line\n"; exit 1; }
	if( @ARGV > 0 ) { print "error, unexpected argument found on command line: @ARGV\n"; exit 1; }
	$v = 1 if $debug;

	print $cmd if $debug;	
}
# ------------------------------------------------
sub Usage
{
	print qq(# -------------------
Usage: $0   parameters

required:
  --in       [name]
  --out      [name]
 optional:
  --trace    [name] change sequence ID using TRACE from file
  --left_to_right
  --right_to_left
  --label    [label] label to use for column 2 in GFF file
  --verbose
  --debug
# -------------------
);
	exit 1;
}
# ------------------------------------------------

