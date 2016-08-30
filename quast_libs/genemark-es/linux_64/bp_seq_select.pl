#!/usr/bin/perl
# ---------------------------------------------
# Alex Lomsadze
# Georgia Institute of Technology, -2014
#
# select sequence for BP model building
# to do - change min-max method
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

my $seq_in  = '';
my $seq_out = '';
my $max_seq_number = 0;
my $min_seq_number = 600;
my $bp_region_length = 0;
my $min_bp_region_length = 40;
my $X = 0.5;

Usage() if ( @ARGV < 1 );
ParseCMD();
CheckInput();
# ------------------------------------------------

my %h;

LoadLengthInfo( $seq_in, \%h, $min_bp_region_length  );

if(!%h) { print "error, hash is empty: $0\n"; exit 1; }

# sort hash by value (intron length frequency) ; bell shape intron length distribution is expected 
# sorted by key (length)
# 1 -  5
# 2 - 10
# 3 -  4
# sorted by value (frequency)
# 2 - 10
# 1 -  5
# 3 -  4
my @keys_v_sorted = sort { $h{$b} <=> $h{$a} } keys(%h);

#----
# find how many introns
my $total_regions = 0;
foreach my $i (@keys_v_sorted)
{
	$total_regions += $h{$i};
}

#----
# find L-R such that x% is under the bell

my $under_the_bell = 0;
my $i = 0;
my $L = $keys_v_sorted[$i];
my $R = $keys_v_sorted[$i];

while( $under_the_bell < $X * $total_regions )
{
	$under_the_bell += $h{$keys_v_sorted[$i]};
	
	if( $keys_v_sorted[$i] < $L )
	{
		$L = $keys_v_sorted[$i];
	}
	
	if( $keys_v_sorted[$i] > $R )
	{
		$R = $keys_v_sorted[$i];
	}
	
	++$i;
}

print "$L $R $under_the_bell $total_regions\n" if $debug;

#----
my $min_length = $L;
my $max_length = $R;

#----
# adjust bp-region length

my $new_bp_region = $bp_region_length;
if( $new_bp_region > $L - 4 )
{
	$new_bp_region = $L - 4;
};

print "def bp_r $bp_region_length ; new_bp_r $new_bp_region\n" if $debug;

# craete output

CreateOutput( $seq_in, $seq_out, $min_length, $max_length, $max_seq_number, $new_bp_region );


exit 0;

# ------------------------------------------------
sub CreateOutput
{
	my( $name_in, $name_out, $min, $max, $limit, $bp_region ) = @_;
	
	open( my $OUT, ">", $name_out ) or die "error on open file $0: $name_out\n$!\n"; 		
	open( my $IN, $name_in ) or die "error on open file $0: $name_in\n$!\n";

	my $length_of_intron;
	my $seq;
	my $out_seq;
	my $count = 0;

	while( my $line = <$IN> )
	{
		if ($count >= $limit ) {last;}
		
		# skip comments and empty
		if ( $line =~ /^\#/ )   {next;}
		if ( $line =~ /^\s+$/ ) {next;}
		
		if ( $line =~ /^(\d+)\s+(\S+)\s*$/ )
		{
			$length_of_intron = $1;
			$seq = $2;
			
			if( $length_of_intron >= $min  && $length_of_intron <= $max )
			{
				$out_seq = substr( $seq, -$bp_region );

				if ( $out_seq !~ /N/ )
				{
					++$count;
					print $OUT (">". $count ."_". $length_of_intron ."\n");
					print $OUT ( $out_seq ."\n");
				}
				else
				{
					print "ignorig from BP search: $out_seq\n" if $debug;
				}
			}
		}
		else { print "error, unexpect format found in input seq $0: $line\n"; exit 1; }			
	}
	close $OUT;
	close $IN;
}
# ------------------------------------------------
sub LoadLengthInfo
{
	my( $name, $ref, $min ) = @_;
	
	if (!$name) { print "error, file name is expected here $0\n"; exit 1; }
	
	my $lines_in = 0;
	my $lines_used = 0;
	
	my $IN;
	
	open( $IN, $name ) or die "error on open file $0: $name\n$!\n"; 
	while( my $line = <$IN> )
	{
		++$lines_in;
		
		# skip comments and empty
		if ( $line =~ /^\#/ )   {next;}
		if ( $line =~ /^\s+$/ ) {next;}
		
		if ( $line =~ /^(\d+)\s+(\S+)\s*$/ )
		{
			if ( length($2) >= $min )
			{
				$ref->{$1} += 1;
				++$lines_used;
			}
			else
			{
				print "ignored: $line" if $debug;
			}
		}
		else { print "error, unexpected format found in input seq $0: $line\n"; exit 1; }
	}
	close $IN;
	
	print "lines in and used: $lines_in  $lines_used\n" if $debug;
}
# ------------------------------------------------
sub CheckInput
{
	if( !$seq_in ) { print "error, input seq file name is not specified $0\n"; exit 1; }
	$seq_in = ResolvePath( $seq_in );
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
		'seq_in=s'      => \$seq_in,
		'seq_out=s'     => \$seq_out,
		'max_seq_num=i' => \$max_seq_number,
		'min_seq_num=i' => \$min_seq_number,
		'bp_region_length=i'     => \$bp_region_length,
		'min_bp_region_length=i' => \$min_bp_region_length,
		'verbose'   => \$v,
		'debug'     => \$debug
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
  --seq_in    [filename]  input sequence; one entry per line:  length_of_intron   sequence upstream of acceptor   
  --seq_out   [filename]  output sequence; FASTA ; subset of input data
  --max_seq_num       [int]  maximum number of sequences to output
  --bp_region_length  [int]  default length of BP-region
  --min_bp_region_length  [int] 
  --min_seq_num       [int]  relax selection if less than this number was found

optional:
  --verbose
  --debug
# -------------------
);
	exit 1;
}
# ------------------------------------------------


