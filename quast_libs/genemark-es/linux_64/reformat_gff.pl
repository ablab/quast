#!/usr/bin/perl
# --------------------------------------------
# Alex Lomsadze
# Georgia Institute of Technology, -2014
#
# Reformat GFF file : change sequence ID in GFF file to one matching contig ID inside the project
# --------------------------------------------

use strict;
use warnings;
use Getopt::Long;
use Cwd qw( abs_path );

my $Version = "1.5";
# --------------------------------------------

my $in = '';
my $out = '';
my $trace = '';
my $back = '';
my $quiet = 0;
my $v = 0;

Usage( $Version ) if ( @ARGV < 1 );
ParseCMD();
CheckBeforeRun();

my %trace_hash;

ReadTraceInfo( $trace, \%trace_hash );

my $id;
my $data;

my $lines_out = 0;

open( IN, $in ) or die "error on open file: $in\n$!\n";
open( OUT, ">$out" ) or die "error: on open file: $out\n$!\n";
while( my $line = <IN> )
{
	if( $line =~ /^\#/ )
	{
		print OUT $line;
	}
	elsif( $line =~ /^\s*$/ )
	{
		;
	}
	elsif ( $line =~ /^(.+)(\t\S+\t\S+\t\S+\t\S+\t\S+\t\S+\t\S+\t.*)/ )
	{
		$id = $1;
		$data = $2;
		
		if ( $id =~ /\t/ ){ print "error, unexpected format found on line: $line\n"; exit 1; }
		
		#trim whitespaces in ID
		$id =~ s/^\s+//;
		$id =~ s/\s+$//;
		
		if ( !exists $trace_hash{ $id }  )
		{
			print "warning, sequence ID in GFF file has no matching ID in trace file: $id\n" if !$quiet;
		}
		else
		{
			print OUT $trace_hash{ $id } . $data ."\n";
			++$lines_out;
		}
	}
	else { print "error, unexpected format found on line: $line\n"; exit 1; }
}
close OUT;
close IN;

# chech output
if ( $lines_out < 1 )
{
	print "error, output file is empty $out\n"; exit 1;
}

print "lines in GFF outfile $out : $lines_out\n" if $v;

exit 0;

# ================== sub =====================
sub ReadTraceInfo
{
	my ( $name, $ref ) = @_;
	
	my $tmp;
	
	open( my $TRACE, $name ) or die "error on open file: $name $!\n";
	while( my $line = <$TRACE> )
	{
		# skip comments and empty lines
		if ( $line =~ /^#/ )    {next;}
		if ( $line =~ /^\s*$/ ) {next;}

		# expected format of trace file:
		#    new_defline   old_file_name   old_defline
		# no whitespace in new_defline
		# old defline may have white spaces, but the fisrt word in defline should be unique id
		
		if ( $line =~ /^\>(\S+)\s+\S+\s+\>(.*)$/ )
		{
			my $new_defline = $1;
			my $old_defline = $2;
			
			# trim whitespace
			$old_defline =~ s/^\s+//;
			$old_defline =~ s/\s+$//;
			
			if( !$back )
			{
				# read as ID first word
				$old_defline =~ /^(\S+)\s*/;
				$old_defline = $1;
			}
			
			if( $back )
			{
				$tmp = $old_defline;
				$old_defline = $new_defline;
				$new_defline = $tmp;
			}
			
			if ( exists $ref->{$old_defline} )
				{ print "error, unique label is expected in trace file: $line\n";  exit 1; }
	
			$ref->{$old_defline} = $new_defline;
				
			print "$old_defline\t$new_defline\n" if $v;
		}
		else { print "error, unexpected format found: $line\n"; exit 1; }
	}
	close $TRACE; 	
}
# --------------------------------------------
sub CheckBeforeRun
{
	if ( !$out )   { print "error, output file name is missing\n"; exit 1; }
	if ( !$in or ! -e $in )       { print "error, input file not found\n"; exit 1; }
	if ( !$trace or ! -e $trace ) { print "error, trace file not found\n"; exit 1; }
	$in    = abs_path($in);
	$trace = abs_path($trace);
};
# --------------------------------------------
sub ParseCMD
{
	my $opt_result = GetOptions
	(
	  'in=s'    => \$in,
	  'out=s'   => \$out,
	  'trace=s' => \$trace,
	  'quiet'   => \$quiet,
	  'back'    => \$back,
	  'v'       => \$v
	);

	if ( !$opt_result ) { print "error on command line: $0\n"; exit 1; }
	if( @ARGV > 0 ) { print "error, unexpected argument found on command line: $0 @ARGV\n"; exit 1; }
}
# --------------------------------------------
sub Usage
{
	my $version = shift;

	print qq(
Usage: $0 --out [filename] --trace [filename] --in [filename] 
version $version
  --in     [filename] input
  --out    [filename] output
  --trace  [filename] read new sequence ID from this file
  --back   change order (new-old) in trace file 
  --quiet  no warning messages
  --v      verbose
);
	exit 1;
}
# --------------------------------------------
