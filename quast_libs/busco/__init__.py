#!/usr/bin/env python
# coding: utf-8
"""
.. package:: busco
   :synopsis: BUSCO - Benchmarking Universal Single-Copy Orthologs.


Copyright (c) 2016-2017, Evgeny Zdobnov (ez@ezlab.org)
Licensed under the MIT license. See LICENSE.md file.

"""
from ._version import __version__ as version
__all__ = ['BuscoAnalysis', 'GeneSetAnalysis', 'GenomeAnalysis', 'TranscriptomeAnalysis', 'BuscoConfig']
__version__ = version
