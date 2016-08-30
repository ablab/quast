#!/usr/bin/perl
# -----------------------------------------
# Alex Lomsadze
#
# run GeneMark with parameters from GeneMarkS training
# or non-GeneMarkS training, but formatted GMS style
#
# This code is modified of GeneMarkS prediction step,
# designed to run with pre-build parameter files outside of GeneMarkS.
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
my $gm = File::Spec->catfile( $gmsuite_dir, "gm" );        # GeneMark gene finding program <gm>, version 2.5p for graph 
my $genemark = File::Spec->catfile( $gmsuite_dir, "gm" );  # use this for printing graph
                                                           # program runs seperatly for graphing
my $ps_gm_overlay = File::Spec->catfile( $gmsuite_dir, "ps_gm_overlay.pl" );  # script to overlay PS files

# ------------------------------------------------
# one of these 3 prediction modes is executed
# if running mode is specified, then force it
# if not, then check for set of available models in the order, as it is specified below

my $mode = '' ;

$mode = 'combine';    # native and heuristic combined
$mode = 'native';     # native only
$mode = 'heuristic';  # heuristic only

my $gm_gcode_option = '';

# ------------------------------------------------
# command line parameters

# not in GMS

my $org = '.';              # directory with species specific parameters
$mode = '';

my $windowsize = 0;
my $stepsize   = 0;
my $threshold  = 0;
my $rbs = '';

# shared with GMS

my $seqfile = '';

my $output = '';            # predicted gene coordinates are here
my $name = '';              # optinal prefix in name of species specific pararameter files; default GeneMark
my $shape = "partial";
my $strand = "both";
my $ps = '';
my $pdf = '';
my $faa = '';
my $fnn = '';

# ------------------------------------------------
# Set expected names for parameter files

my $out_gm = "GeneMark.mat";
my $out_gm_heu = "GeneMark_heuristic.mat";

my $out_gm_suffix = "_gm.mat";
my $out_gm_heu_suffix = "_gm_heuristic.mat";

if( $name )
{
	if ( $name =~ /\s/ ) { print "Error: white space is not allowed in name: $name\n"; exit 1; }

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

my $command = SetCmd( $gm );

print "$command\n" if $debug;

RunSystem( $command, "predict genes\n" );

if( -e "$seqfile.lst" )
{
	rename "$seqfile.lst",  $output;
}

ParseAA( "$seqfile.orf", $faa ) if $faa;
ParseNT( "$seqfile.orf", $fnn ) if $fnn;

unlink "$seqfile.orf";

GetPS_PDF() if ($ps or $pdf);

exit 0;

# ================================================
sub ParseAA
{
	my( $in, $out ) = @_;
	
	my $print_out = 0;
	
	open( my $IN, $in ) or die"error, file not found: $in";
	open( my $OUT, ">$out" ) or die"error, file not found: $out";
	while( my $line = <$IN> )
	{
		if( $line =~ /^; Protein/ )
		{
			$print_out = 1;
		}
		elsif( $line =~ /^; / )
		{
			$print_out = 0;
		}
		elsif( $print_out )
		{
			if ($line =~ /^\s*$/ ){next;}
			
			print $OUT $line;
		}
	}
	close $OUT;
	close $IN;
} 
#------------------------------------------------
sub ParseNT
{
	my( $in, $out ) = @_;
	
	my $print_out = 0;
	
	open( my $IN, $in ) or die"error, file not found: $in";
	open( my $OUT, ">$out" ) or die"error, file not found: $out";
	while( my $line = <$IN> )
	{	
		if( $line =~ /^; Nucleotide/ )
		{
			$print_out = 1;
		}
		elsif( $line =~ /^; / )
		{
			$print_out = 0;
		}
		elsif( $print_out )
		{
			if ($line =~ /^\s*$/ ){next;}
			
			print $OUT $line;
		}
	}
	close $OUT;
	close $IN;
}
#------------------------------------------------
sub SetParamaterFileNames
{
	my( $d ) = shift;
	
	if( ! -e $d ) { print "error, required directory not found: $d\n"; exit 1; }
	
	$out_gm             = File::Spec->catfile( $d, $out_gm );
	$out_gm_heu         = File::Spec->catfile( $d, $out_gm_heu );
	
	my $gcode = '';
	
	$gcode = 4  if ( -e File::Spec->catfile( $org, "gm_4_tbl" ) );
	$gcode = 11 if ( -e File::Spec->catfile( $org, "gm_1_tbl" ) );
	$gcode = 25 if ( -e File::Spec->catfile( $org, "gm_25_tbl" ) );	
	
	if( $gcode )
	{
		$gm_gcode_option = File::Spec->catfile( $org, "gm_". $gcode ."_tbl" )
	}
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

	$command = "$genemark -gkfns  $gm_gcode_option  -w $windowsize  -s $stepsize  -t $threshold  -m $mat  $symlink_name ";
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
sub SetCmd
{
	my $run = shift;
	
	$run .= " -w $windowsize  -s $stepsize  -t $threshold ";

	$run .=  " -on " if $fnn;
	$run .=  " -op " if $faa;

	if ( $rbs eq 'none' )
		{ ; }
	elsif ( $rbs eq ".coli" )
	{
#		$run .= " -R ecoli.rbs ";
	}

	my $par = '';

	$par = $out_gm     if( $mode eq 'combined' );
	$par = $out_gm     if( $mode eq 'native' );
	$par = $out_gm_heu if( $mode eq 'heuristic' );
	
	print "$par\n" if $debug;
	
	$run .= " -m $par ";
	$run .= " ". $seqfile;
	
	return $run;
}
# -----------------------------------------------
sub CheckSetMode
{
	if( !$mode )
    {
		if    ( -e $out_gm )     { $mode = 'combined'; }
		elsif ( -e $out_gm )     { $mode = 'native'; }
		elsif ( -e $out_gm_heu ) { $mode = 'heuristic'; }
		else { print "error, parameter files not found\n"; exit 1; }
	}
	

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
	else { print "error, unexpected mode was found: $mode\n"; exit 1; }
}
# -----------------------------------------------
sub SetOutputNames
{
	if( ! $output )
	{
		$output = File::Spec->splitpath( $seqfile ) .".lst";
	
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
	my $SHAPE_TYPE = "linear|circular|partial";
	my $STRAND = "direct|reverse|both";
	
	if ( $SHAPE_TYPE !~ /\b$shape\b/ )       { print "Error: sequence organization [$shape] is not supported\n"; exit 1 ; }
	if ( $STRAND !~ /\b$strand\b/ )          { print "Error: strand [$strand] is not supported\n"; exit 1; }
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
	    'strand=s'    => \$strand,
	    'ps'          => \$ps,
	    'pdf'         => \$pdf,
	    'faa'         => \$faa,
	    'fnn'         => \$fnn,
	    'windowsize=i' => \$windowsize,
	    'stepsize=i'   => \$stepsize,
	    'threshold=f'  => \$threshold,
	    'rbs=s'        => \$rbs,
	    'debug'        => \$debug
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

--windowsize <number>
--stepsize   <number>
--threshold  <number>
--rbs        <string>

Output options:
(output is in current working directory)
--output    <string> output file with predicted gene coordinates by GeneMark
            (default: <sequence file name>.lst)
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
";
        return $text;
}
# -----------------------------------------------


