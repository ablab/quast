#!/usr/bin/perl
# --------------------------------------------
# Alex Lomsadze
# Georgia Institute of Technology, -2014
#
# Build model following configuration file derectives
# --------------------------------------------

use strict;
use warnings;
use Getopt::Long;
use Cwd qw(abs_path cwd);
use Data::Dumper;
use FindBin;
use lib "$FindBin::Bin/../lib";
use YAML;
use Hash::Merge qw( merge );
use File::Spec;

# ------------------------------------------------
my $v = 0;
my $debug = 0;
	
# system control
#   $cfg - reference on hash with parameters
#   $log - reference on logger

my $cfg;
my $section = 'build_mod';
my $out;

# ------------------------------------------------
Usage() if ( @ARGV < 1 );
ParseCMD();
CheckInput();
print Dumper( $cfg ) if $debug;
# ------------------------------------------------

# temp - shortcut due to time
my $BP = 0;
$BP = 1 if $cfg->{'Parameters'}->{'fungus'};

# hash (key->value)
# keys in model file start with "$"
# remove "$" on load mod and add "$" on save mod

my %base_mod;
LoadMod( \%base_mod, $cfg->{$section}->{'def'} );

# ------------------------------------------------

if( $section eq 'ES_ini' )
{
	$base_mod{'NORM_SITES'} = '1';	
	
	$base_mod{'INI_WIDTH_P'}         = $cfg->{'start_ATG'}->{'width'};
	$base_mod{'INI_MARGIN_P'}        = $cfg->{'start_ATG'}->{'margin'};	
	$base_mod{'TERM_TAA_WIDTH_P'}    = $cfg->{'stop_TAA'}->{'width'};
	$base_mod{'TERM_TAA_MARGIN_P'}   = $cfg->{'stop_TAA'}->{'margin'};
	$base_mod{'TERM_TAG_WIDTH_P'}    = $cfg->{'stop_TAG'}->{'width'};
	$base_mod{'TERM_TAG_MARGIN_P'}   = $cfg->{'stop_TAG'}->{'margin'};
	$base_mod{'TERM_TGA_WIDTH_P'}    = $cfg->{'stop_TGA'}->{'width'};
	$base_mod{'TERM_TGA_MARGIN_P'}   = $cfg->{'stop_TGA'}->{'margin'}; 
	$base_mod{'DONOR_0_WIDTH_P'}     = $cfg->{'donor_GT'}->{'width'};
	$base_mod{'DONOR_0_MARGIN_P'}    = $cfg->{'donor_GT'}->{'margin'};
	$base_mod{'DONOR_1_WIDTH_P'}     = $cfg->{'donor_GT'}->{'width'};
	$base_mod{'DONOR_1_MARGIN_P'}    = $cfg->{'donor_GT'}->{'margin'};
	$base_mod{'DONOR_2_WIDTH_P'}     = $cfg->{'donor_GT'}->{'width'};
	$base_mod{'DONOR_2_MARGIN_P'}    = $cfg->{'donor_GT'}->{'margin'};
	$base_mod{'ACCEPTOR_0_WIDTH_P'}  = $cfg->{'acceptor_AG'}->{'width'};
	$base_mod{'ACCEPTOR_0_MARGIN_P'} = $cfg->{'acceptor_AG'}->{'margin'};
	$base_mod{'ACCEPTOR_1_WIDTH_P'}  = $cfg->{'acceptor_AG'}->{'width'};
	$base_mod{'ACCEPTOR_1_MARGIN_P'} = $cfg->{'acceptor_AG'}->{'margin'};
	$base_mod{'ACCEPTOR_2_WIDTH_P'}  = $cfg->{'acceptor_AG'}->{'width'};
	$base_mod{'ACCEPTOR_2_MARGIN_P'} = $cfg->{'acceptor_AG'}->{'margin'};

	LoadFromCFG( \%base_mod, $cfg->{'user_GMHMM3_parameters'} );

	if( $cfg->{'Parameters'}->{'max_intergenic'} >  0 )
	{
		$base_mod{'INTERGENIC_MAX'} =  $cfg->{'Parameters'}->{'max_intergenic'};
	}
	
	SaveModel( \%base_mod, $cfg->{'ES_ini'}->{'mod'}, $cfg->{'GMHMM3_order_basic'} );
}
elsif(  $section  eq 'ES_A')
{
	UpdateSignal( $cfg->{'start_ATG'}->{'outfile'}, \%base_mod, "INI" );
	UpdateSignal( $cfg->{'stop_TAA'}->{'outfile'},  \%base_mod, "TERM_TAA" );
	UpdateSignal( $cfg->{'stop_TGA'}->{'outfile'},  \%base_mod, "TERM_TGA" );
	UpdateSignal( $cfg->{'stop_TAG'}->{'outfile'},  \%base_mod, "TERM_TAG" );
	UpdateSpliseSignalNoPhase( $cfg->{'donor_GT'}->{'outfile'},    \%base_mod, "DONOR" );
	UpdateSpliseSignalNoPhase( $cfg->{'acceptor_AG'}->{'outfile'}, \%base_mod, "ACCEPTOR" );
	UpdateValue( 'mkch', \%base_mod, "MARKOV" );
	
	SaveModel( \%base_mod, $out, $cfg->{'GMHMM3_order_basic'} );
}
elsif(  $section  eq 'ES_B')
{
	UpdateSignal( $cfg->{'start_ATG'}->{'outfile'}, \%base_mod, "INI" );
	UpdateSignal( $cfg->{'stop_TAA'}->{'outfile'},  \%base_mod, "TERM_TAA" );
	UpdateSignal( $cfg->{'stop_TGA'}->{'outfile'},  \%base_mod, "TERM_TGA" );
	UpdateSignal( $cfg->{'stop_TAG'}->{'outfile'},  \%base_mod, "TERM_TAG" );	
	UpdateSpliseSignalPhased( $cfg->{'donor_GT'}->{'outfile'},    \%base_mod, "DONOR" );
	UpdateSpliseSignalPhased( $cfg->{'acceptor_AG'}->{'outfile'}, \%base_mod, "ACCEPTOR" );	
	UpdateValue( 'mkch', \%base_mod, "MARKOV" );
	UpdateDuration( $cfg->{'intron_DUR'}->{'out'},   \%base_mod, "INTRON",   "intron_DUR" );
	UpdateDuration( $cfg->{'initial_DUR'}->{'out'},  \%base_mod, "INITIAL",  "initial_DUR" );
	UpdateDuration( $cfg->{'internal_DUR'}->{'out'}, \%base_mod, "EXON",     "internal_DUR" );
	UpdateDuration( $cfg->{'terminal_DUR'}->{'out'}, \%base_mod, "TERMINAL", "terminal_DUR" );	
	UpdateDuration( $cfg->{'single_DUR'}->{'out'},   \%base_mod, "SINGLE",   "single_DUR" );
	
	SaveModel( \%base_mod, $out, $cfg->{'GMHMM3_order_basic'} );
}
elsif(  $section  eq 'ES_C')
{
	UpdateSignal( $cfg->{'start_ATG'}->{'outfile'}, \%base_mod, "INI" );
	UpdateSignal( $cfg->{'stop_TAA'}->{'outfile'},  \%base_mod, "TERM_TAA" );
	UpdateSignal( $cfg->{'stop_TGA'}->{'outfile'},  \%base_mod, "TERM_TGA" );
	UpdateSignal( $cfg->{'stop_TAG'}->{'outfile'},  \%base_mod, "TERM_TAG" );	
	UpdateSpliseSignalPhased( $cfg->{'donor_GT'}->{'outfile'},    \%base_mod, "DONOR" );
	UpdateSpliseSignalPhased( $cfg->{'acceptor_AG'}->{'outfile'}, \%base_mod, "ACCEPTOR" );	
	UpdateValue( 'mkch', \%base_mod, "MARKOV" );
	UpdateExonPhase( $cfg->{'ES_C'}->{'phase'},\%base_mod );
	UpdateDuration( $cfg->{'intron_DUR'}->{'out'},   \%base_mod, "INTRON",   "intron_DUR" );
	UpdateDuration( $cfg->{'initial_DUR'}->{'out'},  \%base_mod, "INITIAL",  "initial_DUR" );
	UpdateDuration( $cfg->{'internal_DUR'}->{'out'}, \%base_mod, "EXON",     "internal_DUR" );
	UpdateDuration( $cfg->{'terminal_DUR'}->{'out'}, \%base_mod, "TERMINAL", "terminal_DUR" );	
	UpdateDuration( $cfg->{'single_DUR'}->{'out'},   \%base_mod, "SINGLE",   "single_DUR" );
	UpdateTransition_S_M( $cfg->{'ES_C'}->{'tr_s_vs_m'}, \%base_mod );
	UpdateTransition_I_T( $cfg->{'ES_C'}->{'tr_i_vs_t'}, \%base_mod );

	if( $BP )
	{
		if( !exists $base_mod{'BP'} )
		{
			$base_mod{'BP'} = '1';
			$base_mod{'LIMIT_BP_DURATION'} = '500';
			$base_mod{'TO_BP'}     = '0.5';
			$base_mod{'AROUND_BP'} = '0.5';
		}
		
		if ( $base_mod{'BP'} )
		{
			UpdateSignal( $cfg->{'branch_point'}->{'outfile'}, \%base_mod, "BRANCH" );
			UpdateSpliseSignalPhased( $cfg->{'acceptor_short_AG'}->{'outfile'}, \%base_mod, "ACC_BP" );
			UpdateDuration( $cfg->{'spacer_DUR'}->{'out'},    \%base_mod, "BP_ACC", "spacer_DUR" );
			UpdateDuration( $cfg->{'prespacer_DUR'}->{'out'}, \%base_mod, "DON_BP", "prespacer_DUR" );
			$base_mod{'BP_SPACER_ORDER'} = '1';		
			UpdateValue( 'spacer.mkch', \%base_mod, "MARKOV_BP_SPACER" );
			Update_BP_UpDown( $cfg->{'ES_C'}->{'tr_bp'}, \%base_mod );
		}
	}
	
	if( exists $base_mod{'BP'} )
	{
		my %order;
		Hash::Merge::set_behavior( 'RIGHT_PRECEDENT' );
		%order = %{ merge( $cfg->{'GMHMM3_order_basic'}, $cfg->{'GMHMM3_order_BP'}  ) };
		SaveModel( \%base_mod, $out, \%order );
	}
	else
	{
		SaveModel( \%base_mod, $out, $cfg->{'GMHMM3_order_basic'} );
	} 

}
elsif( $section eq 'ET_ini' )
{
	$base_mod{'NORM_SITES'} = '1';	
	
	$base_mod{'INI_WIDTH_P'}         = $cfg->{'start_ATG'}->{'width'};
	$base_mod{'INI_MARGIN_P'}        = $cfg->{'start_ATG'}->{'margin'};	
	$base_mod{'TERM_TAA_WIDTH_P'}    = $cfg->{'stop_TAA'}->{'width'};
	$base_mod{'TERM_TAA_MARGIN_P'}   = $cfg->{'stop_TAA'}->{'margin'};
	$base_mod{'TERM_TAG_WIDTH_P'}    = $cfg->{'stop_TAG'}->{'width'};
	$base_mod{'TERM_TAG_MARGIN_P'}   = $cfg->{'stop_TAG'}->{'margin'};
	$base_mod{'TERM_TGA_WIDTH_P'}    = $cfg->{'stop_TGA'}->{'width'};
	$base_mod{'TERM_TGA_MARGIN_P'}   = $cfg->{'stop_TGA'}->{'margin'};

	LoadFromCFG( \%base_mod, $cfg->{'user_GMHMM3_parameters'} );

	if( $cfg->{'Parameters'}->{'max_intergenic'} >  0 )
	{
		$base_mod{'INTERGENIC_MAX'} =  $cfg->{'Parameters'}->{'max_intergenic'};
	}	
	
	UpdateSpliseSignalNoPhase( $cfg->{'donor_GT'}->{'outfile'},    \%base_mod, "DONOR" );
	UpdateSpliseSignalNoPhase( $cfg->{'acceptor_AG'}->{'outfile'}, \%base_mod, "ACCEPTOR" );	
	UpdateDuration( $cfg->{'intron_DUR'}->{'out'},   \%base_mod, "INTRON",   "intron_DUR" );

	if( $BP )
	{
		if( !exists $base_mod{'BP'} )
		{
			$base_mod{'BP'} = '1';
			$base_mod{'LIMIT_BP_DURATION'} = '500';
			$base_mod{'TO_BP'}     = '0.5';
			$base_mod{'AROUND_BP'} = '0.5';
		}
		
		if ( $base_mod{'BP'} )
		{
			UpdateSignal( $cfg->{'branch_point'}->{'outfile'}, \%base_mod, "BRANCH" );
			UpdateSpliseSignalNoPhase( $cfg->{'acceptor_short_AG'}->{'outfile'}, \%base_mod, "ACC_BP" );
			UpdateDuration( $cfg->{'spacer_DUR'}->{'out'},    \%base_mod, "BP_ACC", "spacer_DUR" );
			UpdateDuration( $cfg->{'prespacer_DUR'}->{'out'}, \%base_mod, "DON_BP", "prespacer_DUR" );
			$base_mod{'BP_SPACER_ORDER'} = '1';		
			UpdateValue( 'spacer.mkch', \%base_mod, "MARKOV_BP_SPACER" );
			Update_BP_UpDown( $cfg->{'ET_ini'}->{'tr_bp'}, \%base_mod );
		}
	}
	
	$out = $cfg->{'ET_ini'}->{'mod'};
	
	if( exists $base_mod{'BP'} )
	{
		my %order;
		Hash::Merge::set_behavior( 'RIGHT_PRECEDENT' );
		%order = %{ merge( $cfg->{'GMHMM3_order_basic'}, $cfg->{'GMHMM3_order_BP'}  ) };
		SaveModel( \%base_mod, $out, \%order );
	}
	else
	{
		SaveModel( \%base_mod, $out, $cfg->{'GMHMM3_order_basic'} );
	}
}
elsif(  $section  eq 'ET_A')
{
	UpdateSignal( $cfg->{'start_ATG'}->{'outfile'}, \%base_mod, "INI" );
	UpdateSignal( $cfg->{'stop_TAA'}->{'outfile'},  \%base_mod, "TERM_TAA" );
	UpdateSignal( $cfg->{'stop_TGA'}->{'outfile'},  \%base_mod, "TERM_TGA" );
	UpdateSignal( $cfg->{'stop_TAG'}->{'outfile'},  \%base_mod, "TERM_TAG" );
	UpdateValue( 'mkch', \%base_mod, "MARKOV" );

	if( exists $base_mod{'BP'} )
	{
		my %order;
		Hash::Merge::set_behavior( 'RIGHT_PRECEDENT' );
		%order = %{ merge( $cfg->{'GMHMM3_order_basic'}, $cfg->{'GMHMM3_order_BP'}  ) };
		SaveModel( \%base_mod, $out, \%order );
	}
	else
	{
		SaveModel( \%base_mod, $out, $cfg->{'GMHMM3_order_basic'} );
	}
}
elsif(  $section  eq 'ET_B')
{
	UpdateSignal( $cfg->{'start_ATG'}->{'outfile'}, \%base_mod, "INI" );
	UpdateSignal( $cfg->{'stop_TAA'}->{'outfile'},  \%base_mod, "TERM_TAA" );
	UpdateSignal( $cfg->{'stop_TGA'}->{'outfile'},  \%base_mod, "TERM_TGA" );
	UpdateSignal( $cfg->{'stop_TAG'}->{'outfile'},  \%base_mod, "TERM_TAG" );
	UpdateValue( 'mkch', \%base_mod, "MARKOV" );
	UpdateDuration( $cfg->{'initial_DUR'}->{'out'},  \%base_mod, "INITIAL",  "initial_DUR" );
	UpdateDuration( $cfg->{'internal_DUR'}->{'out'}, \%base_mod, "EXON",     "internal_DUR" );
	UpdateDuration( $cfg->{'terminal_DUR'}->{'out'}, \%base_mod, "TERMINAL", "terminal_DUR" );	
	UpdateDuration( $cfg->{'single_DUR'}->{'out'},   \%base_mod, "SINGLE",   "single_DUR" );

	if( exists $base_mod{'BP'} )
	{
		my %order;
		Hash::Merge::set_behavior( 'RIGHT_PRECEDENT' );
		%order = %{ merge( $cfg->{'GMHMM3_order_basic'}, $cfg->{'GMHMM3_order_BP'}  ) };
		SaveModel( \%base_mod, $out, \%order );
	}
	else
	{
		SaveModel( \%base_mod, $out, $cfg->{'GMHMM3_order_basic'} );
	}
}
elsif(  $section  eq 'ET_C')
{
	UpdateSignal( $cfg->{'start_ATG'}->{'outfile'}, \%base_mod, "INI" );
	UpdateSignal( $cfg->{'stop_TAA'}->{'outfile'},  \%base_mod, "TERM_TAA" );
	UpdateSignal( $cfg->{'stop_TGA'}->{'outfile'},  \%base_mod, "TERM_TGA" );
	UpdateSignal( $cfg->{'stop_TAG'}->{'outfile'},  \%base_mod, "TERM_TAG" );
	UpdateValue( 'mkch', \%base_mod, "MARKOV" );
	UpdateDuration( $cfg->{'initial_DUR'}->{'out'},  \%base_mod, "INITIAL",  "initial_DUR" );
	UpdateDuration( $cfg->{'internal_DUR'}->{'out'}, \%base_mod, "EXON",     "internal_DUR" );
	UpdateDuration( $cfg->{'terminal_DUR'}->{'out'}, \%base_mod, "TERMINAL", "terminal_DUR" );	
	UpdateDuration( $cfg->{'single_DUR'}->{'out'},   \%base_mod, "SINGLE",   "single_DUR" );
	UpdateSpliseSignalPhased( $cfg->{'donor_GT'}->{'outfile'},    \%base_mod, "DONOR" );
	UpdateSpliseSignalPhased( $cfg->{'acceptor_AG'}->{'outfile'}, \%base_mod, "ACCEPTOR" );
	UpdateExonPhase( $cfg->{'ET_C'}->{'phase'},\%base_mod );
	UpdateTransition_S_M( $cfg->{'ET_C'}->{'tr_s_vs_m'}, \%base_mod );
	UpdateTransition_I_T( $cfg->{'ET_C'}->{'tr_i_vs_t'}, \%base_mod );


	if ( $BP and $base_mod{'BP'} )
	{
		UpdateSpliseSignalPhased( $cfg->{'acceptor_short_AG'}->{'outfile'}, \%base_mod, "ACC_BP" );
	}

	if( exists $base_mod{'BP'} )
	{
		my %order;
		Hash::Merge::set_behavior( 'RIGHT_PRECEDENT' );
		%order = %{ merge( $cfg->{'GMHMM3_order_basic'}, $cfg->{'GMHMM3_order_BP'}  ) };
		SaveModel( \%base_mod, $out, \%order );
	}
	else
	{
		SaveModel( \%base_mod, $out, $cfg->{'GMHMM3_order_basic'} );
	}
}
else { print "error in section label $0: $section\n"; exit 1; }
	
exit 0;

# ================= subs =========================
sub UpdateDuration
{
	my( $name, $ref, $label_hmm, $label_cfg ) = @_;

	$ref->{ $label_hmm .'_MIN'}  = $cfg->{ $label_cfg}->{'min'};
	$ref->{ $label_hmm .'_MAX'}  = $cfg->{ $label_cfg}->{'max'};
	$ref->{ $label_hmm .'_TAG'}  = '"'. $label_hmm .'_DISTR"';
	$ref->{ $label_hmm .'_TYPE'} = '"'. $label_hmm .'_DISTR"';

	my $mat;
	LoadFromFile( $name, \$mat );

	$ref->{ $label_hmm .'_DISTR'} = $mat;
}
# ------------------------------------------------
sub Update_BP_UpDown
{
	my( $name, $ref ) = @_;
	my %hash;
	LoadMod( \%hash, $name );	
	$ref->{'TO_BP'}     = $hash{'TO_BP'};
	$ref->{'AROUND_BP'} = $hash{'AROUND_BP'};
}
# ------------------------------------------------
sub UpdateTransition_I_T
{
	my( $name, $ref ) = @_;
	my %hash;
	LoadMod( \%hash, $name );	
	$ref->{'ToInternalExon'} = $hash{'ToInternalExon'};
	$ref->{'ToTerminalExon'} = $hash{'ToTerminalExon'};
}
# ------------------------------------------------
sub UpdateTransition_S_M
{
	my( $name, $ref ) = @_;
	my %hash;
	LoadMod( \%hash, $name );	
	$ref->{'ToSingleGene'} = $hash{'ToSingleGene'};
	$ref->{'ToMultiGene'}  = $hash{'ToMultiGene'};
}
# ------------------------------------------------
sub UpdateExonPhase
{
	my( $name, $ref ) = @_;
	my %hash;
	LoadMod( \%hash, $name );
	$ref->{'INITIAL_EXON_PHASE'}  = $hash{'INITIAL_EXON_PHASE'};
	$ref->{'TERMINAL_EXON_PHASE'} = $hash{'TERMINAL_EXON_PHASE'};
	$ref->{'INTERNAL_EXON_PHASE'} = $hash{'INTERNAL_EXON_PHASE'}; 
}
# ------------------------------------------------
sub UpdateValue
{
	my( $name, $ref, $label ) = @_;
	my $value;
	LoadFromFile( $name, \$value );
	$ref->{ $label } = $value; 
}
# ------------------------------------------------
sub UpdateSpliseSignalPhased
{
	my( $name, $ref, $label ) = @_;

	my %hash;
	LoadMod( \%hash, $name );	

	$ref->{ $label .'_0_WIDTH'}    = $hash{ $label .'_0_WIDTH'};
	$ref->{ $label .'_0_WIDTH_P'}  = $hash{ $label .'_0_WIDTH_P'};
	$ref->{ $label .'_0_MARGIN'}   = $hash{ $label .'_0_MARGIN'};
	$ref->{ $label .'_0_MARGIN_P'} = $hash{ $label .'_0_MARGIN_P'};
	$ref->{ $label .'_0_ORDER'}    = $hash{ $label .'_0_ORDER'};
	$ref->{ $label .'_0_MAT'}      = $hash{ $label .'_0_MAT'};
	
	$ref->{ $label .'_1_WIDTH'}    = $hash{ $label .'_1_WIDTH'};
	$ref->{ $label .'_1_WIDTH_P'}  = $hash{ $label .'_1_WIDTH_P'};
	$ref->{ $label .'_1_MARGIN'}   = $hash{ $label .'_1_MARGIN'};
	$ref->{ $label .'_1_MARGIN_P'} = $hash{ $label .'_1_MARGIN_P'};
	$ref->{ $label .'_1_ORDER'}    = $hash{ $label .'_1_ORDER'};
	$ref->{ $label .'_1_MAT'}      = $hash{ $label .'_1_MAT'};
	
	$ref->{ $label .'_2_WIDTH'}    = $hash{ $label .'_2_WIDTH'};
	$ref->{ $label .'_2_WIDTH_P'}  = $hash{ $label .'_2_WIDTH_P'};
	$ref->{ $label .'_2_MARGIN'}   = $hash{ $label .'_2_MARGIN'};
	$ref->{ $label .'_2_MARGIN_P'} = $hash{ $label .'_2_MARGIN_P'};
	$ref->{ $label .'_2_ORDER'}    = $hash{ $label .'_2_ORDER'};
	$ref->{ $label .'_2_MAT'}      = $hash{ $label .'_2_MAT'};
}
# ------------------------------------------------
sub UpdateSpliseSignalNoPhase
{
	my( $name, $ref, $label ) = @_;

	my %hash;
	LoadMod( \%hash, $name );	

	$ref->{ $label .'_0_WIDTH'}    = $hash{ $label .'_WIDTH'};
	$ref->{ $label .'_0_WIDTH_P'}  = $hash{ $label .'_WIDTH_P'};
	$ref->{ $label .'_0_MARGIN'}   = $hash{ $label .'_MARGIN'};
	$ref->{ $label .'_0_MARGIN_P'} = $hash{ $label .'_MARGIN_P'};
	$ref->{ $label .'_0_ORDER'}    = $hash{ $label .'_ORDER'};
	$ref->{ $label .'_0_MAT'}      = $hash{ $label .'_MAT'};
	
	$ref->{ $label .'_1_WIDTH'}    = $hash{ $label .'_WIDTH'};
	$ref->{ $label .'_1_WIDTH_P'}  = $hash{ $label .'_WIDTH_P'};
	$ref->{ $label .'_1_MARGIN'}   = $hash{ $label .'_MARGIN'};
	$ref->{ $label .'_1_MARGIN_P'} = $hash{ $label .'_MARGIN_P'};
	$ref->{ $label .'_1_ORDER'}    = $hash{ $label .'_ORDER'};
	$ref->{ $label .'_1_MAT'}      = $hash{ $label .'_MAT'};
	
	$ref->{ $label .'_2_WIDTH'}    = $hash{ $label .'_WIDTH'};
	$ref->{ $label .'_2_WIDTH_P'}  = $hash{ $label .'_WIDTH_P'};
	$ref->{ $label .'_2_MARGIN'}   = $hash{ $label .'_MARGIN'};
	$ref->{ $label .'_2_MARGIN_P'} = $hash{ $label .'_MARGIN_P'};
	$ref->{ $label .'_2_ORDER'}    = $hash{ $label .'_ORDER'};
	$ref->{ $label .'_2_MAT'}      = $hash{ $label .'_MAT'};
}
# ------------------------------------------------
sub UpdateSignal
{
	my( $name, $ref, $label ) = @_;

	my %hash;
	LoadMod( \%hash, $name );

	$ref->{ $label .'_WIDTH'}    = $hash{ $label .'_WIDTH'};
	$ref->{ $label .'_WIDTH_P'}  = $hash{ $label .'_WIDTH_P'};
	$ref->{ $label .'_MARGIN'}   = $hash{ $label .'_MARGIN'};
	$ref->{ $label .'_MARGIN_P'} = $hash{ $label .'_MARGIN_P'};
	$ref->{ $label .'_ORDER'}    = $hash{ $label .'_ORDER'};
	$ref->{ $label .'_MAT'}      = $hash{ $label .'_MAT'};
}
# ------------------------------------------------
sub KeyOrder
{
	my $ref = shift;
	my @arr;
	
	foreach my $key ( sort{$a<=>$b} keys %{$ref} )
	{
		push @arr, ($ref->{$key});
	}
	
	return @arr;
}
# ------------------------------------------------
sub SaveModel
{
	my( $h, $name, $parameters_order ) = @_;
	
	my @arr = KeyOrder( $parameters_order );
	
	open( my $OUT, ">", $name ) or die "error on open file $0: $name\n$!\n";
	foreach my $key (@arr)
	{
		if( exists $h->{$key} )
		{
			if ( $h->{$key} =~ /\n/ )
			{
				print $OUT  ("\$". $key ."\n". $h->{$key} ."\n");
			}
			else
			{
				print $OUT  ("\$". $key ." ". $h->{$key} ."\n");
			}
		}
		else { print "error, required key is missing for model: $name\n"; exit 1; }
	}
	close $OUT;
}
# ------------------------------------------------
sub LoadFromFile
{
	my( $name, $ref ) = @_;

	my $data = '';
	
	open( my $IN, $name ) or die "error on open file $0: $name\n$!\n";
	while( <$IN> )
	{
		$data .= $_;
	}
	close $IN;
	
	$$ref = $data;
}
# ------------------------------------------------
sub LoadMod
{
	my( $ref, $name ) = @_;
	
	if(!$name) { print "error, file name is missing in LoadMod $0,\n"; exit 1;}
	print "load parameters from file: $name\n" if $v;
	
	my $data = '';
	
	open( my $IN, $name ) or die "error on open file $0: $name\n";
	while( my $line = <$IN> )
	{
		# remove comments
		$line =~ s/\#.*$/\n/;
		
		# ignore empty lines
		if ( $line =~ /^\s*$/) {next;}
		
		$data .= $line;
	}
	close $IN;
	
	my @tmp = split( '\$', $data );
	my $size = scalar  @tmp;
	
	for( my $i = 1; $i < $size; ++$i )
	{
		if ( $tmp[$i] =~ /^(\S+)\s*(\S+)\s*$/s )
		{
			$ref->{$1} = $2;
		}
		elsif ( $tmp[$i] =~ /^(\S+)\s*(\S+.*\S)\s*$/s )
		{
			$ref->{$1} = $2;
		}
		else { print "error, unexpected format found in file $name\n$tmp[$i]\n"; exit 1; }
	}
}
# ------------------------------------------------
sub LoadFromCFG
{
	my ( $target, $source ) = @_;
	
	while( my( $key, $value) = each( %{$source}) )
	{
		if ($value ne "-1")
		{
			$target->{$key} = $value;
		}
	}
}
# ------------------------------------------------
sub CheckInput
{
	if( ! $cfg->{$section}->{'def'} )  { print "error, default  file is not specified $0\n"; exit 1; }

	$cfg->{$section}->{'def'} = ResolvePath( $cfg->{$section}->{'def'} );
}
# ------------------------------------------------
sub ParseCMD
{
	my $cmd = $0;
	foreach my $str (@ARGV) { $cmd .= ( ' '. $str ); }
	
	my %h;
	my $opt_result = GetOptions
	(
		\%h,
		'def=s',
		'cfg=s',
		'out=s'     => \$out,
		'section=s' => \$section,
		'verbose'   => \$v,
		'debug'     => \$debug
	);
	
	if( !$opt_result ) { print "error on command line\n"; exit 1; }
	if( @ARGV > 0 ) { print "error, unexpected argument found on command line: @ARGV\n"; exit 1; }
	$v = 1 if $debug;

	SetDefaultConfiguration( $section );
	
	if( $h{'cfg'} )
	{
		$cfg->{$section}->{'cfg'} = ReadCfgFile( $h{'cfg'} );
	}

	foreach my $key ( keys %h )
	{
		$cfg->{$section}->{$key} = $h{$key};
	}
	
	$cfg->{$section}->{'cmd'} = $cmd;
}
# ------------------------------------------------
sub ReadCfgFile
{
	my( $name, $path ) = @_;
	return '' if !$name;

	print "read config file: $name\n" if $debug;
	
	$name = ResolvePath( $name, $path );
	
	Hash::Merge::set_behavior( 'RIGHT_PRECEDENT' );
	%$cfg = %{ merge( $cfg, YAML::LoadFile( $name )) };
	
	return $name;
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
sub SetDefaultConfiguration
{
	my $label = shift;
	
	# file names
	$cfg->{$label}->{'def'} = ''; # required
	# file name
	$cfg->{$label}->{'cfg'} = '';
}
# ------------------------------------------------
sub Usage
{
	print qq(# -------------------
Usage: $0   parameters

  --def     [name]  read def model from here
  --out     [name]  new model

  --cfg     [name] read parameters from this file
  --section [label] use this section from configuration file
  --verbose
  --debug
# -------------------
);
	exit 1;
}
# ------------------------------------------------
