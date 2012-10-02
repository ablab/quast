#!/usr/bin/perl -w
#=============================================================
#
# This program runs GeneMark.hmm version 2.x [ref 1]
# with Heuristic models version 2.0 [ref 2]
# on single or multi record sequence in FASTA like format
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
#   Alex Lomsadze at alexl@amber.gatech.edu
#
#=============================================================

use strict;
use FindBin qw($RealBin);
use Getopt::Long;
use File::Temp qw/ tempfile /;

my $VERSION = "1.1";

#------------------------------------------------
# installation settings: required directories and files

# GeneMark Suite executables installation directory
my $dir = $RealBin;

# GeneMark Suite directory with configuration and shared files
my $shared_dir = $dir;

# gene finding program GeneMark.hmm: gmhmmp version 2.6q
my $hmm = $dir ."/gmhmmp";

# directory with heuristic models for GeneMark.hmm: heuristic version 2.0;
my $heu_dir = $shared_dir . "/heuristic_mod";

# sequence parsing tool
my $probuild = $dir . "/probuild";

#------------------------------------------------
# command line parameters

my $seqfile;
my $outfile;
my $writeAA;
my $writeNT;
my $iniProb;
my $gcode = "11";
my $verbose;
my $test;

#------------------------------------------------
# constants

# minimum length of sequence in single FASTA record
my $MIN_LENGTH = 40;

my $GENETIC_CODE = "11|4|1";
my $MIN_HEURISTIC_GC = 30;
my $MAX_HEURISTIC_GC = 70;
my $TAIL_SEP = "\#===\n";
my $HEAD_SEP = "\# ";

#------------------------------------------------
# program name

my $name = $0;
$name =~ s/.*\///;

my $usage =
"
Usage: $name [options] -s <sequence file name>

Input sequence in FASTA format

Optional parameters:

  -outfile   <string> output file name
             default: <sequence file name>.lst
  -a         write predicted protein sequence
  -n         write nucleotide sequence of predicted genes
  -i         <number> initial probability of non-coding state
  -gcode     <number> genetic code
             default: $gcode; supported: 11, 4 and 1
  -test      installation test
  -verbose   progress information on

Version $VERSION
";

if ( $#ARGV == -1 ) { print $usage; exit(1); }

# parse command line
if ( !GetOptions
  (
    'seqfile=s'   => \$seqfile,
    'outfile=s'   => \$outfile,
    'a'           => \$writeAA,
    'd'           => \$writeNT,
    'i=f'         => \$iniProb,
    'gcode=s'     => \$gcode,
    'verbose'     => \$verbose,
    'test'        => \$test
  )
) { exit(1); };

# all values are passed by keys
if (  $#ARGV != -1 )
  { print "Error in command line\n"; exit(1); }

#------------------------------------------------
# parse/check input

if ( $verbose ) { print "parse input\n"; }

if ( defined $test ) { &SelfTest(); exit(0); }

if ( !defined $seqfile )
  { print "Error: sequence file name is missing\n"; exit(1); }

if ( !&CheckFile( $seqfile, "efr" ) ) {exit 1;}

if ( $GENETIC_CODE !~ /\b$gcode\b/ )
  { print "Error: genetic code [$gcode] is not supported\n"; exit(1); }

if ( !defined $outfile )
  { $outfile = $seqfile . ".lst"; }

if ( $seqfile eq $outfile )
  { print "Error: input and output files have the same name\n"; exit(1); }

# pass parameters for gmhmmp
my $hmm_opt = "";
if ( defined $writeAA )
  { $hmm_opt .= " -a "; }
if (defined $writeNT )
  { $hmm_opt .= " -d " };
if ( defined $iniProb )
  { $hmm_opt .= " -i $iniProb "; }

#------------------------------------------------
#prepare files

if ( $verbose ) { print "prepare files\n"; }

open(IN, "$seqfile") || die "Can't open $seqfile: $!\n";
open(OUT, ">$outfile") || die "Can't open $outfile: $!\n";

my ( $fh, $tmpfile ) = tempfile( "tmp_seq_XXXXX" );
if ( !fileno($fh) ) { die "Can't open temporally  file: $!\n"; }
close $fh;

if ( $verbose ) { print "tempfile $tmpfile\n"; }

my $tmpout = $tmpfile . ".lst";

my $line = "";
my $line_number = 0;

my $defline = "";
my $record = "";
my $length = 0;

my $status = 1;

my $count_good = 0;
my $count_bad = 0;

#parse sequence

while ( $line = <IN> )
{
  ++$line_number;

  # parse definition line
  if ( $line =~ /^\s*(>.*?)\s*$/ )
  {
    &RunRecord( $defline );

    $defline = $1 . "\n";
    $status = 1;
    $record = "";
    $length = 0;

    next;
  }

  # no ">" allowed in sequence
  if ( $line =~ m/>/ )
  {
    print "Wrong symbol in DNA sequence was found on line $line_number; record ignored\n";
    $status = 0;
    next;
  }

  # remove non alphabet
  $line =~ tr/a-zA-Z//dc;

  # skip empty lines
  if ( $line =~ /^\s*$/ ) { next;}

  # replace allowed nucleic acid code (non A T C G) by N
  $line =~ tr/RYKMSWBDHVrykmswbdhv/N/;

  # mark record if unexpected letter was found
  if ( $line =~ m/[^ATCGNatcgn]/ )
  {
    print "Wrong letter in DNA sequence was found on line $line_number; record ignored\n";
    $status = 0;
    next;
  }

  $record .= ( $line . "\n" );
  $length += length($line);
}

&RunRecord( $defline );

unlink $tmpfile, $tmpout;
close IN;
close OUT;

if ( $verbose )
{
  print "good records $count_good\n";
  print "bad records $count_bad\n";
}

exit(0);

#=============================================================
# sub section:
#=============================================================
sub RunRecord
{
  my( $def ) = @_;

  if ( $verbose ) { print "running $def"; }

  # no fasta record
  if (( $def eq "" )&&( $record eq "" ))
  {
    print "FASTA recod not found\n";
    return 1;
  }

  # no def line for valid record - create one
  if ( $def eq "" )
    { $def = ">definition line missing\n"; }

  # bad record found
  if ( !$status )
  {
     print OUT $HEAD_SEP . $def;
     print OUT "record ignored: bad sequence\n";
     print OUT $TAIL_SEP;

     ++$count_bad;
     return 1;
  }

  # check minimum length
  if ( $length < $MIN_LENGTH )
  {
     print OUT $HEAD_SEP . $def;
     print OUT "record ignored: sequence short\n";
     print OUT $TAIL_SEP;

     ++$count_bad;
     return 1;
  }

  # create temp file with sequence for gmhmmp
  open($fh, ">$tmpfile") || die "Can't open $tmpfile: $!\n";
  print $fh "$def\n";
  print $fh $record;
  close ($fh);

  # calculate GC of sequence and get name of corresponding Heuristic model
  my $gc = `$probuild --gc --seq $tmpfile --GC_LABEL "" --GC_PRECISION 1`;
  chomp($gc);
  if ( !$gc =~ /\d+\.\d/ )
    { print "Error: unexpected format\n"; exit(1); }
  my $mod = &GetHeuristicFileName( $gc, $gcode, $heu_dir, ".mod" );

  # get predictions
  my $result = system( "$hmm -m $mod $tmpfile $hmm_opt" );

  if ( !$result )
  {
    print OUT $HEAD_SEP . $def;
    open($fh, "$tmpout") || die "Can't open $tmpout: $!\n";
    while(<$fh>)
    {
      s/^Sequence file name: \S+,/Sequence file name: $seqfile,/;

      print OUT $_;
    }
    close ($fh);
    print OUT $TAIL_SEP;
    ++$count_good;
  }
  else
  {
    print OUT $HEAD_SEP . $def;
    print OUT "record ignored: error running gmhmmp\n";
    print OUT $TAIL_SEP;
    ++$count_bad;
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

  &CheckFile( $heu_dir, "edrx" );
  &CheckFile( $hmm , "efx");
  &CheckFile( $probuild , "efx");

  my @array = split( /\|/, $GENETIC_CODE );
  my $code  = '';
  my $GC = 0;

  # test presence of heuristic model files for GeneMark.hmm  ".mod"
  foreach $code ( @array )
  {
    for ( $GC = $MIN_HEURISTIC_GC; $GC <= $MAX_HEURISTIC_GC; ++$GC )
    {
      &CheckFile( &GetHeuristicFileName( $GC, $code, $heu_dir, ".mod" ), "efr" );
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
#------------------------------------------------
