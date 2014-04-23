#!/usr/bin/perl -w

############################################################################
# Copyright (c) 2011 Steven L. Salzberg et al.
# All Rights Reserved
# See file LICENSE for details.
############################################################################

$sum = 0;
while(<>) {
       $sum += $_;
}

print "$sum\n";
