<!-- dx-header -->
# QUAST

QUAST stands for Quality Assessment Tool for Genome Assemblies.
<br>
QUAST evaluates a quality of genome assemblies by computing various metrics and providing nice reports.
You can find all project news and the latest versions of source code and manual at 
http://bioinf.spbau.ru/quast.
<br>
<a href="http://dx.doi.org/10.1093/bioinformatics/btt086">QUAST paper</a> was published in Bioinfomatics journal on 15 April 2013.


Please help us to make QUAST better by sending your comments, bug reports, and 
suggestions to <a href="mailto:quast.support@bioinf.spbau.ru">quast.support@bioinf.spbau.ru</a>.

## Inputs

* **List of contigs**: Fasta files with assembled contigs or scaffolds (can be compressed).
* **Reference**: Fasta file with reference genome (can be compressed).
* **Genes**: Annotated genes file. GFF, NCBI and plain text formats are supported.
* **Operons**: Annotated operons file. GFF, NCBI and plain text formats are supported.
* **Min contig threshold**: Lower threshold for contig length.
* **Gene finding**: Uses Gene Finding module.
* **Genome is an eukaryote**: Genome is an eukaryote.
* **Estimated reference length**: Estimated reference length (for computing NGx metrics without a reference genome) .
* **Provided assemblies are scaffolds**: Provided assemblies are scaffolds.


## Outputs

* **Report**: An assessment summary in an interactive HTML file. Detailed metric descriptions can be found in <a href="http://quast.bioinf.spbau.ru/manual.html">manual</a>.
