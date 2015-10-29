Manta Structural Variant Caller
===============================

Version: 0.29.0

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

[UserGuide]:src/markdown/mantaUserGuide.md
[mss]:http://dx.doi.org/10.1101/024232


License
-------

Manta source code is provided under the [GPLv3 license] (LICENSE.txt).
Manta includes several third party packages provided under other
open source licenses, please see [COPYRIGHT.txt] (COPYRIGHT.txt)
for additional details.


Build instructions
------------------

It is recommended to start from one of the [binary distributions on
the Manta releases page] [releases] if a suitable version is available
(note that the CentOS 5 binary distribution is expected to support a
large variety of linux systems).  If building from source start from
the release distributions of the source code, also provided on the
[Manta releases page] [releases]. Cloning/archiving the source
directly from git could result in missing version number entries,
undesirably stringent build requirements or an unstable development
version between releases. Additional build notes for contributors can
be found below.


[releases]:https://github.com/Illumina/manta/releases

[tcistatus]:https://travis-ci.org/Illumina/manta.svg?branch=master
[tcihome]:https://travis-ci.org/Illumina/manta

### Build prerequisites

Manta requires a compiler supporting most of the C++11 standard. These
are the current minimum versions enforced by the build system:

* python 2.4+
* gcc 4.7+ OR clang 3.1+ (OR Visual Studio 2013+, see windev note below)

### Runtime prerequisites

* python 2.4+

### Operating System Guidelines

##### Linux 

Manta is known to build and run on the following linux distributions
(with additional packages as described below):

- Ubuntu 12.04,14.04
- CentOS 5,6,7

##### OS X

Manta builds and passes basic tests on OS X 10.9, but full WGS analyses
are not tested for this platform.

##### Windows

Manta does not build or run on windows. Library-level compilation is
possible for Visual Studio users. See Contributor section below.

### Linux Package Additions

##### Ubuntu 14.04

    apt-get update -qq
    apt-get install -qq gcc g++ make python

##### Ubuntu 12.04

    apt-get update -qq
    apt-get install -qq bzip2 gcc g++ make python python-software-properties
    # add gcc 4.8 from ubuntu ppa:
    add-apt-repository -y ppa:ubuntu-toolchain-r/test
    apt-get update -qq
    apt-get install -qq gcc-4.8 g++-4.8

    # Prior to build configuration, set CC/CXX to gcc 4.8:
    export CC=/usr/bin/gcc-4.8
    export CXX=/usr/bin/g++-4.8

##### CentOS 7

    yum install -y tar bzip2 make gcc gcc-c++

##### CentOS 5 and 6

    yum install -y tar wget bzip2 make gcc gcc-c++
    # add gcc 4.8 from developer tools v2:
    wget http://people.centos.org/tru/devtools-2/devtools-2.repo -O /etc/yum.repos.d/devtools-2.repo
    yum install -y devtoolset-2-gcc devtoolset-2-gcc-c++ devtoolset-2-binutils

    # Prior to build configuration, set CC/CXX to gcc 4.8:
    export CC=/opt/rh/devtoolset-2/root/usr/bin/gcc
    export CXX=/opt/rh/devtoolset-2/root/usr/bin/g++

### Build procedure

After acquiring a release distribution of the source code, the build
procedure is:

* Unpack source code
* Create and move to a separate `build` directory (out-of-source build is required.)
* Configure build
* Compile & Install

Example (building on 4 cores):

    wget https://github.com/Illumina/manta/releases/download/vA.B.C/manta-A.B.C.release_src.tar.bz2
    tar -xjf manta-A.B.C.release_src.tar.bz2
    mkdir build && cd build
    # Ensure that CC and CXX are updated to target compiler if needed, e.g.:
    #     export CC=/path/to/cc
    #     export CXX=/path/to/c++
    ../manta-A.B.C.release_src/src/configure --jobs=4 --prefix=/path/to/install
    make -j4 install

Note that during the configuration step, the following dependencies
will be built from source if they are not found:

* cmake 2.8.0+
* boost 1.56.0+

To accelerate this process the configuration step can be parallelized
over multiple cores, as demonstrated in the example above with the
`--jobs=4` argument to configure.

To see more configure options, run:

    ${MANTA_SRC_PATH}/configure --help

##### Workflow relocation

After Manta is built the installation directory can be relocated to
another directory.  All internal paths used in the workflow are
relative.

### Demo / Build Verification 

To help verify a successful installation, Manta includes a small demo
data set and test script. After completing the installation steps
above, the demo can be run as follows:

    python ${MANTA_INSTALL_PATH}/bin/runMantaWorkflowDemo.py

See [the demo README](src/demo/README.md) for additional information
on the test script and data.


Data analysis and Interpretation
--------------------------------

After completing installation, see the [Manta user guide] [UserGuide]
for instructions on how to run Manta, interpret results, and a
high-level overview of the method.

The user guide is also available within any Manta binary distribution
here:

    ${MANTA_INSTALL_PATH}/doc/html/mantaUserGuide.html

### Runtime hardware requirements

Manta workflows are parallelized at the process level using the
[pyFlow] [pyflow_site] task manager. pyFlow can distrubute Manta
workflows to a specified number of cores on a single host or
SGE-managed cluster.

As a useful runtime benchmark, [Platinum Genomes] [PG] sequencing
reads for NA12878 at 50x coverage (whole genome) can be analyzed in
less than 20 minutes on 20 physical cores using a dual Xeon E5-2680
v2 server with the BAM accessed from a conventional local
drive, peak total memory (RSS) for this run was 2.35 Gb.
Additional hardware notes:

* **Memory** Typical memory requirements are <1Gb/core for germline
analysis and <2Gb/core for cancer/FFPE/highly rearranged
samples. The exact requirement depends on many factors including
sequencing depth, read length, fragment size and sample quality.

* **CPU** Manta does not require or benefit from any specific modern
CPU feature (e.g. NUMA, AVX..), but in general faster clock and
larger caches will improve performance.

* **I/O** I/O can be roughly approximated as 1.1 reads of the
input alignment file per analysis, with no writes that are significant
relative to the alignment file size.

[pyflow_site]:http://illumina.github.io/pyflow
[PG]:http://www.platinumgenomes.org


Contributor build configuration
-------------------------------

When Manta is cloned from github, it is configured for development
rather than user distribution. As such, builds are strict: all
warnings are treated as errors and if cppcheck is found any detected
issue is converted to a build error.

#### Source documentation

If doxygen is found in the path (and optionally dot as well) during
build configuration, then c++ documentation is available as an
additional "doc" target for the makefile:

    make doc

There is no installation for the documentation outside of the build
directory, the root doxygen page after completing this target will be:

    ${MANTA_BUILD_PATH}/c++/doxygen/html/index.html

#### Improving build time

##### ccache

The build system is configured to use ccache whenever this is
found in the path

##### Bundled dependencies

Note that during the configuration step, the following dependencies will be
built from source if they are not found:

* cmake 2.8.0+
* boost 1.56.0+

To avoid the extra time associated with this step, ensure that (1)
cmake 2.8.0+ is in your PATH and (2) BOOST\_ROOT is defined to point
to boost 1.56.0 or newer.

#### Address Sanitizer

The build system offers first-class support for google address sanitizer
when a supporting compiler is detected. To use this mode, start a fresh
installation process with the additional configure option `--build-type=ASan`,
extending from the configuration example in the above build instructions, use:

    ../manta-A.B.C.release_src/src/configure --jobs=4 --prefix=/path/to/install --build-type=ASan

#### Windows development support

Manta does not link or run on windows. However, the build system does
facilitate Visual Studio (VS) users. When cmake configuration is run
on windows, all linking is disabled and most third party libraries are
unpacked for header include access, but are not compiled. Cmake VS
solutions allow the c++ code to be browsed, analyzed and compiled to
the library level.  Note that unit test codes are compiled to
libraries but cannot be run.

C++11 features used by manta require at least VS2013. A Windows
installation of cmake is also required to configure and compile.
Note that the minimum cmake version for Windows is 3.1.0

