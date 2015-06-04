#!/usr/bin/perl
# ------------------------------------------------
# Alex Lomsadze
# Georgia Institute of Technology, -2014
#
# Modification of original code by John Besemer; Vardges TH
# Make from sequence: positional nucleotide frequency matrices (PWM)
# ------------------------------------------------
# to do:
# * check sequence for valid letters at read from file
# * print results to string and then to file or stdout
# * add logger
# ------------------------------------------------

use warnings;
use strict;
use Getopt::Long;
use Cwd qw(abs_path cwd);
use File::Spec;
use Data::Dumper;
use FindBin;
use lib "$FindBin::Bin/../lib";
use YAML;
use Hash::Merge qw( merge );

# ------------------------------------------------
my $v = 0;
my $debug = 0;
# system control
#   $cfg - reference on hash with parameters
#   $log - reference on logger
my $cfg;
my $section = 'site';
# ------------------------------------------------

Usage() if ( @ARGV < 1 );
ParseCMD();
CheckInput();
print Dumper( $cfg ) if $debug;

# ------------------------------------------------
# for zero order: 
# $count->{position}->{letter} = count_letter_at_positon
# for first order:
# $count->{end_position}->{di-letter} = count_di_letter_at_end_positon
# A T C G
# 0 1 2 3    {1}->{AT} = 1  {2}->{TC} = 1  {2}->{CG} = 1
# first position (zero index) has no counts
# ------------------------------------------------

my %count;

my $filter = '.';

if ( defined $cfg->{$section}->{'phase'} )
{
	if( $cfg->{$section}->{'phase'} eq '0' or $cfg->{$section}->{'phase'} eq '1' or $cfg->{$section}->{'phase'} eq '2' )
	{
		$filter = $cfg->{$section}->{'phase'};
	}
	else
	{
		$filter = '.';
	}
} 

CountFromFile( $cfg->{$section}->{'infile'}, $filter );

my @nts = qw( T C A G );

SetToZeroUndefCounts();

if ( $cfg->{$section}->{'pseudocounts'} )
{
	if ( $cfg->{$section}->{'site_size'} == -1 )
	{
		AddPseudocount();
	}
	else
	{
		AddPseudocountExcludingSite();
	}
}

CountOrderZero();
CountOrderOne();

if ( $cfg->{$section}->{'auto_order'} )
{
	$cfg->{$section}->{'order'} = SetOrder();
}

print Dumper($cfg) if $debug;

EstimateFrequencyOrderZero();

my $header = '';
if( $cfg->{$section}->{'format_out'} )
{
	my $label = $cfg->{$section}->{'format_out'};
	
	$header  = '$'. $label ."_WIDTH ".    $cfg->{$section}->{'width'} ."\n";
	$header .= '$'. $label ."_WIDTH_P ".  $cfg->{$section}->{'width'} ."\n";
	$header .= '$'. $label ."_MARGIN ".   $cfg->{$section}->{'margin'} ."\n";
	$header .= '$'. $label ."_MARGIN_P ". $cfg->{$section}->{'margin'} ."\n";
	$header .= '$'. $label ."_ORDER ".    $cfg->{$section}->{'order'} ."\n";
	$header .= '$'. $label ."_MAT\n";
}

if ( $cfg->{$section}->{'order'} == 0 )
{
	OutputMatZero( $cfg->{$section}->{'outfile'}, $header );
}

if ( $cfg->{$section}->{'order'} == 1 )
{
	EstimateFrequencyOrderOne();
	OutputMatOne( $cfg->{$section}->{'outfile'}, $header );
}

print Dumper(\%count) if $debug;

exit 0;

# ================ subs ========================
sub SetToZeroUndefCounts
{
	my $length = $cfg->{$section}->{'width'} - 1;
	
	# zero order counts
	foreach my $i (0 .. 3)
	{
		my $nt = $nts[$i];
		
		foreach my $j (0 .. $length)
		{
			if ( !defined $count{$nt}{$j} )
			{
				$count{$nt}{$j} = 0;
			}
		}
	}
	
	# first order counts
	foreach my $i (0 .. 3)
	{
		foreach my $k (0 .. 3)
		{
			my $di = $nts[$i] . $nts[$k];
			
			foreach my $j (1 .. $length)
			{
				if ( !defined $count{$di}{$j} )
				{
					$count{$di}{$j} = 0;
				}
			}
		}
	}
}
# -------------------------------------
sub SetOrder
{
	my $length = $cfg->{$section}->{'width'} - 1;
	my $margin = $cfg->{$section}->{'margin'};
	my $site_size = $cfg->{$section}->{'site_size'};
	
	my $average_per_position = 0;

	foreach my $i (0 .. 3)
	{
		foreach my $j (0 .. ($margin - 1))
		{
			$average_per_position += $count{'atcg'}{$j};
		}
		
		foreach my $j (($margin + $site_size) .. $length)
		{
			$average_per_position += $count{'atcg'}{$j};
		}	
	}
	
	$average_per_position /= $cfg->{$section}->{'width'};

	print "atcg_per_position: $average_per_position\n" if $v;
	
	if ( $average_per_position  < $cfg->{$section}->{'threshold_zero'} )
	{
		$cfg->{$section}->{'order'} = 0;
	}
	else
	{
		$cfg->{$section}->{'order'} = 1;
	}
	
	return $cfg->{$section}->{'order'};
}
# -------------------------------------
sub CountOrderZero
{
	my $length = $cfg->{$section}->{'width'} - 1;
	
	# sum of all atcg in one column
	
	# $count{'A'}{0} $count{'A'}{1} $count{'A'}{2} ...
	# $count{'C'}{0} $count{'C'}{1} $count{'C'}{2} ...
	# $count{'G'}{0} $count{'G'}{1} $count{'G'}{2} ...
	# $count{'T'}{0} $count{'T'}{1} $count{'T'}{2} ...
	# $count{'actg'}{0} $count{'acgt'}{1} $count{'acgt'}{2} ...
	
	foreach my $i (0 .. 3)
	{
		my $nt = $nts[$i];
		
		foreach my $j (0 .. $length)
		{
			$count{'atcg'}{$j} += $count{$nt}{$j};
		}
	}
}
# -------------------------------------
sub CountOrderOne
{	
	my $length = $cfg->{$section}->{'width'} - 1;
	
	# sum of all di-nuc in one column
	foreach my $i (0 .. 3)
	{
		foreach my $k (0 .. 3)
		{
			my $di = $nts[$i] . $nts[$k];
			
			foreach my $j (1 .. $length)
			{
				$count{ $nts[$i] . "*" }{$j} += $count{$di}{$j};
			}
		}
	}
}
# -------------------------------------
sub EstimateFrequencyOrderZero
{
	my $length = $cfg->{$section}->{'width'} - 1;
	
	foreach my $i (0 .. 3)
	{
		my $nt = $nts[$i];
		
		foreach my $j (0 .. $length)
		{
			my $pos_freq = 0.0;
			
			if( $count{$nt}{$j} )
			{
				$pos_freq = $count{$nt}{$j} / $count{'atcg'}{$j};
			}
			
			$count{ $nt.".p" }{$j} = $pos_freq;
		}
	}
}
# -------------------------------------
sub EstimateFrequencyOrderOne
{
	my $length = $cfg->{$section}->{'width'} - 1;

	foreach my $i (0 .. 3)
	{	
		foreach my $k (0 .. 3)
		{
			my $di = $nts[$i] . $nts[$k];
			
			foreach my $j (1 .. $length)
			{	
				my $pos_freq = 0.0;
				
				if( $count{$di}{$j} )
				{

					$pos_freq = $count{$di}{$j} / $count{ $nts[$i] . "*" }{$j};	
				}
				
				$count{ $di.".p" }{$j} = $pos_freq;
			}
		}	
	}
}
# -------------------------------------
sub OutputMatOne
{
	my( $name, $txt ) = @_;
	
	my $length = $cfg->{$section}->{'width'};
	
	open( OUT, ">$name" ) || die "error on open $name: $!\n"; 
	
	if( $txt )
	{
		print OUT $txt;
	}
	
	my $nt;
	my $di;
	my $pos_freq;
	
	foreach my $i (0 .. $#nts)
	{
		foreach my $k (0 .. $#nts)
		{
			$di = $nts[$i] . $nts[$k];
			
			print OUT "$di ";
			print "$di " if $v;

			# zero order values are in first column 
			{
				$nt = $nts[$k];
				
				$pos_freq = $count{ $nt.".p" }{0};
		
				my $z = ( int( $pos_freq * 10000 + 0.5 ) )/ 10000.0;
				printf OUT "%3.4f",$z;
				
				$z = ( int( $pos_freq * 100 + 0.5 ) )/ 100.0;
				printf("%3.2f", $z ) if $v;
			}
		
			# order one
			
			foreach my $j (1 .. ($length - 1))
			{
				if ($j != 0)
				{
					print OUT " ";
					print " " if $v;
				}
				
				$pos_freq = $count{ $di.".p" }{$j};
		
				my $z = ( int( $pos_freq * 10000 + 0.5 ) )/ 10000.0;
				printf OUT "%3.4f",$z;
				
				$z = ( int( $pos_freq * 100 + 0.5 ) )/ 100.0;
				printf("%3.2f", $z ) if $v;
			}
		
			print OUT "\n";
			print "\n" if $v;
		}
	}
	
	close OUT;
}
# -------------------------------------
sub OutputMatZero
{
	my( $name, $txt ) = @_;
	
	my $length = $cfg->{$section}->{'width'};
	
	open( OUT, ">$name" ) || die "error on open $name: $!\n"; 
	
	if( $txt )
	{
		print OUT $txt;
	}
	
	my $nt;
	my $pos_freq;
	
	foreach my $i (0 .. $#nts)
	{
		$nt = $nts[$i];
		
		print OUT "$nt ";
		print "$nt " if $v;
	
		foreach my $j (0 .. ($length - 1))
		{		
			if ($j != 0)
			{
				print OUT " ";
				print " " if $v;
			}
			
			$pos_freq = $count{ $nt.".p" }{$j};
	
			my $z = ( int( $pos_freq * 10000 + 0.5 ) )/ 10000.0;
			printf OUT "%3.4f",$z;
			
			$z = ( int( $pos_freq * 100 + 0.5 ) )/ 100.0;
			printf("%3.2f", $z ) if $v;
		}
	
		print OUT "\n";
		print "\n" if $v;
	}
	
	close OUT;
}
# -------------------------------------
sub AddPseudocount
{
	my $length = $cfg->{$section}->{'width'} - 1;
	my $pseudo = $cfg->{$section}->{'pseudocounts'};

	foreach my $i (0 .. 3)
	{
		my $nt = $nts[$i];

		foreach my $j (0 .. $length)
		{
			$count{$nt}{$j} += $pseudo;
		}
	}

	foreach my $i (0 .. 3)
	{
		foreach my $k (0 .. 3)
		{
			my $di = $nts[$i] . $nts[$k];
			
			foreach my $j (1 .. $length)
			{
				$count{$di}{$j} += $pseudo;
			}
		}
	}
}
# -------------------------------------
sub AddPseudocountExcludingSite
{
	my $margin    = $cfg->{$section}->{'margin'};
	my $site_size = $cfg->{$section}->{'site_size'};
	my $length    = $cfg->{$section}->{'width'} - 1;
	my $pseudo    = $cfg->{$section}->{'pseudocounts'};
	my $type      = $cfg->{$section}->{'type'};

	# zero order
	foreach my $i (0 .. 3)
	{
		my $nt = $nts[$i];

		foreach my $j (0 .. ($margin - 1))
		{
			$count{$nt}{$j} += $pseudo;
		}
		
		foreach my $j (($margin + $site_size) .. $length)
		{
			$count{$nt}{$j} += $pseudo;
		}	
	}
	
	foreach my $i (0 .. $#nts)
	{
		foreach my $k (0 .. $#nts)
		{
			my $di = $nts[$i] . $nts[$k];
			
			# before site
			foreach my $j (1 .. ($margin - 1))
			{
				$count{$di}{$j} += $pseudo;
			}
			
			# to site
			if ( $nts[$k] eq substr( $type, 0, 1) )
			{	
				$count{$di}{$margin} += $pseudo;
			}
			
			# from site
			if ( $nts[$i] eq substr( $type, -1, 1) )
			{	
				$count{$di}{$margin + $site_size} += $pseudo;
			}
			
			# after site
			foreach my $j (($margin + $site_size + 1) .. $length)
			{
				$count{$di}{$j} += $pseudo;
			}			
		}
	}
}
# -------------------------------------
sub CountFromFile
{
	my( $name, $threshold ) = @_;

	# for debug
	my $lines_read_in = 0;
	my $sequences_used = 0;
	
	# GC range
	my $select_by_gc = 0;
	my $gc_low  = $cfg->{$section}->{'gc_low'};
	my $gc_high = $cfg->{$section}->{'gc_high'};
	if ( $gc_low != 0 or $gc_high != 100 )
	{
		$select_by_gc = 1;
	}

	# auto width
	my $length_from_file = 0;
	my $seq_length = 0;
	if ( $cfg->{$section}->{'width'} == -1 )
	{
		$length_from_file = 1;
	}
	else
	{
		$seq_length = $cfg->{$section}->{'width'};
	}

	# detailed control
	my $site_type   = '';
	my $site_margin = 0;
	my $site_size   = 0;
	my $quite = 0;
	if ( $cfg->{$section}->{'type'} )
	{
		$site_type   = uc( $cfg->{$section}->{'type'} );
		$site_margin = $cfg->{$section}->{'margin'};
		$site_size   = $cfg->{$section}->{'site_size'};
		$quite = $cfg->{$section}->{'quite'};
	}
	
	# count data
	my $seq;
	
	open( IN, $name ) or die "error on open file $0: $name\n$!\n"; 
	while( my $line = <IN>)
	{
		++$lines_read_in;
		
		# skip comments
		if ( $line =~ /^\#/ )        {next;}
		if ( $line =~ /^>/ )         {next;}
		if ( $line =~ /^\s*GC%\s+/ ) {next;}
	
		# if GC is specified:  'gc  seq'
		
		if( $line =~ /^\s*(\d*\.?\d*)\s+(\w+)\s*$/ )
		{
			if ( $select_by_gc )
			{
				my $gc = $1;
				if ( $gc < $gc_low or $gc > $gc_high ) { next; }
			}
			
			$seq = $2;
		}
		elsif( $line =~ /^\s*(\d*\.?\d*)\s+(\w+)\s+(\S)\s*$/ )
		{
			if ( $select_by_gc )
			{
				my $gc = $1;
				if ( $gc < $gc_low or $gc > $gc_high ) { next; }
			}
			
			if( $threshold eq '.' )
			{
				$seq = $2;
			}
			elsif( $threshold eq $3 )
			{
				$seq = $2;
			}
			else { next; }
		}
		elsif ( $line =~ /^\s*(\w+)\s*$/ )
		{
			$seq = $1;
		}
		elsif ( $line =~ /^\s*(\w+)\s+(\S)\s*$/ )
		{
			if( $threshold eq '.' )
			{
				$seq = $1;
			}
			elsif( $threshold eq $2 )
			{
				$seq = $1;
			}
			else { next; }
		}
		else { print "error, unexpected format found on line $lines_read_in: $line\n"; exit 1; }

		$seq = uc($seq);
		
		# auto length
		if ( $length_from_file )
		{
			if ( !$seq_length )
			{
				$seq_length = length($seq);
			}
		}
		
		if ( $seq_length != length($seq) ) { print "error: all sequences not of same lengthon line $lines_read_in: $line\n"; exit 1; }	
		
		# check site type
		if ( $site_type )
		{
			if ( $site_type ne substr( $seq, $site_margin, $site_size) )
			{
				print "warning, unexpected site was found and sequence ignored: $lines_read_in, $line" if !$quite;
			}
		}
		
		++$sequences_used;
		
		# count positional
		my $current_nt;
		my $current_di;
		
		for( my $offset = 0; $offset < $seq_length; ++$offset )
		{
			$current_nt = substr($seq,$offset,1);
			$count{$current_nt}{$offset}++;
			
			if ( $offset > 0 )
			{
				$current_di = substr($seq, $offset - 1, 2);
				$count{$current_di}{$offset}++;
			}
		}
	}
	close IN;
	
	if ( $length_from_file )
	{
		$cfg->{$section}->{'width'}  = $seq_length;
	}
	
	if ( $sequences_used == 0 ) { print "error, no valid sequences were found\n"; exit 1; }
	
	if ( $v )
	{
		print "# input file: $name\n";
		if ( $select_by_gc )
		{
			print "# GC range: $gc_low  $gc_high\n";
		}
		else
		{
			print "# GC range: 0  100\n";
		}
		print "# lines in input: $lines_read_in\n";
		print "# sequences used: $sequences_used\n";
	}
}
# ------------------------------------------------
sub UpdateRunningConfiguration
{
	# use with caution
	my $name = $cfg->{'Config'}->{'run_cfg'};
	if ( !$name ) {return;}
	open( OUT, ">$name") or die "error on open file:$name\n$!\n";
	print OUT Dump($cfg);
	close OUT;
}
# ------------------------------------------------
sub CheckInput
{
	# required input
	if( !$cfg->{$section}->{'infile'} )  { print "error, input file name is not specified $0\n"; exit 1; }
	if( !$cfg->{$section}->{'outfile'} ) { print "error, outfile file is not specified\n"; exit 1; }
	
	$cfg->{$section}->{'infile'} = ResolvePath( $cfg->{$section}->{'infile'} );
	
	# GC
	my $gc_low  = $cfg->{$section}->{'gc_low'};
	my $gc_high = $cfg->{$section}->{'gc_high'};
	
	if ( $gc_low  == -1 ) { $gc_low = 0; }
	if ( $gc_high == -1 ) { $gc_high = 100; }
	if (( $gc_low > $gc_high )or( $gc_high > 100 )or( $gc_low < 0 )) { print "error in GC range of parameters\n", exit 1; }
	$cfg->{$section}->{'gc_low'}  = $gc_low;
	$cfg->{$section}->{'gc_high'} = $gc_high;

	# claculations
	if ( $cfg->{$section}->{'pseudocounts'} < 0 ) { print "error, negative pseudocounts found\n"; exit 1; }
	
	if (($cfg->{$section}->{'order'} != 0) and ($cfg->{$section}->{'order'} != 1))
		{ print "error, unsuported order was specified: $cfg->{$section}->{'order'}\n"; exit 1; }
		
	# site type
	if ( $cfg->{$section}->{'type'} )
	{
		if ( $cfg->{$section}->{'site_size'} == -1 )
		{
			$cfg->{$section}->{'site_size'} = length( $cfg->{$section}->{'type'} );
		}
		elsif ( $cfg->{$section}->{'site_size'} !=  length( $cfg->{$section}->{'type'} ) )
			{ print "error in site size and actual dite type\n"; exit 1; }
		
		if ( $cfg->{$section}->{'margin'} == -1 ) { print "error, margin must be specified is type is specified\n"; exit 1; }
	}
	
	if ( $cfg->{$section}->{'phase'} and ( $cfg->{$section}->{'phase'} !~ /^[.012]$/ ) )
		{ print "error, unsuported phase was specified:  $cfg->{$section}->{'phase'}\n"; exit 1; }
	
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
		'infile=s',
		'outfile=s',
		'format_out=s',
		'order=i',
		'auto_order',
		'threshold_zero=i',
		'pseudocounts=i',				
		'type=s',
		'width=i',
		'margin=i',
		'site_size=i',
		'phase=i',
		'gc_low=i',
		'gc_high=i',
		'quite',
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
	$cfg->{$label}->{'infile'}  = ''; # required
	$cfg->{$label}->{'outfile'} = ''; # required
	# numbers
	$cfg->{$label}->{'pseudocounts'}   = 0;
	$cfg->{$label}->{'order'}          = 0;
	$cfg->{$label}->{'threshold_zero'} = 2000;
	# numbers optional = -1
	$cfg->{$label}->{'width'}     = -1;
	$cfg->{$label}->{'margin'}    = -1;
	$cfg->{$label}->{'site_size'} = -1;
	$cfg->{$label}->{'gc_low'}    = -1;
	$cfg->{$label}->{'gc_high'}   = -1;
	# logical
	$cfg->{$label}->{'auto_order'} = 0;
	$cfg->{$label}->{'quite'}      = 0;
	# label
	$cfg->{$label}->{'type'}       = '';
	$cfg->{$label}->{'format_out'} = '';
	$cfg->{$label}->{'phase'}      = '.';
	# file name
	$cfg->{$label}->{'cfg'} = '';
}
# ------------------------------------------------
sub Usage
{
	print qq(# -------------------
Usage: $0   parameters

required:
  --in   [name] file with input sequence
  --out  [name] output file with PWM probability estimations

optional:
  --pseudocounts   [number] additional counts to all values
  --auto_order     set model order based on the amount of input sequence
  --threshold_zero [number] threshold for zero order in auto detect mode for model order
  --order          [number] model order; default order '0'; supported order: '0', '1'
  --type           [string] type of the site, like GT, AG, etc
  --width          [number] width of site model
  --site_size      [number] width of site itself
  --margin         [number] number of nucleotides before site
  --phase          [.,0,1,2] phase of site
  --gc_low         [number] low GC limit
  --gc_high        [number] high GC limit
  --format_out     [label]
  --quite          ignore non-canonical sites without warnings

  --cfg     [name] read parameters from this file
  --section [label] use this section from configuration file
  --verbose
  --debug
# -------------------
);
	exit 1;
}
# ------------------------------------------------
