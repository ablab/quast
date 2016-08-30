#!/usr/bin/perl
# ==============================================================
# Alex Lomsadze
#
# GeneMark-ES Suite version 4.*
#
# Last modified: April, 2015
#
# Georgia Institute of Technology, Atlanta, Georgia, US
#
# Dr. Mark Borodovsky Bioinformatics Lab
#
# Affiliation:
#  * Joint Georgia Tech and Emory Wallace H Coulter Department of Biomedical Engineering;
#  * Center for Bioinformatics and Computational Genomics at Georgia Tech;
#  * School of Computational Science and Engineering at Georgia Tech;
#
# This eukaryotic gene prediction suite contains:
#   GeneMark.hmm,  GeneMark-ES,  GeneMark-ET,  GeneMark-EP  and  *.PLUS modules
#
# Name of this package:  gmes_petap.pl
#   GeneMark.hmm  -> gm
#   Eukaryotic    -> e
#   Self-training -> s
#   Plus          -> p
#   Evidence      -> e
#   Transcripts   -> t
#     And         -> a
#   Proteins      -> p
#
# ==============================================================
# Algorithms included into this package were described in the following publications:
#
# Gene prediction algorithm Genemark-ET
#    Lomsadze A., Burns P. and  Borodovsky M.
#    "Integration of RNA-Seq Data into Eukaryotic Gene Finding Algorithm
#     with Semi-Supervised Training."
#    Nucleic Acids Research, 2014, July 2
#
# Gene prediction algorithm GeneMark.hmm ES BP version 2.0
#    Ter-Hovhannisyan V., Lomsadze A., Chernoff Y. and Borodovsky M.
#    "Gene prediction in novel fungal genomes using an ab initio
#     algorithm with unsupervised training."
#    Genome Research, 2008, Dec 18(12):1979-90
#
# Gene prediction algorithm GeneMark-ES version 1.0
#    Lomsadze A., Ter-Hovhannisyan V., Chernoff Y. and Borodovsky M.
#    "Gene identification in novel eukaryotic genomes by
#     self-training algorithm."
#    Nucleic Acids Research, 2005, Vol. 33, No. 20, 6494-6506
# ==============================================================
# Copyright:
#   Georgia Institute of Technology, Atlanta, Georgia, USA
#
# Please report problems to:
#   Alex Lomsadze alexl@gatech.edu
#   Mark Borodovsky borodovsky@gatech.edu
# ==============================================================

# ---
# to do:
#  fix DataReport
# warning, ET-score threshold is declared in two sections: 'Parameters' and 'ET_ini'
# ---

use strict;
use warnings;

# PERL modules from standard distribution
use Getopt::Long qw( GetOptions );
use FindBin qw( $RealBin );
use File::Spec;
use File::Path qw( make_path );
use Cwd qw( abs_path cwd );
use Data::Dumper;

# some modules from CPAN are installed locally
use lib './lib';
use YAML;
# modules from CPAN
use Hash::Merge qw( merge );
use Logger::Simple;
use Parallel::ForkManager;

# ------------------------------------------------
my $v = 0;
my $debug = 0;
# system control
my $cfg;  # parameters are stored here; reference to hash of hashes
my $log;  # reference to logger
my $key_bin = 0;
# ------------------------------------------------
my $bin = $RealBin;   # code directory   
my $work_dir = cwd;  # use directory to store temporary and output files
# ------------------------------------------------

ReadParameters();

CreateDirectories()   if $cfg->{'Run'}->{'set_dirs'};
CommitData()          if $cfg->{'Run'}->{'commit_input_data'};
DataReport()          if $cfg->{'Run'}->{'input_data_report'};
CommitTrainingData()  if $cfg->{'Run'}->{'commit_training_data'};
TrainingDataReport()  if $cfg->{'Run'}->{'training_data_report'};

print Dumper($cfg)    if $debug;

PrepareInitialModel() if $cfg->{'Run'}->{'prepare_ini_mod'};

if (  $cfg->{'Run'}->{'run_training'} )
{	
	if( $cfg->{'Parameters'}->{'ES'} )
	{
		RunIterations('ES_A', \&Training_ES_A ) if $cfg->{'ES_A'}->{'iterations'};
		RunIterations('ES_B', \&Training_ES_B ) if $cfg->{'ES_B'}->{'iterations'};
		RunIterations('ES_C', \&Training_ES_C ) if $cfg->{'ES_C'}->{'iterations'};
	}
	elsif( $cfg->{'Parameters'}->{'ET'} )
	{
		RunIterations('ET_A', \&Training_ET_A ) if $cfg->{'ET_A'}->{'iterations'};
		RunIterations('ET_B', \&Training_ET_B ) if $cfg->{'ET_B'}->{'iterations'};
		RunIterations('ET_C', \&Training_ET_C ) if $cfg->{'ET_C'}->{'iterations'};
	}
	else { print "error:\n"; exit 1; }
}

TrainingReportES() if ( $cfg->{'Run'}->{'training_report'} and  $cfg->{'Parameters'}->{'ES'} );
TrainingReportET() if ( $cfg->{'Run'}->{'training_report'} and  $cfg->{'Parameters'}->{'ET'} );

if( $cfg->{'Run'}->{'run_prediction'} )
{	
	my $mod = '';
	
	$mod = ResolvePath( $cfg->{'ES_C'}->{ 'out_mod'}, "run" ) if $cfg->{'Parameters'}->{'ES'};
	$mod = ResolvePath( $cfg->{'ET_C'}->{ 'out_mod'}, "run" ) if $cfg->{'Parameters'}->{'ET'};
	
	PredictGenes( $mod, $cfg->{'Parameters'}->{'min_gene_prediction'} );
}

PredictionReport() if $cfg->{'Run'}->{'prediction_report'};

exit 0;

# ================= subs =========================
sub BuildInitialModelET
{
	print "build initial ET model\n" if $v;
	
	my $mod = shift;
	chdir $work_dir;
	
	my $dir_for_build = "run/ET_ini";
	$dir_for_build = SetDir( $dir_for_build );
	
	chdir "data";
	RunCom("$bin/parse_by_introns.pl  --section ET_ini  --cfg  $cfg->{'Config'}->{'run_cfg'}  --parse_dir $dir_for_build" );
	
	chdir $dir_for_build;
	RunCom( "$bin/make_nt_freq_mat.pl --cfg $cfg->{'Config'}->{'run_cfg'} --section donor_GT    --format DONOR " ); 
	RunCom( "$bin/make_nt_freq_mat.pl --cfg $cfg->{'Config'}->{'run_cfg'} --section acceptor_AG --format ACCEPTOR " );
	RunCom( "$bin/histogram.pl --cfg $cfg->{'Config'}->{'run_cfg'} --section intron_DUR");

	if( $cfg->{'Parameters'}->{'fungus'} )
	{
		RunCom( "$bin/make_nt_freq_mat.pl --cfg $cfg->{'Config'}->{'run_cfg'} --section acceptor_short_AG --format ACC_BP " );

		# run Gibss3 on subset of introns
		RunCom( "$bin/bp_seq_select.pl --seq_in $cfg->{'ET_ini'}->{'bp_region'} --seq_out $cfg->{'ET_ini'}->{'gibbs_seq'}  --max_seq $cfg->{'ET_ini'}->{'gibbs_seq_max'}  --bp_region_length  $cfg->{'ET_ini'}->{'bp_region_length'} " );
		
		# 9     motif length
		# -n    Use nucleic acid alphabet
		# -r    turn off reverse complements with DNA
		# -nopt Don't print Near Optimal output
		# -m    Do not maximize after near optimal sampling
		# -w    pseduocount weight
		# -Z    Don't write progress info
		# -s    random number generator seed
		
		RunCom( "$bin/Gibbs3  gibbs.seq  9 -n -r -o gibbs.out -nopt -m -w 0.001 -Z  -s 1 -P $bin/prior.bp -F" );

		RunCom( "$bin/parse_gibbs.pl --seq gibbs.seq  --gibbs gibbs.out --motif_seq $cfg->{'branch_point'}->{'infile'}  --spacer_len $cfg->{'spacer_DUR'}->{'in'}  --spacer_seq spacer.seq  --tr $cfg->{'ET_ini'}->{'tr_bp'}  "  );
		RunCom( "$bin/make_nt_freq_mat.pl --cfg $cfg->{'Config'}->{'run_cfg'} --section branch_point --format BRANCH " );
		RunCom( "$bin/histogram.pl --cfg $cfg->{'Config'}->{'run_cfg'} --section spacer_DUR");
		
		RunCom( "$bin/scan_for_bp.pl --seq_in $cfg->{'ET_ini'}->{'bp_region'}  --gibbs_in gibbs.out  --pos_out $cfg->{'prespacer_DUR'}->{'in'} ");
		RunCom( "$bin/histogram.pl --cfg $cfg->{'Config'}->{'run_cfg'} --section prespacer_DUR");

		my $str = " --MKCHAIN_L_MARGING 0  --MKCHAIN_R_MARGING 0  --MKCHAIN_PSEUDOCOUNTS 1  --MKCHAIN_PRECISION 6 --ORDM 1 ";
		RunCom( "$bin/probuild --non spacer.seq --mkmod_non spacer.mkch  $str" );
	}

	RunCom( "$bin/build_mod.pl --cfg $cfg->{'Config'}->{'run_cfg'}  --section ET_ini --def $mod ");
	
	$mod = ResolvePath( $cfg->{'ET_ini'}->{'mod'} );
	
	chdir $work_dir;
	
	return $mod;
}
# ------------------------------------------------
sub BuildInitialModelES
{
	print "build initial ES model\n" if $v;
	
	my $mod = shift;
	chdir $work_dir;
	
	my $dir_for_build = "run/ES_ini";
	$dir_for_build = SetDir( $dir_for_build );
	
	chdir $dir_for_build;
	RunCom( "$bin/build_mod.pl --cfg $cfg->{'Config'}->{'run_cfg'}  --section ES_ini --def $mod ");
	
	$mod = ResolvePath( $cfg->{'ES_ini'}->{'mod'} );
	
	chdir $work_dir;
	
	return $mod;
}
# ------------------------------------------------
sub Training_ET_C
{
	my ( $path, $name ) = @_;
	print "training level ET_C: $path\n" if $v;
	
	chdir $path; 
	RunCom("$bin/parse_ET.pl --section ET_C --cfg  $cfg->{'Config'}->{'run_cfg'}  --v" );
	
	RunCom( "$bin/make_nt_freq_mat.pl --cfg $cfg->{'Config'}->{'run_cfg'} --section start_ATG  --format INI" ); 
	RunCom( "$bin/make_nt_freq_mat.pl --cfg $cfg->{'Config'}->{'run_cfg'} --section stop_TAA   --format TERM_TAA" );
	RunCom( "$bin/make_nt_freq_mat.pl --cfg $cfg->{'Config'}->{'run_cfg'} --section stop_TAG   --format TERM_TAG" );
	RunCom( "$bin/make_nt_freq_mat.pl --cfg $cfg->{'Config'}->{'run_cfg'} --section stop_TGA   --format TERM_TGA" );

	my $str = " --MKCHAIN_L_MARGING 0  --MKCHAIN_R_MARGING 0  --MKCHAIN_PSEUDOCOUNTS 1  --MKCHAIN_PRECISION 8";
	$str .= " --revcomp_non  --ORDM 5 ";
	RunCom( "$bin/probuild --cod cod.seq --non non.seq --mkmod_euk mkch   $str" );
	
	RunCom( "$bin/histogram.pl --cfg $cfg->{'Config'}->{'run_cfg'} --section initial_DUR");
	RunCom( "$bin/histogram.pl --cfg $cfg->{'Config'}->{'run_cfg'} --section internal_DUR");
	RunCom( "$bin/histogram.pl --cfg $cfg->{'Config'}->{'run_cfg'} --section terminal_DUR");
	RunCom( "$bin/histogram.pl --cfg $cfg->{'Config'}->{'run_cfg'} --section single_DUR");
	
	RunCom( "$bin/make_nt_freq_mat.pl --cfg $cfg->{'Config'}->{'run_cfg'} --section donor_GT    --format DONOR_0    --phase 0 " ); 
	RunCom( "$bin/make_nt_freq_mat.pl --cfg $cfg->{'Config'}->{'run_cfg'} --section acceptor_AG --format ACCEPTOR_0 --phase 0 " );
	RunCom( " cat  GT.mat > donor.mat " );
	RunCom( " cat  AG.mat > acceptor.mat " );
	
	RunCom( "$bin/make_nt_freq_mat.pl --cfg $cfg->{'Config'}->{'run_cfg'} --section donor_GT    --format DONOR_1    --phase 1 " ); 
	RunCom( "$bin/make_nt_freq_mat.pl --cfg $cfg->{'Config'}->{'run_cfg'} --section acceptor_AG --format ACCEPTOR_1 --phase 1 " );
	RunCom( " cat  GT.mat >> donor.mat " );
	RunCom( " cat  AG.mat >> acceptor.mat " );
	
	RunCom( "$bin/make_nt_freq_mat.pl --cfg $cfg->{'Config'}->{'run_cfg'} --section donor_GT    --format DONOR_2    --phase 2 " ); 
	RunCom( "$bin/make_nt_freq_mat.pl --cfg $cfg->{'Config'}->{'run_cfg'} --section acceptor_AG --format ACCEPTOR_2 --phase 2 " );
	RunCom( " cat  GT.mat >> donor.mat " );
	RunCom( " cat  AG.mat >> acceptor.mat " );
	
	RunCom( " mv donor.mat     GT.mat " );
	RunCom( " mv acceptor.mat  AG.mat " );
	
	if( $cfg->{'Parameters'}->{'fungus'} )
	{
		RunCom( "$bin/make_nt_freq_mat.pl --cfg $cfg->{'Config'}->{'run_cfg'} --section acceptor_short_AG --format ACC_BP_0 --phase 0 " );
		RunCom( " cat AG_SHORT.mat >  acceptor_short.mat " );
		RunCom( "$bin/make_nt_freq_mat.pl --cfg $cfg->{'Config'}->{'run_cfg'} --section acceptor_short_AG --format ACC_BP_1 --phase 1 " );
		RunCom( " cat AG_SHORT.mat >> acceptor_short.mat " );
		RunCom( "$bin/make_nt_freq_mat.pl --cfg $cfg->{'Config'}->{'run_cfg'} --section acceptor_short_AG --format ACC_BP_2 --phase 2 " );
		RunCom( " cat AG_SHORT.mat >> acceptor_short.mat " );
		RunCom( " mv acceptor_short.mat  AG_SHORT.mat " );
	}
	
	RunCom( "$bin/build_mod.pl --cfg $cfg->{'Config'}->{'run_cfg'} --section ET_C --def prev.mod  --out $name ");
	
	chdir $work_dir;
}
# ------------------------------------------------
sub Training_ET_B
{
	my ( $path, $name ) = @_;
	print "training level ET_B: $path\n" if $v;

	chdir $path; 
	RunCom("$bin/parse_ET.pl --section ET_B --cfg  $cfg->{'Config'}->{'run_cfg'}  --v" );
	
	RunCom( "$bin/make_nt_freq_mat.pl --cfg $cfg->{'Config'}->{'run_cfg'} --section start_ATG  --format INI" ); 
	RunCom( "$bin/make_nt_freq_mat.pl --cfg $cfg->{'Config'}->{'run_cfg'} --section stop_TAA   --format TERM_TAA" );
	RunCom( "$bin/make_nt_freq_mat.pl --cfg $cfg->{'Config'}->{'run_cfg'} --section stop_TAG   --format TERM_TAG" );
	RunCom( "$bin/make_nt_freq_mat.pl --cfg $cfg->{'Config'}->{'run_cfg'} --section stop_TGA   --format TERM_TGA" );	

	my $str = " --MKCHAIN_L_MARGING 0  --MKCHAIN_R_MARGING 0  --MKCHAIN_PSEUDOCOUNTS 1  --MKCHAIN_PRECISION 8";
	$str .= " --revcomp_non  --ORDM 5 ";
	RunCom( "$bin/probuild --cod cod.seq --non non.seq --mkmod_euk mkch   $str" );
	
	RunCom( "$bin/histogram.pl --cfg $cfg->{'Config'}->{'run_cfg'} --section initial_DUR");
	RunCom( "$bin/histogram.pl --cfg $cfg->{'Config'}->{'run_cfg'} --section internal_DUR");
	RunCom( "$bin/histogram.pl --cfg $cfg->{'Config'}->{'run_cfg'} --section terminal_DUR");
	RunCom( "$bin/histogram.pl --cfg $cfg->{'Config'}->{'run_cfg'} --section single_DUR");
	
	RunCom( "$bin/build_mod.pl --cfg $cfg->{'Config'}->{'run_cfg'} --section ET_B --def prev.mod  --out $name ");
	
	chdir $work_dir;
}
# ------------------------------------------------
sub Training_ET_A
{
	my ( $path, $name ) = @_;
	print "training level ET_A: $path\n" if $v;
	
	chdir $path; 
	RunCom("$bin/parse_ET.pl --section ET_A --cfg  $cfg->{'Config'}->{'run_cfg'}  --v" );

	RunCom( "$bin/make_nt_freq_mat.pl --cfg $cfg->{'Config'}->{'run_cfg'} --section start_ATG  --format INI" ); 
	RunCom( "$bin/make_nt_freq_mat.pl --cfg $cfg->{'Config'}->{'run_cfg'} --section stop_TAA   --format TERM_TAA" );
	RunCom( "$bin/make_nt_freq_mat.pl --cfg $cfg->{'Config'}->{'run_cfg'} --section stop_TAG   --format TERM_TAG" );
	RunCom( "$bin/make_nt_freq_mat.pl --cfg $cfg->{'Config'}->{'run_cfg'} --section stop_TGA   --format TERM_TGA" );

	my $str = " --MKCHAIN_L_MARGING 0  --MKCHAIN_R_MARGING 0  --MKCHAIN_PSEUDOCOUNTS 1  --MKCHAIN_PRECISION 8";
	$str .= " --revcomp_non  --ORDM 5 ";
	RunCom( "$bin/probuild --cod cod.seq --non non.seq --mkmod_euk mkch   $str" );
	
	RunCom( "$bin/build_mod.pl --cfg $cfg->{'Config'}->{'run_cfg'} --section ET_A --def prev.mod  --out $name ");
	
	chdir $work_dir;
}
# ------------------------------------------------
sub Training_ES_C
{
	my ( $path, $name ) = @_;
	print "training level ES_C: $path\n" if $v;
	chdir $work_dir;

	chdir $path;
	RunCom("$bin/parse_set.pl --section ES_C --cfg  $cfg->{'Config'}->{'run_cfg'}  --v" );

	RunCom( "$bin/make_nt_freq_mat.pl --cfg $cfg->{'Config'}->{'run_cfg'} --section start_ATG   --format INI" ); 
	RunCom( "$bin/make_nt_freq_mat.pl --cfg $cfg->{'Config'}->{'run_cfg'} --section stop_TAA    --format TERM_TAA" );
	RunCom( "$bin/make_nt_freq_mat.pl --cfg $cfg->{'Config'}->{'run_cfg'} --section stop_TAG    --format TERM_TAG" );
	RunCom( "$bin/make_nt_freq_mat.pl --cfg $cfg->{'Config'}->{'run_cfg'} --section stop_TGA    --format TERM_TGA" );

	RunCom( "$bin/make_nt_freq_mat.pl --cfg $cfg->{'Config'}->{'run_cfg'} --section donor_GT    --format DONOR_0    --phase 0 " ); 
	RunCom( "$bin/make_nt_freq_mat.pl --cfg $cfg->{'Config'}->{'run_cfg'} --section acceptor_AG --format ACCEPTOR_0 --phase 0 " );
	RunCom( " cat  GT.mat > donor.mat " );
	RunCom( " cat  AG.mat > acceptor.mat " );
	
	RunCom( "$bin/make_nt_freq_mat.pl --cfg $cfg->{'Config'}->{'run_cfg'} --section donor_GT    --format DONOR_1    --phase 1 " ); 
	RunCom( "$bin/make_nt_freq_mat.pl --cfg $cfg->{'Config'}->{'run_cfg'} --section acceptor_AG --format ACCEPTOR_1 --phase 1 " );
	RunCom( " cat  GT.mat >> donor.mat " );
	RunCom( " cat  AG.mat >> acceptor.mat " );
	
	RunCom( "$bin/make_nt_freq_mat.pl --cfg $cfg->{'Config'}->{'run_cfg'} --section donor_GT    --format DONOR_2    --phase 2 " ); 
	RunCom( "$bin/make_nt_freq_mat.pl --cfg $cfg->{'Config'}->{'run_cfg'} --section acceptor_AG --format ACCEPTOR_2 --phase 2 " );
	RunCom( " cat  GT.mat >> donor.mat " );
	RunCom( " cat  AG.mat >> acceptor.mat " );
	
	RunCom( " mv donor.mat     GT.mat " );
	RunCom( " mv acceptor.mat  AG.mat " );

	my $str = " --MKCHAIN_L_MARGING 0  --MKCHAIN_R_MARGING 0  --MKCHAIN_PSEUDOCOUNTS 1  --MKCHAIN_PRECISION 8";
	$str .= " --revcomp_non  --ORDM 5 ";
	RunCom( "$bin/probuild --cod cod.seq --non non.seq --mkmod_euk mkch   $str" );
	
	RunCom( "$bin/histogram.pl --cfg $cfg->{'Config'}->{'run_cfg'} --section intron_DUR");
	RunCom( "$bin/histogram.pl --cfg $cfg->{'Config'}->{'run_cfg'} --section initial_DUR");
	RunCom( "$bin/histogram.pl --cfg $cfg->{'Config'}->{'run_cfg'} --section internal_DUR");
	RunCom( "$bin/histogram.pl --cfg $cfg->{'Config'}->{'run_cfg'} --section terminal_DUR");
	RunCom( "$bin/histogram.pl --cfg $cfg->{'Config'}->{'run_cfg'} --section single_DUR");
	
	if( $cfg->{'Parameters'}->{'fungus'} )
	{
		RunCom( "$bin/make_nt_freq_mat.pl --cfg $cfg->{'Config'}->{'run_cfg'} --section acceptor_short_AG --format ACC_BP_0 --phase 0 " );
		RunCom( " cat AG_SHORT.mat >  acceptor_short.mat " );
		RunCom( "$bin/make_nt_freq_mat.pl --cfg $cfg->{'Config'}->{'run_cfg'} --section acceptor_short_AG --format ACC_BP_1 --phase 1 " );
		RunCom( " cat AG_SHORT.mat >> acceptor_short.mat " );
		RunCom( "$bin/make_nt_freq_mat.pl --cfg $cfg->{'Config'}->{'run_cfg'} --section acceptor_short_AG --format ACC_BP_2 --phase 2 " );
		RunCom( " cat AG_SHORT.mat >> acceptor_short.mat " );
		RunCom( " mv acceptor_short.mat  AG_SHORT.mat " );
		
		# run Gibss3 on subset of introns
		RunCom( "$bin/bp_seq_select.pl --seq_in $cfg->{'ES_C'}->{'bp_region'} --seq_out $cfg->{'ES_C'}->{'gibbs_seq'}  --max_seq $cfg->{'ES_C'}->{'gibbs_seq_max'}  --bp_region_length  $cfg->{'ES_C'}->{'bp_region_length'} " );
		
		# 9     motif length
		# -n    Use nucleic acid alphabet
		# -r    turn off reverse complements with DNA
		# -nopt Don't print Near Optimal output
		# -m    Do not maximize after near optimal sampling
		# -w    pseduocount weight
		# -Z    Don't write progress info
		# -s    random number generator seed
		
		RunCom( "$bin/Gibbs3  gibbs.seq  9 -n -r -o gibbs.out -nopt -m -w 0.001 -Z  -s 1 -P $bin/prior.bp -F" );

		RunCom( "$bin/parse_gibbs.pl --seq gibbs.seq  --gibbs gibbs.out --motif_seq $cfg->{'branch_point'}->{'infile'}  --spacer_len $cfg->{'spacer_DUR'}->{'in'}  --spacer_seq spacer.seq  --tr $cfg->{'ES_C'}->{'tr_bp'}  "  );
		RunCom( "$bin/make_nt_freq_mat.pl --cfg $cfg->{'Config'}->{'run_cfg'} --section branch_point --format BRANCH " );
		RunCom( "$bin/histogram.pl --cfg $cfg->{'Config'}->{'run_cfg'} --section spacer_DUR");
		
		RunCom( "$bin/scan_for_bp.pl --seq_in $cfg->{'ES_C'}->{'bp_region'}  --gibbs_in gibbs.out  --pos_out $cfg->{'prespacer_DUR'}->{'in'} ");
		RunCom( "$bin/histogram.pl --cfg $cfg->{'Config'}->{'run_cfg'} --section prespacer_DUR");

		my $str = " --MKCHAIN_L_MARGING 0  --MKCHAIN_R_MARGING 0  --MKCHAIN_PSEUDOCOUNTS 1  --MKCHAIN_PRECISION 6 --ORDM 1 ";
		RunCom( "$bin/probuild --non spacer.seq --mkmod_non spacer.mkch  $str" );
	}
	
	RunCom( "$bin/build_mod.pl --cfg $cfg->{'Config'}->{'run_cfg'} --section ES_C --def prev.mod  --out $name ");
	
	chdir $work_dir;
}
# ------------------------------------------------
sub Training_ES_B
{
	my ( $path, $name ) = @_;
	print "training level ES_B: $path\n" if $v;
	chdir $work_dir;
	
	chdir $path; 
	RunCom("$bin/parse_set.pl --section ES_B --cfg  $cfg->{'Config'}->{'run_cfg'}  --v" );

	RunCom( "$bin/make_nt_freq_mat.pl --cfg $cfg->{'Config'}->{'run_cfg'} --section start_ATG   --format INI" ); 
	RunCom( "$bin/make_nt_freq_mat.pl --cfg $cfg->{'Config'}->{'run_cfg'} --section stop_TAA    --format TERM_TAA" );
	RunCom( "$bin/make_nt_freq_mat.pl --cfg $cfg->{'Config'}->{'run_cfg'} --section stop_TAG    --format TERM_TAG" );
	RunCom( "$bin/make_nt_freq_mat.pl --cfg $cfg->{'Config'}->{'run_cfg'} --section stop_TGA    --format TERM_TGA" );

	RunCom( "$bin/make_nt_freq_mat.pl --cfg $cfg->{'Config'}->{'run_cfg'} --section donor_GT    --format DONOR_0    --phase 0 " ); 
	RunCom( "$bin/make_nt_freq_mat.pl --cfg $cfg->{'Config'}->{'run_cfg'} --section acceptor_AG --format ACCEPTOR_0 --phase 0 " );
	RunCom( " cat  GT.mat > donor.mat " );
	RunCom( " cat  AG.mat > acceptor.mat " );
	
	RunCom( "$bin/make_nt_freq_mat.pl --cfg $cfg->{'Config'}->{'run_cfg'} --section donor_GT    --format DONOR_1    --phase 1 " ); 
	RunCom( "$bin/make_nt_freq_mat.pl --cfg $cfg->{'Config'}->{'run_cfg'} --section acceptor_AG --format ACCEPTOR_1 --phase 1 " );
	RunCom( " cat  GT.mat >> donor.mat " );
	RunCom( " cat  AG.mat >> acceptor.mat " );
	
	RunCom( "$bin/make_nt_freq_mat.pl --cfg $cfg->{'Config'}->{'run_cfg'} --section donor_GT    --format DONOR_2    --phase 2 " ); 
	RunCom( "$bin/make_nt_freq_mat.pl --cfg $cfg->{'Config'}->{'run_cfg'} --section acceptor_AG --format ACCEPTOR_2 --phase 2 " );
	RunCom( " cat  GT.mat >> donor.mat " );
	RunCom( " cat  AG.mat >> acceptor.mat " );
	
	RunCom( " mv donor.mat     GT.mat " );
	RunCom( " mv acceptor.mat  AG.mat " );

	my $str = " --MKCHAIN_L_MARGING 0  --MKCHAIN_R_MARGING 0  --MKCHAIN_PSEUDOCOUNTS 1  --MKCHAIN_PRECISION 8";
	$str .= " --revcomp_non  --ORDM 5 ";
	RunCom( "$bin/probuild --cod cod.seq --non non.seq --mkmod_euk mkch   $str" );
	
	RunCom( "$bin/histogram.pl --cfg $cfg->{'Config'}->{'run_cfg'} --section intron_DUR");
	RunCom( "$bin/histogram.pl --cfg $cfg->{'Config'}->{'run_cfg'} --section initial_DUR");
	RunCom( "$bin/histogram.pl --cfg $cfg->{'Config'}->{'run_cfg'} --section internal_DUR");
	RunCom( "$bin/histogram.pl --cfg $cfg->{'Config'}->{'run_cfg'} --section terminal_DUR");
	RunCom( "$bin/histogram.pl --cfg $cfg->{'Config'}->{'run_cfg'} --section single_DUR");
	
	RunCom( "$bin/build_mod.pl --cfg $cfg->{'Config'}->{'run_cfg'} --section ES_B --def prev.mod  --out $name ");
	
	chdir $work_dir;
}
# ------------------------------------------------
sub Training_ES_A
{
	my ( $path, $name ) = @_;
	print "training level ES_A: $path\n" if $v;
	chdir $work_dir;
	
	chdir $path; 
	RunCom("$bin/parse_set.pl --section ES_A --cfg  $cfg->{'Config'}->{'run_cfg'}  --v " );

	RunCom( "$bin/make_nt_freq_mat.pl --cfg $cfg->{'Config'}->{'run_cfg'} --section start_ATG   --format INI" ); 
	RunCom( "$bin/make_nt_freq_mat.pl --cfg $cfg->{'Config'}->{'run_cfg'} --section stop_TAA    --format TERM_TAA" );
	RunCom( "$bin/make_nt_freq_mat.pl --cfg $cfg->{'Config'}->{'run_cfg'} --section stop_TAG    --format TERM_TAG" );
	RunCom( "$bin/make_nt_freq_mat.pl --cfg $cfg->{'Config'}->{'run_cfg'} --section stop_TGA    --format TERM_TGA" );

	RunCom( "$bin/make_nt_freq_mat.pl --cfg $cfg->{'Config'}->{'run_cfg'} --section donor_GT    --format DONOR " ); 
	RunCom( "$bin/make_nt_freq_mat.pl --cfg $cfg->{'Config'}->{'run_cfg'} --section acceptor_AG --format ACCEPTOR " );

	my $str = " --MKCHAIN_L_MARGING 0  --MKCHAIN_R_MARGING 0  --MKCHAIN_PSEUDOCOUNTS 1  --MKCHAIN_PRECISION 8";
	$str .= " --revcomp_non  --ORDM 5 ";
	RunCom( "$bin/probuild --cod cod.seq --non non.seq --mkmod_euk mkch   $str" );
	
	RunCom( "$bin/build_mod.pl --cfg $cfg->{'Config'}->{'run_cfg'} --section ES_A --def prev.mod  --out $name ");
	
	chdir $work_dir;
}
# ------------------------------------------------
sub ConcatenatePredictions
{
	my ( $path ) = @_;
	print "concatenate predictions: $path\n" if $v;
	
	my $dir = ResolvePath( "hmmout", $path );
	
	unlink "$path/set.out" if ( -e "$path/set.out" );
	
	opendir( DIR, $dir );
	foreach  my $file ( grep{ /dna\.fa_\d+\.out$/ } readdir(DIR) )
	{
		$file = "$dir/$file";
		my $com = "cat $file >> $path/set.out";
		system( $com ) && die"$!\n";
		
		unlink $file;
	}
	closedir DIR;
}
# ------------------------------------------------
sub SetDir
{
	my $dir = shift;
	mkdir $dir;
	if ( ! -d $dir) { print "error, directory not found: $dir\n"; exit 1; }
	return abs_path( $dir );
}
# ------------------------------------------------
sub RunIterations
{
	my ( $label , $function ) = @_;
	print "running step $label\n" if $v;
	
	my $current_mod = ResolvePath( $cfg->{$label}->{'in_mod'}, 'run' );
	my $new_mod;
		
	my $end = $cfg->{$label}->{'iterations'};
	foreach my $i (1..$end)
	{
		my $training_dir = SetDir( "run/$label" ."_". $i );
		my $hmmout_dir   = SetDir( "$training_dir/hmmout" );
		
		if ( $cfg->{$label}->{'run_prediction'} )
		{
			RunHmm( $current_mod, $hmmout_dir, '', '', '' );
			ConcatenatePredictions( $training_dir );
		}
		
		$new_mod = "$label\_$i.mod";
		if (  $cfg->{$label}->{'run_training'} )
		{
			RunCom( "ln -sf $current_mod  $training_dir/prev.mod" );
			$function->( $training_dir, $new_mod );
		}
		$current_mod = ResolvePath( $new_mod, $training_dir );
	}
	
	my $out_mod = $cfg->{$label}->{'out_mod'};
	$out_mod = File::Spec->catfile( 'run', $out_mod );
	symlink $current_mod, $out_mod;
}
# ------------------------------------------------
sub PredictGenes
{
	my( $name, $min ) = @_;
	print "predict final gene set\n" if $v;
	chdir $work_dir;
	
	# copy parameter file to output
	if( !$name ) { print "error, final parameter file not specified $0\n"; exit 1; }
	if( ! -e $name ) { print "error, final parameter file not found $0: $name\n"; exit 1; }
	RunCom( "cp $name output/gmhmm.mod" );
	
	# split sequence for prediction step
	my $dir = "output/data"; 
	mkdir $dir;
	SplitFasta( abs_path( "data/dna.fna" ) , $dir, $cfg->{'Parameters'}->{'min_contig_prediction'}, "prediction.trace" );	
	chdir $work_dir;	

	my $evi = '';

	# evidence
	if( $cfg->{'Parameters'}->{'evidence'} )
	{
		RunCom( "$bin/verify_evidence_gmhmm.pl --in data/evidence.gff  --out data/evidence_hmm.gff  --mod  output/gmhmm.mod " );
		RunCom( "$bin/rescale_gff.pl  --in data/evidence_hmm.gff  --trace info/prediction.trace  --out $dir/evidence_prediction.gff" );
		$evi = abs_path( "$dir/evidence_prediction.gff" );
	}
	
	# predict genes
	$dir = "output/gmhmm";
	mkdir $dir;
	
	RunHmm( abs_path( "output/gmhmm.mod" ), "output/gmhmm" , "output/data", ' -f lst -n ', $evi );

	# reformat from LST to GFF
	my @files = ReadContigNames( "output/data" );
	my $final =  "genemark.gtf";
	unlink $final;
	foreach my $f (@files)
	{
		$f = File::Spec->splitpath( $f ) .".out";
		$f = ResolvePath( $f, "output/gmhmm" );
		
		RunCom( "$bin/hmm_to_gtf.pl  --in $f  --app  --out $final  --min $min " );
	}
	
	RunCom( "$bin/reformat_gff.pl --out $final.tmp --trace info/dna.trace --in $final  --back" );
	RunCom( "mv $final.tmp $final" );
}
# ------------------------------------------------
sub RunHmmPBS
{
	my( $mod, $path_output, $path_input, $opt, $evi ) = @_;
	print "running gm.hmm on PBS\n" if $v;
	
	if( !$path_input )
	{
		$path_input = "$work_dir/data/training";
	}
	
	if( !$opt )
	{
		$opt = ' -f tr ';
	}
	
	if( $evi )
	{
		if( ! -e $evi  ) { print "error, evidence file not found\n"; exit 1;}
		
		$opt .= ' -d '. $evi .' ';
	}
	
	my $hmm_par = "\" -m $mod  $opt \"";
	
	my $com = "$bin/run_hmm_pbs.pl --par $hmm_par --out $path_output  --in $path_input  --w ";
	
	$log->write($com);
	RunComWithWarning( $com );
}
# ------------------------------------------------
sub RunComWithWarning
{
	my ( $com, $mess ) = @_;
	$mess = $com if !defined $mess;
	my $res = system( $com );
	if( $res )
	{
		print "warning on: $mess\n";
		$log->write( "warning on: $mess" );
	}

	$log->write( "warning on: $mess" ) if $debug;
}
# ------------------------------------------------
sub RunHmmLocal
{
	my( $mod, $path_output, $path_input, $opt, $evi ) = @_;
	print "running gm.hmm on local system\n" if $v;
	
	my $gm_hmm =  $cfg->{'Config'}->{'gm_hmm'};
	
	if( !$path_input )
	{
		$path_input = "$work_dir/data/training";
	}
	
	my @contigs = ReadContigNames( abs_path( $path_input ) );
	
	if( !$opt )
	{
		$opt = ' -f tr ';
	}
	
	if( $evi )
	{
		if( ! -e $evi  ) { print "error, evidence file not found\n"; exit 1;}
		
		$opt .= ' -d '. $evi .' ';
	}

	if( $key_bin )
	{
		$opt .= " -T $RealBin ";
	}
	
	foreach my $file ( @contigs )
	{
		print "$file\n" if $debug;
		
		my $out = File::Spec->splitpath( $file );
		$out = File::Spec->catfile( $path_output, $out .".out" );
		
		RunComWithWarning( "$gm_hmm  -m $mod  $opt  -o $out  $file");
	}
}
# ------------------------------------------------
sub RunHmmOnCores
{
	my( $mod, $path_output, $cores, $path_input, $opt, $evi ) = @_;
	print "running gm.hmm on local multi-core system\n" if $v;
	
	my $gm_hmm =  $cfg->{'Config'}->{'gm_hmm'};
	
	if( !$path_input )
	{
		$path_input = "$work_dir/data/training";
	}	
	
	my @contigs = ReadContigNames( abs_path( $path_input ) );

	if( !$opt )
	{
		$opt = ' -f tr ';
	}
	
	if( $key_bin )
	{
		$opt .= " -T $RealBin ";
	}
	
	if( $evi )
	{
		if( ! -e $evi  ) { print "error, evidence file not found\n"; exit 1;}
		
		$opt .= ' -d '. $evi .' ';
	}
	
	my $manager = new Parallel::ForkManager( $cores );
	
	for my $file (@contigs)
	{
		print "$file\n" if $debug;
		
		my $out = File::Spec->splitpath( $file );
		$out = File::Spec->catfile( $path_output, $out .".out" );

		$manager->start and next;
		RunComWithWarning( "$gm_hmm  -m $mod  $opt  -o $out  $file");
		$manager->finish;
	}
	
	$manager->wait_all_children;
}
# ------------------------------------------------
sub RunHmm
{
	my( $mod, $path_output, $path_input, $options, $evi ) = @_;

	if( $cfg->{'Parameters'}->{'pbs'} )
	{
		RunHmmPBS( $mod, $path_output, $path_input, $options, $evi );
	}
	else
	{
		if( $cfg->{'Parameters'}->{'cores'} > 1 )
		{
			RunHmmOnCores( $mod, $path_output, $cfg->{'Parameters'}->{'cores'}, $path_input, $options, $evi );
		}
		else
		{
			RunHmmLocal( $mod, $path_output, $path_input, $options, $evi );
		}
	}
}
# ------------------------------------------------
sub ReadContigNames
{
	my $dir = shift;
	my @list;

	opendir( DIR, $dir ) or die "error on open directory $0: $dir, $!\n";
	foreach my $file ( grep{ /dna.fa_\d+$/ } readdir(DIR) )
	{
		$file =  File::Spec->catfile( $dir, $file );
		if ( -f $file )
		{
			$file = abs_path($file);
			push @list, $file;
		}
		else { print "error, unexpected name found $0: $file\n"; exit 1; }
	}
	closedir DIR;

	my $message = scalar @list ." contigs in training";
	$log->write( $message );
 	print "$message\n" if $v;
 	
 	my @sorted_list =
 		map{ $_->[0] }
 		sort { $a->[1] <=> $b->[1] }
 		map { [$_, $_=~/(\d+)/] }
 			@list;
 	
 	return @sorted_list;
}
# ------------------------------------------------
sub GetHeuristicFileName
{
	my( $GC ) = @_;
	$GC = int $GC;

	my $MIN_HEURISTIC_GC = 32;
	my $MAX_HEURISTIC_GC = 70;
	if( $GC < $MIN_HEURISTIC_GC ) { $GC = $MIN_HEURISTIC_GC; }
	if( $GC > $MAX_HEURISTIC_GC ) { $GC = $MAX_HEURISTIC_GC; }
	
	return ResolvePath( "heu_05_gcode_1_gc_$GC.mod", $cfg->{'Config'}->{'heu_dir'} );
}
# ------------------------------------------------
sub GetGCfromStatfile
{
	print "get GC of sequence\n" if $v;
	
	my $name = shift;
	my $GC = 0;
	
	open( my $IN, $name ) or die "error on open file $0: $name, $!\n";
	while(<$IN>)
	{
		if( /^GC\s+(\S+)\s*$/ )
		{
			$GC = $1;
			last;
		}
	}
	close $IN;
	
	if ( !$GC ) { print "error, GC of sequence is zero $0\n"; exit 1; }
	
	return int( $GC + 0.5 );
}
# ------------------------------------------------
sub PrepareInitialModel
{
	print "prepare initial model\n" if $v;
	chdir $work_dir;
	
	my $ini_mod = $cfg->{'Parameters'}->{'ini_mod'};
	
	if( !$ini_mod )
	{
		# choose one of heuristic models
		# default method in ES and pre-request for ET
		
		my $GC = GetGCfromStatfile( "info/training.general" );
		$ini_mod = GetHeuristicFileName( $GC );
		
		# save info about heuristic model name here
		$cfg->{'Parameters'}->{'ini_mod'} = $ini_mod;
		
		print "GC $GC\n" if $v;
		
		if ( $cfg->{'Parameters'}->{'ET'} )
		{
			$ini_mod = BuildInitialModelET( $ini_mod );
		}
		
		if ( $cfg->{'Parameters'}->{'ES'} )
		{
			$ini_mod = BuildInitialModelES( $ini_mod );
		}
	}
	
	if ( !$ini_mod ) { print "error, initiation model file not specified $0\n"; exit 1; }
	if ( !-e $ini_mod ) { print "error, initiation model file not found $0: $ini_mod\n"; exit 1; }
	
	chdir $work_dir;
	RunCom( "ln -sf  $ini_mod  run/ini.mod" );
}
# ------------------------------------------------
sub TrainingDataReport
{
	print "training data report\n" if $v;
	chdir $work_dir;
	if ( ! -e "data/training.fna" ) { print "error, file not found: info/training.fna\n"; exit 1; }
	RunCom( "$bin/probuild --seq data/training.fna --stat info/training.general --allowx  --GC_PRECISION 0 ");
}
# ------------------------------------------------
sub SplitFasta
{
	my( $file, $dir, $min_contig, $trace_name ) = @_;
	
	chdir $work_dir;
	
	# remove old files if any
	opendir( DIR, $dir ) or die "error on open directory: $dir, $!\n";
	foreach my $file ( grep{ /dna.fa_\d+$/ } readdir(DIR) )
	{
		unlink "$dir/$file" or die "Could not unlink $file: $!";
	}
	closedir DIR;

	my $max_contig = $cfg->{'Parameters'}->{'max_contig'};
	my $max_gap    = $cfg->{'Parameters'}->{'max_gap'};
	my $max_mask   = $cfg->{'Parameters'}->{'max_mask'};
	
	chdir $dir;
	
	# --nnn_margin 3 in 2008 ES code
	
	RunCom( "$bin/probuild  --seq $file  --split dna.fa  --max_contig $max_contig --min_contig $min_contig --letters_per_line 100 --split_at_n $max_gap --split_at_x $max_mask --allowx --x_to_n  --trace ../../info/$trace_name " );
	chdir $work_dir;
}
# ------------------------------------------------
sub CommitTrainingData
{
	print "commit training data\n" if $v;
	
	chdir $work_dir;
	my $dir = "data/training";
	
	# this function splits sequence for training and another time for prediction steps
	# currently difference is only in the length of "ignored" short contigs
	# in training:   $min_contig from parameter section
	# in prediction: $min_contig_prediction
	
	SplitFasta( abs_path( "data/dna.fna" ), $dir, $cfg->{'Parameters'}->{'min_contig'}, "training.trace" );	
	chdir $work_dir;
	
	# remove old files if any
	if ( -e "data/training.fna" )
	{
		unlink "data/training.fna" or die "Could not unlink: $!";
	}
	
	opendir( DIR, $dir );
	foreach  my $file ( grep{ /dna.fa_\d+$/ } readdir(DIR) )
	{
		$file = "$dir/$file";
		my $com = "cat $file >> data/training.fna";
		system( $com ) && die "$!\n";
	}
	closedir DIR;
	
	# ET
	if( $cfg->{'Parameters'}->{'ET'} )
	{
		RunCom( "$bin/rescale_gff.pl  --in data/et.gff  --trace info/training.trace  --out data/et_training.gff" );
	}
	
	# evidence
	if( $cfg->{'Parameters'}->{'evidence'} )
	{
		RunCom( "$bin/rescale_gff.pl  --in data/evidence.gff  --trace info/training.trace  --out data/evidence_training.gff" );
	}	
}
# ------------------------------------------------
sub DataReport
{
	print "data report\n" if $v;
	chdir $work_dir;
	
	RunCom( "$bin/probuild  --seq data/dna.fna  --allowx  --stat info/dna.general" );
	RunCom( "$bin/probuild  --seq data/dna.fna  --allowx  --stat_fasta info/dna.multi_fasta" );
	RunCom( "$bin/probuild  --seq data/dna.fna  --allowx  --substring_n_distr info/dna.gap_distr" );
	RunCom( "$bin/gc_distr.pl --in data/dna.fna  --out info/dna.gc.csv  --w 1000,8000" );

	# fix this
	# my $max_gap = $cfg->{'Parameters'}->{'max_gap'};
	# my $com = "$bin/probuild --count_substring_atcg --seq data/dna.fna --max_nnn_substring $max_gap > info/dna.atcg";
	# RunCom( $com, "dna atcg substring locations" );
};
# ------------------------------------------------
sub CommitData
{
	print "commit input data\n" if $v;
	chdir $work_dir;

	#commit genomic sequence
	# replace original FASTA defline; keep old/new relationship in trace file
	# check for valid alphabet
	# if softmask is defined, then hardmask lower case letters
	# uppercase
	if ( $cfg->{'Parameters'}->{'sequence'} )
	{
		my $name = $cfg->{'Parameters'}->{'sequence'};
		my $str = '';
		$str = '--soft_mask' if $cfg->{'Parameters'}->{'soft_mask'}; 
		
		RunCom("$bin/reformat_fasta.pl  $str  --up  --out data/dna.fna  --label _dna  --trace info/dna.trace  --in $name" );
	}
	
	#commit evidence data
	# synchronize values in sequence name column with new FASTA defline
	if ( $cfg->{'Parameters'}->{'evidence'} )
	{
		my $name = $cfg->{'Parameters'}->{'evidence'};
		
		if( !$debug )
		{
			RunCom( "$bin/reformat_gff.pl --out data/evidence.gff  --trace info/dna.trace  --in $name  --quiet" );
		}
		else
		{
			RunCom( "$bin/reformat_gff.pl --out data/evidence.gff  --trace info/dna.trace  --in $name" );
		}
	}
	
	#commit ET data
	if( $cfg->{'Parameters'}->{'ET'} )
	{
		my $name = $cfg->{'Parameters'}->{'ET'};
		
		if( !$debug )
		{
			RunCom( "$bin/reformat_gff.pl --out data/et.gff  --trace info/dna.trace  --in $name  --quiet" );
		}
		else
		{
			RunCom( "$bin/reformat_gff.pl --out data/et.gff  --trace info/dna.trace  --in $name" );
		}
	}
}
# ------------------------------------------------
sub RunCom
{
	my ( $com, $mess ) = @_;
	my $res = system( $com );
	$mess = $com if !defined $mess;
	$log->write( $mess );
	if( $res ) { print "error on call: $mess\n"; $log->write( "error" ); exit 1; }
}
# ------------------------------------------------
sub CreateDirectories
{
	print "create directories\n" if $v;
	chdir $work_dir;
	
	my @list =
	(
		'data',
		'info',
		'data/training',
		'run',
		'output'
	);

	make_path( @list,{ verbose => $debug } );
};
# ------------------------------------------------
sub ReadParameters
{
	SetDefaultValues();
	ReadCfgFile( $cfg->{'Config'}->{'def_cfg'}, $bin );
	# Usage() prints some parameters from default configuration on screen
	# Read configuration file before outputting the Usage()
	Usage() if ( @ARGV < 1 );
	ParseCMD();
	CheckBeforeRun();
	SetLogger();
	SaveRunningConfiguration();
};
# ------------------------------------------------
sub SaveRunningConfiguration
{
	my $name = $cfg->{'Config'}->{'run_cfg'};
	if ( !$name ) { print "error, 'run_cfg' configuration file name is missing: $0\n"; exit 1; }
	$name = abs_path( File::Spec->catfile( $work_dir, $name ) );
	$cfg->{'Config'}->{'run_cfg'} = $name;
	open( my $OUT, ">$name") or die "error on open file $0: $name\n$!\n";
	print $OUT Dump($cfg);
	close $OUT;
};
# ------------------------------------------------
sub SetLogger
{
	my $name = $cfg->{'Config'}->{'log_file'};
	if ( !$name ) { print "error, 'log_file' file name is missing: $0\n"; exit 1; }
	$name = abs_path( File::Spec->catfile( $work_dir, $name ) );
	$cfg->{'Config'}->{'log_file'} = $name;
	$log = Logger::Simple->new( LOG=>$name );
	$log->write( Dumper($cfg) ) if $debug;
};
# ------------------------------------------------
sub CheckBeforeRun
{
	print "check before run\n" if $v;
	
	# check sequence pre-processing parameters
	if( ( $cfg->{'Parameters'}->{'max_contig'} < 10000 ) or
		( $cfg->{'Parameters'}->{'min_contig'} < 1 ) or
		( $cfg->{'Parameters'}->{'max_contig'} <  $cfg->{'Parameters'}->{'min_contig'} ) or
		( $cfg->{'Parameters'}->{'max_gap'} < 0 ) or
		( $cfg->{'Parameters'}->{'max_mask'} < 0 )
	) { print "error, out of range values specified sequence pre-processing in parameters section: $0\n"; exit 1; }

	if( $cfg->{'Parameters'}->{'cores'} < 1 or $cfg->{'Parameters'}->{'cores'} > 64 )
	 { print "error, out of range values specified for cores: $0\n"; exit 1; }

	# move to abs path
	$bin      = ResolvePath( $bin );      $cfg->{'Config'}->{'bin'}      = $bin;
	$work_dir = ResolvePath( $work_dir ); $cfg->{'Config'}->{'work_dir'} = $work_dir;
	$work_dir = $cfg->{'Parameters'}->{'out'};
	$cfg->{'Config'}->{'heu_dir'} = ResolvePath( $cfg->{'Config'}->{'heu_dir'}, $bin );
	$cfg->{'Config'}->{'gm_hmm'}  = ResolvePath( $cfg->{'Config'}->{'gm_hmm'}, $bin );
	
	$cfg->{'Parameters'}->{'sequence'} = ResolvePath( $cfg->{'Parameters'}->{'sequence'} );
	$cfg->{'Parameters'}->{'evidence'} = ResolvePath( $cfg->{'Parameters'}->{'evidence'} );
	$cfg->{'Parameters'}->{'ini_mod'}  = ResolvePath( $cfg->{'Parameters'}->{'ini_mod'} );
	$cfg->{'Parameters'}->{'test_set'} = ResolvePath( $cfg->{'Parameters'}->{'test_set'} );
	$cfg->{'Parameters'}->{'ET'}       = ResolvePath( $cfg->{'Parameters'}->{'ET'} );
	$cfg->{'Parameters'}->{'out'} = ResolvePath( $cfg->{'Parameters'}->{'out'} );

	# check input sequence file 
	if( $cfg->{'Run'}->{'commit_input_data'} )
	{
	 	if( !$cfg->{'Parameters'}->{'sequence'} or !-e $cfg->{'Parameters'}->{'sequence'} )
			{ print "error, file with input sequence not found: $0\n"; exit 1; }
		
		if( !-f $cfg->{'Parameters'}->{'sequence'} )
			{ print "error, input not a file $0: $cfg->{'Parameters'}->{'sequence'}\n"; exit 1; }
	}
	
	# check training mode
	if( !$cfg->{'Parameters'}->{'ES'} and !$cfg->{'Parameters'}->{'ET'})
		{ print "error, ES or ET training should be specified: $0\n"; exit 1; }
		
	if( ! $key_bin )
	{	
		my $file_name = glob('~/.gm_key');
		$key_bin = 1 if ( ! -e $file_name );
		print "test key:  $key_bin\n" if $debug;
	}
};
# ------------------------------------------------
sub ParseCMD
{
	my $cmd = $0;
	foreach my $str (@ARGV) { $cmd .= ( ' '. $str ); } 
	
	my %h;
	my $opt_results = GetOptions
	(
		\%h,
		'max_contig=i',
		'min_contig=i',
		'max_gap=i',
		'max_mask=i',
		'soft_mask',
		'ES',
		'ET=s',
		'fungus',
		'training',
		'prediction',
		'sequence=s',
		'evidence=s',
		'ini_mod=s',
		'usr_cfg=s',
		'test_set=s',
		'min_gene_prediction=i',
		'max_intergenic=i',
		'max_intron=i',
		'et_score=f',
		'out=s',
		'pbs',
		'cores=i',
		'key_bin' => \$key_bin,
		'verbose' => \$v,
		'debug'   => \$debug
	);

	if( !$opt_results ) { print "error on command line: $0\n"; exit 1; }
	if( @ARGV > 0 ) { print "error, unexpected argument found on command line: $0 @ARGV\n"; exit 1; }
	$v = 1 if $debug;
	
	# user may specify additional configuration file on command line
	# parse user specified file first (if any) and then parse other command line parameters
	ReadCfgFile( $h{'usr_cfg'} ) if exists $h{'usr_cfg'};
	
	# update cfg
	foreach my $key ( keys %h )
	{
		$cfg->{'Parameters'}->{$key} = $h{$key};
	}
	
	UpdateRunStatus();
	
	# save informaton for debug
	$cfg->{'Parameters'}->{'v'}     = $v;
	$cfg->{'Parameters'}->{'debug'} = $debug;
	$cfg->{'Parameters'}->{'cmd'}   = $cmd;
	$cfg->{'Parameters'}->{'key_bin'} = $key_bin;
	
	if ( $cfg->{'Parameters'}->{'fungus'} )
	{
		$cfg->{'intron_DUR'}->{'max'}    = $cfg->{'Fungi'}->{'max_intron'};
		$cfg->{'prespacer_DUR'}->{'max'} = $cfg->{'Fungi'}->{'max_intron'};
		$cfg->{'Parameters'}->{'min_gene_prediction'} = $cfg->{'Fungi'}->{'min_gene_prediction'};
	}
	
	# warning, ET-score threshold is declared in two sections: 'Parameters' and 'ET_ini'
	if ( exists $cfg->{'Parameters'}->{'et_score'} )
	{
		$cfg->{'ET_ini'}->{'et_score'} = $cfg->{'Parameters'}->{'et_score'};
	}

	if( $cfg->{'Parameters'}->{'max_intron'} >  0 )
	{
		$cfg->{'intron_DUR'}->{'max'} =  $cfg->{'Parameters'}->{'max_intron'};
		
		if ( $cfg->{'Parameters'}->{'fungus'} > 0 )
		{
			$cfg->{'prespacer_DUR'}->{'max'} = $cfg->{'intron_DUR'}->{'max'};
		}
	}
};
# ------------------------------------------------
sub UpdateRunStatus
{
	if( $cfg->{'Parameters'}->{'training'} and !$cfg->{'Parameters'}->{'prediction'} )
	{
		$cfg->{'Run'}->{'run_prediction'}    = 0;
		$cfg->{'Run'}->{'prediction_report'} = 0;		
	}	

	if( !$cfg->{'Parameters'}->{'training'} and $cfg->{'Parameters'}->{'prediction'} )
	{
		$cfg->{'Run'}->{'set_dirs'}             = 0;
		$cfg->{'Run'}->{'commit_input_data'}    = 1;
		$cfg->{'Run'}->{'input_data_report'}    = 0;
		$cfg->{'Run'}->{'commit_training_data'} = 0;
		$cfg->{'Run'}->{'training_data_report'} = 0;
		$cfg->{'Run'}->{'prepare_ini_mod'}      = 0;
		$cfg->{'Run'}->{'run_training'}         = 0;
		$cfg->{'Run'}->{'training_report'}      = 0;
	}
};
# ------------------------------------------------
sub ReadCfgFile
{
	my( $name, $path ) = @_;
	return '' if !$name;
	print "reading configuration file: $name\n" if $v;
	$name = ResolvePath( $name, $path );
	Hash::Merge::set_behavior( 'RIGHT_PRECEDENT' );
	
	my $cfg_from_file = YAML::LoadFile( $name );
	
	if( defined $cfg_from_file )
	{
		%$cfg = %{ merge( $cfg, $cfg_from_file) };
	}
	else { print "warning, configuration file is empty: $name\n"; }
	
	return $name;
};
# ------------------------------------------------
sub ResolvePath
{
	my( $name, $path ) = @_;
	return '' if !$name;
	$name = File::Spec->catfile( $path, $name ) if ( defined $path and $path );
	if( ! -e $name ) { print "error, file not found $0: $name\n"; exit 1; }
	return abs_path( $name );
};
# ------------------------------------------------
sub SetDefaultValues
{
	# switch sections of algorithm ON or OFF
	$cfg->{'Run'}->{'set_dirs'}             = 1;  # true
	$cfg->{'Run'}->{'commit_input_data'}    = 1;  # true
	$cfg->{'Run'}->{'input_data_report'}    = 1;  # true
	$cfg->{'Run'}->{'commit_training_data'} = 1;  # true
	$cfg->{'Run'}->{'training_data_report'} = 1;  # true
	$cfg->{'Run'}->{'prepare_ini_mod'}      = 1;  # true
	$cfg->{'Run'}->{'run_training'}         = 1;  # true
	$cfg->{'Run'}->{'training_report'}      = 0;  # false
	$cfg->{'Run'}->{'run_prediction'}       = 1;  # true
	$cfg->{'Run'}->{'prediction_report'}    = 0;  # false
	
	# start of command line parameters
	
	# genome sequence processing
	$cfg->{'Parameters'}->{'max_contig'} = 5000000;
	$cfg->{'Parameters'}->{'min_contig'} =   50000;
	$cfg->{'Parameters'}->{'max_gap'}    =    5000;
	$cfg->{'Parameters'}->{'max_mask'}   =    5000;
	$cfg->{'Parameters'}->{'soft_mask'}  = 0;  # false
	
	$cfg->{'Parameters'}->{'min_gene_prediction'}   = 300;  # minimum gene length to predict
	
	$cfg->{'Parameters'}->{'ES'} =     0;  # false
	$cfg->{'Parameters'}->{'fungus'} = 0;  # false
	$cfg->{'Parameters'}->{'ET'} =    '';  # false
	$cfg->{'Parameters'}->{'et_score'} = 4;  # intron score for TopHat like programs: number or reads spanning exon-exon junction
	
	$cfg->{'Parameters'}->{'training'}   = '';
	$cfg->{'Parameters'}->{'prediction'} = '';
	
	$cfg->{'Parameters'}->{'sequence'} = '';
	$cfg->{'Parameters'}->{'evidence'} = '';
	$cfg->{'Parameters'}->{'ini_mod'}  = '';
	$cfg->{'Parameters'}->{'usr_cfg'}  = '';
	$cfg->{'Parameters'}->{'test_set'} = '';

	$cfg->{'Parameters'}->{'pbs'}   = 0;  # false
	$cfg->{'Parameters'}->{'cores'} = 1;  # number of processors to use

	$cfg->{'Parameters'}->{'max_intergenic'} = 0;  # 0=false - use default method for parameter initiation
	$cfg->{'Parameters'}->{'max_intron'} = 0;      # 0=false - use default method for parameter initiation
	
	$cfg->{'Parameters'}->{'v'}     = $v;
	$cfg->{'Parameters'}->{'debug'} = $debug;

	$cfg->{'Parameters'}->{'key_bin'} = $key_bin;

	# end of command line parameters

	$cfg->{'Parameters'}->{'min_contig_prediction'} = 500;  # minimum contig length to find genes on prediction step

	# basic configuration
	$cfg->{'Config'}->{'version'}  = '4.26';
	$cfg->{'Config'}->{'heu_dir'}  = 'heu_dir';
	$cfg->{'Config'}->{'def_cfg'}  = 'gmes.cfg';
	$cfg->{'Config'}->{'gm_hmm'}   = 'gmhmme3';
	$cfg->{'Config'}->{'log_file'} = 'gmes.log';
	$cfg->{'Config'}->{'run_cfg'}  = 'run.cfg';
	$cfg->{'Config'}->{'bin'}      = $bin;
	$cfg->{'Config'}->{'work_dir'} = $work_dir;
};
# ------------------------------------------------
sub Usage
{
	print qq(# -------------------
Usage:  $0  [options]  --sequence [filename]

GeneMark-ES Suite version $cfg->{'Config'}->{'version'}
   includes transcript (GeneMark-ET) and protein (GeneMark-EP) based training and prediction

Input sequence/s should be in FASTA format

Algorithm options
  --ES           to run self-training
  --fungus       to run algorithm with branch point model (most useful for fungal genomes)
  --ET           [filename]; to run training with introns coordinates from RNA-Seq read alignments (GFF format)
  --et_score     [number]; $cfg->{'Parameters'}->{'et_score'} (default) minimum score of intron in initiation of the ET algorithm
  --evidence     [filename]; to use in prediction external evidence (RNA or protein) mapped to genome
  --training     to run training step only
  --prediction   to run prediction step only

Sequence pre-processing options
  --max_contig   [number]; $cfg->{'Parameters'}->{'max_contig'} (default) will split input genomic sequence into contigs shorter then max_contig
  --min_contig   [number]; $cfg->{'Parameters'}->{'min_contig'} (default); will ignore contigs shorter then min_contig in training 
  --max_gap      [number]; $cfg->{'Parameters'}->{'max_gap'} (default); will split sequence at gaps longer than max_gap
                 Letters 'n' and 'N' are interpreted as standing within gaps 
  --max_mask     [number]; $cfg->{'Parameters'}->{'max_mask'} (default); will split sequence at repeats longer then max_mask
                 Letters 'x' and 'X' are interpreted as results of hard masking of repeats
  --soft_mask    to indicate that lowercase letters stand for repeats

Other options
  --cores        [number]; 1 (default) to run program with multiple threads 
  --v            verbose

Developer options:
  --pbs          to run on cluster with PBS support
  --usr_cfg      [filename]; to customize configuration file
  --ini_mod      [filename]; use this file with parameters for algorithm initiation
  --max_intergenic      [number]; 10000 (default) maximum length of intergenic regions
  --max_intron          [number]; 10000/3000 (default) maximum length of intron
  --min_gene_prediction [number]; $cfg->{'Parameters'}->{'min_gene_prediction'} minimum allowed gene length in prediction step
  --test_set     [filename]; to evaluate prediction accuracy on the given test set
  --key_bin
  --debug
# -------------------
);
	exit 1;
};
# ================== END sub =====================
