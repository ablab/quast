<img src="quast_libs/html_saver/static/img/quast_logo_black.png" width="300" title="QUAST">

### Genome assembly evaluation tool

ViralQUAST is QUAST extension for viral genomes. It works with reference genomes
and has two different modes. Main usecase is to find best viral reference
in pangenome, and then run QUAST with this reference.

First mode uses pangenome as reference, and use algorithm to find best viral
genome which then passed to quast as reference.

Second mode additionallyu needs .msh reference. It uses mash tool to find best
reference, and works much faster than first mode, but need preprocessed .msh
reference to work with.

#### System requirements

Linux (64-bit and 32-bit with slightly limited functionality) and macOS are supported.

For the main pipeline:
- Python3 (3.3 or higher)
- biopython package for python
- mash (2.3 or higher)
- minimap2 (2.19 or higher)
- Quast requirements

For the optional submodules:
- wget package for python

Most of those tools are usually preinstalled on Linux. MacOS, however, requires to install
the Command Line Tools for Xcode to make them available.

#### Installation

Basic installation (about 120 MB):

    ./setup.py install
    pip install biopython

Full installation (about 540 MB, includes (1) tools for SV detection based on read pairs, which is used for more precise misassembly detection, 
(2) and tools/data for reference genome detection in metagenomic datasets):

    ./setup.py install_full
    pip install wget

The default installation location is `/usr/local/bin/` for the executable scripts, and `/usr/local/lib/` for 
the python modules and auxiliary files. If you are getting a permission error during the installation, consider running setup.py with
`sudo`, or create a virtual python environment and [install into it](http://docs.python-guide.org/en/latest/dev/virtualenvs/). 
Alternatively, you may use old-style installation scripts (`./install.sh` or `./install_full.sh`), which build QUAST package inplace.

#### Usage

    ./viralquast.py test_data/viralquast/test_scaffolds.fasta /
            --mash-reference test_data/viralquast/test_reference.msh \
            -r test_data/viralquast/test_reference.fasta.gz \
            -o viralquast_test_output

    Or

    ./viralquast.py test_data/viralquast/test_scaffolds.fasta /
            -r test_data/viralquast/test_reference.fasta.gz \
            --no-mash \
            -o viralquast_test_output

#### Output

    cutted_result.fasta     best reference from pangenome
    
    quast_best_results      quast stats for found viral reference

#### Contact & Info 

* Support email: [quast.support@cab.spbu.ru](quast.support@cab.spbu.ru)
* Issue tracker: [https://github.com/ablab/quast/issues](https://github.com/ablab/quast/issues)
* Website: [http://quast.sf.net](http://quast.sf.net)
* Latest news: [https://twitter.com/quast_bioinf](https://twitter.com/quast_bioinf)
    
