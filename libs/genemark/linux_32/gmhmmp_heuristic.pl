#!/usr/bin/perl
#=============================================================
#
# This program runs GeneMark.hmm version 3.x [ref 1]
# with Heuristic models version 2.0 [ref 2]
# on single or multi record sequence in FASTA like format
#
# Optional: GeneMark prediction and grapth
# 
# References:
# [1]
#    Besemer J., Lomsadze A. and Borodovsky M.
#    "GeneMarkS: a self-training method for prediction of gene
#    starts in microbial genomes. Implications for finding
#    sequence motifs in regulatory regions."
#    Nucleic Acids Research, 2001, Vol. 29, No. 12, 2607-2618
# [2]
#    Besemer J. and Borodovsky M.
#    "Heuristic approach to deriving models for gene finding."
#    Nucleic Acids Research, 1999, Vol. 27, No. 19, pp. 3911-3920
#
# Please report problems to
#   Alex Lomsadze at alexl@gatech.edu
#
#=============================================================

use strict;
use warnings;
use FindBin qw($RealBin);
use Getopt::Long;

my $VERSION = "2.0";
my $debug = 0;

#------------------------------------------------
# installation settings: required directories and files

my $gmsuite_dir = $RealBin;                                # GeneMark Suite installation directory
my $hmm = File::Spec->catfile( $gmsuite_dir, "gmhmmp" );   # GeneMark.hmm gene finding program <gmhmmp>; version 3.*
my $genemark = File::Spec->catfile( $gmsuite_dir, "gm" );  # GeneMark gene finding program <gm>, version 2.5p for graph
my $ps_gm_overlay = File::Spec->catfile( $gmsuite_dir, "ps_gm_overlay.pl" );  # script to overlay PS files

# directory with heuristic models for GeneMark.hmm: heuristic version 2.0;
my $heu_mod_dir = File::Spec->catdir( $gmsuite_dir , "heuristic_mod" );
my $heu_mat_dir = File::Spec->catdir( $gmsuite_dir , "heuristic_mat" );

my $heu_11_mod = File::Spec->catfile( $gmsuite_dir, "heu_11.mod" );
my $heu_1_mod  = File::Spec->catfile( $gmsuite_dir, "heu_1.mod" );
my $heu_4_mod  = File::Spec->catfile( $gmsuite_dir, "heu_4.mod" );

my $mgm_11_mod = File::Spec->catfile( $gmsuite_dir, "MetaGeneMark_v1.mod" );

# ------------------------------------------------
# command line parameters

# not in GMS

my $gc_to_run;
my $mod_type;
my $gm_gcode;

# shared with GMS

my $seqfile = '';

my $output = '';            # predicted gene coordinates are here
my $shape = "partial";
my $motif = "1";            # this turns on/off motif in Genemark.hmm prokaryotic
my $strand = "both";
my $format = "LST";
my $ps = '';
my $pdf = '';
my $faa = '';
my $fnn = '';
my $gcode = '11';
my $test;

my $out_name_heuristic = "GeneMark_hmm_heuristic.mod";
my $out_gm_heu = "GeneMark_heuristic.mat";

#------------------------------------------------
# constants

# minimum length of sequence in single FASTA record
my $MIN_LENGTH = 40;

my $GENETIC_CODE = "11|4|1";
my $MIN_HEURISTIC_GC = 30;
my $MAX_HEURISTIC_GC = 70;

#------------------------------------------------

if ( $#ARGV == -1 ) { print Usage(); exit 1; }

ParseCMD();
SelfTest() if $test;
CheckInput_A();
SetParamaterFileNames( $gcode, $mod_type, $gc_to_run );
SetOutputNames();

SetHmmCmd();
my $command = $hmm;

print $command ."\n" if $debug;

RunSystem( $command, "predict genes\n" );

if( $mod_type )
{
	$out_gm_heu = SetMat( $output );
}

GetPS_PDF() if ($ps or $pdf);

exit 0;

#=============================================================
# sub section:
#=============================================================
sub SetMat
{
	my( $file_name ) = shift;
	
	my $gc = 0;
	
	open( my $IN, $file_name ) or die("error on open file: $file_name");
	while( my $line = <$IN> )
	{
		if( $line =~ /^Model information:/ )
		{
			if( $line =~ /_(\d+)\s*$/ )
			{
				$gc = $1;
			}
			
			last;
		}
	}
	close $IN;
	
	if( $gc == 0 ) { die" error, gc value for GeneMark is missing: $file_name\n"; }
	
	return GetHeuristicFileName( $gc, $gcode, $heu_mat_dir, ".mat" );
}
#------------------------------------------------
sub GetPS_PDF
{
	my $tmp_ps = "out.ps";

	MakeGMgraph( $out_gm_heu, $seqfile, $tmp_ps  );

	my $tmp_ps_hmm = $tmp_ps ."_hmm";

	$command = "$ps_gm_overlay -int_file $output -outfile $tmp_ps_hmm  $tmp_ps";
	RunSystem( $command, "overlay hmm graphs: " );

	unlink $tmp_ps;
	rename $tmp_ps_hmm, $tmp_ps;

	system( "ps2pdf  $tmp_ps  $pdf ") if $pdf;
	rename $tmp_ps, $ps if $ps;
	
	unlink $tmp_ps;
}
#------------------------------------------------
sub MakeGMgraph
{
	my( $mat, $sname, $graph_name ) = @_;

	my $symlink_name = File::Spec->splitpath( $sname ) ."_for_gm";
	if ( -e $symlink_name ) { unlink $symlink_name ;}

	if ( symlink $sname, $symlink_name ) {;}
	else { print "error on symlink\n"; exit 1; }
	
	my $gm_gcode_option = ' -c ';

	if( $gcode )
	{
		$gm_gcode_option .= File::Spec->catfile( $gmsuite_dir, "gm_". $gcode .".tbl" )
	}

	$command = "$genemark -gkfns  $gm_gcode_option -m $mat  $symlink_name ";
	RunSystem( $command, "make gm graph: " );

	print $command ."\n" if $debug;

	unlink  $symlink_name;
	unlink  $symlink_name .".lst";

	rename $symlink_name .".ps", $graph_name;
}
# -----------------------------------------------
sub RunSystem
{
	my( $com, $text ) = @_;
	if ( system( $com ) ) { print "error on last system call: $text\n"; exit 1; }
}
# -----------------------------------------------
sub SetHmmCmd
{
	# set prediction for linear genome; no partial genes at the sequence ends
	$hmm = $hmm . " -i 1 " if ( $shape eq "linear" ); 

	# set strand to predict
	if( $strand eq "direct" )      { $hmm .= " -s d "; }
	elsif( $strand eq "reverse" )  { $hmm .= " -s r "; }
		
	if( $format eq "LST" )         { $hmm .= " -f L "; }
	elsif( $format eq "GFF" )      { $hmm .= " -f G "; }
	
	$hmm .= " -g $gcode " if $gcode;
	
	$hmm .= " -o $output ";
	$hmm .= " -D $fnn " if $fnn;
	$hmm .= " -A $faa " if $faa;	
	$hmm .= " -m $out_name_heuristic ";
	$hmm .= " ". $seqfile;
}
# -----------------------------------------------
sub SetOutputNames
{
	if( ! $output )
	{
		if ( $format eq "LST" )      { $output = File::Spec->splitpath( $seqfile ) .".lst";  }
		elsif ( $format eq "GFF" )   { $output = File::Spec->splitpath( $seqfile ) .".gff"; }
	
		$fnn = File::Spec->splitpath( $seqfile ) .".fnn" if $fnn;
		$faa = File::Spec->splitpath( $seqfile ) .".faa" if $faa;
		$ps  = File::Spec->splitpath( $seqfile ). ".ps"  if $ps;
		$pdf = File::Spec->splitpath( $seqfile ). ".pdf" if $pdf;
	}
	else
	{
		$fnn = $output . ".fnn" if $fnn;
		$faa = $output . ".faa" if $faa;
		$ps  = $output . ".ps"  if $ps;
		$pdf = $output . ".pdf" if $pdf;
	}
}
# -----------------------------------------------
sub SetParamaterFileNames
{
	$gm_gcode = File::Spec->catfile( $gmsuite_dir, 'gm_'. $gcode .".tbl" );

	if ( $mod_type and  $mod_type eq "1999" )
	{
		$out_name_heuristic = $heu_11_mod if $gcode eq '11';
		$out_name_heuristic = $heu_1_mod  if $gcode eq '1';
		$out_name_heuristic = $heu_4_mod  if $gcode eq '4';
	}
	elsif( $mod_type and  $mod_type eq "2010" )
	{
		$out_name_heuristic = $mgm_11_mod if $gcode eq '11';
	}
	elsif( $gc_to_run )
	{
		$out_name_heuristic = GetHeuristicFileName( $gc_to_run, $gcode, $heu_mod_dir, ".mod" );
		$out_gm_heu         = GetHeuristicFileName( $gc_to_run, $gcode, $heu_mat_dir, ".mat" );
	}
}
#------------------------------------------------
sub GetHeuristicFileName
{
  my( $GC, $code, $dir, $ext ) = @_;
  $GC = int( $GC + 0.5 );

  if( $GC < $MIN_HEURISTIC_GC ) { $GC = $MIN_HEURISTIC_GC; }
  if( $GC > $MAX_HEURISTIC_GC ) { $GC = $MAX_HEURISTIC_GC; }

  return $dir ."/heu_" . $code . "_" . $GC . $ext;
}
#------------------------------------------------
sub SelfTest
{
  print "installation test ...\n";

  CheckFile( $heu_mod_dir, "er" );
  CheckFile( $heu_mat_dir, "er" );
  CheckFile( $hmm , "ex");

  my @array = split( /\|/, $GENETIC_CODE );
  my $code  = '';
  my $GC = 0;

  foreach $code ( @array )
  {
    for ( $GC = $MIN_HEURISTIC_GC; $GC <= $MAX_HEURISTIC_GC; ++$GC )
    {
       CheckFile( GetHeuristicFileName( $GC, $code, $heu_mod_dir, ".mod" ), "er" );
       CheckFile( GetHeuristicFileName( $GC, $code, $heu_mat_dir, ".mat" ), "er" );
    }
  }

  print "done\n";
}
#------------------------------------------------
sub CheckFile
{
  my( $name, $option ) = @_;
  my @array = split( //, $option );
  my $result = 1;
  my $i;

  foreach $i ( @array )
  {
    if ( $i eq "e" )
      { ( -e $name )||( print("$!, $name\n"), $result = 0 ); }
    elsif ( $i eq "d" )
      { ( -d $name )||( print("$!, $name\n"), $result = 0 ); }
    elsif ( $i eq "f" )
      { ( -f $name )||( print("$!: $name\n"), $result = 0 ); }
    elsif ( $i eq "x" )
      { ( -x $name )||( print("$!, $name\n"), $result = 0 ); }
    elsif ( $i eq "r" )
      { ( -r $name )||( print("$!, $name\n"), $result = 0 ); }
    elsif ( $i eq "w" )
      { ( -w $name )||( print("$!, $name\n"), $result = 0 ); }
    else
      { die( "error, no support for file test option $i\n" ); }
    if( !$result ) { last; }
  }
  return $result;
}
# -----------------------------------------------
sub CheckInput_A
{
	my $OUTPUT_FORMAT = "LST|GFF";
	my $SHAPE_TYPE = "linear|circular|partial";
	my $STRAND = "direct|reverse|both";
	my $GENETIC_CODE = "11|4|1|25";
	
	if ( $OUTPUT_FORMAT !~ /\b$format\b/ )   { print "Error: format [$format] is not supported\n"; exit 1; }
	if ( $SHAPE_TYPE !~ /\b$shape\b/ )       { print "Error: sequence organization [$shape] is not supported\n"; exit 1 ; }
	if ( $STRAND !~ /\b$strand\b/ )          { print "Error: strand [$strand] is not supported\n"; exit 1; }
	if ( $GENETIC_CODE !~ /\b$gcode\b/ )     { print "Error: genetic code [$gcode] is not supported\n"; exit 1 ; }
}
# -----------------------------------------------
sub ParseCMD
{
	if ( !GetOptions
	  (
	    'output=s'    => \$output,
	    'shape=s'     => \$shape,
	    'strand=s'    => \$strand,
	    'format=s'    => \$format,
	    'gcode=s'     => \$gcode,
	    'ps'          => \$ps,
	    'pdf'         => \$pdf,
	    'faa'         => \$faa,
	    'fnn'         => \$fnn,
	    'gc=i'        => \$gc_to_run,
	    'type=s'      => \$mod_type,
	    'test'        => \$test,
	    'debug'       => \$debug
	  )
	) { exit 1; }
	
	if ( $#ARGV == -1 ) { print "Error: sequence file name is missing\n"; exit 1; }
	if ( $#ARGV > 0 ) { print "Error: more than one input sequence file was specified\n"; exit 1; }
	
	$seqfile = shift @ARGV;
}
# -----------------------------------------------
sub Usage
{
my $text =
"
this code runs GeneMark.hmm prokaryotic with GeneMarkS derived parameters
Usage: $0 [options] <sequence file name>

Input sequence in FASTA format

Select parameters:
--type      <string> type of heuristic model to use
--gc        <number>  run with specified GC model
--gcode     <number> genetic code
            (default: $gcode; supported: 11, 4 and 1)

Output options:
(output is in current working directory)
--output    <string> output file with predicted gene coordinates by GeneMarh.hmm
            (default: <sequence file name>.lst)
--format    <string> output coordinates of predicted genes in this format
            (default: $format; supported: LST and GFF)
--fnn       create file with nucleotide sequence of predicted genes
--faa       create file with protein sequence of predicted genes
Attention, graphical output works only if input FASTA with one sequence in file
--ps        create GeneMark graphical output in PostScript format
--pdf       create GeneMark graphical output in PDF format

Set running option
--shape     <string> sequence organization
            (default: $shape; supported: linear, circular and partial)
--strand    <string> sequence strand to predict genes in
            (default: '$strand'; supported: direct, reverse and both )
Developer options:
--test
";
        return $text;
}
# -----------------------------------------------
