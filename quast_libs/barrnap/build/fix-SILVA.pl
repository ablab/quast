#!/usr/bin/env perl
use warnings;
use strict;
use Data::Dumper;

my(@Options, $verbose, $type, $seed);
setOptions();

$type or die "please specify rRNA class using --type";

my @kingdom = qw(Bacteria Eukaryota Archaea);
my %out;
for my $k (@kingdom) {
  $out{$k}{FILENAME} = "$type.$k.aln";
  open $out{$k}{FH}, '>', $out{$k}{FILENAME};
#  $out{$k}{INCOUNT} = 0;
  $out{$k}{COUNT} = 0;
  $out{$k}{DESC} = "$type";
  printf STDERR "Writing $k sequences to: %s\n", $out{$k}{FILENAME};
}


my $k = '';
my $id = '';
my $aln = '';
my $species = '';
my %seen;

while (my $line = <>) {
#  chomp $line;
  if ($line =~ m/^>/) {
    if ($aln) {
      # output the previous alignment
      unless ($seed and $seen{$species}++) {
        $aln =~ s/\./-/g;
        $aln =~ s/ //g;
        printf {$out{$k}{FH}} ">%s %s\n%s\n", $id, $out{$k}{DESC}, $aln;
        $out{$k}{COUNT}++;
      }
    }
    # >FJ805841.1.4128 Bacteria;Cyanobacteria;Cyanobacteria;SubsectionII;FamilyII;Chroococcidiopsis;Chroococcidiopsis thermalis PCC 7203
    print STDERR "\rProcessing: ", join(' ', map { $out{$kingdom[$_]}{COUNT} } 0..2);
    $line =~ m/^>(\S+)\s+(\w+);/;
    $k = $2;
    $id = $1;
    $aln = '';
    my @x = split ' ', $line;
    $x[2] ||= '';
    $species = "$x[1];$x[2]";
    #print STDERR "\t$. @x\n";
    #$species = $k eq 'Bacteria' ? "$x[1];$x[0]" : $x[1];
  }
  else {
    $aln .= $line;
#    print Dumper($id,$k,$aln); exit;
  }
}

print Dumper(\%out);

#----------------------------------------------------------------------
# Option setting routines

sub setOptions {
  use Getopt::Long;

  @Options = (
    {OPT=>"help",    VAR=>\&usage,             DESC=>"This help"},
    {OPT=>"verbose!",  VAR=>\$verbose, DEFAULT=>0, DESC=>"Verbose output"},
    {OPT=>"type=s",  VAR=>\$type, DEFAULT=>'', DESC=>"Type of rRNA in this file eg. 16S 23S 5S ..."},
    {OPT=>"seed!",  VAR=>\$seed, DEFAULT=>0, DESC=>"Only use 1 representative per species"},
  );

  #(!@ARGV) && (usage());

  &GetOptions(map {$_->{OPT}, $_->{VAR}} @Options) || usage();

  # Now setup default values.
  foreach (@Options) {
    if (defined($_->{DEFAULT}) && !defined(${$_->{VAR}})) {
      ${$_->{VAR}} = $_->{DEFAULT};
    }
  }
}

sub usage {
  print "Usage: $0 [options]\n";
  foreach (@Options) {
    printf "  --%-13s %s%s.\n",$_->{OPT},$_->{DESC},
           defined($_->{DEFAULT}) ? " (default '$_->{DEFAULT}')" : "";
  }
  exit(1);
}
 
#----------------------------------------------------------------------
