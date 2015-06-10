#!/usr/bin/perl
# =============================================================
# Copyright Georgia Institute of Technology, Atlanta, Georgia, USA
# Distributed by GeneProbe Inc., Atlanta, Georgia, USA
#
# GeneMarkS
#
# Besemer J., Lomsadze A. and Borodovsky M.
# Nucleic Acids Research, 2001, Vol. 29, No. 12, 2607-2618
# "GeneMarkS: a self-training method for prediction of
#  gene starts in microbial genomes. Implications for
#  finding sequence motifs in regulatory regions."
#
# John Besemer (version 1.*)
# Alex Lomsadze (version 4.*)
#
# Please report problems to Alex Lomsadze at alexl@gatech.edu
#
# This version combines the original 2001 prokaryotic GeneMarkS with later development 
# which extended the unsupervised gene prediction to sequences of intron-less eukaryotes, 
# eukaryotic viruses, phages and EST/cDNA.
# =============================================================

use strict;
use warnings;
use Getopt::Long;
use FindBin qw($RealBin);
use File::Spec;
use Cwd qw(abs_path cwd);

my $VERSION = "4.29 September 2014";
my $debug = 0;

# ------------------------------------------------
# installation settings; required directories and programs

# GeneMarkS installation directory
my $gms_dir = $RealBin;

# directory with configuration and shared files
my $shared_dir = $gms_dir;

# GeneMark.hmm gene finding program <gmhmmp>; version 3.25
my $hmm = File::Spec->catfile( $gms_dir, "gmhmmp" );

# sequence parsing tool <probuild>; version 2.27
my $build = File::Spec->catfile( $gms_dir, "probuild" );

# directopy with heuristic models for GeneMark.hmm; version 2.0
my $heu_dir_hmm = File::Spec->catdir( $shared_dir, "heuristic_mod" );

# directopy with heuristic models for GeneMark.hmm; version 2.5
my $heu_dir_hmm_2_5 = File::Spec->catdir( $shared_dir, "heuristic_mod_2_5" );

# directopy with MGM heuristic models for GeneMark.hmm; version 1.0
my $mgm_dir_hmm = File::Spec->catdir( $shared_dir, "mgm_mod" );
 
# MGM heuristic model in combined format, version 1.0
my $mgm_mod  = File::Spec->catfile( $shared_dir, "MetaGeneMark_v1.mod" );

# gibbs sampler version 1 - NCBI software; for details see README in gibbs9_95
my $gibbs = File::Spec->catfile( $gms_dir, "gibbs" );

# gibbs sampler version 3 - from http://bayesweb.wadsworth.org/gibbs/gibbs.html ; version 3.10.001  Aug 12 2009
my $gibbs3 = File::Spec->catfile( $gms_dir, "Gibbs3" );

# GeneMark gene finding program <gm>, version 2.5p
my $genemark = File::Spec->catfile( $gms_dir, "gm" );

#------------------------------------------------
# installation settings; optional directories and programs

# tool to make model files for GeneMark; version 1.5c
my $mkmat = File::Spec->catfile( $gms_dir, "mkmat" );

# directory with heuristic models for GeneMark; version 2.5p
my $heu_dir_gm = File::Spec->catdir( $shared_dir, "heuristic_mat" );

# codon table alteration for GeneMark
my $gm_4_tbl  = File::Spec->catfile( $shared_dir, "gm_4.tbl" );
my $gm_1_tbl  = File::Spec->catfile( $shared_dir, "gm_1.tbl" );
my $gm_25_tbl = File::Spec->catfile( $shared_dir, "gm_25.tbl" );

# script to overlay PS files
my $ps_gm_overlay = File::Spec->catfile( $gms_dir, "ps_gm_overlay.pl" );

#------------------------------------------------

my $cmd_line = join(" ", @ARGV );

# command line parameters

my $seqfile = '';

my $output = '';
my $name = '';
my $combine = '';
my $gm = '';
my $species = '';
my $clean = '';
my $work_dir = '';

my $order = 2;
my $order_non;            # if undef then = order
my $gcode = "11";
my $shape = "partial";
my $motif = "1";
my $width = 6;
my $prestart = 40;
my $identity = 0.99;
my $maxitr = 10;
my $fixmotif = '';
my $offover = '';
my $strand = "both";

my $prok = '';
my $euk = '';
my $virus = '';
my $est = '';
my $phage = '';

my $par = '';
my $imod = '';
my $test = '';
my $verbose = '';

my $format = "LST";
my $ps = '';
my $pdf = '';
my $faa = '';
my $fnn = '';

my $ext = '';
my $ncbi = '';

my $version = '';

# algorithm configuration
my $gibbs_version = 3;         # Specifies which version of gibbs to use
my $heuristic_version = 2;     # Specifies which version of heuristic to use

# "hard" coded constants
my $SHAPE_TYPE = "linear|circular|partial";
my $GENETIC_CODE = "11|4|1|25";
my $STRAND = "direct|reverse|both";
my $MIN_HEURISTIC_GC = 30;
my $MAX_HEURISTIC_GC = 70;
my $OUTPUT_FORMAT = "LST|GFF";

# "soft" coded constants
my $logfile  = "gms.log";
my $seq = "sequence";
my $start_prefix = "startseq.";
my $gibbs_prefix = "gibbs_out.";
my $mod_prefix = "itr_";
my $mod_suffix = ".mod";
my $hmmout_prefix = "itr_";
my $hmmout_suffix = ".lst";
my $out_name = "GeneMark_hmm.mod";
my $out_name_combined = "GeneMark_hmm_combined.mod";
my $out_name_heuristic = "GeneMark_hmm_heuristic.mod";
my $out_suffix = "_hmm.mod";
my $out_suffix_combined = "_hmm_combined.mod";
my $out_suffix_heu = "_hmm_heuristic.mod";

# GeneMark related
# GeneMarkS is not using GeneMark in training
# but can generate parameters for GeneMark
# or create graphical output

my $out_gm = "GeneMark.mat";
my $out_gm_suffix = "_gm.mat";
my $out_gm_heu = "GeneMark_heuristic.mat";
my $out_gm_heu_suffix = "_gm_heuristic.mat";
my $cod = "cod";
my $non = "non";
my $mkmat_comment = "mkmat_comment";
my $mkmat_order = 4;

my $mkmat_copyright = "-";
my $mkmat_author = "-";
my $mkmat_note = "Training set derived by GeneMarkS, $VERSION\n";

#------------------------------------------------
# support for installation key in shared directory

# $hmm = $hmm . " -T $shared_dir  ";

#------------------------------------------------
# get program name
my $gms = $0; $gms =~ s/.*\///;

if ( $#ARGV == -1 ) { print Usage(); exit 1; }

# parse command line
if ( !GetOptions
  (
    'output=s'    => \$output,
    'name=s'      => \$name,
    'order=i'     => \$order,
    'order_non=i' => \$order_non,
    'gcode=s'     => \$gcode,
    'shape=s'     => \$shape,
    'motif=s'     => \$motif,
    'width=i'     => \$width,
    'prestart=i'  => \$prestart,
    'identity=f'  => \$identity,
    'par=s'       => \$par,
    'imod=s'      => \$imod,
    'test'        => \$test,
    'verbose'     => \$verbose,
    'species=s'   => \$species,
    'euk'         => \$euk,
    'prok'        => \$prok,
    'virus'       => \$virus,
    'est'         => \$est,
    'phage'       => \$phage,
    'offover'     => \$offover,
    'clean'       => \$clean,
    'strand=s'    => \$strand,
    'fixmotif'    => \$fixmotif,
    'combine'     => \$combine,
    'out=s'       => \$work_dir,
    'maxitr=i'    => \$maxitr,
    'gm'          => \$gm,
    'format=s'    => \$format,
    'ps'          => \$ps,
    'pdf'         => \$pdf,
    'faa'         => \$faa,
    'fnn'         => \$fnn,
    'ext=s'       => \$ext,
    'gibbs=i'     => \$gibbs_version,
    'heuristic=i' => \$heuristic_version,
    'gm_order=i'  => \$mkmat_order,
    'ncbi'        => \$ncbi,
    'version'     => \$version,
    'debug'       => \$debug
  )
) { exit 1; }

$verbose = 1 if $debug; 

#------------------------------------------------
# parse/check input/settings

if ( $test ) { SelfTest(); exit 1; }
if ( $version ) { print "GeneMarkS version $VERSION\n"; exit 1; }

if ( $#ARGV == -1 ) { print "Error: sequence file name is missing\n"; exit 1; }
if ( $#ARGV > 0 )   { print "Error: more than one input sequence file was specified\n"; exit 1; }

$seqfile = shift @ARGV;
CheckFile( $seqfile, "er" )||exit 1;

CheckCombinedOptions();

if ( $prok )
{
  $combine = '1';
  $clean   = '1';
}

if ( $phage )
{
  $combine = '1';
  $clean   = '1';
}

if ( $euk )
{
  $gcode    = "1";
  $offover  = '1';
  $clean    = '1';
  $fixmotif = '1';
  $motif    =   1;
  $prestart =   6;
  $width    =  12;
  $order    =   4;
}

if ( $virus )
{
  $combine  = '1';
  $gcode    = "1";
  $clean    = '1';
  $fixmotif = '1';
  $motif    =   1;
  $prestart =   6;
  $width    =  12;
}

if ( $est )
{
  $gcode    = '1';
  $clean    = '1';
  $fixmotif = '1';
  $motif    =   1;
  $prestart =   6;
  $width    =  12;
  $order    =   4; 
}

# build model for GeneMark, if graphical output
$gm = 1 if $ps or $pdf;

# parse gcode from defline
$gcode = parse_deflines( $seqfile, $gcode ) if ( defined $ncbi );

# set noncoding order
$order_non = $order if( ! defined $order_non );

# check input foe allowed values
if ( $OUTPUT_FORMAT !~ /\b$format\b/ )  { print "Error: format [$format] is not supported\n"; exit 1; }
if ( $order !~ /^\d\d?$/ )              { print "Error: Markov chain order [$order] is not supported\n"; exit 1; }
if ( $order_non !~ /^\d$/ )             { print "Error: Markov chain order [$order_non] is not supported\n"; exit 1; }
if ( $GENETIC_CODE !~ /\b$gcode\b/ )    { print "Error: genetic code [$gcode] is not supported\n"; exit 1 ; }
if ( $SHAPE_TYPE !~ /\b$shape\b/ )      { print "Error: sequence organization [$shape] is not supported\n"; exit 1 ; }
if (($motif ne '1')&&($motif ne '0'))   { print "Error: in value of motif option\n"; exit 1 ; }
if ( $width !~ /^\d+$/ )                { print "Error: in value of motif width option\n"; exit 1; }
if ( $prestart !~ /^\d+$/ )             { print "Error: in value of prestart option\n"; exit 1; }
if (( $identity !~ /^\d?\.?\d+$/ )||( $identity > 1 ))  { print "Error: in value of identity option\n"; exit 1 ; }
if ( $maxitr !~ /^\d+$/ )               { print "Error: in value of maximum number of itteration\n"; exit 1; }
if ( $STRAND !~ /\b$strand\b/ )         { print "Error: strand [$strand] is not supported\n"; exit 1; }
if (($gibbs_version != 1)&&($gibbs_version != 3))   { print "Error: in specification of Gibbs version\n"; exit 1 ; }

# support of additional genetic codes may become too complecated in current structure of code
# change it in new realease, work-around for now:
my $input_genetic_code = $gcode;

# 25 has the same stop codons as 4, only protein translation differs
$gcode = 4 if ( $gcode == 25 ); 

if ( !$par )
{
	$par = GetParFileName( $gcode );
	$par = GetParFileName( "EST" ) if $est;
}
CheckFile( $par, "efr" )||exit 1;

if ( $imod )
{
  if ( $imod =~ /\s/ ) { print "Error: white space is not allowed in name: $imod\n"; exit 1; }
  CheckFile( $imod, "efr" )||exit 1;
}

if ( ! $output )
{
  if    ( $format eq "LST" ) { $output = File::Spec->splitpath( $seqfile ) .".lst";  }
  elsif ( $format eq "GFF" ) { $output = File::Spec->splitpath( $seqfile ) .".gff"; }

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

if ( $name )
{
  if ( $name =~ /\s/ ) { print "Error: white space is not allowed in name: $name\n"; exit 1; }

  $out_name           = $name . $out_suffix;
  $out_name_combined  = $name . $out_suffix_combined;
  $out_name_heuristic = $name . $out_suffix_heu;
  $out_gm             = $name . $out_gm_suffix;
  $out_gm_heu         = $name . $out_gm_heu_suffix;
}

if ( $species && $species =~ /\s/ ) { print "Error: white space is not allowed in species name: $species\n"; exit 1; }

if ( $order < 2 || $order > 5 && $combine )
  { print "Error: option 'combine' is supported only for orders 2-5\n"; exit 1; }

CheckFile( $work_dir, "dwrx" ) or exit 1;
chdir $work_dir;

my $combine_with = $imod;

PrintGlobalValues() if $debug;

#------------------------------------------------

# start/appand $logfile file
LogVariblesBeforeRun();

#------------------------------------------------
# more variables/settings

# use <probuild> with GeneMarkS parameter file <$par>
$build .= " --par $par ";

# if species name is provided, set it in a model file
$build .= " --NAME  $species " if $species;

# set options for <gmhmmp>

# switch gene overlap off in GeneMark.hmm; for eukaryotic intron-less genomes
$hmm = $hmm . " -p 0 " if $offover;

# information for protein translation
$hmm = $hmm ." -g $input_genetic_code ";

# set prediction for linear genome; no partial genes at the sequence ends
$hmm = $hmm . " -i 1 " if ( $shape eq "linear" );

# set strand to predict
if ( $strand eq "direct" )
 { $hmm = $hmm . " -s d "; }
elsif ( $strand eq "reverse" )
 { $hmm = $hmm . " -s r "; }

# to run system calls
my $command;

# iteration counter
my $itr;

# model name in iteration cycle
my $mod;

# print prediction to file
my $next;

# file with prediction from previous step
my $prev;

# file with pre start sequence
my $start_seq;

# file with gibbs results
my $gibbs_out;

# difference in consecutive prediction to compare with identity threshold
my $diff;

# list of all temp files
my @list_of_temp;

# sequence G+C content
my $GC = 0;

#------------------------------------------------
# prepare sequence

RunSystem( "$build --clean_join $seq --seq $seqfile --log $logfile", "prepare sequence\n" );
push @list_of_temp, $seq;

my $do_iterations = 1;

#------------------------------------------------
# check amount of seuqnce after cleaning
# if not enough for iterative training, then run heuristic only

$command = "grep MIN_SEQ_SIZE $par";
my $minimum_sequence_size = `$command`;
$minimum_sequence_size =~ s/\s*--MIN_SEQ_SIZE\s+//;

my $sequence_size = -s $seq;

if ( $sequence_size < $minimum_sequence_size )
{
  # this is work-around
  # re-run sequence build whithout cleaning
  # find GC of whole sequence end use it in selection of heuristic model
	
  RunSystem( "$build --clean_join $seq --seq $seqfile --log $logfile --MIN_CONTIG_SIZE 0 --GAP_FILLER ", "prepare sequence\n" );
  $do_iterations = 0;
}

Log( "do_iterations = $do_iterations\n" );

#------------------------------------------------
# determine model for initial <gmhmm> run

Log( "set initial <gmhmmp> model\n" );
if ( ! $imod )
{
  $command = "$build --gc --seq $seq";
  Log( "get GC\n" . $command . "\n" );
  $GC = `$command`;
  chomp($GC);
  $imod = GetHeuristicFileName( $GC, $gcode, $heu_dir_hmm, ".mod" );
  Log( "GC of $seq = $GC\n");
  
  if ( $heuristic_version eq "2")
  {
  	$combine_with = $imod;
  }
  elsif ( $heuristic_version eq "25")
  {
  	$combine_with = GetHeuristicFileName( $GC, $gcode, $heu_dir_hmm_2_5, ".mod", $order );
  }
  else { print "error, unsupported version of heusristic parameters was specified: $heuristic_version\n"; exit 1; }
}
CheckFile( $imod, "efr" )||exit(1);
Log( "initial <gmhmm> model: $imod\n" );

#------------------------------------------------
# copy initial <gmhmm> model into working directory

$itr = 0;
$mod = GetNameForMod( $itr );
RunSystem( "cp $imod  $mod", "copy initial model to working directory\n" );
push @list_of_temp, $mod;

#------------------------------------------------
# run initial prediction

$next = GetNameForNext( $itr );
RunSystem( "$hmm  $seq  -m $mod  -o $next", "run initial prediction\n" );
push @list_of_temp, $next;

#------------------------------------------------
# enter iterations loop

Log( "entering iteration loop\n" );

while( $do_iterations )
{
  $itr++;
  $mod  = GetNameForMod( $itr );

  if ( $motif && !$fixmotif )
  {
    $start_seq = $start_prefix . $itr;
    $gibbs_out = $gibbs_prefix . $itr;
  }

  $command = "$build --mkmod $mod --seq $seq --geneset $next --ORDM $order --order_non $order_non --revcomp_non 1";

  if ( $motif && !$fixmotif )
   { $command .= " --pre_start $start_seq --PRE_START_WIDTH $prestart"; }
  elsif ( $motif && $fixmotif )
   { $command .= " --fixmotif --PRE_START_WIDTH $prestart --width $width --log $logfile"; }

  RunSystem( $command, "build model: $mod for iteration: $itr\n" );
  push @list_of_temp, $mod;

  if ( $motif && !$fixmotif )
  {
    if ( $gibbs_version == 1 )
    {
    	RunSystem( "$gibbs $start_seq $width -n > $gibbs_out", "run gibbs sampler\n" );
    }
    elsif ( $gibbs_version == 3 )
    {
    	RunSystem( "$gibbs3 $start_seq $width -o $gibbs_out -F -Z  -n -r -y -x -m -s 1 -w 0.01", "run gibbs3 sampler\n" );
    }
    
    push @list_of_temp, $start_seq;

    RunSystem( "$build --gibbs $gibbs_out --mod $mod --seq $start_seq --log $logfile", "make prestart model\n" );
    push @list_of_temp, $gibbs_out;
  }

  $prev = $next;
  $next = GetNameForNext( $itr );

  $command = "$hmm  $seq  -m $mod  -o $next";
  $command .= " -r" if $motif;

  RunSystem( $command, "prediction, iteration: $itr\n" );
  push @list_of_temp, $next;

  $command = "$build --compare --source $next --target $prev";
  Log( "compare:\n" . $command . "\n" );

  $diff = `$command`;
  chomp( $diff );
  Log( "compare $prev and $next: $diff\n" );

  if ( $diff >= $identity )
    { Log( "Stopped iterations on identity: $diff\n" ); last; }
  if ( $itr == $maxitr )
    { Log( "Stopped iterations on maximum number: $maxitr\n" ); last; }
}
#------------------------------------------------
# create ouput
# NOTE: initial model can differ from heuristic if option "--imod" was used
# this will modify meaning of "combined" and "heuristic" models

if ( $do_iterations )
{
	RunSystem( "cp $mod $out_name", "output: $out_name\n" );

	if ( $combine  )
	{
		if ( $order < 2 || $order > 5 ) { print "Error: option 'combine' is supported only for orders 2-5\n"; exit 1; }

		RunSystem( "$build  --combine $out_name_combined  --first $mod  --second $combine_with", "Create combined model: $out_name_combined\n" );
	}
}

RunSystem( "cp $imod $out_name_heuristic", "output initial model: $out_name_heuristic\n" );


#------------------------------------------------
# create model for GeneMark
if ( $gm )
{
	if ( $do_iterations )
	{
		$command = "$build --parse --seq $seq --non $non --cod $cod --geneset $next --COD_L_MARGIN 3 --COD_R_MARGIN 3";
		RunSystem( $command, "parse sequence for GeneMark model building\n" );
		push @list_of_temp, $cod;
		push @list_of_temp, $non;

		Log( "create mkmat commnet file\n" );
		CreateMkMatCommentFile( $mkmat_comment);
		push @list_of_temp, $mkmat_comment;

		$command = "$mkmat -x $cod $non $mkmat_order $out_gm < $mkmat_comment";
		RunSystem( $command, "create parameter file for GeneMark: $out_gm\n" );
	}
  
	if ( !$do_iterations or $combine )
	{
		CopyHeuristicForGeneMark( $GC, $gcode );
	}

	CopyCodonTableForGeneMark( $gcode );
}
#------------------------------------------------
# predict genes using GeneMark.hmm
  
$hmm .= " -f G " if ( $format eq "GFF" );
$hmm .= " -c " if $ncbi;
$hmm .= " -e $ext -L $logfile " if $ext;
$hmm .= " -D $fnn " if $fnn;
$hmm .= " -A $faa " if $faa;
$hmm .= " -o $output ";
$hmm .= " $seqfile ";

if ( $do_iterations )
{
  if ( $motif )
  {
    # training with motif detection
    if ( $combine )
    {
       $command = "$hmm -r -m $out_name_combined ";
       RunSystem( $command, "predict genes using combined model with motif\n" );
    }
    else
    {
       $command = "$hmm -r -m $out_name ";
       RunSystem( $command, "predict genes using native model with motif\n" );
    }
  }
  else
  {
    # no moitf option specified
    if ( $combine )
    {
       $command = "$hmm -m $out_name_combined ";
       RunSystem( $command, "predict genes using combined model and no motif\n" );
    }
    else
    {
       $command = "$hmm -m $out_name ";
       RunSystem( $command, "predict genes using native model and no motif\n" );
    }
  }
}
else
{
   # no iterations - use heuristic only
   $command = "$hmm -m $out_name_heuristic ";
   RunSystem( $command, "predict genes using heuristic\n" );
}

#------------------------------------------------

if ($ps or $pdf)
{	
	my $tmp_ps = "gms.ps";
	
	if( $do_iterations and $combine )
	{
		MakeGMgraph( $out_gm, $seqfile, "typical.ps"  );
		push @list_of_temp,  "typical.ps";

		MakeGMgraph( $out_gm_heu, $seqfile, "atypical.ps"  );
		push @list_of_temp,  "atypical.ps";
		
		$command = "$ps_gm_overlay -outfile $tmp_ps  typical.ps  atypical.ps";
		RunSystem( $command, "overlay t and a graphs: " );
		push @list_of_temp, $tmp_ps;
		
	}
	elsif( !$combine and $do_iterations )
	{
		MakeGMgraph( $out_gm, $seqfile, "typical.ps"  );
		rename "typical.ps", $tmp_ps;
		push @list_of_temp, $tmp_ps;
	}
	elsif( ! $do_iterations )
	{
		MakeGMgraph( $out_gm_heu, $seqfile, "atypical.ps"  );
		rename "atypical.ps", $tmp_ps;
		push @list_of_temp, $tmp_ps;
	}
	else { print "error, check here\n"; exit 1; }

	my $tmp_ps_hmm = $tmp_ps ."_hmm";
	
	$command = "$ps_gm_overlay -int_file $output -outfile $tmp_ps_hmm  $tmp_ps";
	RunSystem( $command, "overlay hmm graphs: " );
	unlink $tmp_ps;
	rename $tmp_ps_hmm, $tmp_ps;

	system( "ps2pdf  $tmp_ps  $pdf ") if $pdf;
	rename $tmp_ps, $ps if $ps;
}

#------------------------------------------------
# clean temp, finish
RunSystem( "rm -f @list_of_temp" ) if $clean;

my $time = localtime();
Log( "End: $time\n\n" );

exit 0;


#=============================================================
# sub section:
#=============================================================
sub MakeGMgraph
{
	my( $mat, $sname, $graph_name ) = @_;

	my $symlink_name = File::Spec->splitpath( $sname ) ."_for_gm";

	if ( -e $symlink_name ) { unlink $symlink_name ;}

	if ( symlink $sname, $symlink_name )
		{;}
	else { print "error on symlink\n"; exit 1; }

	my $gm_gcode_option = '';

	if( $gcode == 4 )
	{
		$gm_gcode_option = ' -c '. $gm_4_tbl;
	}
	elsif( $gcode == 1 )
	{
		$gm_gcode_option = ' -c '. $gm_1_tbl;
	}
	elsif( $gcode == 25 )
	{
		$gm_gcode_option = ' -c '. $gm_25_tbl;
	}

	$command = "$genemark -gkfns  $gm_gcode_option  -m $mat  $symlink_name ";
	RunSystem( $command, "make gm graph: " );

	push @list_of_temp,  $symlink_name;
	push @list_of_temp,  $symlink_name .".lst";
	
	rename $symlink_name .".ps", $graph_name;
}
# -----------------------------------------------
# check if more than one combined option was specified 
# -----------------------------------------------
sub CheckCombinedOptions
{
	my $tmp_counter = 0;
	if ( $euk )   { ++$tmp_counter; }
	if ( $prok )  { ++$tmp_counter; }
	if ( $virus ) { ++$tmp_counter; }
	if ( $phage ) { ++$tmp_counter; }
	if ( $est )   { ++$tmp_counter; }
	if ($tmp_counter > 1)
	{
		print "Error: more than one combined optional parameter was specified\n";
		exit 1;
	}
}
#-----------------------------------------------
# $heu_dir_gm $out_gm_heu
# ".mat"
#-----------------------------------------------
sub CopyHeuristicForGeneMark
{
  my ( $GC, $code ) = @_;
  my $gm_heu_name = GetHeuristicFileName( $GC, $code, $heu_dir_gm, ".mat" );
  CheckFile( $gm_heu_name, "efr" )||exit(1);
  RunSystem( "cp $gm_heu_name $out_gm_heu", "copy Heuristic matrix $gm_heu_name for GeneMark\n" );
}
#-----------------------------------------------
# $gm_1_tbl $gm_4_tbl
#-----------------------------------------------
sub CopyCodonTableForGeneMark
{
  my ( $code ) = @_;
  my $table_name = "";

  if ( $code eq "1" )
  {
    $table_name = $gm_1_tbl;
    $table_name =~ s/.*\///;
    RunSystem( "cat $gm_1_tbl > $table_name", "copy codon table file $table_name for GeneMark $code\n" );
  }
  elsif ( $code eq "4" )
  {
    $table_name = $gm_4_tbl;
    $table_name =~ s/.*\///;
    RunSystem( "cat $gm_4_tbl > $table_name", "copy codon table file $table_name for GeneMark $code\n" );
  }
}
#-----------------------------------------------
# $hmmout_prefix $hmmout_suffix
#-----------------------------------------------
sub GetNameForNext
{
  my ( $name ) = @_;
  return  $hmmout_prefix . $name . $hmmout_suffix;
}
#-----------------------------------------------
# mod_prefix $mod_suffix
#-----------------------------------------------
sub GetNameForMod
{
  my ( $name ) = @_;
  return  $mod_prefix . $name . $mod_suffix;
}
#-----------------------------------------------
sub RunSystem
{
  my( $com, $text ) = @_;
  if ( $text ) { Log( $text ); }
  Log( $com . "\n" );
  my $err_code = 0;
  if ( $err_code = system( $com ) )
  {
    Log( "Error on last system call, error code $err_code\nAbort program!!!\n" );
    print "GeneMarkS: error on last system call, error code $err_code\nAbort program!!!\n";
    exit(1);
  }
  else
   { Log( "system call done\n" ); }
}
#-----------------------------------------------
# $verbose $logfile
#-----------------------------------------------
sub Log
{
  my( $text ) = @_;
  $verbose && print $text;
  open( FILE, ">>$logfile" )||die( "$!, $logfile," );
  print FILE $text;
  close FILE;
}
#-----------------------------------------------
# $shared_dir
# "/par_" ".default"
#-----------------------------------------------
sub GetParFileName
{
  my( $code ) = @_;
  return File::Spec->catfile( $shared_dir, "par_" . $code . ".default" );
}
#------------------------------------------------
# $MIN_HEURISTIC_GC $MAX_HEURISTIC_GC
# "/heu_" "_"
#------------------------------------------------
sub GetHeuristicFileName
{
  my( $GC, $code, $dir, $ext, $ord ) = @_;
  $GC = int $GC;

  if( $GC < $MIN_HEURISTIC_GC ) { $GC = $MIN_HEURISTIC_GC; }
  if( $GC > $MAX_HEURISTIC_GC ) { $GC = $MAX_HEURISTIC_GC; }

  if ( defined $ord )
  {
  	return $dir ."/heu_" . $code . "_" . $GC . "_" . $ord . $ext;
  }
  else
  {
    return $dir ."/heu_" . $code . "_" . $GC . $ext;
  }
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
      { ( -f $name )||( print("error, not a file: $name\n"), $result = 0 ); }
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
#------------------------------------------------
sub SelfTest
{
  print "installation test ...\n";
  SelfTestRequired();
  SelfTestOptional();
  print "done\n";
}
#------------------------------------------------
# $gms_dir $heu_dir_hmm $hmm $build $gibbs
# $GENETIC_CODE $MIN_HEURISTIC_GC $MAX_HEURISTIC_GC
# ".mod"
#------------------------------------------------
sub SelfTestRequired
{
	 print "Testing required files ...\n";
	 
	CheckFile( $gms_dir, "edrx" );
	CheckFile( $shared_dir, "edr" );
	CheckFile( $heu_dir_hmm, "edr" );
	CheckFile( $hmm , "efx");
	CheckFile( $build, "efx" );
	CheckFile( $gibbs, "efx" );
	CheckFile( $gibbs3, "efx" );

  my @array = split( /\|/, $GENETIC_CODE );
  my $code  = '';
  my $GC = 0;

  foreach $code ( @array )
    { CheckFile( GetParFileName($code), "efr" ); }

  # test files for GeneMark.hmm  ".mod"
  foreach $code ( @array )
  {
    $GC = $MIN_HEURISTIC_GC;
    while( $GC <= $MAX_HEURISTIC_GC )
    {
      CheckFile( GetHeuristicFileName( $GC, $code, $heu_dir_hmm, ".mod" ), "efr" );
      $GC++;
    }
  }
}
#------------------------------------------------
# $heu_dir_gm $mkmat $gm_1_tbl $gm_4_tbl
# $GENETIC_CODE $MIN_HEURISTIC_GC $MAX_HEURISTIC_GC
# ".mat"
#------------------------------------------------
sub SelfTestOptional
{
	 print "Testing required files ...\n";
	
	CheckFile( $heu_dir_hmm_2_5, "edrx" );
	
	# new features, keep in optional yet
	CheckFile( $mgm_dir_hmm,     "edrx" );
	CheckFile( $mgm_mod,         "efr" );
	
	CheckFile( $heu_dir_gm, "edrx" );
	CheckFile( $mkmat, "efx" );
	CheckFile( $gm_1_tbl, "efr" );
	CheckFile( $gm_4_tbl, "efr" );

  my @array = split( /\|/, $GENETIC_CODE );
  my $code  = '';
  my $GC = 0;

  foreach $code ( @array )
  {
    $GC = $MIN_HEURISTIC_GC;
    while( $GC <= $MAX_HEURISTIC_GC )
    {
      CheckFile( GetHeuristicFileName( $GC, $code, $heu_dir_gm, ".mat" ), "efr" );
      $GC++;
    }
  }
}
#------------------------------------------------
# $species $mkmat_author $mkmat_copyright $mkmat_note
#------------------------------------------------
sub CreateMkMatCommentFile
{
  my ( $name ) = @_;
  my $mat_name = "-";
  if ( $species )
    { $mat_name = $species; }

  open( OUT, ">$name" )||die( "$!, $name, " );
  print OUT "$mat_name\n";
  print OUT $mkmat_author . "\n";
  print OUT $mkmat_copyright . "\n";
  print OUT $mkmat_note;
  print OUT localtime() . "\n";
  print OUT ".\n";
  close OUT;
}
#------------------------------------------------
sub ParseProteinsFromGFF
{
  my ( $source, $destination ) = @_;

  open( IN, $source ) or  die "$!\n";
  open( OUT, ">$destination" ) or die "$!\n";
  
  my $aa = 0;

  while(<IN>)
  {
    if ( /^\#\#Protein\s+(\d+)\s*/ )
    {
       $aa = 1;
       print OUT ">gene_id_$1\n";
       next;
    }

    if ( /^\#\#end-Protein/ )
    {
      $aa = 0;
      next;
    }

    if ( $aa && /^\#\#(\w+)\s*/ )
    {
      print OUT "$1\n";
    }
  }

  close IN;
  close OUT;
}
#------------------------------------------------
sub ParseNucleotidesFromGFF
{
  my ( $source, $destination ) = @_;

  open( IN, $source ) or die "$!\n";
  open( OUT, ">$destination" ) or die "$!\n";
  
  my $nt = 0;

  while(<IN>)
  {
    if ( /^\#\#DNA\s+(\d+)\s*/ )
    {
       $nt = 1;
       print OUT ">gene_id_$1\n";
       next;
    }

    if ( /^\#\#end-DNA/ )
    {
      $nt = 0;
      next;
    }

    if ( $nt && /^\#\#(\w+)\s*/ )
    {
      print OUT "$1\n";
    }
  }

  close IN;
  close OUT;
}
#------------------------------------------------
sub ParseProteinsFromLST
{
  my ( $source, $destination ) = @_;

  open( IN, $source ) or die "$!\n";
  open( OUT, ">$destination" ) or die "$!\n";
  
  my $aa = 0;

  while(<IN>)
  {
    if ( /^.gene_\d+\|GeneMark.hmm\|\d+_aa\|/ )
    {
       $aa = 1;
       print OUT $_;
       next;
    }

    if ( /^\s*$/ )
    {
      $aa = 0;
      next;
    }

    if ( $aa )
    {
      print OUT $_;
    }
  }

  close IN;
  close OUT;
}
#------------------------------------------------
sub ParseNucleotidesFromLST
{
  my ( $source, $destination ) = @_;

  open( IN, $source ) or die "$!\n";
  open( OUT, ">$destination" ) or die "$!\n";
  
  my $nt = 0;

  while(<IN>)
  {
    if ( /^.gene_\d+\|GeneMark.hmm\|\d+_nt\|/ )
    {
       $nt = 1;
       print OUT $_;
       next;
    }

    if ( /^\s*$/ )
    {
      $nt = 0;
      next;
    }

    if ( $nt )
    {
      print OUT $_;
    }
  }

  close IN;
  close OUT;
}

#------------------------------------------------
sub parse_deflines
{
	my ( $name, $gcode ) = @_;
	
	my %h;

	open( IN , $name ) or die "Error on open file $name: $!\n";
	while( my $line = <IN>)
	{
		next if ( $line !~ /^\s*>/ );

		%h = $line =~ m/\[\s*(\S+)\s*=\s*(\S.*?)s*\]/g;
		
		if ( exists $h{ 'gcode' } )
		{
			if ( defined $gcode && ( $gcode ne $h{ 'gcode' } ))
			{
				die "Error: different genetic codes are specified for the same input: |$gcode|$h{ 'gcode' }|\n";
			}
			else
			{
				$gcode = $h{ 'gcode' };
			}
		}
	}
	close IN;

	return $gcode;
}
# -----------------------------------------------
sub LogVariblesBeforeRun
{
        my $time = localtime();
        my $logline =
"\n\n
-------------------------------------------------
start time            : $time
GMS version           : $VERSION
working directory     : $work_dir
command line          : $cmd_line
output file with predictions : $output
input sequence        : $seqfile
output name for model : $name
combine model         : $combine
GeneMark model        : $gm
species name          : $species
delete temp files     : $clean
markov chain order    : $order
non-coding order      : $order_non
genetic code          : $input_genetic_code
sequence organization : $shape
search for motif      : $motif
motif width           : $width
prestart length       : $prestart
identity threshold    : $identity
maximum iteration     : $maxitr
fixed motif position  : $fixmotif
gene overlap off      : $offover
strand to predict on  : $strand
mode prokaryotic      : $prok
mode eukaryotic       : $euk
mode virus            : $virus
mode phage            : $phage
mode est              : $est
GeneMarkS parameters  : $par
initial hmm model     : $imod
output format         : $format
output PostScript     : $ps
output PDF            : $pdf
output nucleotides    : $fnn
output proteins       : $faa
gibbs version         : $gibbs_version
heuristic version     : $heuristic_version
evidense data file    : $ext
combine native with   : $combine_with
use input defline     : $ncbi

      run starts here:
";

        Log($logline);
}
# -----------------------------------------------
sub PrintGlobalValues
{
        # Developer sub, print values for all grobal variables

        print "VERSION    $VERSION\n";
        print "gms_dir    $gms_dir\n";
        print "shared_dir $shared_dir\n";
        print "hmm        $hmm\n";
        print "build      $build\n";
        print "heu_dir_hmm     $heu_dir_hmm\n";
        print "heu_dir_hmm_2_5 $heu_dir_hmm_2_5\n";
        print "mgm_dir_hmm     $mgm_dir_hmm\n";
        print "mgm_mod    $mgm_mod\n";
        print "gibbs      $gibbs\n";
        print "gibbs3     $gibbs3\n";
		print "genemark   $genemark\n";
        print "mkmat      $mkmat\n";
        print "heu_dir_gm      $heu_dir_gm\n";
        print "gm_4_tbl   $gm_4_tbl\n";
        print "gm_1_tbl   $gm_1_tbl\n";
        print "cmd        $cmd_line\n";
        print "output     $output\n";
        print "seqfile    $seqfile\n";
        print "name       $name\n";
        print "combine    $combine\n";
        print "gm         $gm\n";
        print "species    $species\n";
        print "clean      $clean\n";
        print "order      $order\n";
        print "order_non  $order_non\n";
        print "gcode      $gcode\n";
        print "shape      $shape\n";
        print "motif      $motif\n";
        print "width      $width\n";
        print "prestart   $prestart\n";
        print "identity   $identity\n";
        print "maxitr     $maxitr\n";
        print "fixmotif   $fixmotif\n";
        print "offover    $offover\n";
        print "strand     $strand\n";
        print "prok       $prok\n";
        print "euk        $euk\n";
        print "virus      $virus\n";
        print "est        $est\n";
        print "phage      $phage\n";
        print "par        $par\n";
        print "imod       $imod\n";
        print "test       $test\n";
        print "verbose    $verbose\n";
        print "format     $format\n";
        print "ps         $ps\n";
        print "pdf        $pdf\n";
        print "faa        $faa\n";
        print "fnn        $fnn\n";
        print "ext        $ext\n";
        print "ncbi       $ncbi\n";
        print "gibbs_version       $gibbs_version\n";
        print "heuristic_version   $heuristic_version\n";
        print "version    $version\n";
        print "SHAPE_TYPE      $SHAPE_TYPE\n";
        print "GENETIC_CODE    $GENETIC_CODE\n";
        print "STRAND          $STRAND\n";
        print "MIN_HEURISTIC_GC    $MIN_HEURISTIC_GC\n";
        print "MAX_HEURISTIC_GC    $MAX_HEURISTIC_GC\n";
        print "OUTPUT_FORMAT       $OUTPUT_FORMAT\n";
        print "logfile       $logfile\n";
        print "seq           $seq\n";
        print "start_prefix  $start_prefix\n";
        print "gibbs_prefix  $gibbs_prefix\n";
        print "mod_prefix    $mod_prefix\n";
        print "mod_suffix    $mod_suffix\n";
        print "hmmout_prefix $hmmout_prefix\n";
        print "hmmout_suffix $hmmout_suffix\n";
        print "out_name      $out_name\n";
        print "out_name_combined    $out_name_combined\n";
        print "out_name_heuristic   $out_name_heuristic\n";
        print "$out_suffix   $out_suffix\n";
        print "out_suffix_combined  $out_suffix_combined\n";
        print "out_suffix_heu       $out_suffix_heu\n";
        print "out_gm          $out_gm\n";
        print "out_gm_suffix   $out_gm_suffix\n";
        print "out_gm_heu      $out_gm_heu\n";
        print "out_gm_heu_suffix   $out_gm_heu_suffix\n";
        print "cod             $cod\n";
        print "non             $non\n";
        print "mkmat_comment   $mkmat_comment\n";
        print "mkmat_order     $mkmat_order\n";
        print "mkmat_copyright $mkmat_copyright\n";
        print "mkmat_author    $mkmat_author\n";
        print "mkmat_note      $mkmat_note\n";
        print "gms             $gms\n";
        if (defined $work_dir){print "work_dir        $work_dir\n";}
        else                  {print "work_dir        undefined\n";}
        if (defined $combine_with){print "combine_with    $combine_with\n";}
        else                      {print "combine_with    undefined\n";}

}
# -----------------------------------------------
sub Usage
{
my $text =
"
GeneMarkS  version $VERSION
Usage: $gms [options] <sequence file name>

Input sequence in FASTA format

Output options:
(output is in current working directory)

--format    <string> output coordinates of predicted genes in this format
            (default: $format; supported: LST and GFF)
--output    <string> output file with predicted gene coordinates by GeneMarh.hmm using parameters derived by GeneMarkS.
            (default: <sequence file name>.lst)
            GeneMark.hmm can be executed independently after finishing GeneMarkS training.
            This method may be the preferable option in some situations, as it provides accesses to GeneMarh.hmm options.
--name      <string> file name of output model file generated for GeneMark.hmm
            (default: '$out_name'; otherwise: '<name>$out_suffix')
--gm        generate model file for GeneMark
            (default name of output: '$out_gm'
            if option --name is specified then the name of the output is: '<name>$out_gm_suffix')
--species   <string> name of a species inside model file
            (default: is set in --par file; no white space in the name!)
--clean     delete all temporary files
            (if not specified: keep temporary files in current working directory)
--fnn       create file with nucleotide sequence of predicted genes
--faa       create file with protein sequence of predicted genes

--ps        create GeneMark graphical output in PostScript format
--pdf       create GeneMark graphical output in PDF format
			
			Attention, graphical output works only if input FASTA with one sequence in file

Run options:

--combine   combine the GeneMarkS generated (native) and Heuristic model parameters into one integrated model
            (default name of output: '$out_name_combined'
            if option --name is specified then the name of the output is: '<name>$out_suffix_combined')
--order     <number> markov chain order
            (default: $order; supported in range: >= 0)
--order_non <number> order for non-coding parameters
            (default: is the same as the order of coding model)
--gm_order  <number> order for GeneMark model, if option --gm was specified
            (default: $mkmat_order)
--gcode     <number> genetic code
            (default: $gcode; supported: 11, 4, 25 and 1)
--shape     <string> sequence organization
            (default: $shape; supported: linear, circular and partial)
--motif     <number> iterative search for a sequence motif associated with CDS start
            (default: $motif; supported: 1 <true> and 0 <false>)
--width     <number> motif width
            (default: $width; supported in range: >= 3)
--prestart  <number> length of sequence upstream of translation initiation site that presumably includes the motif
            (default: $prestart; supported in range: >= 0)
--identity  <number> identity level assigned for termination of iterations
            (default: $identity; supported in range: >=0 and <= 1)
--maxitr    <number> maximum number of iterations
            (default: $maxitr; supported in range: >= 1)
--fixmotif  option indicating that the motif is located at a fixed position with regard to the start; motif could overlap start codon
            (if this option is on, it changes the meaning of --prestart option (see example below) which in this case will define the
            distance from start codon to motif start)
--offover   prohibits gene overlap
            (if not specified: overlaps are allowed)
--strand    <string> sequence strand to predict genes in
            (default: '$strand'; supported: direct, reverse and both )
--ext       <filename> name of the file with external information in GFF format (PLUS mode of GMS)
--ncbi      parse genetic code and shape of sequence from definition line of input FASTA file

Combined output and run options:

--prok      to run program on prokaryotic sequence
            (this option is the same as:  --combine --clean --gm )
--euk       to run program on eukaryotic intron-less sequence (i.e. low eukaryote)
            (this option is the same as:  --offover --gcode 1 --clean --fixmotif --prestart 6 --width 12 --order 4 --gm)
--virus     to run program on a eukaryotic viral genome
            (this option is the same as:  --combine --gcode 1 --clean --fixmotif --prestart 6 --width 12 --gm)
--est       to run program on EST sequence
            (this option is the same as:  --par par_EST.default --clean --motif 0 --order 4)
--phage     to run program on phage sequence
            (this option is the same as:  --combine --clean --gm )

Test/developer options:

--par      <file name> custom parameters for GeneMarkS
           (default is selected based on gcode value: 'par_<gcode>.default' )
--imod     <file name> custom initiation model for GeneMarkS
           (default: heuristic model derived from GC composition of input sequence
           if option --combine is specified, custom model will be combined with GeneMarkS )
--gibbs    <number> version of Gibbs sampler software
           (default: $gibbs_version; supported versions: 1 and 3 ) 
--test     installation test
--ncbi     parse genetic code and shape of sequence from definition line of input FASTA file
--version  print version of GMS program
--verbose

";

	return $text;
}
# -----------------------------------------------
