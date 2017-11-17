#!/usr/bin/env python3
# coding: utf-8
"""
.. module:: Analysis
   :synopsis: base class for the analysis pipeline
.. versionadded:: 3.0.0
.. versionchanged:: 3.0.0

Copyright (c) 2016-2017, Evgeny Zdobnov (ez@ezlab.org)
Licensed under the MIT license. See LICENSE.md file.

"""

from abc import ABCMeta, abstractmethod


class Analysis:

    # declare a metaclass ABCMeta, which means that this class is abstract
    __metaclass__ = ABCMeta

    TMP_DEFAULT = './tmp/'

    """
    This class defines methods required for any pipeline and
    provides implementations for utility methods
    """

    # Standard Genetic code for translating nucleotides
    CODONS = {'TTT': 'F', 'TTC': 'F', 'TTY': 'F',
              'TTA': 'L', 'TTG': 'L', 'CTT': 'L',
              'CTC': 'L', 'CTA': 'L', 'CTG': 'L', 'CTN': 'L',
              'YTN': 'L', 'TTR': 'L',
              'ATT': 'I', 'ATC': 'I', 'ATA': 'I', 'ATH': 'I',
              'ATG': 'M',
              'GTT': 'V', 'GTC': 'V', 'GTA': 'V', 'GTG': 'V', 'GTN': 'V',
              'TCT': 'S', 'TCC': 'S', 'TCA': 'S', 'TCG': 'S', 'TCN': 'S',
              'AGT': 'S', 'AGC': 'S', 'WSN': 'S', 'AGY': 'S',
              'CCT': 'P', 'CCC': 'P', 'CCA': 'P', 'CCG': 'P', 'CCN': 'P',
              'ACT': 'T', 'ACC': 'T', 'ACA': 'T', 'ACG': 'T', 'ACN': 'T',
              'GCT': 'A', 'GCC': 'A', 'GCA': 'A', 'GCG': 'A', 'GCN': 'A',
              'TAT': 'Y', 'TAC': 'Y', 'TAY': 'Y',
              'TAA': 'X', 'TAG': 'X', 'TGA': 'X', 'TRR': 'X', 'TAR': 'X',
              'NNN': 'X',
              'CAT': 'H', 'CAC': 'H', 'CAY': 'H',
              'CAA': 'Q', 'CAG': 'Q', 'CAR': 'Q',
              'AAT': 'N', 'AAC': 'N', 'AAY': 'N',
              'AAA': 'K', 'AAG': 'K', 'AAR': 'K',
              'GAT': 'D', 'GAC': 'D', 'GAY': 'D',
              'GAA': 'E', 'GAG': 'E', 'GAR': 'E',
              'TGT': 'C', 'TGC': 'C', 'TGY': 'C',
              'TGG': 'W',
              'CGT': 'R', 'CGC': 'R', 'CGA': 'R', 'CGG': 'R', 'MGN': 'R',
              'CGN': 'R', 'AGR': 'R',
              'AGA': 'R', 'AGG': 'R',
              'GGT': 'G', 'GGC': 'G', 'GGA': 'G', 'GGG': 'G', 'GGN': 'G'}

    # Complementary nucleotides
    COMP = {'A': 'T', 'T': 'A', 'U': 'A',
            'C': 'G', 'G': 'C',
            'B': 'V', 'V': 'B',
            'D': 'H', 'H': 'D',
            'K': 'M', 'M': 'K',
            'S': 'S', 'W': 'W',
            'R': 'Y', 'Y': 'R',
            'X': 'X', 'N': 'N'}

# end of helper methods

    def __init__(self, config):
        """
        :param config: An instance of a class loading all parameters from config file
        :type config: PipeConfig
        """
        self._sequences = ''
        self._params = config
        self._init_tools()

    @abstractmethod
    def run_analysis(self):
        """
        Abstract method, override to call all needed steps for running the child analysis.
        """
        pass

    def check_nucleotide_file(self):
        """
        This function checks that the provided file is nucleotide
        :raises SystemExit: if AA found
        """
        aas = set(Analysis.CODONS.values())
        nucls = Analysis.COMP.keys()
        for nucl in nucls:
            try:
                aas.remove(nucl)
            except KeyError:
                pass
        nucl_file = open(self._sequences)
        n = 0
        for line in nucl_file:
            if n > 10:
                break
            n += 1
            if '>' not in line:
                for aa in aas:
                    if aa.upper() in line or aa.lower() in line:
                        nucl_file.close()
                        return False
        nucl_file.close()
        return True

    def check_protein_file(self):
        """
        This function checks that the provided (sequence) file is protein
        :raises SystemExit: if only ACGTN is found over a reasonable
        amount of lines
        """
        aas = set(Analysis.CODONS.values())
        aas.remove('A')
        aas.remove('C')
        aas.remove('G')
        aas.remove('T')
        aas.remove('N')
        is_aa = False
        prot_file = open(self._sequences)
        n = 0
        for line in prot_file:
            if n > 100:
                break
            n += 1
            if '>' not in line:
                for aa in aas:
                    if aa.lower() in line or aa.upper() in line:
                        is_aa = True
                        break
        prot_file.close()
        if not is_aa:
            return False
        return True

    @abstractmethod
    def _init_tools(self):
        """
        Abstract method, override to init the tools of the child analysis
        """
        pass


# end of validated methods
