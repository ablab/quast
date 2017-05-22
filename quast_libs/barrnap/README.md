# Barrnap

BAsic Rapid Ribosomal RNA Predictor

## Author

Torsten Seemann - torsten.seemann@gmail.com - @torstenseemann

## Description

Barrnap predicts the location of ribosomal RNA genes in genomes.
It supports bacteria (5S,23S,16S), archaea (5S,5.8S,23S,16S),
mitochondria (12S,16S) and eukaryotes (5S,5.8S,28S,18S).

It takes FASTA DNA sequence as input, and write GFF3 as output.
It uses the new NHMMER tool that comes with HMMER 3.1 for HMM searching in RNA:DNA style.
NHMMER binaries for 64-bit Linux and Mac OS X are included and will be auto-detected.
Multithreading is supported and one can expect roughly linear speed-ups with more CPUs.

## Download

* Tarballs: https://github.com/tseemann/barrnap/releases/
* Source: https://github.com/tseemann/barrnap

## Install

    % cd $HOME
    % tar zxvf barrnap-0.X.tar.gz
    % echo "PATH=$PATH:$HOME/barrnap-0.x/bin" >> .bashrc
    (logout and log back in)

## Usage

    % barrnap --quiet examples/small.fna
    ##gff-version 3
    P.marinus	barrnap:0.7	rRNA	353314	354793	0	+	.	Name=16S_rRNA;product=16S ribosomal RNA
    P.marinus	barrnap:0.7	rRNA	355464	358334	0	+	.	Name=23S_rRNA;product=23S ribosomal RNA
    P.marinus	barrnap:0.7	rRNA	358433	358536	7.5e-07	+	.	Name=5S_rRNA;product=5S ribosomal RNA

    % barrnap -q -k mito examples/mitochondria.fna 
    ##gff-version 3
    AF346967.1	barrnap:0.7	rRNA	643	1610	.	+	.	Name=12S_rRNA;product=12S ribosomal RNA
    AF346967.1	barrnap:0.7	rRNA	1672	3228	.	+	.	Name=16S_rRNA;product=16S ribosomal RNA

## Caveats

Barrnap does not do anything fancy. It has HMM models for each different rRNA gene. 
They are built from full length seed alignments. 

## Requirements

* Perl >= 5.6
* HMMER >= 3.1b

## License

Barrnap is free software, released under the GPL (version 3).

## Comparison with RNAmmer

Barrnap is designed to be a substitute for RNAmmer. It was motivated by
my desire to remove <A HREF="https://github.com/tseemann/prokka">Prokka's</A> dependency on RNAmmer
which is encumbered by a free-for-academic sign-up license, and by RNAmmer's
dependence on legacy HMMER 2.x which conflicts with HMMER 3.x that most people are using now.

RNAmmer is more sophisticated than Barrnap, and more accurate because it uses HMMER 2.x in glocal alignment mode whereas NHMMER 3.x currently only supports local alignment (Sean Eddy expects glocal to be supported in 2014). In practice, Barrnap will find all the typical rRNA genes in a few seconds (in bacteria), but may get the end points out by a few bases and will probably miss wierd rRNAs. The HMM models it uses are derived from Rfam, Silva and RefSeq.

## Data sources for HMM models

<pre>
Bacteria (70S)  
        LSU 50S
                5S      RF00001
                23S     SILVA-LSU-Bac
        SSU 30S
                16S     RF00177

Archaea (70S)   
        LSU 50S
                5S      RF00001
                5.8S    RF00002
                23S     SILVA-LSU-Arc
        SSU 30S
                16S     RF01959

Eukarya (80S)   
        LSU 60S
                5S      RF00001
                5.8S    RF00002
                28S     SILVA-LSU-Euk
        SSU 40S
                18S     RF01960
        Mito
                12S     RefSeq (MT-RNR1, s-rRNA, rns)
                16S     RefSeq (MT-RNR2, l-rRNA, rnl)       

TODO: [Sajeet Haridas]
Fungi
        LSU 35S ?
                5S
                5.8S
                25S
        SSU ?
                18S
        Mito [http://www.ncbi.nlm.nih.gov/nuccore/NC_001224.1]
                15S 
                21S (multiple exons)
                

TODO:
Apicoplast [http://www.ncbi.nlm.nih.gov/nuccore/U87145.2]
                LSU ~2500bp 28S ?
                SSU ~1500bp 16S ?

Plastid [Shaun Jackman]
	?

</pre>

## Where does the name come from?

The name Barrnap was originally derived from _Bacterial/Archaeal Ribosomal RNA Predictor_.
However it has since been extended to support mitochondrial and eukaryotic rRNAs, and has been
given the new backronym _BAsic Rapid Ribosomal RNA Predictor_.
The project was originally spawned at CodeFest 2013 in Berlin, Germany 
by Torsten Seemann and Tim Booth.

