#!/usr/bin/perl
#=================================
# This program takes as an input a prediction file from GeneMark.hmm
# eukaryotic program and translates it to GFF3 format.
#
# GTF - format
#
# phase definition is in GeneMark.hmm style
# direct strand,  codon 0-1-2
# reverse strand, codon 2-1-0
# 
# Alex Lomsadze, October 22, 2007
# Georgia Institute of Technology
# alexl@gatech.edu
#=================================

use warnings;
use strict;
use Getopt::Long;
use File::Spec;

use FindBin qw( $RealBin );
use lib $FindBin::Bin ."/lib";
use ReadBackwards;

my $v = 0;
my $debug = 0;

my $infile = '';
my $outfile = '';
my $app = 0;
my $min = 0;
my $format = 'gtf';

Usage() if ( @ARGV < 1 );
ParseCMD();
CheckInput();

print $infile ."\n" if $v;

my %gene_length_hash;
my $defline = CreateLengthHash( $infile, \%gene_length_hash , $min );
print $defline ."\n" if $v;

my ( $seq_id, $shift ) = ParseDefline( $defline );
print "$seq_id $shift\n" if $v;

my $last_gene_id = GetLastId( $outfile );

print $last_gene_id ."\n" if $v;

open( IN, "$infile") || die( "error on open input file $0: $infile, $!\n" );

my $OUT;
if ( $app )
{
	open( $OUT, ">>$outfile") || die( "error on open output file $0: $outfile, $!\n" );
}
else
{
	open( $OUT, ">$outfile") || die( "error on open output file $0: $outfile, $!\n" );
}

my $source = "GeneMark.hmm";
my $type   = "";
my $start  = 0;
my $end    = 0;
my $score  = ".";
my $strand = ".";
my $phase  = ".";
my $attributes = ".";

# tmp to print start/stop codon positions
my $pos;

# tmp for gene id value
my $current_gene_count;

# attributes for different $type
my $att_start = '';
my $att_stop = '';
my $att_exon = '';
my $att_CDS = '';

# tag for name in $attributes field
my $tag_g;
my $tag_t;

# evidence labels
my $evi_start = 0;
my $evi_end = 0;
my $att_evi = '';

my $gmhmm_gene_id;

while(<IN>)
{
  # GeneMark.hmm output format
  # gene_id   exon_id   strand   type   start   end   length   start_frame   end_frame   supported(10-11)
  #   1          2        3       4       5      6       7         8            9      10       11
  if( /^\s*(\d+)\s+(\d+)\s+([\+-])\s+(\w+)\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+([-+])\s+([-+])\s*/ )
  {
  	$gmhmm_gene_id = $1;
	
	if( exists $gene_length_hash{ $gmhmm_gene_id } )
	{
		if( $gene_length_hash{ $gmhmm_gene_id } )
		{
			$gmhmm_gene_id = $gene_length_hash{ $gmhmm_gene_id };
		}
		else
		{
			next;
		}
	}
	else { print "error, unexpected file format found: $_" ; exit 1;}
  	
    $strand = $3;
    $start  = $5 + $shift - 1;
    $end    = $6 + $shift - 1;

    if ( $strand eq '+' )
      { $phase = (3 - $8 + 1)%3; }
    else
      { $phase = (3 - $9 + 1)%3; }
 
	$current_gene_count = $last_gene_id + $gmhmm_gene_id;

    $tag_g =  $current_gene_count ."_g";
    $tag_t =  $current_gene_count ."_t";

	if( $10 eq '+' ) {$evi_start = 1;} else {$evi_start = 0;}
	if( $11 eq '+' ) {$evi_end = 1;}   else {$evi_end = 0;}
	$att_evi = $evi_start ."_". $evi_end;

    $attributes = "gene_id \"". $tag_g ."\"; transcript_id \"". $tag_t ."\";";

    $att_start = $attributes;
    $att_stop  = $attributes;
    $att_exon  = $attributes;
    $att_CDS   = $attributes;
    
 	if( $att_evi ne '0_0' )
	{
		$att_CDS  = $attributes . " evidence $att_evi;";
	}   

	if ( $strand eq "+" )
	{
		if( $4 eq "Initial" )
		{
			print $OUT "$seq_id\t$source\texon\t$start\t$end\t0\t$strand\t.\t$att_CDS\n";
			$pos = $start + 2;
			print $OUT "$seq_id\t$source\tstart_codon\t$start\t$pos\t$score\t$strand\t0\t$att_start\n";
			print $OUT "$seq_id\t$source\tCDS\t$start\t$end\t$score\t$strand\t$phase\t$att_CDS\n";
		}
		elsif( $4 eq "Internal")
		{
			print $OUT "$seq_id\t$source\texon\t$start\t$end\t0\t$strand\t.\t$att_CDS\n";
			print $OUT "$seq_id\t$source\tCDS\t$start\t$end\t$score\t$strand\t$phase\t$att_CDS\n";
		}
		elsif( $4 eq "Terminal" )
		{
			print $OUT "$seq_id\t$source\texon\t$start\t$end\t0\t$strand\t.\t$att_CDS\n";
			if ( $format eq 'gtf2' )
			{
				if ( $end - $start + 1 > 3 )
				{
					print $OUT "$seq_id\t$source\tCDS\t$start\t". ($end -3) ."\t$score\t$strand\t$phase\t$att_CDS\n";
				}
			}
			else
			{
				print $OUT "$seq_id\t$source\tCDS\t$start\t$end\t$score\t$strand\t$phase\t$att_CDS\n";
			}

			$pos = $end - 2;
			print $OUT "$seq_id\t$source\tstop_codon\t$pos\t$end\t$score\t$strand\t0\t$att_stop\n";
		}
		elsif ( $4 eq "Single" )
		{
			print $OUT "$seq_id\t$source\texon\t$start\t$end\t0\t$strand\t.\t$att_CDS\n";
			$pos = $start + 2;
                        print $OUT "$seq_id\t$source\tstart_codon\t$start\t$pos\t$score\t$strand\t0\t$att_start\n";

			if ( $format eq 'gtf2' )
                        {
				if ( $end - $start + 1 > 3 )
                        	{
                                	print $OUT "$seq_id\t$source\tCDS\t$start\t". ($end -3) ."\t$score\t$strand\t$phase\t$att_CDS\n";
                        	}
			}
			else
			{
				print $OUT "$seq_id\t$source\tCDS\t$start\t$end\t$score\t$strand\t$phase\t$att_CDS\n";
			}

                        $pos = $end - 2;
                        print $OUT "$seq_id\t$source\tstop_codon\t$pos\t$end\t$score\t$strand\t0\t$att_stop\n";
		}
	}
	elsif ( $strand eq "-" )
	{
		if( $4 eq "Terminal" )
		{
			print $OUT "$seq_id\t$source\texon\t$start\t$end\t0\t$strand\t.\t$att_CDS\n";
			$pos = $start + 2;
			print $OUT "$seq_id\t$source\tstop_codon\t$start\t$pos\t$score\t$strand\t0\t$att_stop\n";

			if ( $format eq 'gtf2' )
			{
				if( $end - $start + 1 > 3)
				{
					print $OUT "$seq_id\t$source\tCDS\t". ($start +3)."\t$end\t$score\t$strand\t$phase\t$att_CDS\n";
				}
			}
			else
			{
				print $OUT "$seq_id\t$source\tCDS\t$start\t$end\t$score\t$strand\t$phase\t$att_CDS\n";
			}
		}
		elsif( $4 eq "Internal" )
		{
			print $OUT "$seq_id\t$source\texon\t$start\t$end\t0\t$strand\t.\t$att_CDS\n";
			print $OUT "$seq_id\t$source\tCDS\t$start\t$end\t$score\t$strand\t$phase\t$att_CDS\n";
		}
		elsif( $4 eq "Initial" )
		{
			print $OUT "$seq_id\t$source\texon\t$start\t$end\t0\t$strand\t.\t$att_CDS\n";
			print $OUT "$seq_id\t$source\tCDS\t$start\t$end\t$score\t$strand\t$phase\t$att_CDS\n";
			$pos = $end - 2;
			print $OUT "$seq_id\t$source\tstart_codon\t$pos\t$end\t$score\t$strand\t0\t$att_start\n";
		}
		elsif( $4 eq "Single" )
		{
			print $OUT "$seq_id\t$source\texon\t$start\t$end\t0\t$strand\t.\t$att_CDS\n";
			$pos = $start + 2;
                        print $OUT "$seq_id\t$source\tstop_codon\t$start\t$pos\t$score\t$strand\t0\t$att_stop\n";

			if ( $format eq 'gtf2' )
			{
	                        if( $end - $start + 1 > 3)
        	                {
                	                print $OUT "$seq_id\t$source\tCDS\t". ($start +3)."\t$end\t$score\t$strand\t$phase\t$att_CDS\n";
                        	}
			}
			else
			{
				print $OUT "$seq_id\t$source\tCDS\t$start\t$end\t$score\t$strand\t$phase\t$att_CDS\n";
			}
			$pos = $end - 2;
                        print $OUT "$seq_id\t$source\tstart_codon\t$pos\t$end\t$score\t$strand\t0\t$att_start\n";
		}
	}
  }
}

close IN;
close $OUT;

exit 0;

# ----------------- sub --------------------------
sub GetLastId
{
	my $name = shift;
	my $i = 0;
	
	if( ! -e $name )
	{
		return $i;
	}
	
	my $fp =  File::ReadBackwards->new( $name ) or die "error on open $name $!" ;
	while( !$fp->eof )
	{
		if ( $fp->readline() =~ /gene_id \"(\d+)_g\";/ )
		{
			$i = $1;
			last;
		}
	}
	
	return $i;
}
# ------------------------------------------------
sub ParseDefline
{
	my $line = shift;
	
	my $id = "seq";
	my $coord = 1;
	
	if(  $line =~ /^>?\S+\s+(.*)\s+(\d+)\s+\d+\s*$/ )
	{
		$id = $1;
		$id =~ s/^\s+//;
		$id =~ s/\s+$//;
		
		$coord = $2;
	}
	
	return ( $id, $coord );
}
# ------------------------------------------------
sub CreateLengthHash
{
	my( $name, $ref, $m ) = @_;
	
	my $defline = '';
	my $counter = 0;
	
	open( my $IN, $name ) or die"error on open file $0: $name\n$!\n";
	while(<$IN>)
	{	
		if ( /^FASTA defline:\s+(.*)$/ )
		{
			$defline = $1;
		}
		
		if ( /^>gene_(\d+)\|GeneMark.hmm\|(\d+)_nt/ )
		{
			if ( $2 < $m )
			{
				$ref->{$1} = 0;
			}
			else
			{
				++$counter;
				$ref->{$1} = $counter;
			}
		}
	}
	close $IN;
	
	return $defline;
}
# ------------------------------------------------
sub CheckInput
{
	# required input
	if( !$infile or ! -e $infile )  { print "error, input file is misssing $0\n"; exit 1; }
	if( !$outfile  ) { print "error, outfile file name is misssing $0\n"; exit 1; }
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
		'infile=s'  => \$infile,
		'outfile=s' => \$outfile,
		'app'       => \$app,
		'min=i'     => \$min,
		'verbose'   => \$v,
		'debug'     => \$debug,
		'format=s'  => \$format
	);
	
	if( !$opt_result ) { print "error on command line\n"; exit 1; }
	if( @ARGV > 0 ) { print "error, unexpected argument found on command line: @ARGV\n"; exit 1; }
	$v = 1 if $debug;
}
# -------------------------------------------------------------
sub Usage
{
	my $text =
"
Usage: $0 --in <input file> --out <output file> [optional]

   <input file>  - name of file with predictions by GeneMark.hmm eukaryotic
   <output file> - name of file to save predictions in GTF format
   --app         - append to output file
   --min         - [length] filter out short gene predictions
   --format      - [gtf|gtf2] output format
";
	print $text;
	
	exit 1;
}
# -------------------------------------------------------------
