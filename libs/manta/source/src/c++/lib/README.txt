alignment:
sequence alignment utilities

applications/X:
code specific to command-line application X

appstats:
shared performance tracking code between applications

blt_util:
general utility functions from manta/starling/strelka/gvcftools

common:
general utility functions from CASAVA/Grouper/Isaac

format:
conversion of data into external formats

htsapi:
various c++ wrapper objects built on top of samtools/htslib
and other utilities for standard genomic indexed file formats
like bam/cram,bed,vcf, etc...

manta:
common code to the manta project (ie. too specific for general utility
libraries but does not fit the category of another library). This is also
the default library to which logic should be added until there is a clear
pattern to justify a new library.

options:
command-line options objects which are shared between applications

svgraph:
SV locus graph components

