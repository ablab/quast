#!/usr/bin/perl
# ------------------------------------------------
# Alex Lomsadze
# Georgia Institute of Technology, -2014
#
# Parse donor, acceptor sequences and intron length
# from sequence and intron coordinates
# ------------------------------------------------
# add logger
# check sequence on read
# ------------------------------------------------

use strict;
use warnings;
use Data::Dumper;

use Getopt::Long;
use Cwd qw(abs_path cwd);
use File::Spec;

use YAML;
use Hash::Merge qw( merge );

# ------------------------------------------------
my $v = 0;
my $debug = 0;
# system control
#   $cfg - reference on hash with parameters
#   $log - reference on logger
my $cfg;
my $log;
# ------------------------------------------------
my $section = 'ET_ini_parse';
# ------------------------------------------------

Usage() if ( @ARGV < 1 );
ParseCMD();
CheckInput();

print Dumper( $cfg ) if $debug;

# ------------------------------------------------

# temp solution, time constrain
my $BP = 0;
$BP = 1 if $cfg->{'Parameters'}->{'fungus'};

# temp 
my $count_gc_donor = 0;
my $count = 0;
my $count_valid = 0;

# hash %h - keys - seq_id
# hash %h - value - array of hashes - each hash - intron

my %h;
LoadFromGFF( $cfg->{$section}->{'introns_in'}, \%h, 'INTRON', $cfg->{$section}->{'et_score'} );

my $sequence;
my $id;

my $out = File::Spec->catfile( $cfg->{$section}->{'parse_dir'}, $cfg->{$section}->{'parse_out'} );

open( IN, $cfg->{$section}->{'seq_in'} ) or die "error on open file: $cfg->{$section}->{'seq_in'}\n $!\n";
open( OUT, '>', $out ) or die "error on open file $0: $out\n $!\n";

# <IN> inside LoadSequence
while( LoadSequence( \$sequence, \$id ) )
{
	print "$id\n" if $v;
	
	if ( exists $h{$id} )
	{
		# print OUT inside Parse
		Parse();
	}
}
close OUT;
close IN;

print "$count  $count_valid\n" if $v;

SplitSet($out);

print "GC donors: $count_gc_donor\n" if $v;

exit 0;

# ================= subs =========================
sub SplitSet
{
	my $name = shift;
	
	open( my $PARSE, $name )or die"error on open file $0: $name\n";
	
	my $DON;
	my $ACC;
	my $INTRON_LEN;
	my $BP_REGION;
	my $ACC_SHORT;
	
	if( $cfg->{$section}->{'don'} )
	{
		$name = File::Spec->catfile( $cfg->{$section}->{'parse_dir'}, $cfg->{$section}->{'don'} );
		open( $DON, ">", $name ) or die"error on open file $0: $name\n";
	}

	if( $cfg->{$section}->{'acc'} )
	{
		$name = File::Spec->catfile( $cfg->{$section}->{'parse_dir'}, $cfg->{$section}->{'acc'} );
		open( $ACC, ">", $name ) or die"error on open file $0: $name\n";
	}

	if( $cfg->{$section}->{'intron_len'} )
	{
		$name = File::Spec->catfile( $cfg->{$section}->{'parse_dir'}, $cfg->{$section}->{'intron_len'} );
		open( $INTRON_LEN, ">", $name ) or die"error on open file $0: $name\n";
	}

	if( $BP and $cfg->{$section}->{'bp_region'} )
	{
		$name = File::Spec->catfile( $cfg->{$section}->{'parse_dir'}, $cfg->{$section}->{'bp_region'} );
		open( $BP_REGION, ">", $name ) or die"error on open file $0: $name\n";
	}

	if( $BP and $cfg->{$section}->{'acc_short'} )
	{
		$name = File::Spec->catfile( $cfg->{$section}->{'parse_dir'}, $cfg->{$section}->{'acc_short'} );
		open( $ACC_SHORT, ">", $name ) or die"error on open file $0: $name\n";
	}

	my $label;
	my $value;

	while( my $line = <$PARSE> )
	{
		if( $line =~ /^(\S+)\s+(\S.*)\s*/ )
		{
			$label = $1;
			$value = $2;
			
			if( $DON and  $label eq 'DON' )
			{
				print $DON "$value\n";
			}

			if( $ACC and  $label eq 'ACC' )
			{
				print $ACC "$value\n";
			}

			if( $INTRON_LEN and  $label eq 'intron_len' )
			{
				print $INTRON_LEN "$value\n";
			}
			
			if( $BP and $BP_REGION and ($label eq 'bp_region') )
			{
				print $BP_REGION "$value\n";
			}

			if( $BP and $ACC_SHORT and ($label eq 'ACC') )
			{
				print $ACC_SHORT ( substr($value, $cfg->{'acceptor_AG'}->{'width'} - $cfg->{'acceptor_short_AG'}->{'width'} ) ."\n" );
			}
		}
		else
			{ print "error, unexpected format found $0: $line\n"; exit 1; }
	}
	
	close $DON if $DON;
	close $ACC if $ACC;
	close $INTRON_LEN if $INTRON_LEN;
	close $BP_REGION if $BP_REGION;
	close $ACC_SHORT if $ACC_SHORT;
	close $PARSE;
}
# ------------------------------------------------
sub Parse
{
	my $out_str;

	my $don_site_pos;
	my $don_site_seq;
	
	my $acc_site_pos;
	my $acc_site_seq;
	
	my $bp_region_pos;
	my $bp_region_seq;
	
	my $bp_region_length = $cfg->{$section}->{'bp_region_length'};

	my $max = length($sequence);
	my $valid;	
	
	foreach my $rec ( @{$h{$id}} )
	{
		++$count;
		$valid = 1;
		
		if( $rec->{'right'} > $max ) { $valid = 0; print "warning, out of region intron found\n" if $v; next; }
		
		if ( $rec->{'strand'} eq '+' )
		{
			$don_site_pos = $rec->{'left'}  - $cfg->{'donor_GT'}->{'margin'}    - 1;
			$acc_site_pos = $rec->{'right'} - $cfg->{'acceptor_AG'}->{'margin'} - 2;
			
			if ( $don_site_pos < 0  or $acc_site_pos < 0 ) { $valid = 0; print "warning, out of region site found\n" if $v; next; }
			if ( $don_site_pos + $cfg->{'donor_GT'}->{'width'} > $max or $acc_site_pos + $cfg->{'acceptor_AG'}->{'width'} > $max ) { $valid = 0; print "warning, out of region site found\n" if $v; next; }
			
			$don_site_seq = substr( $sequence, $don_site_pos, $cfg->{'donor_GT'}->{'width'} );
			$acc_site_seq = substr( $sequence, $acc_site_pos, $cfg->{'acceptor_AG'}->{'width'} );
		}
		elsif ( $rec->{'strand'} eq '-' )
		{
			$don_site_pos = $rec->{'right'} - $cfg->{'donor_GT'}->{'width'}    + $cfg->{'donor_GT'}->{'margin'} ;
			$acc_site_pos = $rec->{'left'}  - $cfg->{'acceptor_AG'}->{'width'} + $cfg->{'acceptor_AG'}->{'margin'} + 1;
			
			if ( $don_site_pos < 0  or $acc_site_pos < 0 ) { $valid = 0; print "warning, out of region site found\n" if $v; next; }
			if ( $don_site_pos + $cfg->{'donor_GT'}->{'width'} > $max or $acc_site_pos + $cfg->{'acceptor_AG'}->{'width'} > $max ) { $valid = 0; print "warning, out of region site found\n" if $v; next; }
			
			$don_site_seq = substr( $sequence, $don_site_pos, $cfg->{'donor_GT'}->{'width'} );
			$acc_site_seq = substr( $sequence, $acc_site_pos, $cfg->{'acceptor_AG'}->{'width'} );
			
			$don_site_seq = RevComp( $don_site_seq );
			$acc_site_seq = RevComp( $acc_site_seq );
		}
		else
		{
			print "warning, ignored intron without strand information\n" if $v;
			$valid = 0; print Dumper($rec) if $debug;
			next;
		}

		if ( substr( $don_site_seq, $cfg->{'donor_GT'}->{'margin'}, 2 ) ne 'GT'  )
			{ print "warning non GT donor found: $don_site_seq\n" if $debug; $valid = 0; print Dumper($rec) if $debug; }

		if ( substr( $don_site_seq, $cfg->{'donor_GT'}->{'margin'}, 2 ) eq 'GC'  )
			{ ++$count_gc_donor; }

		if ( substr( $acc_site_seq, $cfg->{'acceptor_AG'}->{'margin'}, 2 ) ne 'AG'  )
			{ print "warning non AG acceptor found: $acc_site_seq\n" if $debug; $valid = 0; print Dumper($rec) if $debug; }

		if (!$valid) { next; }

		++$count_valid;

		$out_str = "DON\t". $don_site_seq ."\n";
		$out_str .= "intron_len\t". ( $rec->{'right'} - $rec->{'left'} + 1) ."\n";
		$out_str .= "ACC\t". $acc_site_seq ."\n";
		
		if( $BP )
		{
			if ( $rec->{'strand'} eq '+' )
			{
				$bp_region_pos = $rec->{'right'} - $bp_region_length - 2;
				if ( $bp_region_pos < 0  ) { $valid = 0; print "warning, out of region found\n" if $v; next; }
				$bp_region_seq = substr( $sequence, $bp_region_pos , $bp_region_length );
			}
			elsif ( $rec->{'strand'} eq '-' )
			{
				$bp_region_pos = $rec->{'left'} + 1;
				if( $bp_region_pos + $bp_region_length > $max ) { $valid = 0; print "warning, out of region found\n" if $v; next; }
				$bp_region_seq = substr( $sequence, $bp_region_pos , $bp_region_length );
				$bp_region_seq = RevComp( $bp_region_seq );
			}
			else
			{
				print "warning, strand is not defined, intron was ignored\n" if $v;
				print Dumper($rec) if $debug;
			}
			
			if ( ($rec->{'right'} - $rec->{'left'} + 1) - 2 < length($bp_region_seq) )
			{
				$bp_region_seq = substr( $bp_region_seq, -($rec->{'right'} - $rec->{'left'} - 1) );
			}
			
			$out_str .= "bp_region\t". ( $rec->{'right'} - $rec->{'left'} + 1) ."\t". $bp_region_seq ."\n";
		}
		
		print OUT $out_str;
	}
}
# ------------------------------------------------
sub RevComp
{
	my $str = shift;

	$str = reverse($str);

    $str =~ tr/aA/12/;
    $str =~ tr/tT/aA/;
    $str =~ tr/12/tT/;
    
    $str =~ tr/cC/12/;
    $str =~ tr/gG/cC/;
    $str =~ tr/12/gG/;
    
    return $str;
}
# ------------------------------------------------
sub LoadSequence
{
	my( $s, $id ) = @_;

	$$s = "";
	$$id = "";

	while( my $line = <IN> )
	{	
		if( $line =~ /^>/ )
		{
			if( !$$s )
			{
				# start of the record, parse ID
				if( $line =~ /^>\s*(\S+)\s*/ )
				{
					$$id = $1;
				}
			}
			else
			{
				# end of the record, go step back
				seek( IN, -length($line), 1);
				return 1;
			}
		}
		else
		{
			$line =~ s/\s//;
			$$s .= $line;
		}
	}
	
	if( $$s )
	{
		return 1;
	}
	
	return 0;
}
# ------------------------------------------------
sub LoadFromGFF
{
	my( $name, $ref, $label, $score ) = @_;
	
	my $id;  # $ref hash key
	my %tmp; # $ref hash value - array of %tmp
	
	# count for verbose
	my $count_all = 0;  # all informative GFF lines
	my $count_in  = 0;  # unique GFF lines selected with type - $label
	my $count_dub = 0;  # number of dublications in input - the same intron from alternative spliced genes
	
	# to resolve dublications
	my $uniq_id; # form unique id for each line
	my %check;   # put and check it using hash
	
	open( my $GFF, $name ) or die "error on open file $0: $name\n$!\n";
	while( my $line = <$GFF> )
	{
		if( $line =~ /^\#/ ) {next;}
		if( $line =~ /^\s*$/ ) {next;}
		
		if ( $line =~ /^(\S+)\t(\S+)\t(\S+)\t(\d+)\t(\d+)\t(\S+)\t(\S+)\t(\S+)\t(.+)/ )
		{
			++$count_all;
			
			$id = $1;
		
			$tmp{'type'}   = uc $3;
			$tmp{'left'}   = $4;
			$tmp{'right'}  = $5;
			$tmp{'score'}  = $6;
			$tmp{'strand'} = $7;
		
			if( $tmp{'type'} ne $label ) {next;}
			
			if( $tmp{'score'} ne '.' and  $tmp{'score'} < $score ) { next; }
		
			$uniq_id = $id .'_'. $tmp{'left'} .'_'. $tmp{'right'};
			
			$tmp{'uniq_id'} = $uniq_id;
			
			if( ! exists $check{$uniq_id} )
			{
				$check{$uniq_id} = 1;
				push @{$ref->{$id}}, { %tmp };
				++$count_in;
			}
			else
			{
				++$count_dub;
			}
		}
		else { print "error, unexpected format found on line $0: $line\n"; exit 1; }
	}
	close $GFF;
	
	print "From $count_all loaded $count_in and ignored dublications $count_dub\n" if $v;
}
# ------------------------------------------------
sub CheckInput
{
	if( ! $cfg->{$section}->{'seq_in'} )     { print "error, sequence input file is not specified $0\n"; exit 1; }
	if( ! $cfg->{$section}->{'introns_in'} ) { print "error, intron input file is not specified $0\n"; exit 1; }
	if( ! $cfg->{$section}->{'parse_out'} )  { print "error, output file is not specified $0\n"; exit 1; }
	if( ! $cfg->{$section}->{'parse_dir'} )  { print "error, output directory is not specified $0\n"; exit 1; }

	$cfg->{$section}->{'seq_in'}     = ResolvePath( $cfg->{$section}->{'seq_in'} );
	$cfg->{$section}->{'introns_in'} = ResolvePath( $cfg->{$section}->{'introns_in'} );
	$cfg->{$section}->{'parse_dir'}  = ResolvePath( $cfg->{$section}->{'parse_dir'} );
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
		'seq_in=s',
		'introns_in=s',
		'parse_out=s',
		'parse_dir=s',
		'score=i',
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
	$cfg->{$label}->{'cfg'} = '';
	
	$cfg->{$label}->{'seq_in'}     = ''; # required
	$cfg->{$label}->{'introns_in'} = ''; # required
	$cfg->{$label}->{'parse_out'}  = ''; # required
	$cfg->{$label}->{'parse_dir'}  = '.';
	
	# number
	$cfg->{$label}->{'et_score'} = 0;
	
	# file names	
	$cfg->{$label}->{'don'}        = '';
	$cfg->{$label}->{'acc'}        = '';
	$cfg->{$label}->{'intron_len'} = '';
	$cfg->{$label}->{'bp_region'}  = '';
	$cfg->{$label}->{'short_acc'}  = '';
	
	$cfg->{$label}->{'bp_region_length'} = 50;
	
	# this is oveloaded by $cfg
	$cfg->{'donor_GT'}->{'width'}  = 9;
	$cfg->{'donor_GT'}->{'margin'} = 3;
	$cfg->{'acceptor_AG'}->{'width'}  = 21;
	$cfg->{'acceptor_AG'}->{'margin'} = 18;
}
# ------------------------------------------------
sub Usage
{
	print qq(# -------------------
Usage: $0   parameters

required:
  --seq_in     [name]
  --introns_in [name]
  --parse_out  [name]  output file with parse

 optional:
  --parse_dir  [name]   put data here
  --score      [number] filter out introns with scores below threshold

  --cfg     [name] read parameters from this file
  --section [label] use this section from configuration file
  --verbose
  --debug
# -------------------
);
	exit 1;
}
# ------------------------------------------------
