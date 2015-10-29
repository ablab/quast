
# Manta Contributor Guide - Alignment Library

This is a subsection of the manta contributor guide focusing on the alignment library.

## Debugging

All alignment options support the DEBUG_ALN definition. When defined, the score matrix and backtrace pointer for each element in the
alignment structure are printed to stderr, in addition to the backtrace sequence. Due to the volume of output, this can be
most usefully applied to a single alignment unit test.

For even richer debugging the DEBUG_ALN_MATRIX value can be defined. This is currently only enabled for the more complex jump aligners:
the standard jump aligner and intron jump aligner. When this option is defined the entire score matrix is saved (this is normally discarded
as soon as each element row is no longer required), and the score+backpointer matrix for each state is printed to stderr. This is only
effective for very small unit test cases.

## Running single alignment unit tests

Note that unit tests can be run outside of the manta build system, and additionally reduced to run a single test suite or single test. This
can be usefully combined with the rich debug output options above. Note that this test selection interface is a standard defined by
the boost unit test library.

To run unit tests directly. First go through the standard build procedure. Then from the build directory, the alignment unit tests can be run
by executing:

    c++/lib/alignment/test/manta_unit_test_alignment

You can run the tests for the, e.g. intron aligner only using the `-t` option as follows:

    c++/lib/alignment/test/manta_unit_test_alignment -t test_GlobalJumpIntronAligner

...or select as single test out of this test suite:

    c++/lib/alignment/test/manta_unit_test_alignment -t test_GlobalJumpIntronAligner/test_GlobalJumpAlignerSplice
