#!/usr/bin/perl
# --------------------------------------------
# Alex Lomsadze
# Georgia Institute of Technology, -2014
#
# input:  positive lengths
# output: probability density estimation
#         nearest neighbors and/or naive method
# --------------------------------------------
# add logger
# --------------------------------------------

use strict;
use warnings;
use Getopt::Long;
use Cwd qw(abs_path cwd);
use Data::Dumper;
use FindBin;
use lib "$FindBin::Bin/../lib";
use YAML;
use Hash::Merge qw( merge );
use File::Spec;

# ------------------------------------------------
my $v = 0;
my $debug = 0;

# system control
#   $cfg - reference on hash with parameters
#   $log - reference on logger

my $cfg;
my $section = 'length';
# ------------------------------------------------

Usage() if ( @ARGV < 1 );
ParseCMD();
CheckInput();
print Dumper( $cfg ) if $debug;

# ------------------------------------------------

my %in_counts;

if( $cfg->{$section}->{'in'} )
{
	LoadData( $cfg->{$section}->{'in'}, \%in_counts );
}
elsif( $cfg->{$section}->{'gamma'} )
{
	LoadGamma( $cfg->{$section}->{'gamma'}, \%in_counts );
}
else { print "error input is not specified $0\n"; exit 1; }


SetMinMax( \%in_counts );

my %counts;
FilterCounts( \%in_counts, \%counts, $cfg->{$section}->{'smooth3'} );

my $min_;
my $max_;

if( !$cfg->{$section}->{'smooth3'} )
{
	$min_ = $cfg->{$section}->{'min'};
	$max_ = $cfg->{$section}->{'max'};	
}
else
{
	$min_ = int $cfg->{$section}->{'min'}/3;
	$max_ = int $cfg->{$section}->{'max'}/3;
}

my %stat;

$stat{'sum'}  = 0;
$stat{'n'}    = 0;
$stat{'mean'} = 0;
$stat{'std'}  = 0;
$stat{'q1'}   = 0;
$stat{'q3'}   = 0;
$stat{'iqr'}  = 0;
	
GetStat( \%counts, \%stat );

my $K = GetK( \%stat );
my $W = GetW( \%stat );

my @data;
CopyHashToArray( \%counts, \@data, $max_ );

my @histo;
for( my $i = 0; $i <= $max_; ++$i )
{
	$histo[$i] = 0;
}

my $com = uc( $cfg->{$section}->{'com'} );

if ( $com =~ /K/ and $com !~ /W/ )
{
	Histogram_K( \@data, \@histo, $K, $min_, $max_ );
}
elsif ( $com =~ /W/ and $com !~ /K/ )
{
	Histogram_W( \@data, \@histo, $W, $min_, $max_ );
}
elsif ( $com =~ /KW/ )
{
	Histogram_K( \@data, \@histo, $K, $min_, $max_ );
	my @tmp_arr = @histo;
	Histogram_W( \@tmp_arr, \@histo, $W, $min_, $max_ );
}
elsif ( $com =~ /WK/ )
{
	Histogram_W( \@data, \@histo, $W, $min_, $max_ );
	my @tmp_arr = @histo;
	Histogram_K( \@tmp_arr, \@histo, $K, $min_, $max_ );
}

my @outarr;
CreateOutputArray( \@histo, \@outarr, $min_, $max_ );
NormArray( \@outarr );


if( $cfg->{$section}->{'smooth3'} )
{
	%counts = ();
	FilterCounts( \%in_counts, \%counts, 0 );
	@data = ();
	CopyHashToArray( \%counts, \@data,  $cfg->{$section}->{'max'} );
}

NormArray( \@data );


PrintResult( \@outarr, \@data );

exit 0;

# ================= subs =========================
sub LoadGamma
{
	my( $par, $ref ) = @_;
	print "load gamma: $par\n" if $v;
	
	my $A = 0;
	my $B = 0;
	
	if ( $par =~ /(\d+),(\d+)/)
	{
		$A = $1;
		$B = $2;
	}
	
	my $i = $cfg->{$section}->{'min'};
	$i = $i - ($i%3);
	
	for( ; $i < 6000; $i += 3 )
	{
		$ref->{ $i } = ($i**$A)*exp( -$i / $B );
	}
}
# ------------------------------------------------
sub PrintResult
{
	my( $h_ref, $d_ref ) = @_;
	
	print "formating output\n" if $v;
	
	my $report;
	my $sep;
	
	if ( $cfg->{$section}->{'csv'} )
	{
		$sep = ",";
		$report = "#length,histogram,counts\n";
	}
	else
	{
		$sep = "\t";
		$report = '';
	}

	my $nozero = $cfg->{$section}->{'nozero'};
	my $nonorm = $cfg->{$section}->{'nonorm'};
	my $scale3 = $cfg->{$section}->{'scale3'};
	
	my $min = $cfg->{$section}->{'min'};
	my $max = $cfg->{$section}->{'max'};	
	
	my $line;
	
	for( my $i = $min; $i <= $max; ++$i )
	{
		if ( $nozero )
		{
			if( !$h_ref->[$i] && !$d_ref->[$i] ) { next; }
		}
		
		if ( $scale3 && ($i%3) )
		{
			if( !$h_ref->[$i] && !$d_ref->[$i] )
				{ next;}
			else
				{ print "warning, non-zero count found with scale3 option:  $h_ref->[$i]  $d_ref->[$i] $i\n"; }			
		}

		if ( $nonorm )
		{
			$line = sprintf( "%d%s%.1f", $i, $sep, $h_ref->[$i] );
		}
		else
		{
			$line = sprintf( "%d%s%.10f", $i, $sep, $h_ref->[$i] );
		}
		
		if ( $com =~ /R/ )
		{
			if ( $nonorm )
			{
				$line .= sprintf( "%s%d", $sep, $d_ref->[$i] );
			}
			else
			{
				$line .= sprintf( "%s%.10f", $sep, $d_ref->[$i] );
			}
		}
		
		$report .= ( $line ."\n" );
	}

	if( $cfg->{$section}->{'out'} )
	{
		my $name = $cfg->{$section}->{'out'};
		open(OUT,">$name") || die "error on open $name: $!\n";
		print OUT $report;
		close OUT;
		
		print "file with output: $name\n" if $v;
	}
	else
	{
		print $report;
	}
}
# ------------------------------------------------
sub NormArray
{
	my( $ref ) = @_;
	
	return if $cfg->{$section}->{'nonorm'};
	
	print "normalizing array\n" if $v;	
	my $sum = 0;
	my $size = scalar @{$ref};
	
	for( my $i = 0; $i < $size; ++$i )
	{
		$sum += $ref->[$i];
	}
	
	print "norm sum: $sum\n" if $v;

	for( my $i = 0; $i < $size; ++$i )
	{
		$ref->[$i] /= $sum;
	}
}
# ------------------------------------------------
sub CreateOutputArray
{
	my( $h_ref, $out_ref, $L, $R ) = @_;
	
	my $smooth3 = $cfg->{$section}->{'smooth3'};

	if ( !$smooth3 )
	{
		@{$out_ref} = @{$h_ref};
	}
	else
	{
		my $scale3  = $cfg->{$section}->{'scale3'};
			
		if( !$scale3 )
		{
			for( my $i = $L; $i <= $R; ++$i )
			{
				$out_ref->[ 3*$i + 0 ] = $h_ref->[$i]/3;
				$out_ref->[ 3*$i + 1 ] = $h_ref->[$i]/3;
				$out_ref->[ 3*$i + 2 ] = $h_ref->[$i]/3;
			}
			
			for( my $i = 0; $i < $cfg->{$section}->{'min'}; ++$i )
			{
				$out_ref->[$i] = 0;
			}
		}	
		else
		{
			for( my $i = $L; $i <= $R; ++$i )
			{
				$out_ref->[ 3*$i + 0 ] = $h_ref->[$i];
				$out_ref->[ 3*$i + 1 ] = 0;
				$out_ref->[ 3*$i + 2 ] = 0;
			}
			
			for( my $i = 0; $i < $cfg->{$section}->{'min'}; ++$i )
			{
				$out_ref->[$i] = 0;
			}
		}
	}
}
# ------------------------------------------------
sub Histogram_W
{
	my( $in_ref, $h_ref, $stop, $L, $R ) = @_;
		
	print "calculating naive histogram\n" if $v;
	
	# windows left and right boundaries
	my $left;
	my $right;
	# sum of frequencies in window
	my $weight;
		
	for( my $i = $L; $i <= $R; ++$i )
	{	
		# starting new window
		$weight = $in_ref->[$i];
		$left = $i;
		$right = $i;

		# increase size of window, until width W
		for ( --$left, ++$right; $right - $left + 1 <= $stop ; --$left, ++$right)
		{
			# if out of range, stop program
			if( $left < $L && $right > $R ) { die "error in setting of smoothing parameter W = $stop\n"; }

			# update weight
			if( $left >= $L )
			{
				$weight += $in_ref->[$left];
			}
			
			if( $right <= $R )
			{
				$weight += $in_ref->[$right];
			}
  		}
  		
		$h_ref->[$i] = $weight/$stop;
	}
}
# ------------------------------------------------
sub Histogram_K
{
	my( $in_ref, $h_ref, $stop, $L, $R ) = @_;
	
	print "calculateing NN histogram\n" if $v;

	# windows left and right boundaries
	my $left;
	my $right;
	# sum of frequencies in window
	my $weight;

	for( my $i = $L; $i <= $R; ++$i )
	{
		# starting new window
		$weight = $in_ref->[$i];
		$left  = $i;
		$right = $i;

		# increase size of window, until at list K points are included
		while( $weight < $stop )
		{
			--$left;
			++$right;
	
			# if maximum possible $weight < $K, stop program
			if ( $left < $L  && $right > $R ) { die "error in setting of smoothing parameter K = $stop\n"; }
	
			# update weight
			if( $left >= $L )
			{
				$weight += $in_ref->[$left];
			}
	
			if( $right <= $R )
			{
				$weight += $in_ref->[$right];
			}
		}
	
		$h_ref->[$i] = $weight/($right - $left + 1);
	}
}
# ------------------------------------------------
sub CopyHashToArray
{
	my( $h_ref, $a_ref, $R ) = @_;
	
	for( my $i = 0; $i <= $R ; ++$i )
	{ 
		$a_ref->[$i] = 0;
	}

	while( my ($key, $value) = each (%{$h_ref}) )
	{
		$a_ref->[ $key ] = $value;
	}
}
# ------------------------------------------------
sub GetK
{
	my $ref = shift;
	
	my $z = $cfg->{$section}->{'K'};
	
	if( $z == -1 )
	{
		$z = sqrt( $ref->{'n'} );
	}
	
	$z = int $z;
	
	print "K = $z\n" if $v;
	
	return $z;
}
# ------------------------------------------------
sub GetW
{
	my $ref = shift;
	
	my $z = $cfg->{$section}->{'W'};
	
	if( $z == -1 )
	{
		if( $ref->{'std'} < $ref->{'iqr'}/1.34 )
		{
			$z = 0.9*$ref->{'std'}/($ref->{'n'}**0.2);
		}
		else
		{
			$z = 0.9*$ref->{'iqr'}/(1.34*($ref->{'n'}**0.2));
		}
		
		$z *= 2;
	}
	
	$z = int $z;
	
	if( $z %2 == 0 )
	{
		$z += 1;
	}
	
	print "W = $z\n" if $v;
	
	return $z;
}
# ------------------------------------------------
sub GetStat
{
	my( $ref, $s ) = @_;

	my $tmp;

	# find standard deviation
	my $sum = 0;
	my $n = 0;
	my $mean = 0;
	my $std = 0;

	while( my ($key, $value) = each (%{$ref}) )
	{
  		$sum += ($key * $value);
  		$n += $value;
	}

	$mean = $sum/$n;
	 
	$tmp = 0;
	while( my ($key, $value) = each (%{$ref}) )
	{
		$tmp += (($key-$mean)*($key-$mean)*$value);
	}
	
	$std = sqrt( $tmp/($n-1) );

	$s->{'sum'}  = $sum;
	$s->{'n'}    = $n;
	$s->{'mean'} = $mean;
	$s->{'std'}  = $std;

	# find interquartile range; this is ~
	my $q1 = 0;
	my $q3 = 0;
	my $n_q1 = $n/4;
	my $n_q3 = $n*3/4;
	my $iqr = 0;

	$tmp = 0;
	foreach my $key ( sort {$a <=> $b} keys %{$ref} )
	{
		$tmp += $counts{$key};
		
		if(  $tmp >= $n_q1 && $q1 == 0 )
		{
			$q1 = $key;
		}
		
		if( $tmp >= $n_q3  && $q3 == 0 )
		{
			$q3 = $key;
		}
	}

	$iqr = $q3 - $q1;

	$s->{'q1'}  = $q1;
	$s->{'q3'}  = $q3;
	$s->{'iqr'} = $iqr;

	if($v)
	{
		print "sum = $sum\n";
		print "n = $n\n";
		print "mean = $mean\n";
		print "std = $std\n";
		print "q1 = $q1\n";
		print "q3 = $q3\n";
		print "iqr = $iqr\n"
	}
}
# ------------------------------------------------
sub FilterCounts
{
	my( $ref_in, $ref_out, $squeeze )= @_;
	
	my $min = $cfg->{$section}->{'min'};
	my $max = $cfg->{$section}->{'max'};

	my $counts_all = 0;
	my $counts_in = 0;
	my $sum_all = 0;
	my $sum_in = 0;

	while( my ($key, $value) = each (%{$ref_in}) )
	{
		$counts_all += 1;
		$sum_all += $value;
		
		if( $key >= $min and $key <= $max )
		{
			if( $squeeze )
			{
				$key = int( $key/3);
			}

			$ref_out->{$key} += $value;
			
			$counts_in += 1;
			$sum_in += $value;
		}
	}
	
	if( ! scalar keys %{$ref_out} ) { print "error, no data in specified range $0\n"; exit 1; }
	
	print "length observed in: $counts_all\n" if $v;
	print "length selected: $counts_in\n" if $v;
	print "out of range: ". ($sum_all - $sum_in) ."\n" if $v;
}
# ------------------------------------------------
sub SetMinMax
{
	my $ref = shift;

	my @in_index = sort {$a <=> $b} keys %{$ref};

	if( $cfg->{$section}->{'min'} == -1 )
	{
		$cfg->{$section}->{'min'} = $in_index[0];
	} 
	
	if( $cfg->{$section}->{'max'} == -1 )
	{
		$cfg->{$section}->{'max'} = $in_index[$#in_index];
	}
	
	if( $cfg->{$section}->{'min'} >  $cfg->{$section}->{'max'} ){ print "error on min/max, $0\n"; exit 1; }
	
	print "counts range:  $in_index[0] - $in_index[$#in_index]\n" if $v;
	print "histogram range:  $cfg->{$section}->{'min'} - $cfg->{$section}->{'max'}\n" if $v;
}
# ------------------------------------------------
sub LoadData
{
	my( $name, $ref ) = @_;
	print "load data: $name\n" if $v;
	
	my $count_all = 0;
	my $count_in  = 0;
	
	open( IN, $name ) or die "error on open file $0: $name\n$!\n";
	while( my $line = <IN> )
	{
		$count_all += 1;
		
		if ( $line =~ /^\s*$/ ) { next; }
		if ( $line =~ /^\s*\#/ ) { next; }
		
		if (( $line =~ /^\s*\S+\s+(\d+)\s+/ )or( $line =~/^\s*(\d+)\s+/ ))
		{
			$ref->{ $1 } += 1;
			++$count_in;
		}
		else { print "error in input file format $0: $line\n"; exit 1; }
	}
	close IN;
	
	print "lines in: $count_all\n" if $v;
	print "lines selected: $count_in\n" if $v;
}
# ------------------------------------------------
sub CheckInput
{
	if( ! $cfg->{$section}->{'com'} ) { print "error, command is not specified: $0\n"; exit 1; }

	if (!$cfg->{$section}->{'gamma'})
	{
		if( ! $cfg->{$section}->{'in'} )  { print "error, input  file is not specified: $0\n"; exit 1; }
		$cfg->{$section}->{'in'} = ResolvePath( $cfg->{$section}->{'in'} );
	}
	else
	{
		if(  $cfg->{$section}->{'gamma'} =~ /\d+,\d+/ )
		{
			;
		}
		else
			{print "error in values of gamma $0:  $cfg->{$section}->{'gamma'}\n"; exit 1; }
	}
	
	if( $cfg->{$section}->{'min'} >  $cfg->{$section}->{'max'} ){ print "error on min/max, $0\n"; exit 1; }
}
# ------------------------------------------------
sub ParseCMD
{
	my $cmd = $0;
	foreach my $str (@ARGV) { $cmd .= ( ' '. $str ); }
	
	my %h;
	my $opt_result = GetOptions
	(
		\%h,
		'in=s',
		'out=s',
		'com=s',
		'min=i',
		'max=i',
		'K=i',
		'W=i',
		'smooth3',
		'scale3',
		'nonorm',
		'csv',
		'nozero',
		'gamma=s',
		'cfg=s',
		'section=s' => \$section,
		'verbose'   => \$v,
		'debug'     => \$debug
	);
	
	if( !$opt_result ) { print "error on command line\n"; exit 1; }
	if( @ARGV > 0 ) { print "error, unexpected argument found on command line: @ARGV\n"; exit 1; }
	$v = 1 if $debug;

	SetDefaultConfiguration( $section );

	if( $h{'cfg'} )
	{
		$cfg->{$section}->{'cfg'} = ReadCfgFile( $h{'cfg'} );
	}	
	
	foreach my $key ( keys %h )
	{
		$cfg->{$section}->{$key} = $h{$key};
	}
	
	$cfg->{$section}->{'cmd'} = $cmd;
	
	if( $cfg->{$section}->{'scale3'} )
	{
		$cfg->{$section}->{'smooth3'} = 1;
	}
}
# ------------------------------------------------
sub ReadCfgFile
{
	my( $name, $path ) = @_;
	return '' if !$name;

	print "read config file: $name\n" if $debug;
	
	$name = ResolvePath( $name, $path );
	
	Hash::Merge::set_behavior( 'RIGHT_PRECEDENT' );
	%$cfg = %{ merge( $cfg, YAML::LoadFile( $name )) };
	
	return $name;
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
sub SetDefaultConfiguration
{
	my $label = shift;
	
	# file names
	$cfg->{$label}->{'in'}  = ''; # required
	$cfg->{$label}->{'out'} = '';
	# combined \d+,\d+
	$cfg->{$label}->{'gamma'} = '';
	# label
	$cfg->{$label}->{'com'} = ''; # required
	# optional numbers = -1
	$cfg->{$label}->{'min'} = -1;
	$cfg->{$label}->{'max'} = -1;
	$cfg->{$label}->{'K'}   = -1;
	$cfg->{$label}->{'W'}   = -1;
	# logical
	$cfg->{$label}->{'smooth3'} = 0;
	$cfg->{$label}->{'scale3'} = 0;
	$cfg->{$label}->{'nonorm'} = 0;
	$cfg->{$label}->{'csv'}    = 0;
	$cfg->{$label}->{'nozero'} = 0;
	# file name
	$cfg->{$label}->{'cfg'} = '';
}
# ------------------------------------------------
sub Usage
{
	print qq(# -------------------
Usage: $0   parameters

required:
  --in   [name] file with input data
  --com  ( k , kw, kr, kwr, w, wr ) type of data processing
optional:
  --out  [name] output file
  --min  [number] minimum length, ignore shorter in input
  --max  [number] maximum length, ignore longer in input
  --K    [number] count for nearest neighbor
  --W    [number] smoothing window size for naive
  --smooth3   join counts for mod(3)==0,1,2
  --scale3    join counts for mod(3)==0,1,2
  --nonorm    output counts instead of frequencies
  --csv       output in comma separated format
  --nozero    output only lines with non-zero values
  --gamma  [number],[number] parameters for gamma distibution

  --cfg     [name] read parameters from this file
  --section [label] use this section from configuration file
  --verbose
  --debug
# -------------------
);
	exit 1;
}
# ------------------------------------------------
