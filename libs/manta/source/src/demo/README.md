Manta Workflow Demo
-------------------

This directory contains a small dateset which can be used to verify
correct installation and demonstrate basic elements of the
workflow. To run the demonstration, run the demo script found in the
installation bin directory:

```
python ${MANTA_INSTALL_PATH}/bin/runMantaWorkflowDemo.py
```

This script creates a `MantaDemoAnalysis` directory under the current
working directory, runs Manta on a small demo dataset, and compares
the somatic structural variant output to an expected result.

The demo data contain reads from HCC1954/HCC1954BL mapped in the
vicinity of somatic translocation breakends corresponding to COSMIC
variant [COST16011][1]. The demo sequencing data is extracted from
[TCGA Benchmark 4][2].

[1]:http://grch37-cancer.sanger.ac.uk/cosmic/rearrangement/overview?id=16011
[2]:https://cghub.ucsc.edu/datasets/benchmark_download.html
