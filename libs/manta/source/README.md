Manta Structural Variant Caller
===============================

Version: 0.29.6

Manta calls structural variants (SVs) and indels from mapped
paired-end sequencing reads. It is optimized for analysis of germline
variation in small sets of individuals and somatic variation in
tumor/normal sample pairs. Manta discovers, assembles and scores
large-scale SVs, medium-sized indels and large insertions within a
single efficient workflow. The method is designed for rapid analysis
on standard compute hardware: NA12878 at 50x genomic coverage is
analyzed in less than 20 minutes on a 20 core server, and most WGS
tumor/normal analyses can be completed within 2 hours. Manta combines
paired and split-read evidence during SV discovery and scoring to
improve accuracy, but does not require split-reads or successful
breakpoint assemblies to report a variant in cases where there is
strong evidence otherwise. It provides scoring models for germline
variants in small sets of diploid samples and somatic variants in
matched tumor/normal sample pairs. There is experimental support for
analysis of unmatched tumor samples as well. Manta accepts input read
mappings from BAM or CRAM files and reports all SV and indel inferences
in VCF 4.1 format. See the [user guide] [UserGuide] for a full
description of capabilities and limitations, and the [Manta preprint]
[mss] for additional methods and benchmarking details.

[UserGuide]:docs/userGuide/README.md
[mss]:http://dx.doi.org/10.1101/024232


License
-------

Manta source code is provided under the [GPLv3 license] (LICENSE.txt).
Manta includes several third party packages provided under other
open source licenses, please see [COPYRIGHT.txt] (COPYRIGHT.txt)
for additional details.


Getting Started
---------------

For linux users, it is recommended to start from the most recent
[binary distribution on the Manta releases page] [releases], this
distribution can be unpacked, moved to any convenient directory and
tested by [running a small demo](docs/userGuide/installation.md#demo)
included with the release distribution. Manta can also be installed
and run on OS X. Please see the [installation instructions](docs/userGuide/installation.md)
for full build and installation details of all supported cases.

[releases]:https://github.com/Illumina/manta/releases


Data Analysis and Interpretation
--------------------------------

After completing installation, see the [Manta user guide] [UserGuide]
for instructions on how to run Manta, interpret results and estimate
hardware requirements/compute cost, in addition to a high-level methods
overview.


Manta Code Development
----------------------

For manta code development and debugging details, see the
[Manta developer guide] [DeveloperGuide]. This includes details
on Manta's developement protocols, special build instructions,
recommended workflows for investigating
calls, and internal documentation details.

[DeveloperGuide]:docs/developerGuide/README.md
