<link rel='stylesheet' href='userGuide.css' />

Manta User Guide
================

## Table of Contents
* [Introduction](#introduction)
* [Method Overview](#method-overview)
* [Capabilities](#capabilities)
* [Input requirements](#input-requirements)
* [Outputs](#outputs)
* [Run configuration and execution](#run-configuration-and-execution)
* [Extended use cases](#extended-use-cases)

## Introduction

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
analysis of unmatched tumor samples as well (see details below). Manta
accepts input read mappings from BAM or CRAM files and reports all SV
and indel inferences in VCF 4.1 format. For extended methods and benchmarking
details, please see the [Manta preprint][mss].

[mss]:http://dx.doi.org/10.1101/024232


## Method Overview

Manta divides the SV and indel discovery process into two primary
steps: (1) scanning the genome to find SV associated regions and (2)
analysis, scoring and output of SVs found in such regions.

1. **Build breakend association graph** In this step the entire genome
is scanned to discover evidence of possible SVs and large indels. This
evidence is enumerated into a graph with edges connecting all regions
of the genome which have a possible breakend association. Edges may
connect two different regions of the genome to represent evidence of a
long-range association, or an edge may connect a region to itself to
capture a local indel/small SV association. Note that these
associations are more general than a specific SV hypothesis, in that
many breakend candidates may be found on one edge, although typically
only one or two candidates are found per edge.

2. **Analyze graph edges to find SVs** The second step is to analyze
individual graph edges or groups of highly connected edges to discover
and score SVs associated with the edge(s). The substeps of this
process include inference of SV candidates associated with the edge,
attempted assembly of the SVs breakends, scoring/genotyping and
filtration of the SV under various biological models (currently
diploid germline and somatic), and finally, output to VCF.


## Capabilities

Manta is capable of detecting all structural variant types which are
identifiable in the absence of copy number analysis and large-scale
de-novo assembly. Detectable types are enumerated further below.

For each structural variant and indel, Manta attempts to assemble the
breakends to basepair resolution and report the left-shifted breakend
coordinate (per the [VCF 4.1][1] SV reporting guidelines), together
with any breakend homology sequence and/or inserted sequence between
the breakends. It is often the case that the assembly will fail to
provide a confident explanation of the data -- in such cases the
variant will be reported as `IMPRECISE`, and scored according to the
paired-end read evidence only.

The sequencing reads provided as input to Manta are expected to be
from a paired-end sequencing assay which results in an "innie"
orientation between the two reads of each sequence fragment, each
presenting a read from the outer edge of the fragment insert inward.

Manta is primarily tested for whole-genome and whole-exome (or other
targeted enrichement) sequencing assays on DNA. For these assays the
following applications are supported:

* Joint analysis of small sets of diploid individuals (where 'small' means
family-scale -- roughly 10 or fewer samples)
* Subtractive analysis of a matched tumor/normal sample pair
* Analysis of an individual tumor sample

For the first use case above, note that there is no specific restriction against
using Manta for the joint analysis of larger cohorts, but this has not 
been extensively tested so there may be stability or call quality
issues.

Per the final use case above, tumor samples can be analyzed without a matched normal sample. In
this case no scoring function is available, but the supporting
evidence counts and many filters can still be usefully applied.

RNA-Seq analysis is still in development and not fully supported. It
can be configured with the `--rna` flag. This will adjust filtration
levels and take other RNA-specific filtration and intron handling steps
(more details are provided further below).

### Detected variant classes

Manta is able to detect all variation classes which can be explained
as novel DNA adjacencies in the genome. Simple insertion/deletion
events can be detected down to a configurable minimum size cutoff
(defaulting to 8). All DNA adjacencies are classified into the
following categories based on the breakend pattern:

* Deletions
* Insertions
    * Fully-assembled insertions
    * Partially-assembled (ie. inferred) insertions
* Inversions
* Tandem Duplications
* Interchromosomal Translocations

### Known Limitations

Manta should not be able to detect the following variant types:

* Dispersed duplications
* Most expansion/contraction variants of a reference tandem repeat
* Small inversions
    * The limiting size is not tested, but in theory detection falls off
  below ~200bases. So-called micro-inversions might be detected indirectly as
  combined insertion/deletion variants.
* Fully-assembled large insertions
    * The maximum fully-assembled insertion size should correspond to
  approximately twice the read-pair fragment size, but note that power to fully
  assemble the insertion should fall off to impractical levels before this
  size
    * Note that manta does detect and report very large insertions
  when the breakend signature of such an event is found, even though
  the inserted sequence cannot be fully assembled.

More general repeat-based limitations exist for all variant types:

* Power to assemble variants to breakend resolution falls to zero as
  breakend repeat length approaches the read size.
* Power to detect any breakend falls to (nearly) zero as the breakend
  repeat length approaches the fragment size.

Note that while Manta classifies novel DNA-adjacencies, it does not
infer the higher level constructs implied by the classification. For
instance, a variant marked as a deletion by manta indicates an
intrachromosomal translocation with a deletion-like breakend pattern,
however there is no test of depth, b-allele frequency or intersecting
adjacencies to directly infer the SV type.

## Input requirements

The sequencing reads provided as input to Manta are expected to be
from a paired-end sequencing assay with an "innie" orientation between
the two reads of each DNA fragment, each presenting a read from the
outer edge of the fragment insert inward.

Manta can tolerate non-paired reads in the input, so long as
sufficient paired-end reads exist to estimate the paired fragment size
distribution. Non-paired reads will still be used in discovery,
assembly and split-read scoring if their alignments (or SA tag split
alignments) support a large indel or SV, or mismatch/clipping suggests
a possible breakend location.

Manta requires input sequencing reads to be mapped by an external tool
and provided as input in either BAM or CRAM format. Each input file must be
coordinate sorted and indexed to produce a`samtools/htslib`-style index in a
file named to match the input BAM orCRAM file with an additional '.bai', '.crai'
or '.csi' filename extension.

At configuration time, at least one BAM or CRAM file must be provided for the
normal or tumor sample. A matched tumor-normal sample pair can be
provided as well. If multiple input files are provided for the normal
sample, each file will be treated as a separate sample as part of a
joint diploid sample analysis.

The following limitations exist on the input BAM or CRAM files provided to
Manta:

* Alignments cannot contain the "=" character in the SEQ field.
* Alignments cannot use the sequence match/mismatch ("="/"X") CIGAR notation
* RG (read group) tags in the alignment records are ignored -- each file will be
treated as representing one sample.
* Alignments with basecall quality values greater than 70 are rejected (these
  are not supported on the assumption that this indicates an offset error)

Manta also requires a reference sequence in fasta format. This must be
the same reference used for mapping the input alignment files. The reference
must include a `samtools/htslib`-style index in a file named to match the
input fasta with an additional '.fai' file extension.


## Outputs

### Structural Variant predictions

The primary Manta outputs are a set of [VCF 4.1][1] files, found in
`${MANTA_ANALYSIS_PATH}/results/variants`. Currently there are 3 VCF
files created for a germline analysis, and an additional somatic VCF
is produced for a tumor/normal subtraction. These files are:

* __diploidSV.vcf.gz__
    * SVs and indels scored and genotyped under a diploid model for the set of
  samples in a joint diploid sample analysis or for the normal sample in a
  tumor/normal subtraction analysis. In the case of a tumor/normal subtraction,
  the scores in this file do not reflect any information from the tumor sample.
* __somaticSV.vcf.gz__
    * SVs and indels scored under a somatic variant model. This file
  will only be produced if a tumor sample alignment file is supplied during
  configuration
* __candidateSV.vcf.gz__
    * Unscored SV and indel candidates. Only a minimal amount of supporting
  evidence is required for an SV to be entered as a candidate in this file.
  An SV or indel must be a candidate to be considered for scoring, therefore
  an SV cannot appear in the other VCF outputs if it is not present in this
  file. Note that by default this file includes indels down to a very small
  size (>= 8 bases). These are intended to be passed on to a small variant
  caller without scoring by manta itself (by default manta scoring starts
  at size 51).
* __candidateSmallIndels.vcf.gz__
    * Subset of the candidateSV.vcf.gz file containing only simple insertion and
  deletion variants of size 50 or less. Passing this file to a small variant caller
  like strelka or starling (Isaac Variant Caller) will provide continuous
  coverage over all indel sizes when the small variant caller and manta outputs are
  evaluated together. Alternate small indel candidate sets can be parsed out of the
  candidateSV.vcf.gz file if this candidate set is not appropriate.

For tumor-only analysis, Manta will produce an additional VCF:

* __tumorSV.vcf.gz__
    * Unscored SV and indel candidates (same content as the __candidateSV.vcf.gz__ above),
  but including additional details: (1) paired and split read supporting evidence counts
  for each allele (2) a subset of the filters from the scored tumor-normal model
  are applied to the single tumor case to improve precision.

### Manta VCF reporting format

Manta VCF output follows the VCF 4.1 spec for describing structural
variants. It uses standard field names whereever possible. All custom
fields are described in the VCF header.  The section below highlights
some of the variant representation details and lists the primary VCF
field values.

#### VCF Sample Names

Sample names printed into the VCF output are extracted from each input
alignment file from the first read group ('@RG') record found in the
header. Any spaces found in the name will be replaced with
underscores. If no sample name is found a default SAMPLE1, SAMPLE2,
etc.. label will be used instead.

#### Small indels

All variants are reported in the VCF using symbolic alleles unless
they are classified as a small indel, in which case full sequences are
provided for the VCF `REF` and `ALT` allele fields. A variant is
classified as a small indel if all of these criteria are met:

* The variant can be entirely expressed as a combination of inserted and deleted sequence.
* The deletion or insertion length is not 1000 or greater.
* The variant breakends and/or the inserted sequence are not imprecise.

When VCF records are printed in the small indel format, they will also
include the `CIGAR` INFO tag describing the combined insertion and
deletion event.

#### Insertions with incomplete insert sequence assembly

Large insertions are reported in some cases even when the insert
sequence cannot be fully assembled.  In this case Manta reports the
insertion using the `<INS>` symbolic allele and includes the special
INFO fields `LEFT_SVINSSEQ` and `RIGHT_SVINSSEQ` to describe the
assembled left and right ends of the insert sequence. The following is
an example of such a record from the joint diploid analysis of
NA12878, NA12891 and NA12892 mapped to hg19:

```
chr1    11830208        MantaINS:1577:0:0:0:3:0 T       <INS>   999     PASS    END=11830208;SVTYPE=INS;CIPOS=0,12;CIEND=0,12;HOMLEN=12;HOMSEQ=TAAATTTTTCTT;LEFT_SVINSSEQ=TAAATTTTTCTTTTTTCTTTTTTTTTTAAATTTATTTTTTTATTGATAATTCTTGGGTGTTTCTCACAGAGGGGGATTTGGCAGGGTCACGGGACAACAGTGGAGGGAAGGTCAGCAGACAAACAAGTGAACAAAGGTCTCTGGTTTTCCCAGGCAGAGGACCCTGCGGCCTTCCGCAGTGTTCGTGTCCCTGATTACCTGAGATTAGGGATTTGTGATGACTCCCAACGAGCATGCTGCCTTCAAGCATCTGTTCAACAAAGCACATCTTGCACTGCCCTTAATTCATTTAACCCCGAGTGGACACAGCACATGTTTCAAAGAG;RIGHT_SVINSSEQ=GGGGCAGAGGCGCTCCCCACATCTCAGATGATGGGCGGCCAGGCAGAGACGCTCCTCACTTCCTAGATGTGATGGCGGCTGGGAAGAGGCGCTCCTCACTTCCTAGATGGGACGGCGGCCGGGCGGAGACGCTCCTCACTTTCCAGACTGGGCAGCCAGGCAGAGGGGCTCCTCACATCCCAGACGATGGGCGGCCAGGCAGAGACACTCCCCACTTCCCAGACGGGGTGGCGGCCGGGCAGAGGCTGCAATCTCGGCACTTTGGGAGGCCAAGGCAGGCGGCTGCTCCTTGCCCTCGGGCCCCGCGGGGCCCGTCCGCTCCTCCAGCCGCTGCCTCC  GT:FT:GQ:PL:PR:SR       0/1:PASS:999:999,0,999:22,24:22,32      0/1:PASS:999:999,0,999:18,25:24,20    0/0:PASS:230:0,180,999:39,0:34,0
```

#### VCF INFO Fields

ID | Description
--- | ---
IMPRECISE | Flag indicating that the structural variation is imprecise, i.e. the exact breakpoint location is not found
SVTYPE | Type of structural variant
SVLEN | Difference in length between REF and ALT alleles
END | End position of the variant described in this record
CIPOS | Confidence interval around POS
CIEND | Confidence interval around END
CIGAR | CIGAR alignment for each alternate indel allele
MATEID | ID of mate breakend
EVENT | ID of event associated to breakend
HOMLEN | Length of base pair identical homology at event breakpoints
HOMSEQ | Sequence of base pair identical homology at event breakpoints
SVINSLEN | Length of insertion
SVINSSEQ | Sequence of insertion
LEFT_SVINSSEQ | Known left side of insertion for an insertion of unknown length 
RIGHT_SVINSSEQ | Known right side of insertion for an insertion of unknown length
INV3 | Flag indicating that inversion breakends open 3' of reported location
INV5 | Flag indicating that inversion breakends open 5' of reported location
BND_DEPTH | Read depth at local translocation breakend
MATE_BND_DEPTH | Read depth at remote translocation mate breakend
JUNCTION_QUAL | If the SV junction is part of an EVENT (ie. a multi-adjacency variant), this field provides the QUAL value for the adjacency in question only
SOMATIC | Flag indicating a somatic variant
SOMATICSCORE | Somatic variant quality score
JUNCTION_SOMATICSCORE | If the SV junction is part of an EVENT (ie. a multi-adjacency variant), this field provides the SOMATICSCORE value for the adjacency in question only

#### VCF FORMAT Fields

ID | Description
--- | ---
GT | Genotype
FT | Sample filter, 'PASS' indicates that all filters have passed for this sample
GQ | Genotype Quality
PL | Normalized, Phred-scaled likelihoods for genotypes as defined in the VCF specification
PR | Number of spanning read pairs which strongly (Q30) support the REF or ALT alleles
SR | Number of split-reads which strongly (Q30) support the REF or ALT alleles

#### VCF FILTER Fields

ID | Description
--- | ---
MinQUAL | QUAL score is less than 20
MinGQ | GQ score is less than 15 (filter applied at sample level and record level if all samples are filtered)
MinSomaticScore | SOMATICSCORE is less than 30
Ploidy | For DEL & DUP variants, the genotypes of overlapping variants (with similar size) are inconsistent with diploid expectation
MaxDepth | Depth is greater than 3x the median chromosome depth near one or both variant breakends
MaxMQ0Frac | For a small variant (<1000 bases), the fraction of reads in all samples with MAPQ0 around either breakend exceeds 0.4
NoPairSupport | For variants significantly larger than the paired read fragment size, no paired reads support the alternate allele in any sample

#### Converting Manta VCF to BEDPE format

It can sometimes be convenient to express structural variants in BEDPE
format. For such applications we recommend the script `vcfToBedpe`
available from:

https://github.com/ctsa/svtools

This repository is forked from @hall-lab with edits to support VCF 4.1
SV format and match Manta's portability contstaints.

Note that BEDPE format greatly reduces structural variant information
compared to Manta's VCF output. In particular breakend orientation,
breakend homology and insertion sequence are lost, in addition to the
ability to define fields for locus and sample specific
information. For this reason we only recommend BEDPE as a temporary
intermediate output for applications which require it.


### Statistics

Additional secondary output is provided in `${MANTA_ANALYSIS_PATH}/results/stats`

* __alignmentStatsSummary.txt__
    * fragment length quantiles for each input alignment file
* __svLocusGraphStats.tsv__
    * statistics and runtime information pertaining to the SV locus graph
* __svCandidateGenerationStats.tsv__
    * statistics and runtime information pertaining to the SV candidate generation
* __svCandidateGenerationStats.xml__
    * xml data backing the svCandidateGenerationStats.tsv report 
    

## Run configuration and execution

Manta is run in a two step procedure: (1) configuration and (2)
workflow execution. The configuration step is used to specify the
input data and any options pertaining to the variant calling methods
themselves. The execution step is used to specify any parameters
pertaining to _how_ manta is executed (such as the total number of
cores or SGE nodes over which the jobs should be parallelized). The
second execution step can also be interrupted and restarted without
changing the final result of the workflow.

### Configuration

The workflow is configured with the script:
`${MANTA_INSTALL_PATH}/bin/configManta.py` . Running this script with
no arguments will display all standard configuration options to
folder. Note that all input alignment (BAM or CRAM) files and reference sequence must
contain the same chromosome names in the same order. In addition all
input alignment files and reference sequences must be indexed with
`samtools` (or a utility which creates equivilent index
files). Manta's default settings assume a whole genome DNA-Seq
analysis, but there are configuration options for exome/targeted
sequencing analysis in addition to RNA-Seq.

Single Diploid Sample Analysis -- Example Configuration:

```
${MANTA_INSTALL_PATH}/bin/configManta.py \
--bam NA12878_S1.bam \
--referenceFasta hg19.fa \
--runDir ${MANTA_ANALYSIS_PATH}
```

Joint Diploid Sample Analysis -- Example Configuration:

```
${MANTA_INSTALL_PATH}/bin/configManta.py \
--bam NA12878_S1.cram \
--bam NA12891_S1.cram \
--bam NA12892_S1.cram \
--referenceFasta hg19.fa \
--runDir ${MANTA_ANALYSIS_PATH}
```

Tumor Normal Analysis -- Example Configuration:

```
${MANTA_INSTALL_PATH}/bin/configManta.py \
--normalBam HCC1187BL.cram \
--tumorBam HCC1187C.cram \
--referenceFasta hg19.fa \
--runDir ${MANTA_ANALYSIS_PATH}
```

Tumor-Only Analysis -- Example Configuration:

```
${MANTA_INSTALL_PATH}/bin/configManta.py \
--tumorBam HCC1187C.cram \
--referenceFasta hg19.fa \
--runDir ${MANTA_ANALYSIS_PATH}
```

On completion, the configuration script will create the workflow run
script `${MANTA_ANALYSIS_PATH}/runWorkflow.py`. This can be used to
run the workflow in various parallel compute modes per the
instructions in the [Execution] section below.

#### Advanced configuration options

There are two sources of advanced configuration options:

* Options listed in the file: `${MANTA_INSTALL_PATH}/bin/configManta.py.ini`
    * These parameters are not expected to change frequently. Changing the file
  listed above will re-configure all manta runs for the installation. To change
  parameters for a single run, copy the configManta.py.ini file to another location,
  change the desired parameter values and supply the new file using the configuration
  script's `--config FILE` option.
* Advanced options listed in: `${MANTA_INSTALL_PATH}/bin/configManta.py --allHelp`
    * These options are intended primarily for workflow development and
  debugging, but could be useful for runtime optimization in some specialized
  cases.

### Execution

The configuration step creates a new workflow run script in the
requested run directory:

`{MANTA_ANALYSIS_PATH}/runWorkflow.py`

This script is used to control parallel execution of Manta via the
[pyFlow][2] task engine. It can be used to parallelize structural
variant analysis via one of two modes:

1. Parallelized across multiple cores on a single node.
2. Parallelized across multiple nodes on an SGE cluster.

A running workflow can be interrupted at any time and resumed where it
left off. If desired, the resumed analysis can use a different running
mode or total core count.

For a full list of execution options, see:

`{MANTA_ANALYSIS_PATH}/runWorkflow.py -h`

Example execution on a single node:

```
${MANTA_ANALYSIS_PATH}/runWorkflow.py -m local -j 8
```

Example execution on an SGE cluster:

```
${MANTA_ANALYSIS_PATH}/runWorkflow.py -m sge -j 36
```

#### Advanced execution options

These options are useful for Manta development and debugging:

* Stderr logging can be disabled with `--quiet` argument. Note this
  log is replicated to
  `${MANTA_ANALYSIS_PATH}/workspace/pyflow.data/logs/pyflow_log.txt`
  so there is no loss of log information.
* The `--rescore` option can be provided to force the workflow to
  re-execute candidates discovery and scoring, but not the initial
  graph generation steps.

### Extended use cases

#### Exome/Targeted

Supplying the '--exome' flag at configuration time will provide
appropriate settings for WES and other regional enrichment
analyses. At present this flag disables all high depth filters, which
are designed to exclude pericentromeric reference compressions in the
WGS case but cannot be applied correctly to a targeted analysis.

For small targeted regions, it may also be helpful to consider the
high sensitivity calling documentation below.

#### Unpaired tumor sample

Manta supports SV calling for tumor sample only. The tumor-only mode 
can be triggered by supplying a tumor sample alignment file but no alignments for the normal sample. 
The results are reported in __tumorSV.vcf.gz__. This file contains all
SV candidates (similar to the __candidateSV.vcf.gz__ file), but also
includes paired and split read evidence for each allele and a
subset of the filters used for the tumor-normal comparative analysis. 
Note that Manta does not yet provide a quality scoring model for unpaired
tumor sample analysis.

For low allele frequency variants, it may also be helpful to consider the
high sensitivity calling documentation below.

### RNA-Seq

Supplying the '--rna' flag at configuration time will provide
experimental settings for RNA-Seq Fusion calling. At present this flag
disables all high depth filters which are designed to exclude
pericentromeric reference compressions in the WGS case but cannot be
applied correctly to RNA-Seq analysis.  In addition many custom RNA
read processing and alignment steps are invoked. This mode is designed
to function as part of larger workflow with additional steps to reduce
overall false positive rate which take place downstream from Manta's
fusion calling step.

It may also be helpful to consider the high sensitivity calling
documentation below for this mode.

### High sensitivity calling

Manta is configured with a discovery sensitivity appropriate for
general WGS applications.  In targeted or other specialized contexts
the candidate sensitivity can be increased. A recommended general high
sensitivity mode can be obtained by changing the two values
'minEdgeObservations' and 'minCandidateSpanningCount' in the manta
configuration file (see 'Advanced configuration options' above) to 2
observations per candidate (the default is 3):

```
# Remove all edges from the graph unless they're supported by this many 'observations'.
# Note that one supporting read pair or split read usually equals one observation, but
# evidence is sometimes downweighted.
minEdgeObservations = 2

# Run discovery and candidate reporting for all SVs/indels with at least this
# many spanning support observations
minCandidateSpanningCount = 2
```


[1]: http://www.1000genomes.org/wiki/Analysis/Variant%20Call%20Format/vcf-variant-call-format-version-41
[2]: http://sequencing.github.io/pyflow/
