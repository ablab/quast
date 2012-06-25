#!/usr/bin/perl -w

use Getopt::Long;
use Pod::Usage;

my $RefFile = shift or die("Usage: $0 fasta_seq_file assembly_seq_file out_header libs_dir (gff_file gff_region cds_family)\n");
my $AssFile = shift or die("Usage: $0 fasta_seq_file assembly_seq_file out_header libs_dir (gff_file gff_region cds_family)\n");
my $header = shift or die ("Usage: $0 fasta_seq_file assembly_seq_file out_header libs_dir (gff_file gff_region cds_family)\n");
my $verbose = 0;
my $symp = 0;
my $cyclic = 0;
my $rcinem = 0; # reverse complementarity is not an extensive misassemble
my $help;
my $maxun = 10;
my $peral = 0.99;
my $smgap = 1000;
my $gff_file = 0;
my $region = 0;
my $cds = 0;

GetOptions(
           'reference=s' => \$RefFile,
           'assembly=s' => \$AssFile,
           'header=s' => \$header,
           'gff=s' => \$gff_file,
           'region=s' => \$region,
           'cds=s' => \$cds,
           'max_unaligned=i' => \$maxun,
           'per_aligned=f' => \$peral,
           'sm_gap_size' => \$smgap,
           'verbose' => \$verbose,
           'cyclic' => \$cyclic,
           'rc' => \$rcinem,
           'help' => \$help) || pod2usage(2);
           
pod2usage("$0: Reference fasta must be specified.") if !$RefFile;
pod2usage("$0: Assembly fasta must be specified.") if !$AssFile;
pod2usage("$0: If region type is CDS, then the cds option must be specified..") if ($region eq "CDS" && !$cds);
pod2usage(2) if $help;


use Cwd 'abs_path';
use File::Basename;

#Removing very long contig headers.   These can cause problems for nucmer.
print "Cleaning up contig headers...\n";
if (!-e "$AssFile.clean" ) {
	system "sed 's/>\\([0-9]\\+\\) .*/>\\1/' $AssFile > $AssFile.clean";
} else {
	print "\tCleaned fasta file exists, skipping cleaning.\n";
}

#Aligning to the reference genome, via nucmer.
print "Aligning contigs to reference...\n";
if ( !-e "$header.coords" ) {
	if ( -e "${header}.snps" ) {
		system "rm ${header}.snps";
	}
	print "\tRunning nucmer...\n";
	system "nucmer --maxmatch -p $header $RefFile $AssFile.clean";
  system "show-coords -B $header.delta > $header.coords.btab";
} else {
	print "\t$header.coords exists already, skipping.\n";
}
