#!/usr/bin/perl
# --------------------------------------------
# Alex Lomsadze
# Georgia Institute of Technology, -2014
# 
# Run gene finder on multi-sequence input file
# --------------------------------------------

use strict;
use warnings;
use Getopt::Long;
use Cwd qw(abs_path getcwd);
use Data::Dumper;
use FindBin qw( $RealBin );


my $seq_file = '';
my $parameters = '';
my $out_file = '';
my $exe = "/home/alexl/workspace/gm_es_petap/gmhmme3";
my $data_format = "gbk";
my $gene_finder = "gmhmme3";
my $pbs = 0;
my $v = 0;
my $debug = 0;

# --------------------------------------------

Usage() if ( @ARGV < 1 );
ParseCMD();

CheckInput();
CreateHeader( $out_file );
SetParameter();

my $record_counter = 0;
my $sequence;
my $id;


my $loader;
if ( $data_format eq 'gbk' )
{
	$loader = \&LoadSequenceGBK;
}
elsif ( $data_format eq 'fasta' )
{
	$loader = \&LoadSequenceFasta;
}
else {die;}

my @pbs_names;
my $PBS_LIST;
if( $pbs )
{
	open( $PBS_LIST, ">pbs_list" ) or die "error on open file for pbs_list\n";
}

open( my $IN, "$seq_file" ) or die "error on open file: $seq_file\n";
while( $loader->( \$sequence, \$id,  ) )
{
	$record_counter += 1;
	print $id ."\t". $record_counter ."\n" if $v;
	
	CleanSequence( \$sequence );
	SaveFasta( \$id, \$sequence );

	if( ! $pbs )
	{	
		RunPrediction( $id );
	
		# remove tmp files
		if ( !$debug )
		{
			CleanTmpFiles( $id );
		}
	}
	else
	{
		print $PBS_LIST "$id\n";
		system("mv  $id.fna  $id ");
		push @pbs_names, "$id";
	}
}
close $IN;

if( $pbs )
{
	close $PBS_LIST;
	
	$parameters = abs_path( $parameters );

	my $hmm_par = "\" -m $parameters  \"";
	
	my $com = "$RealBin/run_hmm_pbs.pl --par $hmm_par  --list pbs_list  --w ";
	system($com);

	foreach my $name (@pbs_names)
	{
		system("cat $name.out  >> $out_file" );
	}
}

print "records processed: $record_counter\n";
exit 0;

#======================================
sub RunPrediction
{
	my $name = shift;
	
	my $result;
	
	if ( $gene_finder eq "gmhmme3")
	{
		$result = system( " $exe -o $name.lst $name.fna " );
		
		if( $result ) { print "error on run of gmhmme3 with: $name\n"; exit 1; }
		if ( ! -r "$name.lst" ) { print "error, file with predictions not found: $name\n"; exit 1; }
		
		system( " cat $name.lst >> $out_file ");
		if ( !$debug )
		{
			unlink "$name.lst";
		}
	}
	elsif ( $gene_finder eq "augustus" ||  $gene_finder eq "snap" )
	{
		$result = system( " $exe $name.fna > $name.gff" );
		
		if( $result ) { print "error on run of $gene_finder with: $name\n"; exit 1; }
		
		system( " cat $name.gff >> $out_file ");
		if ( !$debug )
		{
			unlink "$name.gff";
		}
	}
	else
		{exit 1;};
}
# -------------------------------------
sub CleanTmpFiles
{
	my $name = shift;

	if ( ! -w "$name.fna"  ) { print "error, file not found: $name.fna\n"; exit 1; }

	unlink "$name.fna";
}
# -------------------------------------
sub SaveFasta
{
	my ( $ref_id, $s ) = @_;
	
	open( FASTA, ">$id.fna" ) or die "error on open file: $id.fna\n";
	print FASTA ">$id\n";
	print FASTA $$s;
	print FASTA "\n";
	close FASTA;
}
# -------------------------------------
sub CleanSequence
{
	my $s = shift;
	
	$$s =~ tr/ \t0-9//d;
}
# -------------------------------------
sub LoadSequenceGBK
{
	my ( $s, $ref_id ) = @_;
	

	$$s = "";
	$$ref_id = "";
	
	my $status_reading_sequence = 0;
		
	while( my $line = <$IN> )
	{	
		# end of GBK record
		if ( $line =~ /^\/\// )
		{
			$status_reading_sequence = 0;
			last;
		}
		
		# reading sequence from GBK record		
		if ( $status_reading_sequence )
		{
			$$s .= $line;
			next;
		}
		
		# start of GBK record
		if ( $line =~ /^LOCUS/ )
		{
			if ( $line =~ /^LOCUS\s+(\S+)\s+/ )
			{
				$$ref_id = $1;
				$status_reading_sequence = 0;
				next;
			}
			else
			{
				print "error in file format, line: $line\n";
				exit 1;
			}
		}
		
		# start of sequence in GBK record
		if ( $line =~ /ORIGIN/ )
		{
			$status_reading_sequence = 1;
			next;
		}
	}
		
	if ( $$ref_id ne "" and $$s ne "" )
	{
		return 1;
	}
		
	if ( $$ref_id eq "" and $$s eq "" )
	{
		return 0;
	}
		
	print "error, unexpected GBK record found\n";
	exit 1;
}
# ------------------------------------------------
sub LoadSequenceFasta
{
	my( $s, $id ) = @_;

	$$s = "";
	$$id = "";

	while( my $line = <$IN> )
	{	
		if( $line =~ /^>/ )
		{
			print $line if $debug;
			
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
				seek( $IN, -length($line), 1);
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
sub CreateHeader
{
	my $name = shift;
	
	my $str;
	
	open( OUT, ">$name" ) or die "error on open file: $name\n";
	
	$str = localtime;
	$str = "# date: ". $str . "\n";
	print OUT $str;

	$str = `ps -o args $$`;
	$str =~ s/COMMAND\n//; 
	$str = "# cmd: ". $str;
	print OUT $str;
	
	$str = getcwd();
	$str = "# cwd: ". $str . "\n";
	print OUT $str;
	
	$str = abs_path( $exe );
	$str = "# code: ". $str . "\n";
	print OUT $str;
	
	if ( $gene_finder =~ /gmhmme3/ )
	{
		$str = `$gene_finder`;
		$str =~ /\n*(.*version.*?)\n/;
		$str = $1;
		$str = "# version: ". $str ."\n";
		print OUT $str;
		
		$str = $parameters;
		( $str=~ /^\s*(\S+)\s*$/ or $str =~ /-m\s+(\S+)\s*/ );
		$str = $1;
		$str = abs_path( $str );
		$str = "# model: ". $str . "\n";
		print OUT $str;
	}

	if ( $gene_finder =~ /snap/ )
	{
		$str = $parameters;
		$str = abs_path( $str );
		$str = "# model: ". $str . "\n";
		print OUT $str;		
	}

	$str = abs_path( $seq_file );
	$str = "# seq: ". $str . "\n";
	print OUT $str;
	
	$str = "#\n\n";
	print OUT $str;

	close OUT;
}
# -------------------------------------
sub SetParameter
{
	if ( $gene_finder eq "gmhmme3" )
	{
		if ( $parameters =~ /^\s*(\S+)\s*$/ )
		{
			$exe = $exe . " -m $1 ";
		}
		else
		{
			$exe = $exe . " $parameters ";
		}
	}
	elsif ( $gene_finder eq "augustus" )
	{
		$exe = $exe ." --AUGUSTUS_CONFIG_PATH=/storage/alexl/euk/src/augustus.2.7/config   --species=". $parameters ;
	}
	elsif ( $gene_finder eq "snap" )
	{
		$exe = $exe ." -gff ". $parameters ;
	}
}
# -------------------------------------
sub CheckInput
{
	if ( !$seq_file )    { print "error, sequence file name is missing\n"; exit 1; }
	if ( !$parameters )  { print "error, gene finding parameters are missing\n"; exit 1; }
	if ( !$out_file )    { print "error, output file name is missing\n"; exit 1; }
	if ( !$exe )         { print "error, gene prediction code is not specified\n"; exit 1; }
	if ( !$data_format ) { print "error, sequence format is not specified\n"; exit 1; }
	if ( ! -r $seq_file ){ print "error on open sequence file: $seq_file\n"; exit 1; }
	if ( ! -x $exe )     { print "error on run gene finder file: $exe\n"; exit 1; }
	
	if(( $data_format ne "gbk") and ($data_format ne "fasta" ))
		{ print "error, unexpected sequence format specified: $data_format\n"; exit 1; }
	
	if ( $gene_finder ne "gmhmme3" and $gene_finder ne "augustus" and $gene_finder ne "snap")
		{ print "error, unknown gene finder specified: $gene_finder\n"; exit 1; }
	
	if ( $gene_finder eq "gmhmme3" )
	{
		if ( $parameters =~ /^\s*(\S+)\s*$/ or $parameters =~ /-m\s+(\S+)\s*/ )
		{
			my $mod = $1;
			if ( ! -r $mod ) { print "error on open file with parameters: $mod\n"; exit 1; }
		}
		else
			{ print "error, unexpected value of parameter specified: $parameters\n"; exit 1; }
	}
	
	if ( $gene_finder eq "snap" )
	{
		if ( ! -r $parameters ) { print "error on open file with parameters: $parameters\n"; exit 1; }
	}
}
# --------------------------------------------
sub ParseCMD
{
	my $opt_result = GetOptions
	(
		'seq=s' => \$seq_file,
		'par=s' => \$parameters,
		'out=s' => \$out_file,
		'exe=s' => \$exe,
		'data_format=s' => \$data_format,
		'gene_finder=s' => \$gene_finder,
		'pbs' => \$pbs,
		'verbose' => \$v,
		'debug'   => \$debug
	);

	if ( !$opt_result ) { print "error on command line: $0\n"; exit 1; }
	if( @ARGV > 0 ) { print "error, unexpected argument found on command line: $0 @ARGV\n"; exit 1; }
	$v = 1 if $debug;
}
# --------------------------------------------
sub Usage
{
	print qq(
 Usage:

 $0  --seq [name]  --par [name]  --out [name]

    --seq [name]  sequence file name
    --par [str]   gene finder command line parameters
    --out [name]  ouput file name

    --pbs

 Optional parameters:

    --exe [exe]  gene prediction program to run
      default: $exe
    --data_format [label]  format of input sequuence: gbk
      default: $data_format
    --gene_finder [label]  name of gene finder
      default: $gene_finder

    --verbose
);
	exit 1;
}
# --------------------------------------------
