#!/usr/bin/perl
# ===========================================
# Alex Lomsadze
# Georgia Institute of Technology, -2014
#
# Run GeneMark.hmm eukaryotic on cluster using PBS
# Input: directory with FASTA formated sequence - one record per file
# ===========================================

use strict;
use warnings;
use Getopt::Long;
use File::Spec;
use Cwd qw( abs_path );
use File::Temp qw( tempfile );
use FindBin qw( $RealBin );

my $version = "2.4";
my $hmm = File::Spec->catfile( $RealBin, "gmhmme3 ");

# -------------------------------------------

my $in = ".";
my $out = ".";

my $par;
my $ext = ".out";

my $list;

my $log;
my $w;

my $v;

my $intron;
my $force;

Usage() if ( @ARGV < 1 );
CmdLine();
CheckBeforeRun();

# resolve direcory and file id's variables

my @id;
ResolveNames();

if ( $#id < 0 ) { die "error: no files to analyze\n"; } 

my $script;
my $job_id;
my %running;

# create & run PBS script
foreach my $i ( @id )
{
	$script = CreateScript( $i );
	
	$job_id = `qsub $script`;
	$job_id =~ s/\s*$//;
	$running{$script} = 1;
	
	print "$script $job_id $i\n" if $v;
}

# optional wait
if ( defined $w )
{
	Wait();
	CleanPBSfiles();
}

exit 0;

# ================== sub =====================
sub CleanPBSfiles
{
	my $name;
	my $filesize;
	
	sleep(1);
	
	foreach my $key ( keys %running )
	{
		$name = $key .".log";
		$filesize = -s $name;
		
		print "$name $filesize\n" if $v;
		
		if ( ! $filesize )
		{
			unlink $name;
			unlink $key;
		}
	}
}
# --------------------------------------------
sub Wait
{	
	my $status = 1;

	while( $status )
	{
		$status = 0;
		
		foreach my $file ( keys %running )
		{
			if ( $running{$file} )
			{	
				if( -f "$file.log" )
				{
					$running{$file} = 0;
				}
			
				$status += $running{$file};	
			}
		}
	}
}
# ------------------------------------------
sub FormatQuery
{
	my ( $index, $name ) = @_;

	my $seq = File::Spec->catfile( $in, "dna.fa_". $index );
	my $lst = File::Spec->catfile( $out, "dna.fa_". $index . $ext );

	my $seq_id = "dna.fa_". $index;

	my $str = "";

	if (defined $intron )
	{
		my $intron_opt = File::Spec->catfile( $intron, "dna.fa_". $index . $ext . ".info" );
		$str = " -b $intron_opt ";
	}
	
	if( defined $force)
	{
		my $force_opt = File::Spec->catfile( $force, "dna.fa_". $index .".gff" );
		$str .= " -d $force_opt ";
	}
	
	if ( ! -f $seq ) { die "error, file with sequence not found: $seq\n"; }

my $text = "#!/bin/bash
#PBS -o $name.log
#PBS -j oe
#PBS -l nodes=1:ppn=1
#PBS -l walltime=0:20:00

$hmm  $par -o $lst $str  $seq
";
	return $text;
}
# ------------------------------------------
sub CreateScript
{
	my $index = shift;

	my ( $fh, $script ) = tempfile( "pbs_XXXXX" );
	if ( !fileno($fh) ) { die "error on open temporally file: $!\n"; }

	my $text = FormatQuery( $index, $script );
	print $fh $text;

	close $fh;
	
	return $script;
}
# ------------------------------------------
sub Usage
{
	my $text = 	"Usage: $0  --par [string]
Parameters:
    --in    [dir] input directory; default is the current working directory
            
            files matching the regex dna.fa_\\d+ will be analyzed
                or alternatively
            explicit filenames can be provided by optional parameter [--list]
            leading zeros '0' in the numbering of files are not allowed
            
    --out   [dir] output directory; default is current working directory
            
    --par   [string] pass this string to GeneMark.hmm

Optional

    --list  [file]
            file with sequence filenames; one name per line
    --ext   [string]
            output filenames are formed by adding this extension to sequence filenames
            default is $ext
    --log   [file]
            add information to logfile
    --w     wait until done
    --v     verbose

    --intron [dir]    
    --force [dir] dir with evidence files
version $version
";
	print $text;
	exit 1;
}
# ------------------------------------------
sub CmdLine
{	
	my $opt_results = GetOptions
	(
		'in=s'   => \$in,
		'out=s'  => \$out,
		'par=s'  => \$par,
		'list=s' => \$list,
		'ext=s'  => \$ext,
		'log=s'  => \$log,
		'v'      => \$v,
		'w'      => \$w,
		'force=s'=> \$force,
		'intron=s' => \$intron
	);
	
	if ( !$opt_results ) { print "error on command line\n"; exit 1; }
	if ( @ARGV > 0 ) { print "error on command line, unexpected argument was provided: @ARGV\n"; exit 1; }	
}
# ------------------------------------------
sub CheckBeforeRun
{
	if ( ! defined $par ) { print  "error, parameter/s for HMM are missing\n"; exit 1; } 	
	if( !defined $in  or !defined $out or ! -d $in or ! -d $out ) { print  "error on find directory\n"; exit 1; }
	
	$in  = abs_path( $in ); 
	$out = abs_path( $out );
	$intron = abs_path( $intron ) if defined $intron;
	$force = abs_path($force) if defined $force;;
	
	print "--in $in\n--out $out\n" if $v;
}
# ------------------------------------------
sub ResolveNames
{
	if ( defined $list )
	{
		GetIDsFromList( $list, $in );
	}
	else
	{
		GetIDsFromDir( $in );
	}
}
# ------------------------------------------
sub ParseID
{
	my $str = shift;
	
	if ( $str =~ /^dna\.fa_(\d+)$/ and  $str !~ /_0.+$/ )
	{
		return $1;
	}
	
	return;
}
# ------------------------------------------
sub GetIDsFromList
{
	my ( $file, $dir ) = @_;
	
	my $line;
	my $filename;
	my $index;
	
	open( IN, $file ) or die "error on open file: $!\n";
	while(<IN>)
	{
		if( /^#/ ) {next;}
		if( /^\s*$/ ) {next;}
		
		s/^\s*//;
		s/\s*$//;
		
		$line = $_;
		
		if ( defined $v )
		{
			print "$line\n";
		}
 
		$index = ParseID( $line );
		if ( !defined $index ) { die "error in file name format: $line\n"; }
 
		$filename = File::Spec->catfile( $dir, $line );
		if ( !-f $filename ) { die "error on find file: $line\n"; }

		push @id, $index;
	}
	close IN;
}
# ------------------------------------------
sub GetIDsFromDir
{
	my $dir = shift; 
	
	opendir( DIR, $dir ) or die "error on open dirirectory: $!\n";
	foreach my $name ( readdir DIR )
	{
		print "$name\n" if $v;
		
		my $index = ParseID( $name);
		
		if ( defined $index )
		{
			push @id, $index;
		}
	}
	closedir DIR;
}
# ------------------------------------------
