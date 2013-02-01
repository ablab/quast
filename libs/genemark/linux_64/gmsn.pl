#!/usr/bin/perl -w
#=============================================================
# Copyright Georgia Institute of Technology, Atlanta, Georgia, USA
# Distributed by GeneProbe Inc., Atlanta, Georgia, USA
#
# GeneMarkS version 4.7a (January 2011)
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
# please report problems to
# Alex Lomsadze at alexl@amber.gatech.edu
#
# This program combines the original 2001 prokaryotic
# version of GeneMarkS with later extensions to intron-less
# eukaryotes and viruses.
#
# NOTE: several options are switched off in this version
# Option "--combine" for orders other than "2"
#=============================================================

use strict;
use Getopt::Long;
use FindBin qw($RealBin);
use File::Spec;

my $VERSION = "4.7b";

#------------------------------------------------
# installation settings; required directories/programs

# GeneMarkS installation directory
my $gms_dir = $RealBin;

# directory with configuration and shared files
# my $shared_dir = "";
my $shared_dir = $gms_dir;

# GeneMark.hmm gene finding program <gmhmmp>; version 2.8
my $hmm = $gms_dir . "/gmhmmp";

# sequence parsing tool <probuild>; version 2.10
my $build = $gms_dir . "/probuild";

# directopy with heuristic models for GeneMark.hmm; version 2.0
my $heu_dir_hmm = $shared_dir . "/heuristic_mod";

# gibbs sampler - NCBI software; for details see README in gibbs9_95
my $gibbs = $gms_dir . "/gibbs";

#------------------------------------------------
# installation settings; optional directories/programs

# tool to make model files for GeneMark; version 1.5a
my $mkmat = $gms_dir ."/mkmat";

# directory with heuristic models for GeneMark; version 2.0
my $heu_dir_gm = $shared_dir . "/heuristic_mat";

# codon table alteration for GeneMark
my $gm_4_tbl = $shared_dir . "/gm_4.tbl";
my $gm_1_tbl = $shared_dir . "/gm_1.tbl";

#------------------------------------------------
# command line parameters

my $output = '';

my $seqfile = '';

my $name = '';
my $combine = '';
my $gm = '';
my $species = '';
my $clean = '';

my $order = 2;
my $gcode = "11";
my $shape = "partial";
my $motif = "1";
my $width = 6;
my $prestart = 26;
my $identity = 0.99;
my $maxitr = 10;
my $fixmotif = '';
my $offover = '';
my $strand = "both";

my $prok = '';
my $euk = '';
my $virus = '';
my $est = '';

my $par = '';
my $imod = '';
my $test = '';
my $verbose = '';

# "hard coded constants"
my $SHAPE_TYPE = "linear|circular|partial";
my $GENETIC_CODE = "11|4|1";
my $STRAND = "direct|reverse|both";
my $MIN_HEURISTIC_GC = 30;
my $MAX_HEURISTIC_GC = 70;

# "soft coded constants"
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
# This training procedure doesn't use GeneMark,
# but can generate parameters file for GeneMark
my $out_gm = "GeneMark.mat";
my $out_gm_suffix = "_gm.mat";
my $out_gm_heu = "GeneMark_heuristic.mat";
my $out_gm_heu_suffix = "_gm_heuristic.mat";
my $cod = "cod";
my $non = "non";
my $mkmat_comment = "mkmat_comment";
my $mkmat_order = 2;

my $mkmat_copyright = "-";
my $mkmat_author = "-";
my $mkmat_note = "Training set derived by GeneMarkS, $VERSION\n";

#------------------------------------------------
# get program name
my $gms = $0; $gms =~ s/.*\///;

my $usage =
"GeneMarkS  version $VERSION
Usage: $gms [options] <sequence file name>

input sequence in FASTA format

Output options:
(output is in current working directory)

--output    <string> output file with predicted gene coordinates by GeneMarh.hmm
            and species parameters derived by GeneMarkS.
            (default: <sequence file name>.lst)
 
            GeneMark.hmm can be executed independently after finishing GeneMarkS training.
            This method may be the preferable option in many situations, as it provides accesses to GeneMarh.hmm options.

--name      <string> name of output model file generated for GeneMark.hmm
            (default: '$out_name'; otherwise: '<name>$out_suffix')
--combine   combine the GeneMarkS generated (native) and Heuristic model parameters into one integrated model
            (default name of output: '$out_name_combined'
            if option --name is specified then the name of the output is: '<name>$out_suffix_combined')
--gm        generate model file for GeneMark
            (default name of output: '$out_gm'
            if option --name is specified then the name of the output is: '<name>$out_gm_suffix')
--species   <string> name of a species in a model file
            (default: is set in --par file; no white space in the name!)
--clean     delete all temporary files
            (if not specified: keep temporary files in current working directory)

Run options:

--order     <number> markov chain order
            (default: $order; supported in range: >= 0)
--gcode     <number> genetic code
            (default: $gcode; supported: 11, 4 and 1)
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

Combined output and run options:

--prok      to run program on prokaryotic sequence or phage with building models for both GeneMark and GeneMark.hmm
            (this option is the same as:  --combine --clean --gm )
--euk       to run program on eukaryotic intron-less sequence (i.e. low eukaryote)
            (this option is the same as:  --offover --gcode 1 --clean --fixmotif --prestart 6 --width 12 --order 4 --gm)
--virus     to run program on a eukaryotic viral genome
            (this option is the same as:  --combine --gcode 1 --clean --fixmotif --prestart 6 --width 12 --gm)
--est       to run program on EST sequence
            (this option is the same as:  --par par_EST.default --clean --motif 0 --order 4)

Test/developer options:

--par      <file name> custom parameters for GeneMarkS
           (default is selected based on gcode value: 'par_<gcode>.default' )
--imod     <file name> custom initiation model for GeneMarkS
           (default: heuristic model derived from GC composition of input sequence
           if option --combine is specified, custom model will be combined with GeneMarkS )
--test     installation test
--verbose
";

if ( $#ARGV == -1 ) { print $usage; exit(0); }

# parse command line
if ( !GetOptions
  (
    'output=s'    => \$output,
    'name=s'      => \$name,
    'order=i'     => \$order,
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
    'offover'     => \$offover,
    'clean'       => \$clean,
    'strand=s'    => \$strand,
    'fixmotif'    => \$fixmotif,
    'combine'     => \$combine,
    'maxitr=i'    => \$maxitr,
    'gm'          => \$gm
  )
) { exit(1); };

#------------------------------------------------
# parse/check input/settings

if ( $test ) { &SelfTest(); exit(0); }

if ( $#ARGV == -1 ) { print "Error: sequence file name is missing\n"; exit(1); }

$seqfile = shift @ARGV;
&CheckFile( $seqfile, "efr" )||exit(1);

if ( ! $output )
{
  $output = File::Spec->splitpath( $seqfile ) .".lst"
}

if (($prok && $euk)||($prok && $virus)||($euk && $virus)||($euk && $est)||($prok && $est)||($virus && $est))
{ print "Error: in option selection: --prok, --euk, --virus or --est\n"; exit(1); }

if ( $prok )
{
  $combine = '1';
  $clean = '1';
  $gm = '1';
}

if ( $euk )
{
  $offover = '1';
  $gcode = "1";
  $clean = '1';
  $fixmotif = '1';
  $prestart = 6;
  $width = 12;
  $order = 4;
  $gm = '1';
}

if ( $virus )
{
  $combine = '1';
  $gcode = "1";
  $clean = '1';
  $fixmotif = '1';
  $prestart = 6;
  $width = 12;
  $gm = '1';
}

if ( $est )
{
  $motif = 0;
  $clean = '1';
  $order = 4;
  $par = $shared_dir ."/par_EST.default";
}

if ( $order !~ /^\d\d?$/ )
  { print "Error: Markov chain order [$order] is not supported\n"; exit(1); }

$mkmat_order = $order;

if ( $GENETIC_CODE !~ /\b$gcode\b/ )
  { print "Error: genetic code [$gcode] is not supported\n"; exit(1); }

if ( $SHAPE_TYPE !~ /\b$shape\b/ )
  { print "Error: sequence organization [$shape] is not supported\n"; exit(1); }

if (($motif ne '1')&&($motif ne '0'))
  { print "Error: in value of motif option\n"; exit(1); }

if ( $width !~ /^\d+$/ )
  { print "Error: in value of motif width option\n"; exit(1); }

if ( $prestart !~ /^\d+$/ )
  { print "Error: in value of prestart option\n"; exit(1); }

if (( $identity !~ /^\d?\.?\d+$/ )||( $identity > 1 ))
  { print "Error: in value of identity option\n"; exit(1); }

if ( $maxitr !~ /^\d+$/ )
  { print "Error: in value of maximum number of itteration\n"; exit(1); }

if ( $STRAND !~ /\b$strand\b/ )
  { print "Error: strand [$strand] is not supported\n"; exit(1); }

if ( !$par )
  { $par = &GetParFileName( $gcode ); }
&CheckFile( $par, "efr" )||exit(1);

if ( $imod )
{
  if ( $imod =~ /\s/ )
    { print "Error: white space is not allowed in imod: $imod\n"; exit(1); }
  &CheckFile( $imod, "efr" )||exit(1);
}

my $work_dir = `pwd`;
chomp($work_dir);
&CheckFile( $work_dir, "dwr" )||exit(1);

if ( $name )
{
  if ( $name =~ /\s/ )
    { print "Error: white space is not allowed in name: $name\n"; exit(1); }

  $out_name = $name . $out_suffix;
  $out_name_combined = $name . $out_suffix_combined;
  $out_name_heuristic = $name . $out_suffix_heu;
  $out_gm = $name . $out_gm_suffix;
  $out_gm_heu = $name . $out_gm_heu_suffix;
}

if (( $species )&&( $species =~ /\s/ ))
  { print "Error: white space is not allowed in species: $species\n"; exit(1); }

#------------------------------------------------
# start/appand $logfile file

my $time = localtime();
my $logline =
"\n\n
-------------------------------------------------
start time            : $time
working directory     : $work_dir
input sequence        : $seqfile
output name           : $name
combine model         : $combine
GeneMark model        : $gm
species name          : $species
delete temp files     : $clean
markov chain order    : $order
genetic code          : $gcode
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
GeneMarkS parameters  : $par
initial hmm model     : $imod

         run starts here:
";

&Log($logline);

#------------------------------------------------
# more variables/settings

# use <probuild> with GeneMarkS parameter file <$par>
$build = $build . " --par " . $par;

# if species name is provided, set it in a model file
if ( $species )
  { $build = "$build --NAME  $species"; }

# set options for <gmhmmp>

# switch gene overlap off in GeneMark.hmm; for eukaryotic intron-less genomes
if ( $offover )
  { $hmm = $hmm . " -p 0"; }

# set prediction for linear genome; no partial genes at the sequence ends
if ( $shape eq "linear" )
  { $hmm = $hmm . " -i 1"; }

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

&RunSystem( "$build --clean_join $seq --seq $seqfile --log $logfile", "prepare sequence\n" );
push @list_of_temp, $seq;

#------------------------------------------------
# tmp solution: get sequence size, get minimum sequence size from --par <file>
# compare, skip iterations if short

my $sequence_size = -s $seq;

$command = "grep MIN_SEQ_SIZE $par";
my $minimum_sequence_size = `$command`;
$minimum_sequence_size =~ s/\s*--MIN_SEQ_SIZE\s+//;

my $do_iterations = 1;

if ( $sequence_size < $minimum_sequence_size )
{
  &RunSystem( "$build --clean_join $seq --seq $seqfile --log $logfile --MIN_CONTIG_SIZE 0 --GAP_FILLER ", "prepare sequence\n" );
  $do_iterations = 0;
}

&Log( "do_iterations = $do_iterations\n" );

#------------------------------------------------
# determine model for initial <gmhmm> run

&Log( "set initial <gmhmmp> model\n" );
if ( !($imod) )
{
  $command = "$build --gc --seq $seq";
  &Log( "get GC\n" . $command . "\n" );
  $GC = `$command`;
  chomp($GC);
  $imod = GetHeuristicFileName( $GC, $gcode, $heu_dir_hmm, ".mod" );
  &Log( "GC of $seq = $GC\n");
}
&CheckFile( $imod, "efr" )||exit(1);
&Log( "initial <gmhmm> model: $imod\n" );

#------------------------------------------------
# copy initial <gmhmm> model into working directory

$itr = 0;
$mod = &GetNameForMod( $itr );
&RunSystem( "cat $imod > $mod", "copy initial model to working directory\n" );
push @list_of_temp, $mod;

#------------------------------------------------
# run initial prediction

$next = &GetNameForNext( $itr );
&RunSystem( "$hmm  $seq  -m $mod  -o $next", "run initial prediction\n" );
push @list_of_temp, $next;

#------------------------------------------------
# enter iterations loop

&Log( "entering iteration loop\n" );

while( $do_iterations )
{
  $itr++;
  $mod  = GetNameForMod( $itr );

  if ( $motif && !($fixmotif) )
  {
    $start_seq = $start_prefix . $itr;
    $gibbs_out = $gibbs_prefix . $itr;
  }

  $command = "$build --mkmod $mod --seq $seq --geneset $next --ORDM $order";

  if ( $motif && !$fixmotif )
   { $command .= " --pre_start $start_seq --PRE_START_WIDTH $prestart"; }
  elsif ( $motif && $fixmotif )
   { $command .= " --fixmotif --PRE_START_WIDTH $prestart --width $width --log $logfile"; }

  &RunSystem( $command, "build model: $mod for iteration: $itr\n" );
  push @list_of_temp, $mod;

  if ( $motif && !$fixmotif )
  {
#    &Log( "run gibbs sampler\n" . "$gibbs $start_seq $width -n > $gibbs_out\n" );
    &RunSystem( "$gibbs $start_seq $width -n > $gibbs_out", "run gibbs sampler\n" );
    push @list_of_temp, $start_seq;

    &RunSystem( "$build --gibbs $gibbs_out --mod $mod --seq $start_seq --log $logfile", "make prestart model\n" );
    push @list_of_temp, $gibbs_out;
  }

  $prev = $next;
  $next = &GetNameForNext( $itr );

  $command = "$hmm  $seq  -m $mod  -o $next";
  if ( $motif )
    { $command .= " -r"; }

  &RunSystem( $command, "prediction, iteration: $itr\n" );
  push @list_of_temp, $next;

  $command = "$build --compare --source $next --target $prev";
  &Log( "compare:\n" . $command . "\n" );

  $diff = `$command`;
  chomp( $diff );
  &Log( "compare $prev and $next: $diff\n" );

  if ( $diff >= $identity )
    { &Log( "Stopped iterations on identity: $diff\n" ); last; }
  if ( $itr == $maxitr )
    { &Log( "Stopped iterations on maximum number: $maxitr\n" ); last; }
}
#------------------------------------------------
# create ouput
# NOTE: initial model can differ from heuristic if option "--imod" was used
# this will modify meaning of "combined" and "heuristic" models

if ( $do_iterations )
{
  &RunSystem( "cp $mod $out_name", "output: $out_name\n" );

  if ( $combine && ( $order == 2 ) )
    { &RunSystem( "$build  --combine $out_name_combined  --first $mod  --second $imod", "Create combined model: $out_name_combined\n" ); }
}
else
  { &RunSystem( "cp $imod $out_name_heuristic", "output initial model: $out_name_heuristic\n" ); }

#------------------------------------------------
# create output for GeneMark
if ( $gm )
{

if ( $do_iterations )
{
  $command = "$build --parse --seq $seq --non $non --cod $cod --geneset $next --COD_L_MARGIN 3 --COD_R_MARGIN 3";
  &RunSystem( $command, "parse sequence for GeneMark model building\n" );
  push @list_of_temp, $cod;
  push @list_of_temp, $non;

  &Log( "create mkmat commnet file\n" );
  &CreateMkMatCommentFile( $mkmat_comment);
  push @list_of_temp, $mkmat_comment;

  $command = "$mkmat -x $cod $non $mkmat_order $out_gm < $mkmat_comment";
  &RunSystem( $command, "create parameter file for GeneMark: $out_gm\n" );
}
  if ( $combine )
    { &CopyHeuristicForGeneMark( $GC, $gcode ); }

  &CopyCodonTableForGeneMark( $gcode );
}
#------------------------------------------------
# predict genes using GeneMark.hmm with simple output options

if ( $do_iterations )
{
  if ( $motif )
  {
    # training with motif detection
    if ( $combine )
    {
       $command = "$hmm -r -m $out_name_combined -o $output $seqfile";
       &RunSystem( $command, "predict genes using combined model with motif\n" );      
    }
    else
    {
       $command = "$hmm -r -m $out_name -o $output $seqfile";
       &RunSystem( $command, "predict genes using native model with motif\n" );
    }
  }
  else
  {
    # no moitf option specified
    if ( $combine )
    {
       $command = "$hmm -m $out_name_combined -o $output $seqfile";
       &RunSystem( $command, "predict genes using combined model and no motif\n" );      
    }
    else
    {
       $command = "$hmm -m $out_name -o $output $seqfile";
       &RunSystem( $command, "predict genes using bative model and no motif\n" );
    }
  }
}
else
{
   # no iterations - use heuristic only
   $command = "$hmm -m $out_name_heuristic -o $output $seqfile";
   &RunSystem( $command, "predict genes using heuristic\n" );
}
#------------------------------------------------
# clean temp, finish

if ( $clean )
  { &RunSystem( "rm @list_of_temp" ); }

$time = localtime();
&Log( "End: $time\n\n" );

exit(0);


#=============================================================
# sub section:
#=============================================================


#-----------------------------------------------
# $heu_dir_gm $out_gm_heu
# ".mat"
#-----------------------------------------------
sub CopyHeuristicForGeneMark
{
  my ( $GC, $code ) = @_;
  my $gm_heu_name = &GetHeuristicFileName( $GC, $code, $heu_dir_gm, ".mat" );
  &CheckFile( $gm_heu_name, "efr" )||exit(1);
  &RunSystem( "cp $gm_heu_name $out_gm_heu", "copy Heuristic matrix $gm_heu_name for GeneMark\n" );
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
    &RunSystem( "cat $gm_1_tbl > $table_name", "copy codon table file $table_name for GeneMark $code\n" );
  }
  elsif ( $code eq "4" )
  {
    $table_name = $gm_4_tbl;
    $table_name =~ s/.*\///;
    &RunSystem( "cat $gm_4_tbl > $table_name", "copy codon table file $table_name for GeneMark $code\n" );
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
  if ( $text ) { &Log( $text ); }
  &Log( $com . "\n" );
  my $err_code = 0;
  if ( $err_code = system( $com ) )
  {
    &Log( "Error on last system call, error code $err_code\nAbort program!!!\n" );
    print "GeneMarkS: error on last system call, error code $err_code\nAbort program!!!\n";
    exit(1);
  }
  else
   { &Log( "system call done\n" ); }
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
  return $shared_dir . "/par_" . $code . ".default";
}
#------------------------------------------------
# $MIN_HEURISTIC_GC $MAX_HEURISTIC_GC
# "/heu_" "_"
#------------------------------------------------
sub GetHeuristicFileName
{
  my( $GC, $code, $dir, $ext ) = @_;
  $GC = int $GC;

  if( $GC < $MIN_HEURISTIC_GC ) { $GC = $MIN_HEURISTIC_GC; }
  if( $GC > $MAX_HEURISTIC_GC ) { $GC = $MAX_HEURISTIC_GC; }

  return $dir ."/heu_" . $code . "_" . $GC . $ext;
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
  print "required ...\n";
  &SelfTestRequired();
  print "optional ...\n";
  &SelfTestOptional();
  print "done\n";
}
#------------------------------------------------
# $gms_dir $heu_dir_hmm $hmm $build $gibbs
# $GENETIC_CODE $MIN_HEURISTIC_GC $MAX_HEURISTIC_GC
# ".mod"
#------------------------------------------------
sub SelfTestRequired
{
  &CheckFile( $gms_dir, "edrx" );
  &CheckFile( $shared_dir, "edr" );
  &CheckFile( $heu_dir_hmm, "edr" );
  &CheckFile( $hmm , "efx");
  &CheckFile( $build, "efx" );
  &CheckFile( $gibbs, "efx" );

  my @array = split( /\|/, $GENETIC_CODE );
  my $code  = '';
  my $GC = 0;

  foreach $code ( @array )
    { &CheckFile( &GetParFileName($code), "efr" ); }

  # test files for GeneMark.hmm  ".mod"
  foreach $code ( @array )
  {
    $GC = $MIN_HEURISTIC_GC;
    while( $GC <= $MAX_HEURISTIC_GC )
    {
      &CheckFile( &GetHeuristicFileName( $GC, $code, $heu_dir_hmm, ".mod" ), "efr" );
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
  &CheckFile( $heu_dir_gm, "edr" );
  &CheckFile( $mkmat, "efx" );
  &CheckFile( $gm_1_tbl, "efr" );
  &CheckFile( $gm_4_tbl, "efr" );

  my @array = split( /\|/, $GENETIC_CODE );
  my $code  = '';
  my $GC = 0;

  foreach $code ( @array )
  {
    $GC = $MIN_HEURISTIC_GC;
    while( $GC <= $MAX_HEURISTIC_GC )
    {
      &CheckFile( &GetHeuristicFileName( $GC, $code, $heu_dir_gm, ".mat" ), "efr" );
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
