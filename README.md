<img src="quast_libs/html_saver/static/img/quast_logo_black.png" width="300" title="QUAST">

### Genome assembly evaluation tool

QUAST evaluates genome assemblies by computing various metrics.

It works both with and without reference genomes.

The tool accepts multiple assemblies, thus is suitable for comparison.

#### Installation

Basic installation (36MB):

    ./setup.py install

Full installation (815MB, includes (1) tools for SV detection based on read pairs, which is used for more precise misassembly detection, (2) and tools/data for reference genome detection in metagenomes):

    ./setup.py install_full

#### Usage

    ./quast.py test_data/contigs_1.fasta \
               test_data/contigs_2.fasta \
            -R test_data/reference.fasta.gz \
            -G test_data/genes.txt \
            -O test_data/operons.txt \
            -1 test_data/reads1.fastq.gz -2 test_data/reads2.fastq.gz \
            -o quast_test_output

#### Output

    report.txt     summary table
    report.tsv     tab-separated version, for parsing, or for spreadsheets (Google Docs, Excel, etc)  
    report.tex     Latex version
    report.pdf     PDF version, includes all tables and plots for some statistics
    report.html    everything in an interactive HTML file
    alignment.svg  visualized alignement of contigs to reference


Metrics based only on contigs:

* Number of large contigs (i.e., longer than 500 bp) and total length of them.  
* Length of the largest contig.  
* N50 (length of a contig, such that all the contigs of at least the same length together cover at least 50% of the assembly).
* Number of predicted genes, discovered either by GeneMark.hmm (for prokaryotes), GeneMark-ES or GlimmerHMM (for eukaryotes), or MetaGeneMark (for metagenomes).

When a reference is given:

* Numbers of misassemblies of different kinds (inversions, relocations, translocations, interspecies translocations (metaQUAST only) or local).
* Number and total length of unaligned contigs.  
* Numbers of mismatches and indels, over the assembly and per 100 kb.  
* Genome fraction %, assembled part of the reference.  
* Duplication ratio, the total number of aligned bases in the assembly divided by the total number of those in the reference. If the assembly contains many contigs that cover the same regions, its duplication ratio will significantly exceed 1. This occurs due to multiple reasons, including overestimating repeat multiplicities and overlaps between contigs.  
* Number of genes in the assembly, completely or partially covered, based on a user-provided list of gene positions in the reference.  
* NGA50, a reference-aware version of N50 metric. It is calculated using aligned blocks instead of contigs. Such blocks are obtained after removing unaligned regions, and then splitting contigs at misassembly breakpoints. Thus, NGA50 is the length of a block, such that all the blocks of at least the same length together cover at least 50% of the reference.  

<br>
For the full documentation, see the [manual.html](http://quast.bioinf.spbau.ru/manual.html).

You can also check out the web interface: [http://quast.bioinf.spbau.ru](http://quast.bioinf.spbau.ru)

Please refer to the LICENSE.txt file for copyrights and citing instructions.


#### System requirements

For the main pipeline:
- python 2 (2.5 or higher)
- perl 5.6.0 or higher
- basic UNIX tools (g++, make, sh, csh, sed, awk, ar)

For the optional submodules:
- Time::HiRes perl module for GeneMark-ES (if using with `--gene-finding`)
- Boost (tested with v1.56.0) for Manta (if using with reads) and E-MEM (for a higher contig alignment performance)
- cmake (tested with v2.8.12) for Manta
- Java JDK (tested with OpenJDK 6) for GAGE

All those tools are usually preinstalled on Linux. Mac OS, however, requires to install 
the Command Line Tools for Xcode to make them available. 

QUAST draws plots in two formats: HTML and PDF. If you need the PDF versions, make sure that you have installed 
Matplotlib. We recommend to use Matplotlib version 1.1 or higher. QUAST is fully tested with Matplotlib v.1.3.1.
Installation on Ubuntu:

    sudo apt-get update && sudo apt-get install -y pkg-config libfreetype6-dev libpng-dev python-matplotlib
