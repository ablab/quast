<img src="quast_libs/html_saver/static/img/quast_logo_black.png" width="300" title="QUAST">

### Genome assembly evaluation tool

QUAST stands for QUality ASsessment Tool. It evaluates genome/metagenome assemblies by computing various metrics.
The current QUAST toolkit includes the general QUAST tool for genome assemblies, 
MetaQUAST, the extension for metagenomic datasets, 
QUAST-LG, the extension for large genomes (e.g., mammalians), and Icarus, the interactive visualizer for these tools.

The QUAST package works both with and without reference genomes. 
However, it is much more informative if at least a close reference genome is provided along with the assemblies.
The tool accepts multiple assemblies, thus is suitable for comparison.

This README file gives a brief introduction into installation, basic usage and parsing of output of QUAST. 
A much more detailed description of these and many other topics is available in the
[online manual](http://quast.sf.net/manual.html). 
There are also many more installation methods for the latest stable release of the QUAST toolkit, 
all of them are discussed [here](http://quast.sf.net/install.html). For the cutting-edge version, 
please clone our [GitHub repo](https://github.com/ablab/quast).

Please refer to the [LICENSE.txt](http://quast.sf.net/docs/LICENSE.txt) file for copyrights and citing instructions. We warmly welcome external contributions to the QUAST project. If you plan to participate, please consult with our [Contributor Covenant](CODE_OF_CONDUCT.md).

#### System requirements

Linux (64-bit and 32-bit with slightly limited functionality) and macOS are supported.

For the main pipeline:
- Python2 (2.5 or higher) or Python3 (3.3 or higher)
- Perl 5.6.0 or higher
- GCC 4.7 or higher
- GNU make and ar
- zlib development files

For the optional submodules:
- Time::HiRes perl module for GeneMark-ES (needed when using `--gene-finding --eukaryote`)
- Java 1.8 or later for GRIDSS (needed for SV detection)
- R for GRIDSS (needed for SV detection)

Most of those tools are usually preinstalled on Linux. MacOS, however, requires to install
the Command Line Tools for Xcode to make them available. 

QUAST draws plots in two formats: HTML and PDF. If you need the PDF versions, make sure that you have installed 
[Matplotlib](https://matplotlib.org/). We recommend to use Matplotlib version 1.1 or higher. QUAST is fully tested with Matplotlib v.1.3.1.
Installation on Ubuntu (tested on Ubuntu 20.04):

    sudo apt-get update && sudo apt-get install -y pkg-config libfreetype6-dev libpng-dev python3-matplotlib

#### Installation

QUAST automatically compiles all its sub-parts when needed (on the first use). 
Thus, installation is not required. However, if you want to precompile everything and add quast.py to your `PATH`, you may choose either:

Basic installation (about 120 MB):

    ./setup.py install

Full installation (about 540 MB, includes (1) tools for SV detection based on read pairs, which is used for more precise misassembly detection, 
(2) and tools/data for reference genome detection in metagenomic datasets):

    ./setup.py install_full

The default installation location is `/usr/local/bin/` for the executable scripts, and `/usr/local/lib/` for 
the python modules and auxiliary files. If you are getting a permission error during the installation, consider running setup.py with
`sudo`, or create a virtual python environment and [install into it](http://docs.python-guide.org/en/latest/dev/virtualenvs/). 
Alternatively, you may use old-style installation scripts (`./install.sh` or `./install_full.sh`), which build QUAST package inplace.

#### Usage

    ./quast.py test_data/contigs_1.fasta \
               test_data/contigs_2.fasta \
            -r test_data/reference.fasta.gz \
            -g test_data/genes.txt \
            -1 test_data/reads1.fastq.gz -2 test_data/reads2.fastq.gz \
            -o quast_test_output

#### Output

    report.txt      summary table
    report.tsv      tab-separated version, for parsing, or for spreadsheets (Google Docs, Excel, etc)  
    report.tex      Latex version
    report.pdf      PDF version, includes all tables and plots for some statistics
    report.html     everything in an interactive HTML file
    icarus.html     Icarus main menu with links to interactive viewers
    contigs_reports/        [only if a reference genome is provided]
      misassemblies_report  detailed report on misassemblies
      unaligned_report      detailed report on unaligned and partially unaligned contigs
    k_mer_stats/            [only if --k-mer-stats is specified]
      kmers_report          detailed report on k-mer-based metrics
    reads_stats/            [only if reads are provided]
      reads_report          detailed report on mapped reads statistics

Metrics based only on contigs:

* Number of large contigs (i.e., longer than 500 bp) and total length of them.  
* Length of the largest contig.  
* N50 (length of a contig, such that all the contigs of at least the same length together cover at least 50% of the assembly).
* Number of predicted genes, discovered either by GeneMark.hmm (for prokaryotes), GeneMark-ES or GlimmerHMM (for eukaryotes), 
or MetaGeneMark (for metagenomes).

When a reference is given:

* Numbers of misassemblies of different kinds (inversions, relocations, translocations, interspecies translocations (metaQUAST only) or local).
* Number and total length of unaligned contigs.  
* Numbers of mismatches and indels, over the assembly and per 100 kb.  
* Genome fraction %, assembled part of the reference.  
* Duplication ratio, the total number of aligned bases in the assembly divided by the total number of those in the reference. 
If the assembly contains many contigs that cover the same regions, its duplication ratio will significantly exceed 1. 
This occurs due to multiple reasons, including overestimating repeat multiplicities and overlaps between contigs.  
* Number of genes in the assembly, completely or partially covered, based on a user-provided list of gene positions in the reference.  
* NGA50, a reference-aware version of N50 metric. It is calculated using aligned blocks instead of contigs. 
Such blocks are obtained after removing unaligned regions, and then splitting contigs at misassembly breakpoints. 
Thus, NGA50 is the length of a block, such that all the blocks of at least the same length together cover at least 50% of the reference.  


#### Contact & Info 

* Support email: [quast.support@cab.spbu.ru](quast.support@cab.spbu.ru)
* Issue tracker: [https://github.com/ablab/quast/issues](https://github.com/ablab/quast/issues)
* Website: [http://quast.sf.net](http://quast.sf.net)
* Latest news: [https://twitter.com/quast_bioinf](https://twitter.com/quast_bioinf)
    
