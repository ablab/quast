#!/usr/bin/perl
# -----------------------------------------
# Alex Lomsadze
#
# run GeneMark.hmm prokaryotic with parameters from GeneMarkS training
#   or non-GeneMarkS training, but formatted GMS style
#
# This code is modified of GeneMarkS prediction step,
# designed to run with pre-build parameter files outside of GeneMarkS itself.
# This script shares many command line options with GeneMarkS
#
# ps2pdf - required
# -----------------------------------------

use strict;
use warnings;
use FindBin qw($RealBin);
use Getopt::Long;
use File::Spec;

my $VERSION = "1.0 September 2014";
my $debug = 0;

# ------------------------------------------------
# installation settings; required directories and programs

my $gmsuite_dir = $RealBin;                                # GeneMark Suite installation directory
my $hmm = File::Spec->catfile( $gmsuite_dir, "gmhmmp" );   # GeneMark.hmm gene finding program <gmhmmp>; version 3.*
my $genemark = File::Spec->catfile( $gmsuite_dir, "gm" );  # GeneMark gene finding program <gm>, version 2.5p for graph
my $ps_gm_overlay = File::Spec->catfile( $gmsuite_dir, "ps_gm_overlay.pl" );  # script to overlay PS files

# ------------------------------------------------
# one of these 3 prediction modes is executed
# if running mode is specified, then force it
# if not, then check for set of available models in the order, as it is specified below

my $mode = '' ;

$mode = 'combine';    # native and heuristic combined
$mode = 'native';     # native only
$mode = 'heuristic';  # heuristic only

# ------------------------------------------------
# command line parameters

# not in GMS

my $org = '.';              # directory with species specific parameters
$mode = '';

# shared with GMS

my $seqfile = '';

my $output = '';            # predicted gene coordinates are here
my $name = '';              # optinal prefix in name of species specific pararameter files; default GeneMark
my $shape = "partial";
my $motif = "1";            # this turns on/off motif in Genemark.hmm prokaryotic
my $strand = "both";
my $format = "LST";
my $ps = '';
my $pdf = '';
my $faa = '';
my $fnn = '';

# ------------------------------------------------
# Set expected names for parameter files

my $out_name = "GeneMark_hmm.mod";
my $out_name_combined = "GeneMark_hmm_combined.mod";
my $out_name_heuristic = "GeneMark_hmm_heuristic.mod";
my $out_gm = "GeneMark.mat";
my $out_gm_heu = "GeneMark_heuristic.mat";

my $out_suffix = "_hmm.mod";
my $out_suffix_combined = "_hmm_combined.mod";
my $out_suffix_heu = "_hmm_heuristic.mod";
my $out_gm_suffix = "_gm.mat";
my $out_gm_heu_suffix = "_gm_heuristic.mat";

my $gcode = '';

if( $name )
{
	if ( $name =~ /\s/ ) { print "Error: white space is not allowed in name: $name\n"; exit 1; }

	$out_name = $name . $out_suffix;
	$out_name_combined = $name . $out_suffix_combined;
	$out_name_heuristic = $name . $out_suffix_heu;
	$out_gm = $name . $out_gm_suffix;
	$out_gm_heu = $name . $out_gm_heu_suffix;
}
# ------------------------------------------------

if ( $#ARGV == -1 ) { print Usage(); exit 1; }

ParseCMD();
CheckInput_A();
SetParamaterFileNames( $org );
SetOutputNames();

# ------------------------------------------------

# set this only after above step
CheckSetMode();

print "mode $mode\n" if $debug;

SetHmmCmd();
my $command = $hmm;

print "$command\n" if $debug;

RunSystem( $command, "predict genes\n" );

GetPS_PDF() if ($ps or $pdf);

exit 0;

# ================================================
sub SetParamaterFileNames
{
	my( $d ) = shift;
	
	if( ! -e $d ) { print "error, required directory not found: $d\n"; exit 1; }
	
	$out_name           = File::Spec->catfile( $d, $out_name );
	$out_name_combined  = File::Spec->catfile( $d, $out_name_combined );
	$out_name_heuristic = File::Spec->catfile( $d, $out_name_heuristic );
	$out_gm             = File::Spec->catfile( $d, $out_gm );
	$out_gm_heu         = File::Spec->catfile( $d, $out_gm_heu );

	$gcode = File::Spec->catfile( $org, 'gm_4_tbl' )  if ( -e File::Spec->catfile( $org, "gm_4_tbl" ) );
	$gcode = File::Spec->catfile( $org, 'gm_1_tbl' )  if ( -e File::Spec->catfile( $org, "gm_1_tbl" ) );
	$gcode = File::Spec->catfile( $org, 'gm_25_tbl' ) if ( -e File::Spec->catfile( $org, "gm_25_tbl" ) );
}
#------------------------------------------------
sub GetPS_PDF
{
	my $tmp_ps = "out.ps";

	if( $mode eq 'combined' )
	{
		MakeGMgraph( $out_gm,     $seqfile, "typical.ps"  );
		MakeGMgraph( $out_gm_heu, $seqfile, "atypical.ps"  );
		$command = "$ps_gm_overlay -outfile $tmp_ps  typical.ps  atypical.ps";
		RunSystem( $command, "overlay t and a graphs: " );
		unlink "typical.ps";
		unlink "atypical.ps";
	}
	elsif( $mode eq 'native' )
	{
		MakeGMgraph( $out_gm, $seqfile, "typical.ps"  );
		rename "typical.ps", $tmp_ps;
	}
	elsif( $mode eq 'heuristic' )
	{
		MakeGMgraph( $out_gm_heu, $seqfile, "atypical.ps"  );
		rename "atypical.ps", $tmp_ps;
	}

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

	my $gm_gcode_option = '';
	if ( $gcode )
	{
		$gm_gcode_option = " -c $gcode "
	}

	$command = "$genemark -gkfns  $gm_gcode_option  -m $mat  $symlink_name ";
	RunSystem( $command, "make gm graph: " );

	unlink  $symlink_name;
	unlink  $symlink_name .".lst";

	rename $symlink_name .".ps", $graph_name;
}
#-----------------------------------------------
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
	
	$hmm .= " -o  $output ";

	$hmm .=  " -D $fnn " if $fnn;
	$hmm .=  " -A $faa " if $faa;

	my $par = '';

	$par = $out_name_combined  if( $mode eq 'combined' );
	$par = $out_name           if( $mode eq 'native' );
	$par = $out_name_heuristic if( $mode eq 'heuristic' );
	
	print "$par\n" if $debug;
	
	$hmm .= " -m $par ";
	
	$motif = CheckSetMotif( $par );
	
	$hmm .= ' -r ' if $motif;

	$hmm .= " ". $seqfile;
}
# -----------------------------------------------
sub CheckSetMotif
{
	my ( $file ) = shift;
	
	if( $motif )
	{
		$motif = '';
		
		open( my $IN, $file ) or die;
		while( <$IN> )
		{
			if( /RBSM/ )
			{
				$motif = 1;
			}		
		}
		close $IN;
	}
	
	return $motif;
}
# -----------------------------------------------
sub CheckSetMode
{
	if( !$mode )
	{
		if    ( -e $out_name_combined )  { $mode = 'combined'; }
		elsif ( -e $out_name )           { $mode = 'native'; }
		elsif ( -e $out_name_heuristic ) { $mode = 'heuristic'; }
		else { print "error, parameter files not found: $out_name\n"; exit 1; }
	}
	
	if( $mode eq 'combined' or $mode eq 'native' or $mode eq 'heuristic' )
	{
		if( $mode eq 'combined' and ! -e $out_name_combined )
			{ print "error, required parameter files are not found: $out_name_combined\n"; exit 1; }
		elsif( $mode eq 'native' and ! -e $out_name )
			{ print "error, required parameter files are not found: $out_name\n"; exit 1; }
		elsif ( $mode eq 'heuristic' and ! -e $out_name_heuristic )
			{ print "error, required parameter files are not found: $out_name_heuristic\n"; exit 1; }
		
		if( $pdf or $ps )
		{
			if( $mode eq 'combined' )
			{
				if( ! -e $out_gm  or ! -e $out_gm_heu )
					{ print "error, required parameter files are not found: $out_gm $out_gm_heu\n"; exit 1; }
			}
			elsif( $mode eq 'native' )
			{
				if( ! -e $out_gm  )
					{ print "error, required parameter files are not found: $out_gm\n"; exit 1; }				
			}
			elsif( $mode eq 'heuristic' )
			{
				if( ! -e $out_gm_heu  )
					{ print "error, required parameter files are not found: $out_gm_heu\n"; exit 1; }				
			}
		}
	}
	else { print "error, unexpected mode was found: $mode\n"; exit 1; }
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
sub CheckInput_A
{
	my $OUTPUT_FORMAT = "LST|GFF";
	my $SHAPE_TYPE = "linear|circular|partial";
	my $STRAND = "direct|reverse|both";
	
	if ( $OUTPUT_FORMAT !~ /\b$format\b/ )   { print "Error: format [$format] is not supported\n"; exit 1; }
	if ( $SHAPE_TYPE !~ /\b$shape\b/ )       { print "Error: sequence organization [$shape] is not supported\n"; exit 1 ; }
	if ( $STRAND !~ /\b$strand\b/ )          { print "Error: strand [$strand] is not supported\n"; exit 1; }
	if ( ($motif ne '1')&&($motif ne '0') )  { print "Error: in value of motif option\n"; exit 1 ; }
}
# -----------------------------------------------
sub ParseCMD
{
	if ( !GetOptions
	  (
	    'org=s'       => \$org,
	    'mode=s'      => \$mode,
	    'output=s'    => \$output,
	    'name=s'      => \$name,
	    'shape=s'     => \$shape,
	    'motif=s'     => \$motif,
	    'strand=s'    => \$strand,
	    'format=s'    => \$format,
	    'ps'          => \$ps,
	    'pdf'         => \$pdf,
	    'faa'         => \$faa,
	    'fnn'         => \$fnn
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
--org       <string> path to directory with parameters
--mode      <string> select parameter type: combined, native or heuristic
--name      <string> file name of parameters file to use
            (default: '$out_name'; otherwise: '<name>$out_suffix')

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
--motif     <number> search for a sequence motif associated with CDS start
            (default: $motif; supported: 1 <true> and 0 <false>)
--strand    <string> sequence strand to predict genes in
            (default: '$strand'; supported: direct, reverse and both )
";
        return $text;
}
# -----------------------------------------------


