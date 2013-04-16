#!/usr/bin/perl -w
# Alex Lomsadze, 
# 2011, GaTech
# ---------------------
# This script takes as input the GFF formatted
# gene predictions by GeneMark.hmm prokaryotic version 2.7
# and parses out nucleotide sequence of genes predicted
# in FASTA format
#
# Usage:  nt_from_gff.pl < predictions.gff > nucleotides.fasta

use strict;

my $nt = 0;

while(<>)
{

  if ( /\#\#DNA\s+(\d+)\s*/ )
  {
	$nt = 1;
	print ">gene_id_$1\n";
	next;
  }

  if ( /\#\#end-DNA/ )
  {
	$nt = 0;
	next;
  }

  if ( $nt && /\#\#(\w+)\s*/ )
  {
	print "$1\n";
  }
}
