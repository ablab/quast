#!/usr/bin/perl
# ---------------------------------------------
# Alex Lomsadze
# Georgia Institute of Technology, -2014
#
# get spacer and motif sequence form gibbs output
# ---------------------------------------------

use warnings;
use strict;
use Getopt::Long;
use Cwd qw(abs_path cwd);
use Data::Dumper;

# ------------------------------------------------
my $v = 0;
my $debug = 0;
# ------------------------------------------------
my $gibbs = '';
my $string_length = 0;
my $seqfile = '';

my $out_motif_seq = '';
my $out_spacer_length = '';
my $out_prespacer_length = '';
my $out_spacer_seq = '';
my $out_transition_pij = '';

# ------------------------------------------------
Usage() if ( @ARGV < 1 );
ParseCMD();
CheckInput();
# ------------------------------------------------

my %h;

my $motifs_expected = 0;
my $motifs_found = 0;

LoadGibbs( $gibbs, \%h );
LoadSequence( $seqfile, \%h );

# print Dumper( \%h ) if $debug;

PrintMotifSeq( $out_motif_seq, \%h );
PrintSpacerLength( $out_spacer_length, \%h, $string_length );
# PrintPreSpacerLength( $out_prespacer_length, \%h );
PrintSpacerSeq( $out_spacer_seq, \%h, $string_length );

PrintTransitionPij($out_transition_pij);

exit 0;

# ------------------------------------------------
sub PrintTransitionPij
{
	my( $name ) = @_;
	
	my $pij = 0;
	my $pseudo = 0.01;
	
    $pij = ( $motifs_found + $pseudo*$motifs_expected ) /( $motifs_expected + 2*$pseudo*$motifs_expected );
	
	open( my $OUT, ">", $name ) or die "error on open file $0: $name\n$!\n";
	print $OUT  "\$TO_BP ". $pij ."\n";
	print $OUT  "\$AROUND_BP ". (1 - $pij) ."\n";
	close $OUT;
	
	print "output bp tr probability done\n" if $debug;
}
# ------------------------------------------------
sub PrintSpacerSeq
{
	my( $name, $ref, $len ) = @_;
	
	my $seq;
	
	open( my $OUT, ">", $name ) or die "error on open file $0: $name\n$!\n"; 		
	foreach my $key ( keys %{$ref} )
	{
		# -2 in length
		#  |BP|------|AG|
		#      first 2 positions after BP differ from the others in spacer 
		#      scanning starts on some "distance" from BP 
		$seq = substr(  $ref->{$key}->{'seq'}, -($len + $ref->{$key}->{'spacer'} - 2 ) );
		
		print $OUT $seq ."N\n";
	}
	close $OUT;
	
	print "output spacer sequence done\n" if $debug;
}
# ------------------------------------------------
sub PrintPreSpacerLength
{
	my( $name, $ref ) = @_;
	
	my $len;
	
	open( my $OUT, ">", $name ) or die "error on open file $0: $name\n$!\n"; 		
	foreach my $key ( keys %{$ref} )
	{
		$len = $ref->{$key}->{'intron_len'} - $ref->{$key}->{'spacer'} - 9 ;
		
		if ($len > 0 )
		{
			print $OUT $len ."\n";
		}
	}
	close $OUT;
	
	print "output pre-spacer length done\n" if $debug;
}
# ------------------------------------------------
sub PrintSpacerLength
{
	my( $name, $ref, $len ) = @_;
	
	open( my $OUT, ">", $name ) or die "error on open file $0: $name\n$!\n"; 		
	foreach my $key ( sort{$a<=>$b} keys %{$ref} )
	{
		print $OUT  ($len + $ref->{$key}->{'spacer'}) ."\n";
	}
	close $OUT;
	
	print "output spacer length done\n" if $debug;
}
# ------------------------------------------------
sub PrintMotifSeq
{
	my( $name, $ref ) = @_;
	
	open( my $OUT, ">", $name ) or die "error on open file $0: $name\n$!\n"; 		
	foreach my $key (  sort{$a<=>$b} keys %{$ref} )
	{
		print $OUT $ref->{$key}->{'motif'} ."\n";
	}
	close $OUT;
	
	print "output motif sequence done\n" if $debug;
}
# ------------------- sub ------------------------
sub LoadSequence
{
	my( $name, $ref ) = @_;
	
	my $lines_read_in = 0;
	my $seq_id;
	my $intron_len;
	
	open( my $IN, $name ) or die "error on open file $0: $name\n$!\n"; 
	while( my $line = <$IN>)
	{
		++$lines_read_in;
		
		# skip comments and empty
		if ( $line =~ /^\#/ )   {next;}
		if ( $line =~ /^\s+$/ ) {next;}
		
		# read seq
		if ( $line =~ /^>/ )
		{
			# parse seq id
			if ( $line =~ /^>(\d+)_(\d+)\s*$/ )
			{
				$seq_id = $1;
				$intron_len = $2;
			}
			else { print "error, unexpected format found $0: $line"; exit 1; }
			
			# read - parse seq
			$line = <$IN>;
			if ( $line =~ /^([ACTGNX]+)\s*$/ )
			{
				# gibbs may exclude seq from output 
				if( exists $ref->{$seq_id}  )
				{
					$ref->{$seq_id}{'seq'} = $1;
					$ref->{$seq_id}{'intron_len'} = $intron_len;
					
					if ( !$string_length )
					{
						$string_length = length($ref->{$seq_id}{'seq'});
					}
					elsif ( $string_length != length( $ref->{$seq_id}{'seq'} ) )
					{
						print "error, length of bp_region in $name is not the constant: $string_length : $line\n";
						exit 1;
					}
				}
			}
			else { print "error, unexpected format found $0 : $line"; exit 1; }
		}
	}
	close $IN;
	
	print "sequence load done\n" if $debug;
}
# ------------------- sub ------------------------
sub LoadGibbs
{
	my( $name, $ref ) = @_;
	
	my $lines_read_in = 0;
	my $start_of_data = 0;
	
	my $seq_id;
	my $motif;
	my $spacer;
	my $motif_id;
	my $left;
	my $right;
	
	open( my $IN, $name ) or die "error on open file $0: $name\n$!\n"; 
	while( my $line = <$IN>)
	{
		++$lines_read_in;
		
		if( !$start_of_data  )
		{ 
			# skip comments and empty
			if ( $line =~ /^\#/ )   {next;}
			if ( $line =~ /^>/ )    {next;}
			if ( $line =~ /^\s+$/ ) {next;}
			
			if ( $line =~ /^nNumMotifs\s+=\s+(\d+)\s*$/ )
			{
				$motifs_expected = $1;
				
				print "motifs expected: $motifs_expected\n" if $debug;
			}
			
			if ( $line =~ /^Num Motifs:\s+(\d+)\s*$/ )
			{
				$motifs_found = $1;
				$start_of_data = 1;
				
				print "motifs found: $motifs_found\n" if $debug;
			}
			
			next;
		}
		
		if( $line =~/^\s*(\d+)\,\s+(\d+)\s+(\d+)\s.+\s([ATGC]+)\s.+\s(\d+)\s+\S+\s+\S+\s+\d+_\d+\s*$/ )
		{
			$seq_id = $1;
			$motif_id = $2;
			$left = $3;
			$motif = $4;
			$right = $5;
			
			if( $motif_id != 1 )
			{
				print "$line ignored: $line" if $debug;
				next;
			}
			
			$spacer = (- $right + 2);
			
			$ref->{$seq_id}{'spacer'} = $spacer;
			$ref->{$seq_id}{'motif'}  = $motif;
		}
		else
		{
			if( $line =~ /^\s+\*+\s*$/ )
			{
				last;
			}
			else { print "error, unexpected format found $0: $line"; exit 1; }
		}
	}
	close $IN;
	
	print "gibbs load done\n" if $debug;
}
# ------------------------------------------------
sub CheckInput
{
	if( !$seqfile ) { print "error, input seq file name is not specified $0\n"; exit 1; }
	if( !$gibbs ) { print "error, input gibbs file name is not specified $0\n"; exit 1; }
	$seqfile = ResolvePath( $seqfile );
	$gibbs = ResolvePath( $gibbs );
	
	print "seqfile $seqfile\n" if $debug;
	print "gibbs $gibbs\n" if $debug;
}
# ------------------------------------------------
sub ResolvePath
{
	my ( $name, $path ) = @_;
	return '' if !$name;
	$name = File::Spec->catfile( $path, $name ) if ( defined $path and $path );
	if( ! -e $name ) { print "error, file not found $0: $name\n"; exit 1; }
	return abs_path( $name );
}
# ------------------------------------------------
sub ParseCMD
{
	my $cmd = $0;
	foreach my $str (@ARGV) { $cmd .= ( ' '. $str ); }
	
	my $opt_result = GetOptions
	(
		'motif_seq=s'  => \$out_motif_seq,
		'spacer_len=s' => \$out_spacer_length,
		'prespacer_len=s' => \$out_prespacer_length,
		'spacer_seq=s' => \$out_spacer_seq,
		'tr=s'         => \$out_transition_pij,
		'gibbs=s'  => \$gibbs,
		'strlen=i' => \$string_length,
		'seq=s'    => \$seqfile,
		'verbose'  => \$v,
		'debug'    => \$debug
	);
	
	if( !$opt_result ) { print "error on command line\n"; exit 1; }
	if( @ARGV > 0 ) { print "error, unexpected argument found on command line: @ARGV\n"; exit 1; }
	$v = 1 if $debug;
}
# ------------------------------------------------
sub Usage
{
	print qq(# -------------------
Usage: $0   parameters

required:
  --gibss  [name] results of Gibss3 search 
  --strlen [number] length of seq (all the same) in gibbs input
  --seq    [filename]  input sequence in gibbs run

  --motif_seq     output filename
  --spacer_len    output filename
  --prespacer_len    output filename
  --spacer_seq    output filename
  --tr            output filename

optional:
  --verbose
  --debug
# -------------------
);
	exit 1;
}
# ------------------------------------------------
