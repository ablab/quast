#!/usr/bin/env python
# coding: utf-8
"""
.. module:: BUSCO
   :synopsis: BUSCO - Benchmarking Universal Single-Copy Orthologs.
.. moduleauthor:: Felipe A. Simao <felipe.simao@unige.ch>
.. moduleauthor:: Robert M. Waterhouse <robert.waterhouse@unige.ch>
.. moduleauthor:: Mathieu Seppey <mathieu.seppey@unige.ch>
.. versionadded:: 1.0
.. versionchanged:: 2.0.1

BUSCO - Benchmarking Universal Single-Copy Orthologs.

To get help, ``python BUSCO.py -h``. See also the user guide.

Visit our website `<http://busco.ezlab.org/>`_

Copyright (C) 2016 E. Zdobnov lab

BUSCO is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version. See <http://www.gnu.org/licenses/>

BUSCO is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

"""

import os
import sys
import subprocess
from quast_libs.ra_utils import argparse
from quast_libs.ra_utils.argparse import RawTextHelpFormatter
import time
import threading
import heapq
import logging
import copy
import traceback
import random
from abc import ABCMeta, abstractmethod
from collections import deque
from os.path import join

from quast_libs import qconfig
from quast_libs.qutils import is_non_empty_file, get_blast_fpath

try:
    import queue as Queue
except ImportError:
    import Queue  # Python 2


class BUSCOLogger(object):
    """
    This class customizes the _logger class
    """

    def __init__(self, name):
        """
        :param name: the name of the BUSCOLogger instance to be created
        :type name: str
        """
        self._logger = logging.getLogger(name)
        self._logger.setLevel(logging.INFO)
        self._has_warning = False
        self._formatter = logging.Formatter('%(levelname)s\t%(message)s')
        self._thread_formatter = logging.Formatter('%(levelname)s:%(threadName)s\t%(message)s')
        self._formatter_blank_line = logging.Formatter('')
        console_handler = logging.StreamHandler(sys.stdout)
        self._logger.addHandler(console_handler)

    def set_up_file_handler(self, output_dirpath):
        self._log_fpath = os.path.join(output_dirpath, 'busco.log')
        for handler in self._logger.handlers:
            self._logger.removeHandler(handler)
        self._out_hdlr = logging.FileHandler(self._log_fpath, mode='w')
        self._out_hdlr.setFormatter(self._formatter)
        self._out_hdlr.setLevel(logging.INFO)
        self._logger.addHandler(self._out_hdlr)

    def add_blank_line(self):
        """
        This function add a blank line in the logs
        """
        self._logger.info('')

    def add_thread_info(self):
        """
        This function appends the thread name to the logs output, e.g. INFO:BUSCO.py:thread_name
        """
        self._out_hdlr.setFormatter(self._thread_formatter)

    def remove_thread_info(self):
        """
        This function disables the thread name in the logs output, e.g. INFO:BUSCO.py
        """
        self._out_hdlr.setFormatter(self._formatter)

    def warn(self, msg, *args, **kwargs):
        """
        This function overrides the _logger class warn
        :param msg: the message to log
        :type msg: str
        """
        self.warning(msg, *args, **kwargs)

    def warning(self, msg, *args, **kwargs):
        """
        This function overrides the _logger class warning
        :param msg: the message to log
        :type msg: str
        """
        self._has_warning = True
        self._logger.warning(msg, *args, **kwargs)

    def has_warning(self):
        """
        :return: whether the _logger did log warnings
        :rtype: book
        """
        return self._has_warning

    def info_external_tool(self, tool, msg, *args, **kwargs):
        """
        This function logs an info line mentioning this is an external tool
        :param tool: the name of the tool
        :type tool: str
        :param msg: the message
        :type msg: str
        :return:
        """
        if msg != '':  # do not log blank lines
            self._logger.info('[%s]\t%s' % (tool, msg), *args, **kwargs)


class Analysis(object):
    """
    This class is the parent of all type of analysis that can be done by BUSCO. It has to be extended by a subclass \
    to represent an actual analysis
    """

    class _AugustusThreads(threading.Thread):
        """
        This class extends ``threading.Thread`` to run Augustus in a multi-threaded manner.
        .. seealso:: Analysis._augustus(), Analysis._augustus_rerun(), Analysis._process_augustus_tasks()
        """

        CLASS_ABREV = 'augustus'

        def __init__(self, thread_id, name, analysis):
            """
            :param thread_id: an int id for the thread
            :type thread_id: int
            :param name: a name for the thread
            :type name: str
            :param analysis: the Analysis object that is bound to this thread
            :type analysis: Analysis
            """
            threading.Thread.__init__(self)
            self.thread_id = thread_id
            self.name = name
            self.analysis = analysis

        def run(self):
            """
            This function defines what is run by within the thread
            """
            self.analysis._process_augustus_tasks()

    class _HmmerThreads(threading.Thread):
        """
        This class extends ``threading.Thread`` to run hmmersearch in a multi-threaded manner.
        .. seealso:: Analysis._hmmer(), Analysis._augustus_rerun(), Analysis._process_hmmer_tasks()
        """

        CLASS_ABREV = 'hmmer'

        def __init__(self, thread_id, name, analysis):
            """
            :param thread_id: an int id for the thread
            :type thread_id: int
            :param name: a name for the thread
            :type name: str
            :param analysis: the Analysis object that is bound to this thread
            :type analysis: Analysis
            """
            threading.Thread.__init__(self)
            self.thread_id = thread_id
            self.name = name
            self.analysis = analysis

        def run(self):
            """
            This function defines what is run by within the thread
            """
            self.analysis._process_hmmer_tasks()

    class _Gff2gbSmallDNAThreads(threading.Thread):
        """
        This class extends ``threading.Thread`` to run the gff2gbSmallDNA.pl script in a multi-threaded manner.
        .. seealso:: Analysis._process_gff2gbsmalldna_tasks()
        """

        CLASS_ABREV = 'gff2gbsmalldna'

        def __init__(self, thread_id, name, analysis):
            """
            :param thread_id: an int id for the thread
            :type thread_id: int
            :param name: a name for the thread
            :type name: str
            :param analysis: the Analysis object that is bound to this thread
            :type analysis: Analysis
            """
            threading.Thread.__init__(self)
            self.thread_id = thread_id
            self.name = name
            self.analysis = analysis

        def run(self):
            """
            This function defines what is run by within the thread
            """
            self.analysis._process_gff2gbsmalldna_tasks()

    # declare a metaclass ABCMeta, which means that this class is abstract
    __metaclass__ = ABCMeta

    # Default params
    EVALUE_DEFAULT = 1e-3
    MAX_FLANK = 20000
    REGION_LIMIT_DEFAULT = 3
    CPUS_DEFAULT = 1
    TMP_DEFAULT = './tmp'
    SPECIES_DEFAULT = 'fly'

    # Genetic code for translating nucleotides
    CODONS = {'TTT': 'F', 'TTC': 'F', 'TTY': 'F',
              'TTA': 'L', 'TTG': 'L', 'CTT': 'L', 'CTC': 'L', 'CTA': 'L', 'CTG': 'L', 'CTN': 'L',
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
              'TAA': 'X', 'TAG': 'X', 'TGA': 'X', 'TRR': 'X', 'TAR': 'X', 'NNN': 'X',
              'CAT': 'H', 'CAC': 'H', 'CAY': 'H',
              'CAA': 'Q', 'CAG': 'Q', 'CAR': 'Q',
              'AAT': 'N', 'AAC': 'N', 'AAY': 'N',
              'AAA': 'K', 'AAG': 'K', 'AAR': 'K',
              'GAT': 'D', 'GAC': 'D', 'GAY': 'D',
              'GAA': 'E', 'GAG': 'E', 'GAR': 'E',
              'TGT': 'C', 'TGC': 'C', 'TGY': 'C',
              'TGG': 'W',
              'CGT': 'R', 'CGC': 'R', 'CGA': 'R', 'CGG': 'R', 'MGN': 'R', 'CGN': 'R', 'AGR': 'R',
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

    @staticmethod
    def cmd_exists(cmd):
        """
        Check if command exists and is accessible from the command-line
        :param cmd: a bash command
        :type cmd: str
        :return: True if the command can be run, False if it is not the case
        :rtype: bool
        """
        return subprocess.call('type %s' % cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE) == 0

    @staticmethod
    def p_open(cmd, name, shell=False):
        """
        This function call subprocess.Popen for the provided command and log the results with the provided name
        :param cmd: the command to execute
        :type cmd: list
        :param name: the name to use in the log
        :type name: str
        :param shell: whether to use the shell parameter to Popen. Needed if wildcard charcter used (*?). See on web
        :type shell: bool
        """
        # note, all augustus related commands do not write to the stdout and stderr and therefore get nothing here
        process = subprocess.Popen(cmd, stderr=subprocess.PIPE, stdout=subprocess.PIPE, shell=shell)
        process_out = process.stderr.readlines() + process.stdout.readlines()
        for line in process_out:
            _log.info_external_tool(name, line.decode("utf-8").strip())

    @staticmethod
    def check_output(*popenargs, **kwargs):
        r"""Run command with arguments and return its output as a byte string.
        Backported from Python 2.7 as it's implemented as pure python on stdlib.
        """
        process = subprocess.Popen(stdout=subprocess.PIPE, *popenargs, **kwargs)
        output, unused_err = process.communicate()
        retcode = process.poll()
        if retcode:
            cmd = kwargs.get("args")
            if cmd is None:
                cmd = popenargs[0]
            error = subprocess.CalledProcessError(retcode, cmd)
            error.output = output
            raise error
        return output

    @staticmethod
    def check_fasta_header(header):
        """
        This function checks problematic characters in fasta headers, and warns the user and stops the execution
        :param header: a fasta header to check
        :type header: str
        :raises SystemExit: if a problematic character is found
        """
        for char in FORBIDDEN_HEADER_CHARS:
            if char in header:
                _logger.error('The character \'%s\' is present in the fasta header %s, '
                              'which will crash BUSCO. '
                              'Please clean the header of your input file.' % (char, header.strip()))
                raise SystemExit

        for char in FORBIDDEN_HEADER_CHARS_BEFORE_SPLIT:
            if char in header.split()[0]:
                _logger.error('The character \'%s\' is present in the fasta header %s, '
                              'which will crash BUSCO. '
                              'Please clean the header of your input file.' % (char, header.split()[0].strip()))
                raise SystemExit

        if header.split()[0] == '>':
            _logger.error('A space is present in the fasta header %s, directly after \'>\' '
                          'which will crash BUSCO. '
                          'Please clean the header of your input file.' % (header.strip()))
            raise SystemExit

    @staticmethod
    def _check_blast():
        """
        Check if blast is accessible from command-line (tblastn)
        :raises SystemExit: if blast is not accessible
        """
        if not get_blast_fpath('tblastn'):
            _logger.error('Blast is not accessible')
            raise SystemExit

    @staticmethod
    def _check_hmmer():
        """
        Check if it is accessible from command-line (as 'hmmsearch')
        Also check if HMMer is the correct version (3.1+)
        :raises SystemExit: if HMMer is not accessible or not the correct version
        """
        if not hmmer_cmd:
            _logger.error('HMMer is not accessible')
            raise SystemExit
        else:
            try:
                hmmer_check = Analysis.check_output(hmmer_cmd + ' -h', shell=True)
                hmmer_check = hmmer_check.decode('utf-8')
                hmmer_check = hmmer_check.split('\n')[1].split()[2]
                hmmer_check = float(hmmer_check[:3])
            except ValueError:
                # to avoid a crash with super old version and notify the user, will be useful
                hmmer_check = Analysis.check_output(hmmer_cmd + ' -h', shell=True)
                hmmer_check = hmmer_check.decode('utf-8')
                hmmer_check = hmmer_check.split('\n')[1].split()[1]
                hmmer_check = float(hmmer_check[:3])
            except subprocess.CalledProcessError:
                _logger.error('HMMer is not accessible')
                raise SystemExit
            if hmmer_check >= 3.1:
                pass
            else:
                _logger.error('HMMer version detected is not supported, please use HMMer 3.1+')
                raise SystemExit

    @staticmethod
    def _split_seq_id(seq_id):
        """
        This function split the provided seq id into id, start and stop
        :param seq_id: the seq id to split
        :type seq_id: str
        :return: a dict containing the id, the start, the end found in seq_id
        :rtype: dict
        """
        # -2,-1 instead of 0,1, if ':' in the fasta header, same for [,]
        name = seq_id.replace(']', '').split('[')[-1].split(':')[-2]
        start = seq_id.replace(']', '').split('[')[-1].split(':')[-1].split('-')[0]
        end = seq_id.replace(']', '').split('[')[-1].split(':')[-1].split('-')[1]
        return {'id': name, 'start': start, 'end': end}

    @staticmethod
    def _reformats_seq_id(seq_id):
        """
        This function reformats the sequence id to its original values, if it was somehow modified during the process
        It has to be overriden by subclasses when needed
        :param seq_id: the seq id to reformats
        :type seq_id: str
        :return: the reformatted seq_id
        :rtype: str
        """
        return seq_id

    def check_dataset(self):
        """
        Check the dataset integrity, if files and folder are present
        :raises SystemExit: if the dataset miss files or folders
        """
        # hmm folder
        flag = False
        for dirpath, dirnames, files in os.walk('%shmms' % self._clade_path):
            if files:
                flag = True
        if not flag:
            _logger.error('The dataset you provided lacks hmm profiles in %shmms' % self._clade_path)
            raise SystemExit
            # note: score and length cutoffs are checked when read, see _load_scores and _load_lengths

    def _check_augustus(self):
        """
        Check if Augustus is accessible from command-line and properly configured.
        :raises SystemExit: if Augustus is not accessible
        :raises SystemExit: if Augustus config path is not writable or not set at all
        :raises SystemExit: if Augustus config path does not contain the needed species
        :raises SystemExit: if additional perl scripts for retraining are not present
        """
        if not augustus_cmd:
            _logger.error('Augustus is not accessible from the command-line, please add it to the environment')
            raise SystemExit

        try:
            if not os.access(self._augustus_config_path, os.W_OK):
                _logger.error('Cannot write to Augustus config path, please make sure you have '
                              'write permissions to %s' % self._augustus_config_path)
                raise SystemExit
        except TypeError:
            _logger.error('The environment variable AUGUSTUS_CONFIG_PATH is not set')
            raise SystemExit

        if not os.path.exists(self._augustus_config_path + '/species/%s' % self._target_species):
            _logger.error('Impossible to locate the species "%s" in Augustus config path (%sspecies), check '
                          'that AUGUSTUS_CONFIG_PATH is properly set and contains this species. '
                          '\n\t\tSee the help if you want to provide an alternative species'
                          % (self._target_species, self._augustus_config_path))
            raise SystemExit
        if not get_augustus_script('gff2gbSmallDNA.pl'):
            _logger.error('Impossible to locate the required script gff2gbSmallDNA.pl. Check that you declared '
                          'the Augustus scripts folder in your $PATH environmental variable')
            raise SystemExit
        if not get_augustus_script('new_species.pl'):
            _logger.error('Impossible to locate the required script new_species.pl. Check that you declared '
                          'the Augustus scripts folder in your $PATH environmental variable')
            raise SystemExit
        if self._long and not get_augustus_script('optimize_augustus.pl'):
            _logger.error('Impossible to locate the required script optimize_augustus.pl. Check that you declared '
                          'the Augustus scripts folder in your $PATH environmental variable')
            raise SystemExit

    def _check_nucleotide(self):
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
                        _logger.error('Please provide a nucleotide file as input, it should not contains \'%s or %s\''
                                      % (aa.upper(), aa.lower()))
                        nucl_file.close()
                        raise SystemExit
        nucl_file.close()

    def _check_protein(self):
        """
        This function checks that the provided file is protein
        :raises SystemExit: if only ACGTN is found over a reasonable amount of lines
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
            _logger.error('Please provide a protein file as input')
            raise SystemExit

    def _define_checkpoint(self, nb=None):
        """
        This function update the checkpoint file with the provided id or delete it if none is provided
        :param nb: the id of the checkpoint
        :type nb: int
        """
        if nb:
            open('%scheckpoint.tmp' % self.mainout, 'w').write('%s.%s.%s' % (nb, self._mode, self._random))
        else:
            if os.path.exists('%scheckpoint.tmp' % self.mainout):
                os.remove('%scheckpoint.tmp' % self.mainout)

    def _get_checkpoint(self, reset_random_suffix=False):
        """
        This function return the checkpoint if the checkpoint.tmp file exits or None if absent
        :param reset_random_suffix: to tell whether to reset the self._random value with the one found the checkpoint
        :type reset_random_suffix: bool
        :return: the checkpoint name
        :rtype: int
        """
        if os.path.exists('%scheckpoint.tmp' % self.mainout):
            line = open('%scheckpoint.tmp' % self.mainout, 'r').readline()
            if reset_random_suffix:
                _logger.debug('Resetting random suffix to %s' % self._random)
                self._random = line.split('.')[-1]
            return int(int(line.split('.')[0]))
        else:
            return None

    def _extract_missing_and_frag_buscos_ancestral(self, ancestral_variants=False):
        """
        This function extracts from the file ancestral the sequences that match missing or fragmented buscos
        :param ancestral_variants: tell whether to use the ancestral_variants file
        :type ancestral_variants: bool
        """
        if self._has_variants_file:
            _logger.info('Extracting missing and fragmented buscos from the ancestral_variants file...')
        else:
            _logger.info('Extracting missing and fragmented buscos from the ancestral file...')

        if ancestral_variants:
            ancestral = open('%sancestral_variants' % self._clade_path, 'r')
            output = open('%sblast_output/missing_and_frag_ancestral_variants' % self.mainout, 'w')
        else:
            ancestral = open('%sancestral' % self._clade_path, 'r')
            output = open('%sblast_output/missing_and_frag_ancestral' % self.mainout, 'w')

        result = ''

        buscos_to_retrieve = self._missing_busco_list + self._fragmented_busco_list
        buscos_retrieved = []
        add = False
        for line in ancestral:
            if line.startswith('>'):
                if ancestral_variants:
                    line = '_'.join(line.split("_")[:-1])
                    # This pattern can support name like EOG00_1234_1
                busco_id = line.strip().strip('>')
                if busco_id in buscos_to_retrieve:
                    _logger.debug('Found contig %s' % busco_id)
                    add = True
                    buscos_retrieved.append(busco_id)
                else:
                    add = False
            if add:
                if line.endswith('\n'):
                    result += line
                else:
                    result += line + '\n'
        if len(list(set(buscos_to_retrieve) - set(buscos_retrieved))) > 0:
            if self._has_variants_file:
                _logger.warning('The busco id(s) %s were not found in the ancestral_variants file' %
                                list(set(buscos_to_retrieve) - set(buscos_retrieved)))
            else:
                _logger.warning('The busco id(s) %s were not found in the ancestral file' %
                                list(set(buscos_to_retrieve) - set(buscos_retrieved)))

        output.write(result)
        output.close()
        ancestral.close()
        output.close()

    @staticmethod
    def _check_overlap(a, b):
        """
        This function checks whether two regions overlap
        :param a: first region, start and end
        :type a: list
        :param b: second region, start and end
        :type b: list
        :return: the number of overlapping positions
        :rtype: int
        """
        return max(0, min(a[1], b[1]) - max(a[0], b[0]))

    @staticmethod
    def _define_boundary(a, b):
        """
        This function defines the boundary of two overlapping regions
        :param a: first region, start and end
        :type a: list
        :param b: second region, start and end
        :type b: list
        :return: the boundaries, i.e. start and end
        :rtype: collections.deque
        """
        temp_start = a[0]
        temp_end = a[1]
        current_start = b[0]
        current_end = b[1]
        boundary = None
        if temp_start < current_start and temp_end < current_start:
            # i.e. entry is fully before
            # append left, IF entry is the first one; otherwise put into the proper position
            boundary = deque([a, b])
        elif temp_start < current_start <= temp_end <= current_end:
            # i.e. overlap starts before, but ends inside
            boundary = deque([temp_start, current_end])
        elif current_start <= temp_start <= current_end < temp_end:
            # overlap starts inside, but ends outside
            boundary = deque([current_start, temp_end])
        elif temp_start > current_end and temp_end > current_end:
            # i.e. query is fully after
            # append right; otherwise put into the proper position
            boundary = deque([b, a])
        elif current_start <= temp_start <= temp_end <= current_end:
            # i.e. query is fully inside, no further operations needed
            boundary = deque(b)
        elif temp_start == current_start and temp_end == current_end:
            boundary = deque(a)
        elif temp_start <= current_start and temp_end >= current_end:
            # i.e. query is longer and contains all coordinates
            # replace by the query
            boundary = deque(a)
        return boundary

    @staticmethod
    def _gargantua(deck):
        """
        :param deck:
        :type deck: list
        :return:
        :rtype: int
        """
        # todo comment
        total = 0
        for entry in deck:
            total += entry[1] - entry[0]
        return total

    @staticmethod
    def _sixpack(seq):
        """
        Gets the sixframe translation for the provided sequence
        :param seq: the sequence to be translated
        :type seq: str
        :return: the six translated sequences
        :rtype: list
        """
        s1 = seq
        s2 = seq[1:]
        s3 = seq[2:]
        rev = ''
        for letter in seq[::-1]:
            try:
                rev += Analysis.COMP[letter]
            except KeyError:
                rev += Analysis.COMP['N']
        r1 = rev
        r2 = rev[1:]
        r3 = rev[2:]
        transc = []
        frames = [s1, s2, s3, r1, r3, r2]
        for sequence in frames:
            part = ''
            new = ''
            for letter in sequence:
                if len(part) == 3:
                    try:
                        new += Analysis.CODONS[part]
                    except KeyError:
                        new += 'X'
                    part = ''
                    part += letter
                else:
                    part += letter
            if len(part) == 3:
                try:
                    new += Analysis.CODONS[part]
                except KeyError:
                    new += 'X'
            transc.append(new)
        return transc

    @staticmethod
    def _measuring(nested):
        """
        :param nested:
        :type nested:
        :return:
        :rtype:
        """
        # todo comment
        total_len = 0

        if isinstance(nested, str):
            return '0'
        scaffolds = list(nested.keys())
        if len(nested) == 1:
            total_len = [0]
            for hit in nested[scaffolds[0]]:
                total_len[0] += hit[1] - hit[0]
        elif len(nested) > 1:
            total_len = [0] * len(nested)
            for entry in range(0, len(scaffolds)):
                for hit in nested[scaffolds[entry]]:
                    total_len[entry] += hit[1] - hit[0]
        return total_len

    @staticmethod
    def _remove_bad_ratio_genes(original):
        """
        This function removes duplicate positive results if the score is above a 0.85 threshold compared to the top
        scoring match
        :param original: a dict with BUSCO as key, and a dict of matching genes as values
        :type original: dict
        :return: the filtered dict
        :rtype: dict
        """
        ratio = 0.85
        filtered = copy.deepcopy(original)
        for k in original.keys():
            max_score = 0
            for k2 in original[k]:
                if original[k][k2][2] > max_score:
                    max_score = original[k][k2][2]
            for k2 in original[k]:
                if original[k][k2][2] < max_score * ratio:
                    del filtered[k][k2]
                    if len(filtered[k]) == 0:
                        del filtered[k]

        return filtered

    @staticmethod
    def _filter_multi_match_genes(original):
        """
        This function identifies genes that match the same BUSCO, and keeps the one with the best score only
        :param original: a dict with BUSCO as key, and a dict of matching genes as values
        :type original: dict
        :return: the filtered dict
        :rtype: dict
        """
        filtered = copy.deepcopy(original)

        # reorganize the key/value by gene
        gene_dict = {}
        for d in original.values():
            for key in d.keys():
                if key in gene_dict:
                    gene_dict[key].append(d[key])
                else:
                    gene_dict[key] = [d[key]]

        # identify genes belonging to two or more buscos
        # keep the best score
        to_keep = {}
        for key in gene_dict.keys():
            if len(gene_dict[key]) > 1:
                max_score = 0
                busco_to_keep = ""
                for busco in gene_dict[key]:
                    if max_score < busco[2]:
                        max_score = busco[2]
                        busco_to_keep = busco[5][0:11]
                to_keep[key] = busco_to_keep

        # clean the original list
        for k in original.keys():
            for k2 in original[k].keys():
                if k2 in to_keep.keys():
                    if to_keep[k2] != k:
                        del filtered[k][k2]
                        if len(filtered[k]) == 0:
                            del filtered[k]

        return filtered

    @abstractmethod
    def __init__(self, params):
        """
        Initialize an instance, need to be overriden by subclasses
        :param params: Values of all parameters that have to be defined
        :type params: dict
        """
        # todo move child specific variable to its child class, e.g. self._transcriptome_by_scaff
        self._random = "_" + str(random.getrandbits(32))  # to have a unique value for temporary file names
        self._abrev = params['abrev']
        self._tmp = params['tmp']
        self._force = params['force']
        self._restart = params['restart']
        self._sequences = params['sequences']
        self._cpus = params['cpus']
        self._clade_path = params['clade_path']
        self._clade_name = params['clade_name']
        self._domain = params['domain']
        self._ev_cutoff = params['ev_cutoff']
        self._region_limit = params['region_limit']
        self._flank = params['flank']
        self._long = params['long']
        self._target_species = params['target_species']
        self._augustus_parameters = params['augustus_parameters']
        self._augustus_config_path = params['augustus_config_path']
        self._tarzip = params['tarzip']
        self._dataset_creation_date = params['dataset_creation_date']
        self._dataset_nb_species = params['dataset_nb_species']
        self._dataset_nb_buscos = params['dataset_nb_buscos']
        self.mainout = None
        self._totalbuscos = 0
        self._total = 0
        self._cutoff_dictionary = {}
        self._thread_list = None
        self._no_prediction = None
        self._exit_flag = None
        self._queue_lock = None
        self._work_queue = None
        self._missing_busco_list = []
        self._fragmented_busco_list = []
        self._location_dic = {}
        self._single_copy_files = None
        self._transcriptome_by_scaff = {}
        self._mode = None
        self._has_variants_file = False
        self._contig_length = {}

    def cleanup(self):
        """
        This function cleans temporary files. \
        It has to be overriden by subclasses when needed
        """
        Analysis.p_open(['rm', '%stemp_%s%s' % (self._tmp, self._abrev, self._random)], 'bash', shell=False)
        if self._tarzip:
            self._run_tarzip()

    def _run_tarzip(self):
        """
        This function tarzips results folder
        It has to be overriden by subclasses when needed
        """
        Analysis.p_open(['tar', '-C', '%s' % self.mainout, '-zcf',
                         '%shmmer_output.tar.gz' % self.mainout, 'hmmer_output',
                         '--remove-files'],
                        'bash', shell=False)

    def _get_coordinates(self):
        """
        This function gets coordinates for candidate regions from tblastn result file. \
        It has to be overriden by subclasses when needed
        """
        pass

    def _extract_scaffolds(self, missing_and_frag_only=False):
        """
        This function extract the scaffold having blast results
        :param missing_and_frag_only: to tell which coordinate file to look for, complete or just missing and fragment
        :type missing_and_frag_only: bool
        """
        _logger.info('Pre-Augustus scaffold extraction...')
        if missing_and_frag_only:
            coord = open('%s/blast_output/coordinates_%s_missing_and_frag_rerun.tsv' % (self.mainout, self._abrev))
        else:
            coord = open('%s/blast_output/coordinates_%s.tsv' % (self.mainout, self._abrev))
        dic = {}
        scaff_list = []
        for i in coord:
            i = i.strip().split()
            if len(i) != 2:
                dic[i[0]] = [i[1], i[2], i[3]]
                if i[1] not in scaff_list:
                    scaff_list.append(i[1])
        coord.close()
        f = open(self._sequences)
        check = 0
        out = None
        contig_len = 0
        contig_id = None
        for i in f:
            if i.startswith('>'):
                i = i.split()
                i = i[0][1:]
                if i in scaff_list:
                    out = open('%s%s%s%s_.temp' % (self._tmp, i, self._abrev, self._random), 'w')
                    out.write('>%s\n' % i)
                    check = 1
                    contig_id = i
                else:
                    check = 0
            elif check == 1:
                out.write(i)
                if i != '\n':
                    contig_len += len(i)
        f.close()
        if out:
            out.close()
            # Keep track of the contig max lenght
            #  Not used yet but might be useful
            self._contig_length.update({contig_id: contig_len})

    @abstractmethod
    def _hmmer(self):
        """
        This function runs hmmsearch. It has to be overriden by subclasses
        """
        pass

    def _write_output_header(self, out):
        """
        This function adds a header to the provided file
        :param out: a file to which the header will be added
        :type out: file
        """
        out.write('# BUSCO version is: %s \n# The lineage dataset is: %s (Creation date: %s,'
                  ' number of species: %s, number of BUSCOs: %s)\n'
                  % (VERSION, self._clade_name, self._dataset_creation_date, self._dataset_nb_species,
                     self._dataset_nb_buscos))
        out.write('# To reproduce this run: %s\n#\n' % _rerun_cmd)

    @staticmethod
    def _write_full_table_header(out):
        """
        This function adds a header line to the full table file
        :param out: a full table file
        :type out: file
        """
        out.write('# Busco id\tStatus\tSequence\tScore\tLength\n')

    def _blast(self, missing_and_frag_only=False, ancestral_variants=False):
        """
        This function runs tblastn
        :param missing_and_frag_only: to tell whether to blast only missing and fragmented buscos
        :type missing_and_frag_only: bool
        :param ancestral_variants: to tell whether to use the ancestral_variants file
        :type ancestral_variants: bool
        """
        if ancestral_variants:
            ancestral_suffix = '_variants'
        else:
            ancestral_suffix = ''

        if missing_and_frag_only:
            self._extract_missing_and_frag_buscos_ancestral(ancestral_variants)
            output_suffix = '_missing_and_frag_rerun'
            query_file = '%sblast_output/missing_and_frag_ancestral%s' % (self.mainout, ancestral_suffix)
        else:
            output_suffix = ''
            query_file = '%sancestral%s' % (self._clade_path, ancestral_suffix)

        if not missing_and_frag_only:
            _logger.info('Create blast database...')
            Analysis.p_open([get_blast_fpath('makeblastdb'), '-in', self._sequences, '-dbtype', 'nucl', '-out',
                             '%s%s%s' % (self._tmp, self._abrev, self._random)], 'makeblastdb',
                            shell=False)
            if not os.path.exists('%sblast_output' % self.mainout):
                Analysis.p_open(['mkdir', '%sblast_output' % self.mainout], 'bash', shell=False)
        _logger.info('Running tblastn, writing output to %sblast_output/tblastn_%s%s.tsv...'
                     % (self.mainout, self._abrev, output_suffix))

        Analysis.p_open([get_blast_fpath('tblastn'), '-evalue', str(self._ev_cutoff), '-num_threads', '1',
                         '-query', query_file,
                         '-db', '%s%s%s' % (self._tmp, self._abrev, self._random),
                         '-out', '%sblast_output/tblastn_%s%s.tsv'
                         % (self.mainout, self._abrev, output_suffix), '-outfmt', '7'], 'tblastn',
                        shell=False)
        # check that blast worked
        if not os.path.exists('%sblast_output/tblastn_%s%s.tsv' % (self.mainout, self._abrev, output_suffix)):
            _logger.error('tblastn failed !')
            raise SystemExit
        # check that the file is not truncated
        try:
            if "processed" not in open('%sblast_output/tblastn_%s%s.tsv' % (self.mainout, self._abrev, output_suffix),
                                       'r').readlines()[-1]:
                _logger.error('tblastn has ended prematurely (the result file lacks the expected final line), '
                              'which will produce incomplete results in the next steps ! '
                              'This problem likely appeared in blast+ 2.4 and seems not fully fixed in 2.6. '
                              'It happens only when using multiple cores. You can use a single core (-c 1) or '
                              'downgrade to blast+ 2.2.x, a safe choice regarding this issue. '
                              'See blast+ documentation for more information.')
                raise SystemExit
        except IndexError:
            pass  # if the tblastn result file is empty, for example in phase 2 if 100% was found in phase 1

    def _run_threads(self, command_strings, thread_class, display_percents=True):
        """
        This class creates and run threads of the provided type for each provided command
        :param command_strings: the list of commands to be run in the threads
        :type command_strings: list
        :param thread_class: the type of thread class to create
        :type thread_class: type Analysis.threads
        :param display_percents: to tell whether to display a log for 0% and 100% complete
        :type display_percents: bool
        """
        self.slate = [100, 90, 80, 70, 60, 50, 40, 30, 20, 10]
        if len(command_strings) < 11:
            self.slate = [100, 50]  # to avoid false progress display if not enough entries
        self._exit_flag = 0
        # Create X number of threads
        self._thread_list = []
        self._total = len(command_strings)
        mark = 0
        for i in range(int(self._cpus)):
            mark += 1
            self._thread_list.append("%s-%s-%s" % (thread_class.CLASS_ABREV, self._abrev, str(i + 1)))
            if mark >= self._total:
                break
        self._queue_lock = threading.Lock()
        self._work_queue = Queue.Queue(len(command_strings))
        threads = []
        thread_id = 1

        # Generate the new threads
        for t_name in self._thread_list:
            thread = thread_class(thread_id, t_name, self)
            thread.start()
            threads.append(thread)
            thread_id += 1

        if display_percents:
            _logger.info('%s =>\t0%% of predictions performed (%i to be done)'
                         % (time.strftime("%m/%d/%Y %H:%M:%S"), self._total))

        # Fill the queue with the commands
        self._queue_lock.acquire()
        for word in command_strings:
            self._work_queue.put(word)
        self._queue_lock.release()

        # Wait for all jobs to finish (i.e. queue being empty)
        while not self._work_queue.empty():
            time.sleep(0.01)
            pass
        # Send exit signal
        self._exit_flag = 1

        # Wait for all threads to finish
        for t in threads:
            t.join()

        if display_percents:
            _logger.info('%s =>\t100%% of predictions performed' % time.strftime("%m/%d/%Y %H:%M:%S"))

    def _augustus(self):
        """
        This function runs Augustus
        """
        # Run Augustus on all candidate regions
        # 1- Get the temporary sequence files (no multi-fasta support in Augustus)
        # 2- Build a list with the running commands (for threading)
        # 3- Launch Augustus in paralell using Threading
        # 4- Prepare the sequence files to be analysed with HMMer 3.1

        # Extract candidate contigs/scaffolds from genome assembly
        # Augustus can't handle multi-fasta files, each sequence has to be present in its own file
        # Write the temporary sequence files

        # First, delete the augustus result folder, in case we are in the restart mode at this point
        Analysis.p_open(['rm -rf %saugustus_output/*' % self.mainout], 'bash', shell=True)

        # get all scaffolds with blast results
        self._extract_scaffolds()

        # Now run Augustus on each candidate region with its respective Block-profile

        _logger.info('Running Augustus prediction using %s as species:' % self._target_species)

        if self._augustus_parameters:
            _logger.info('Additional parameters for Augustus are %s: ' % self._augustus_parameters)

        augustus_log = '%saugustus_output/augustus.log' % self.mainout
        _logger.info('[augustus] Please find all logs related to Augustus here: %s' % augustus_log)
        if not os.path.exists('%saugustus_output/predicted_genes' % self.mainout):
            Analysis.p_open(['mkdir', '-p', '%saugustus_output/predicted_genes' % self.mainout], 'bash', shell=False)

        # coordinates of hits by BUSCO
        self._location_dic = {}
        f = open('%s/blast_output/coordinates_%s.tsv' % (self.mainout, self._abrev))
        for line in f:
            line = line.strip().split('\t')
            scaff_id = line[1]
            scaff_start = line[2]
            scaff_end = line[3]
            group_name = line[0]

            if group_name not in self._location_dic:
                self._location_dic[group_name] = [[scaff_id, scaff_start, scaff_end]]  # scaffold,start and end
            elif group_name in self._location_dic:
                self._location_dic[group_name].append([scaff_id, scaff_start, scaff_end])
        f.close()
        # Make a list containing the commands to be executed in parallel with threading.
        augustus_first_run_strings = []

        for entry in self._location_dic:

            for location in self._location_dic[entry]:
                scaff = location[0] + self._abrev + str(self._random) + '_.temp'
                scaff_start = location[1]
                scaff_end = location[2]
                output_index = self._location_dic[entry].index(location) + 1

                out_name = '%saugustus_output/predicted_genes/%s.out.%s' % (self.mainout, entry, output_index)

                augustus_call = augustus_cmd + ' --codingseq=1 --proteinprofile=%(clade)sprfl/%(busco_group)s.prfl ' \
                                '--predictionStart=%(start_coord)s --predictionEnd=%(end_coord)s ' \
                                '--species=%(species)s %(augustus_parameters)s \'%(tmp)s%(scaffold)s\' > %(output)s ' \
                                '2>> %(augustus_log)s' % \
                                {'clade': self._clade_path, 'species': self._target_species,
                                 'augustus_parameters': self._augustus_parameters,
                                 'busco_group': entry, 'start_coord': scaff_start, 'end_coord': scaff_end,
                                 'tmp': self._tmp, 'scaffold': scaff, 'output': out_name, 'augustus_log': augustus_log}
                augustus_first_run_strings.append(augustus_call)  # list of call strings

        self._run_threads(augustus_first_run_strings, self._AugustusThreads)

        # Preparation of sequences for use with HMMer

        # Parse Augustus output files ('run_XXXX/augustus') and extract protein sequences
        # to a FASTA file ('run_XXXX/augustus_output/extracted_proteins').
        _logger.info('Extracting predicted proteins...')
        files = os.listdir('%saugustus_output/predicted_genes' % self.mainout)
        files.sort()
        for entry in files:
            Analysis.p_open(['sed -i.bak \'1,3d\' %saugustus_output/predicted_genes/%s;'
                             'rm %saugustus_output/predicted_genes/%s.bak'
                             % (self.mainout, entry, self.mainout, entry)], 'bash', shell=True)
        if not os.path.exists(self.mainout + 'augustus_output/extracted_proteins'):
            Analysis.p_open(['mkdir', '%saugustus_output/extracted_proteins' % self.mainout], 'bash', shell=False)

        self._no_predictions = []
        for entry in files:
            self._extract(self.mainout, entry)
            self._extract(self.mainout, entry, aa=False)

        Analysis.p_open(['find %saugustus_output/extracted_proteins -size 0 -delete' % self.mainout], 'bash',
                        shell=True)

    def _augustus_rerun(self):
        """
        This function runs Augustus and hmmersearch for the second time
        """
        # todo: augustus and augustus rerun should use the same code when possible, and hmmer should not be here

        augustus_log = '%saugustus_output/augustus.log' % self.mainout

        if not os.path.exists('%saugustus_output/gffs' % self.mainout):
            Analysis.p_open(['mkdir', '%saugustus_output/gffs' % self.mainout], 'bash', shell=False)
        if not os.path.exists('%saugustus_output/gb' % self.mainout):
            Analysis.p_open(['mkdir', '%saugustus_output/gb' % self.mainout], 'bash', shell=False)

        _logger.info('Training Augustus using Single-Copy Complete BUSCOs:')
        _logger.info('%s =>\tConverting predicted genes to short genbank files...'
                     % time.strftime("%m/%d/%Y %H:%M:%S"))

        gff2gbsmalldna_strings = []

        for entry in self._single_copy_files:
            check = 0
            file_name = self._single_copy_files[entry].split('-')[-1]
            target_seq_name = self._single_copy_files[entry].split('[')[0]
            group_name = file_name.split('.')[0]

            # create GFFs with only the "single-copy" BUSCO sequences
            pred_file = open('%saugustus_output/predicted_genes/%s' % (self.mainout, file_name))
            gff_file = open('%saugustus_output/gffs/%s.gff' % (self.mainout, group_name), 'w')
            for line in pred_file:
                if line.startswith('# start gene'):
                    pred_name = line.strip().split()[-1]
                    if pred_name == target_seq_name:
                        check = 1
                elif line.startswith('#'):
                    check = 0
                elif check == 1:
                    gff_file.write(line)
            pred_file.close()
            gff_file.close()

            gff2gbsmalldna_call = (get_augustus_script('gff2gbSmallDNA.pl') + ' %saugustus_output/gffs/%s.gff %s \
                1000 %saugustus_output/gb/%s.raw.gb 1>>%s 2>&1' %
                                   (self.mainout, entry, self._sequences, self.mainout, entry, augustus_log))

            gff2gbsmalldna_strings.append(gff2gbsmalldna_call)  # list of call strings

        self._run_threads(gff2gbsmalldna_strings, self._Gff2gbSmallDNAThreads, display_percents=False)
        _logger.info('%s =>\tAll files converted to short genbank files, now running the training scripts...'
                     % time.strftime("%m/%d/%Y %H:%M:%S"))

        # bacteria clade needs to be flagged as "prokaryotic"
        if self._domain == 'prokaryota':
            Analysis.p_open([get_augustus_script('new_species.pl') + ' --prokaryotic --species=BUSCO_%s%s '
                             '--AUGUSTUS_CONFIG_PATH=%s 1>>%s 2>&1' %
                             (self._abrev, self._random, self._augustus_config_path, augustus_log)], 'new_species.pl',
                            shell=True)  # create new species config file from template
        else:
            Analysis.p_open([get_augustus_script('new_species.pl') + ' --species=BUSCO_%s%s --AUGUSTUS_CONFIG_PATH=%s'
                             ' 1>>%s 2>&1' %
                             (self._abrev, self._random, self._augustus_config_path, augustus_log)], 'new_species.pl', shell=True)

        Analysis.p_open(
            ['find %saugustus_output/gb/ -type f -name "*.gb" -exec cat {} \; > %saugustus_output/training_set_%s.txt'
             % (self.mainout, self.mainout, self._abrev)],
            'bash',
            shell=True)

        # train on new training set (complete single copy buscos)
        Analysis.p_open([etraining_cmd + ' --species=BUSCO_%s%s %saugustus_output/training_set_%s.txt 1>>%s 2>&1' %
                         (self._abrev, self._random, self.mainout, self._abrev, augustus_log)], 'augustus etraining',
                        shell=True)
        # long mode (--long) option - runs all the Augustus optimization scripts (adds ~1 day of runtime)
        if self._long:
            _logger.warning('%s => Optimizing augustus metaparameters, this may take a very long time...'
                            % time.strftime("%m/%d/%Y %H:%M:%S"))
            Analysis.p_open([get_augustus_script('optimize_augustus.pl') + ' --cpus=%s --species=BUSCO_%s%s %saugustus_output/training_set_%s.txt \
                1>>%s 2>&1' %
                             (self._cpus, self._abrev, self._random, self.mainout, self._abrev,
                              augustus_log)], 'optimize_augustus.pl',
                            shell=True)
            Analysis.p_open([etraining_cmd + ' --species=BUSCO_%s%s %saugustus_output/training_set_%s.txt 1>>%s 2>&1' %
                             (self._abrev, self._random, self.mainout, self._abrev, augustus_log)],
                            'augustus etraining',
                            shell=True)

        #######################################################

        # get all scaffolds with blast results
        self._extract_scaffolds(missing_and_frag_only=True)

        _logger.info('Re-running Augustus with the new metaparameters, '
                     'number of target BUSCOs: %s' % len(self._missing_busco_list + self._fragmented_busco_list))

        augustus_rerun_strings = []
        augustus_rerun_seds = []
        hmmer_rerun_strings = []

        # Move first run and create a folder for the rerun
        Analysis.p_open(['mv', '%saugustus_output/predicted_genes'
                         % self.mainout, '%saugustus_output/predicted_genes_run1'
                         % self.mainout], 'bash', shell=False)
        Analysis.p_open(['mkdir', '%saugustus_output/predicted_genes' % self.mainout], 'bash', shell=False)

        Analysis.p_open(['mv', '%shmmer_output/' % self.mainout, '%shmmer_output_run1/' % self.mainout], 'bash',
                        shell=False)
        Analysis.p_open(['mkdir', '%shmmer_output/' % self.mainout], 'bash', shell=False)

        # Update the location dict with the new blast search
        # coordinates of hits by BUSCO
        self._location_dic = {}
        f = open('%s/blast_output/coordinates_%s_missing_and_frag_rerun.tsv' % (self.mainout, self._abrev))
        for line in f:
            line = line.strip().split('\t')
            scaff_id = line[1]
            scaff_start = line[2]
            scaff_end = line[3]
            group_name = line[0]

            if group_name not in self._location_dic:
                self._location_dic[group_name] = [[scaff_id, scaff_start, scaff_end]]  # scaffold,start and end
            elif group_name in self._location_dic:
                self._location_dic[group_name].append([scaff_id, scaff_start, scaff_end])
        f.close()

        for entry in self._missing_busco_list + self._fragmented_busco_list:
            if entry in self._location_dic:
                for location in self._location_dic[entry]:
                    scaff = location[0] + self._abrev + str(self._random) + '_.temp'
                    scaff_start = location[1]
                    scaff_end = location[2]
                    output_index = self._location_dic[entry].index(location) + 1

                    out_name = '%saugustus_output/predicted_genes/%s.out.%s' % (self.mainout, entry, output_index)

                    augustus_call = augustus_cmd + ' --codingseq=1 --proteinprofile=%(clade)sprfl/%(busco_group)s.prfl \
                        --predictionStart=%(start_coord)s --predictionEnd=%(end_coord)s --species=BUSCO_%(species)s \
                        \'%(tmp)s%(scaffold)s\'  %(augustus_parameters)s > %(output)s 2>> %(augustus_log)s' % \
                                    {'clade': self._clade_path, 'species': self._abrev + str(self._random),
                                     'busco_group': entry, 'augustus_parameters': self._augustus_parameters,
                                     'start_coord': scaff_start, 'augustus_log': augustus_log, 'tmp': self._tmp,
                                     'end_coord': scaff_end, 'scaffold': scaff, 'output': out_name}
                    augustus_rerun_strings.append(augustus_call)  # list of call strings

                    sed_call = 'sed -i.bak \'1,3d\' %s;rm %s.bak' % (out_name, out_name)
                    augustus_rerun_seds.append(sed_call)

                    out_name = '%shmmer_output/%s.out.%s' % (self.mainout, entry, output_index)
                    augustus_fasta = '%saugustus_output/extracted_proteins/%s.faa.%s' \
                                     % (self.mainout, entry, output_index)

                    hmmer_call = [hmmer_cmd,
                                  '--domtblout', '%s' % out_name,
                                  '-o', '%stemp_%s%s' % (self._tmp, self._abrev, self._random),
                                  '--cpu', '1',
                                  '%shmms/%s.hmm' % (self._clade_path, entry),
                                  '%s' % augustus_fasta]

                    hmmer_rerun_strings.append(hmmer_call)

            else:
                pass

        self._run_threads(augustus_rerun_strings, self._AugustusThreads)

        for sed_string in augustus_rerun_seds:
            Analysis.p_open(['%s' % sed_string], 'bash', shell=True)
        # Extract fasta files from augustus output
        _logger.info('Extracting predicted proteins...')
        self._no_predictions = []
        for entry in self._missing_busco_list + self._fragmented_busco_list:
            if entry in self._location_dic:
                for location in self._location_dic[entry]:
                    output_index = self._location_dic[entry].index(location) + 1
                    # when extract gets reworked to not need MAINOUT, change to OUT_NAME
                    plain_name = '%s.out.%s' % (entry, output_index)
                    self._extract(self.mainout, plain_name)
                    self._extract(self.mainout, plain_name, aa=False)
            else:
                pass

        # Run hmmer
        hmmer_rerun_strings_filtered = []

        # filter out the line that have to augustus prediction
        for word in hmmer_rerun_strings:
            target_seq = word[-1].split('/')[-1]
            if target_seq not in self._no_predictions:
                hmmer_rerun_strings_filtered.append(word)

        _logger.info('****** Step 3/3, current time: %s ******' % time.strftime("%m/%d/%Y %H:%M:%S"))
        _logger.info('Running HMMER to confirm orthology of predicted proteins:')

        self._run_threads(hmmer_rerun_strings_filtered, self._HmmerThreads)

        # Fuse the run1 and rerun folders
        Analysis.p_open(['mv %saugustus_output/predicted_genes/*.* %saugustus_output/predicted_genes_run1/ 2> /dev/null'
                         % (self.mainout, self.mainout)], 'bash', shell=True)
        Analysis.p_open(['mv %shmmer_output/*.* %shmmer_output_run1/ 2> /dev/null'
                         % (self.mainout, self.mainout)], 'bash', shell=True)
        Analysis.p_open(['rm', '-r', '%saugustus_output/predicted_genes' % self.mainout], 'bash', shell=False)
        Analysis.p_open(['rm', '-r', '%shmmer_output' % self.mainout], 'bash', shell=False)
        Analysis.p_open(['mv', '%saugustus_output/predicted_genes_run1' % self.mainout,
                         '%saugustus_output/predicted_genes' % self.mainout], 'bash', shell=False)
        Analysis.p_open(['mv', '%shmmer_output_run1' % self.mainout, '%shmmer_output' % self.mainout], 'bash',
                        shell=False)

        # Compute the final results
        self._produce_short_summary()

        if len(self._missing_busco_list) == self._totalbuscos:
            _log.add_blank_line()
            _logger.warning('BUSCO did not find any match. Do not forget to check the file %s '
                            'to exclude a problem regarding Augustus'
                            % augustus_log)
        # get single-copy files as fasta
        if not os.path.exists('%ssingle_copy_busco_sequences' % self.mainout):
            Analysis.p_open(['mkdir', '%ssingle_copy_busco_sequences' % self.mainout], 'bash', shell=False)

        _logger.debug('Getting single-copy files...')
        for entry in self._single_copy_files:
            check = 0

            file_name = self._single_copy_files[entry].split('-')[-1].replace('out', 'faa')
            file_name_nucl = self._single_copy_files[entry].split('-')[-1].replace('out', 'fna')
            target_seq_name = self._single_copy_files[entry].split('[')[0]
            group_name = file_name.split('.')[0]
            seq_coord_start = self._single_copy_files[entry].split(']-')[0].split('[')[1]

            pred_fasta_file = open('%saugustus_output/extracted_proteins/%s' % (self.mainout, file_name))
            single_copy_outfile = open('%ssingle_copy_busco_sequences/%s.faa' % (self.mainout, group_name), 'w')

            pred_fasta_file_nucl = open('%saugustus_output/extracted_proteins/%s' % (self.mainout, file_name_nucl))
            single_copy_outfile_nucl = open('%ssingle_copy_busco_sequences/%s.fna' % (self.mainout, group_name), 'w')

            for line in pred_fasta_file:
                if line.startswith('>%s' % target_seq_name):
                    single_copy_outfile.write('>%s:%s:%s\n' % (group_name, self._sequences, seq_coord_start))
                    check = 1
                elif line.startswith('>'):
                    check = 0
                elif check == 1:
                    single_copy_outfile.write(line)

            for line in pred_fasta_file_nucl:
                if line.startswith('>%s' % target_seq_name):
                    single_copy_outfile_nucl.write('>%s:%s:%s\n' % (group_name, self._sequences, seq_coord_start))
                    check = 1
                elif line.startswith('>'):
                    check = 0
                elif check == 1:
                    single_copy_outfile_nucl.write(line)

            pred_fasta_file.close()
            single_copy_outfile.close()
            pred_fasta_file_nucl.close()
            single_copy_outfile_nucl.close()

    def _extract(self, path, group, aa=True):
        """
        This function extracts fasta files from augustus output
        :param path: the path to the BUSCO run folder
        :type path: str
        :param group: the BUSCO group id
        :type group: str
        :param aa: to tell whether to extract amino acid instead of nucleotide sequence
        :type aa: bool
        """
        ext = 'fna'
        start_str = '# coding sequence'
        end_str = '# protein'
        if aa:
            ext = 'faa'
            start_str = '# protein'
            end_str = '# end'

        count = 0
        group_name = group.split('.')[0]
        try:
            group_index = int(group.split('.')[-1])
        except IndexError:
            group_index = '1'
            group += '.out.1'
        group_index = str(group_index)

        f = open('%saugustus_output/predicted_genes/%s' % (path, group))
        written_check = 0
        check = 0
        while True:
            line = f.readline()
            if not line:
                break
            if line.startswith('# start gene'):
                line = f.readline()
                line = line.split()
                places = [line[0], line[3], line[4]]
            elif line.startswith(start_str):
                line = line.strip().split('[')
                count += 1
                if written_check == 0:
                    out = open('%saugustus_output/extracted_proteins/%s.%s.%s'
                               % (path, group_name, ext, group_index), 'w')
                    written_check = 1
                out.write('>g%s[%s:%s-%s]\n' % (count, places[0], places[1], places[2]))
                if line[1][-1] == ']':
                    line[1] = line[1][:-1]
                out.write(line[1])
                check = 1
            else:
                if line.startswith('# sequence of block'):
                    check = 0
                elif line.startswith(end_str):
                    check = 0
                    out.write('\n')
                elif check == 1:
                    line = line.split()[1]
                    if line[-1] == ']':
                        line = line[:-1]
                    out.write(line)
        f.close()
        if written_check == 1:
            out.close()
        else:
            self._no_predictions.append('%s.faa.%s' % (group_name, group_index))

    def _parse_hmmer(self, hmmer_results_files):
        """
        This function parses the hmmsearch output files and produces the full_table output file
        :param hmmer_results_files: the list of all output files
        :type hmmer_results_files: list
        """
        # todo: replace self._mode ==... by a proper parent-child behavior that overrides code when needed

        env = []
        everything = {}  # all info from hit_dic + lengths

        for file_name in hmmer_results_files:

            f = open('%shmmer_output/%s' % (self.mainout, file_name))

            hit_dic = {}
            bit_score_list = []
            busco_query = None

            for line in f:
                if line.startswith('#'):
                    pass
                else:
                    line = line.strip().split()

                    if self._mode == 'genome':
                        prot_id = line[0] + '-' + file_name
                    else:
                        prot_id = line[0]
                    busco_query = line[3]

                    bit_score = float(line[7])
                    bit_score_list.append(bit_score)
                    hmm_start = int(line[15])
                    hmm_end = int(line[16])

                    # new protein that passes score cutoff
                    if bit_score >= self._cutoff_dictionary[busco_query]['score']:
                        if prot_id not in hit_dic.keys():
                            hit_dic[prot_id] = [[hmm_start, hmm_end, bit_score]]
                        else:
                            hit_dic[prot_id].append([hmm_start, hmm_end, bit_score])
            f.close()

            length = Analysis._measuring(hit_dic)

            length_count = 0

            if busco_query:
                if busco_query not in everything:
                    everything[busco_query] = hit_dic
                else:
                    for part in hit_dic:
                        everything[busco_query][part] = hit_dic[part]

                for hit in hit_dic:
                    everything[busco_query][hit][0].append(length[length_count])
                    length_count += 1
                # classify genes using sigmas
                for entry in everything[busco_query]:
                    size = everything[busco_query][entry][0][3]
                    sigma = (self._cutoff_dictionary[busco_query]['length'] -
                             size) / self._cutoff_dictionary[busco_query]['sigma']
                    everything[busco_query][entry][0].append(sigma)
                    everything[busco_query][entry][0].append(file_name)

        # REFINE CLASSIFICATION

        # separate complete into multi and single-copy, and keep gene over 2 sigma in a separate dict
        is_complete = {}
        is_fragment = {}
        is_very_large = {}

        for thing in everything:
            for sequence in everything[thing]:

                all_data = everything[thing][sequence][0]
                sigma = everything[thing][sequence][0][-2]
                seq_name = sequence
                if -2 <= sigma <= 2:
                    if thing not in is_complete:
                        is_complete[thing] = {}
                    is_complete[thing][seq_name] = all_data
                elif sigma > 2:
                    if thing not in is_fragment:
                        is_fragment[thing] = {}
                    is_fragment[thing][seq_name] = all_data
                else:
                    if thing not in is_very_large:
                        is_very_large[thing] = {}
                    is_very_large[thing][seq_name] = all_data

        the_sc = {}
        the_mc = {}
        the_fg = {}

        sc_count = 0
        mc_count = 0
        fg_count = 0

        has_complete_match = []

        # filter gene matching two BUSCOs
        is_complete = Analysis._filter_multi_match_genes(is_complete)
        is_very_large = Analysis._filter_multi_match_genes(is_very_large)
        # filter duplicated gene that have a bad ratio compared to the top scoring match
        is_complete = Analysis._remove_bad_ratio_genes(is_complete)
        is_very_large = Analysis._remove_bad_ratio_genes(is_very_large)

        for entity in is_complete:
            # single copy
            if len(is_complete[entity]) == 1:  # e.g. BUSCOaEOG7QCM97
                the_sc[entity] = is_complete[entity]
                sc_count += 1
                has_complete_match.append(entity)
            elif len(is_complete[entity]) >= 2:
                the_mc[entity] = is_complete[entity]
                mc_count += 1
                has_complete_match.append(entity)

        # consider the very large genes as true findings only if there is no BUSCO < 2 sigma already found.
        for entity in is_very_large:
            if entity not in the_sc and entity not in the_mc:
                if len(is_very_large[entity]) >= 2:
                    the_mc[entity] = is_very_large[entity]
                else:
                    the_sc[entity] = is_very_large[entity]
                has_complete_match.append(entity)

        for entity in is_fragment:
            if entity not in has_complete_match:
                best_fragment_key = list(is_fragment[entity].keys())[0]
                for fragment_key in list(is_fragment[entity].keys()):
                    if is_fragment[entity][fragment_key][2] > is_fragment[entity][best_fragment_key][2]:  # best score
                        best_fragment_key = fragment_key
                fg_count += 1
                the_fg[entity] = {best_fragment_key: is_fragment[entity][best_fragment_key]}

        sc_count = len(the_sc)
        mc_count = len(the_mc)

        env.append(sc_count)
        env.append(mc_count)
        env.append(fg_count)

        out = open('%sfull_table_%s.tsv' % (self.mainout, self._abrev), 'w')
        self._write_output_header(out)
        self._write_full_table_header(out)
        out_lines = []

        not_missing = []
        fragmented = []
        csc = {}

        for entity in the_sc:

            for seq_id in the_sc[entity]:
                bit_score = the_sc[entity][seq_id][2]
                seq_len = the_sc[entity][seq_id][3]

                not_missing.append(entity)

                if self._mode == 'proteins' or self._mode == 'tran':
                    out_lines.append('%s\tComplete\t%s\t%s\t%s\n' % (entity, self._reformats_seq_id(seq_id),
                                                                     bit_score, seq_len))
                elif self._mode == 'genome':
                    scaff = self._split_seq_id(seq_id)
                    out_lines.append(
                        '%s\tComplete\t%s\t%s\t%s\t%s\t%s\n' %
                        (entity, self._reformats_seq_id(scaff['id']), scaff['start'], scaff['end'], bit_score, seq_len))
                    csc[entity] = seq_id

        for entity in the_mc:
            for seq_id in the_mc[entity]:
                bit_score = the_mc[entity][seq_id][2]
                seq_len = the_mc[entity][seq_id][3]

                not_missing.append(entity)

                if self._mode == 'proteins' or self._mode == 'tran':
                    out_lines.append('%s\tDuplicated\t%s\t%s\t%s\n' % (entity, self._reformats_seq_id(seq_id),
                                                                       bit_score, seq_len))
                elif self._mode == 'genome':
                    scaff = self._split_seq_id(seq_id)
                    out_lines.append(
                        '%s\tDuplicated\t%s\t%s\t%s\t%s\t%s\n' % (entity, self._reformats_seq_id(scaff['id']),
                                                                  scaff['start'], scaff['end'],
                                                                  bit_score, seq_len))

        for entity in the_fg:
            for seq_id in the_fg[entity]:
                bit_score = the_fg[entity][seq_id][2]
                seq_len = the_fg[entity][seq_id][3]

                not_missing.append(entity)
                fragmented.append(entity)

                if self._mode == 'proteins' or self._mode == 'tran':
                    out_lines.append('%s\tFragmented\t%s\t%s\t%s\n' % (entity, self._reformats_seq_id(seq_id),
                                                                       bit_score, seq_len))
                elif self._mode == 'genome':
                    scaff = self._split_seq_id(seq_id)
                    out_lines.append(
                        '%s\tFragmented\t%s\t%s\t%s\t%s\t%s\n' % (entity, self._reformats_seq_id(scaff['id']),
                                                                  scaff['start'], scaff['end'],
                                                                  bit_score, seq_len))

        missing = []
        miss_file = open('%smissing_busco_list_%s.tsv' % (self.mainout, self._abrev), 'w')
        self._write_output_header(miss_file)
        for busco_group in self._cutoff_dictionary:
            if busco_group in not_missing:
                pass
            else:
                out_lines.append('%s\tMissing\n' % busco_group)
                missing.append(busco_group)

        env.append(missing)
        env.append(fragmented)
        env.append(csc)

        for line in sorted(missing):
            miss_file.write('%s\n' % line)
        miss_file.close()
        for line in sorted(out_lines):
            out.write(line)
        out.close()
        return env

    def _load_score(self):
        """
        This function loads the score cutoffs file
        :raises SystemExit: if the scores_cutoff file cannot be read
        """
        try:
            score_file = open('%sscores_cutoff' % self._clade_path)  # open target scores file
        except IOError:
            _logger.error('Impossible to read the scores in %sscores_cutoff' % self._clade_path)
            raise SystemExit
        score_dic = {}
        for entry in score_file:
            entry = entry.strip().split()
            try:
                score_dic[entry[0]] = float(entry[1])  # name : score
                self._cutoff_dictionary[entry[0]] = {'score': float(entry[1])}
            except(IndexError, KeyError):
                pass
            self._totalbuscos = len(list(self._cutoff_dictionary.keys()))
        score_file.close()

    def _load_length(self):
        """
        This function loads the length cutoffs file
        """
        leng_dic = {}
        sd_dic = {}
        try:
            f = open('%slengths_cutoff' % self._clade_path)
        except IOError:
            _logger.error('Impossible to read the lengths in %slengths_cutoff' % self._clade_path)
            raise SystemExit

        for line in f:
            line = line.strip().split()

            leng_dic[line[0]] = float(line[3])  # legacy
            sd_dic[line[0]] = float(line[2])  # legacy

            self._cutoff_dictionary[line[0]]['sigma'] = float(line[2])
            # there is an arthropod profile with sigma 0 that causes a crash on divisions
            if float(line[2]) == 0.0:
                self._cutoff_dictionary[line[0]]['sigma'] = 1

            self._cutoff_dictionary[line[0]]['length'] = float(line[3])
        f.close()

    def _create_directory(self):
        """
        This function creates the run and the temporary directories
        :raises SystemExit: if a run with the same name already exists and the force option is not set
        :raises SystemExit: if the user cannot write in the tmp directory
        """
        # create the run directory
        self.mainout = ROOT_FOLDER + '/run_%s/' % self._abrev  # final output directory
        # complain about the -r option if there is no checkpoint.tmp file
        if not self._get_checkpoint() and self._restart:
            _logger.warning('This is not an uncompleted run that can be restarted')
            self._restart = False

        if not os.path.exists(self.mainout) and self._abrev:
            Analysis.p_open(['mkdir', self.mainout], 'bash', shell=False)
        else:
            if not self._force and not self._restart:
                restart_msg1 = ''
                restart_msg2 = ''
                if self._get_checkpoint():
                    restart_msg1 = ' and seems uncompleted'
                    restart_msg2 = ', or use the -r option to continue an uncompleted run'
                _logger.error('A run with that name already exists%s...'
                              '\n\tIf you are sure you wish to overwrite existing files, please use the -f option%s'
                              % (restart_msg1, restart_msg2))

                raise SystemExit
            elif not self._restart:
                _logger.info('Delete the current result folder and start a new run')
                Analysis.p_open(['rm -rf %s*' % self.mainout], 'bash', shell=True)

        # create the tmp directory
        if self._tmp != './':
            if not os.path.exists(self._tmp):
                Analysis.p_open(['mkdir', self._tmp], 'bash', shell=False)
            if self._tmp[-1] != '/':
                self._tmp += '/'
        _logger.info('Temp directory is %s' % self._tmp)

        if not os.access(self._tmp, os.W_OK):
            _logger.error('Cannot write to the temp directory, please make sure you have '
                          'write permissions to %s' % self._tmp)
            raise SystemExit

    @abstractmethod
    def check_dependencies(self):
        """
        This function checks that all dependencies are satisfied.  It has to be overriden by subclasses
        """
        pass

    def _produce_short_summary(self):
        """
        This function reads the result files and produces the final short summary file
        """

        hmmer_results = os.listdir('%shmmer_output' % self.mainout)
        hmmer_results.sort()
        hmmer_results_files = []
        for entry in hmmer_results:
            hmmer_results_files.append(entry)

        results_from_hmmer = self._parse_hmmer(hmmer_results_files)
        single_copy = results_from_hmmer[0]  # int
        multi_copy = results_from_hmmer[1]  # int
        only_fragments = results_from_hmmer[2]  # int
        self._missing_busco_list = results_from_hmmer[3]  # list of BUSCO ids
        self._fragmented_busco_list = results_from_hmmer[4]  # list of BUSCO ids
        self._single_copy_files = results_from_hmmer[5]

        summary_file = open('%sshort_summary_%s.txt' % (self.mainout, self._abrev), 'w')
        self._write_output_header(summary_file)
        summary_file.write('# Summarized benchmarking in BUSCO notation for file %s\n# BUSCO was run in mode: %s\n\n'
                           % (self._sequences, self._mode))
        s_percent = round((single_copy / float(self._totalbuscos)) * 100, 1)
        d_percent = round((multi_copy / float(self._totalbuscos)) * 100, 1)
        f_percent = round((only_fragments / float(self._totalbuscos)) * 100, 1)
        _logger.info('Results:')
        out_line = ('\tC:%s%%[S:%s%%,D:%s%%],F:%s%%,M:%s%%,n:%s\n\n' %
                    (round(s_percent + d_percent, 1), s_percent, d_percent, f_percent,
                     round(100 - s_percent - d_percent - f_percent, 1), self._totalbuscos))
        summary_file.write(out_line)
        _logger.info(out_line.replace('\t', '').strip())
        out_line = ('\t%s\tComplete BUSCOs (C)\n' % (single_copy + multi_copy))
        summary_file.write(out_line)
        _logger.info(out_line.replace('\t', ' ').strip())
        out_line = ('\t%s\tComplete and single-copy BUSCOs (S)\n' % single_copy)
        summary_file.write(out_line)
        _logger.info(out_line.replace('\t', ' ').strip())
        out_line = ('\t%s\tComplete and duplicated BUSCOs (D)\n' % multi_copy)
        summary_file.write(out_line)
        _logger.info(out_line.replace('\t', ' ').strip())
        out_line = ('\t%s\tFragmented BUSCOs (F)\n' % only_fragments)
        summary_file.write(out_line)
        _logger.info(out_line.replace('\t', ' ').strip())
        out_line = ('\t%s\tMissing BUSCOs (M)\n' %
                    str(self._totalbuscos - single_copy - multi_copy - only_fragments))
        summary_file.write(out_line)
        _logger.info(out_line.replace('\t', ' ').strip())
        out_line = ('\t%s\tTotal BUSCO groups searched\n' % self._totalbuscos)
        summary_file.write(out_line)
        _logger.info(out_line.replace('\t', ' ').strip())
        summary_file.close()

    def _process_augustus_tasks(self):
        """
        This function takes Augustus tasks in the queue and executes them.
        """
        while not self._exit_flag:
            self._queue_lock.acquire()
            if not self._work_queue.empty():
                data = self._work_queue.get()
                self._queue_lock.release()
                check = len([name for name in os.listdir('%saugustus_output/predicted_genes' % self.mainout) if
                             os.path.isfile(os.path.join('%saugustus_output/predicted_genes' % self.mainout, name))])
                state = 100 * check / self._total
                if state > self.slate[-1]:
                    _logger.info('%s =>\t%s%% of predictions performed (%i/%i candidate regions)'
                                 % (time.strftime("%m/%d/%Y %H:%M:%S"), self.slate.pop(), check, self._total))
                Analysis.p_open([data], 'augustus', shell=True)
            else:
                self._queue_lock.release()

    def _process_hmmer_tasks(self):
        """
        This function takes hmmersearch tasks in the queue and executes them.
        """
        while not self._exit_flag:
            self._queue_lock.acquire()
            if not self._work_queue.empty():
                data = self._work_queue.get()
                self._queue_lock.release()
                files = set(name for name in os.listdir('%shmmer_output' % self.mainout) if
                         os.path.isfile(os.path.join('%shmmer_output' % self.mainout, name)))
                check = len(files)
                state = 100 * check / self._total
                if state > self.slate[-1]:
                    _logger.info('%s =>\t%s%% of predictions performed (%i/%i candidate proteins)'
                                 % (time.strftime("%m/%d/%Y %H:%M:%S"), self.slate.pop(), check, self._total))
                Analysis.p_open(data, 'hmmersearch', shell=False)
            else:
                self._queue_lock.release()

    def _process_gff2gbsmalldna_tasks(self):
        """
        This function takes gff2gbSmallDNA.pl tasks in the queue and executes them.
        """
        while not self._exit_flag:
            self._queue_lock.acquire()
            if not self._work_queue.empty():
                data = self._work_queue.get()
                self._queue_lock.release()
                Analysis.p_open([data], 'gff2gbSmallDNA.pl', shell=True)
            else:
                self._queue_lock.release()

    @abstractmethod
    def run_analysis(self):
        """
        This function calls all needed steps for running the analysis. It has to be overriden by subclasses
        """
        pass


class GenomeAnalysis(Analysis):
    """
    This class runs a BUSCO analysis on a genome. It extends Analysis.
    """

    def check_dataset(self):
        """
        Check if the dataset integrity, if files and folder are present
        :raises SystemExit: if the dataset miss files or folders
        """
        # prfl folder
        flag = False
        for dirpath, dirnames, files in os.walk('%sprfl' % self._clade_path):
            if files:
                flag = True
        if not flag:
            _logger.error('The dataset you provided lacks elements in %sprfl' % self._clade_path)
            raise SystemExit
        super(GenomeAnalysis, self).check_dataset()
        # note: score and length cutoffs are checked when read, see _load_scores and _load_lengths
        # ancestral would cause blast to fail, and be detected, see _blast()
        # dataset.cfg is not mandatory

        # check whether the ancestral_variants file is present
        if os.path.exists('%sancestral_variants' % self._clade_path):
            self._has_variants_file = True
        else:
            self._has_variants_file = False
            _logger.warning("The dataset you provided does not contain the file ancestral_variants, "
                            "likely because it is an old version. "
                            "All blast steps will use the file ancestral instead")

    def check_dependencies(self):
        """
        This function checks that all dependencies are satisfied.
        """
        self._check_augustus()
        Analysis._check_blast()
        Analysis._check_hmmer()

    def __init__(self, params):
        """
        Initialize an instance.
        :param params: Values of all parameters that have to be defined
        :type params: dict
        """
        super(GenomeAnalysis, self).__init__(params)
        self._mode = 'genome'

    def _fix_restart_augustus_folder(self):
        """
        This function resets and checks the augustus folder to make a restart possible in phase 2
        :raises SystemExit: if it is not possible to fix the folders
        """
        if os.path.exists('%saugustus_output/predicted_genes_run1' % self.mainout) and \
                os.path.exists('%shmmer_output_run1' % self.mainout):
            Analysis.p_open(['rm', '-fr', '%saugustus_output/predicted_genes' % self.mainout], 'bash', shell=False)
            Analysis.p_open(['mv', '%saugustus_output/predicted_genes_run1'
                             % self.mainout, '%saugustus_output/predicted_genes'
                             % self.mainout], 'bash', shell=False)
            Analysis.p_open(['rm', '-fr', '%shmmer_output' % self.mainout], 'bash', shell=False)
            Analysis.p_open(['mv', '%shmmer_output_run1/' % self.mainout, '%shmmer_output/' % self.mainout], 'bash',
                            shell=False)

        elif os.path.exists('%saugustus_output/predicted_genes' % self.mainout) and \
                os.path.exists('%shmmer_output' % self.mainout):
            pass
        else:
            _logger.error('Impossible to restart the run, necessary folders are missing. '
                          'Use the -f option instead of -r')
            raise SystemExit

    def run_analysis(self):
        """
        This function calls all needed steps for running the analysis.
        """
        self.check_dataset()
        self._check_nucleotide()
        self._create_directory()
        _log.add_blank_line()
        if self._restart:
            checkpoint = self._get_checkpoint(reset_random_suffix=True)
            _logger.warning('Restarting an uncompleted run')
        else:
            checkpoint = 0  # all steps will be done

        _logger.info('****** Phase 1 of 2, initial predictions ******')
        if checkpoint < 1:
            _logger.info('****** Step 1/3, current time: %s ******' % time.strftime("%m/%d/%Y %H:%M:%S"))
            self._blast()
            self._define_checkpoint(1)

        if checkpoint < 2:
            _logger.info('****** Step 2/3, current time: %s ******' % time.strftime("%m/%d/%Y %H:%M:%S"))
            self._get_coordinates()
            self._augustus()
            _logger.info('****** Step 3/3, current time: %s ******' % time.strftime("%m/%d/%Y %H:%M:%S"))
            self._hmmer()
            self._define_checkpoint(2)
        self._load_score()
        self._load_length()
        if checkpoint == 2 or checkpoint == 3:
            _logger.info('Phase 1 was already completed.')
        if checkpoint == 3:
            self._fix_restart_augustus_folder()
        self._produce_short_summary()
        _log.add_blank_line()
        _logger.info('****** Phase 2 of 2, predictions using species specific training ******')
        if checkpoint < 3:
            _logger.info('****** Step 1/3, current time: %s ******' % time.strftime("%m/%d/%Y %H:%M:%S"))
            if self._has_variants_file:
                self._blast(missing_and_frag_only=True, ancestral_variants=True)
                self._get_coordinates(missing_and_frag_only=True)
            else:
                self._blast(missing_and_frag_only=True, ancestral_variants=False)
                self._get_coordinates(missing_and_frag_only=True)
            self._define_checkpoint(3)
        _logger.info('****** Step 2/3, current time: %s ******' % time.strftime("%m/%d/%Y %H:%M:%S"))
        self._augustus_rerun()
        self._move_retraining_parameters()
        self.cleanup()
        self._define_checkpoint()  # remove the checkpoint, run is done

    @staticmethod
    def _write_full_table_header(out):
        """
        This function adds a header line to the full table file
        :param out: a full table file
        :type out: file
        """
        out.write('# Busco id\tStatus\tContig\tStart\tEnd\tScore\tLength\n')

    def _move_retraining_parameters(self):
        """
        This function moves retraining parameters from augustus species folder to the run folder
        """
        if os.path.exists(self._augustus_config_path + ('/species/BUSCO_%s%s' % (self._abrev, self._random))):
            Analysis.p_open(['cp', '-r', '%s/species/BUSCO_%s%s'
                             % (self._augustus_config_path, self._abrev, self._random),
                             '%saugustus_output/retraining_parameters' % self.mainout], 'bash', shell=False)
            Analysis.p_open(['rm', '-rf', '%s/species/BUSCO_%s%s'
                             % (self._augustus_config_path, self._abrev, self._random)],
                            'bash', shell=False)
        else:
            _log.add_blank_line()
            _logger.warning('Augustus did not produce a retrained species folder, please check the augustus log file '
                            'in the run folder to ensure that nothing went wrong (%saugustus_output/augustus.log)'
                            % self.mainout)

    def cleanup(self):
        """
        This function cleans temporary files and move some files to their final place
        """
        super(GenomeAnalysis, self).cleanup()
        Analysis.p_open(['rm %s*%s%s_.temp' % (self._tmp, self._abrev, self._random)], 'bash', shell=True)
        Analysis.p_open(['rm %(tmp)s%(abrev)s.*ns? %(tmp)s%(abrev)s.*nin %(tmp)s%(abrev)s.*nhr'
                         % {'tmp': self._tmp, 'abrev': self._abrev + str(self._random)}], 'bash', shell=True)

    def _run_tarzip(self):
        """
        This function tarzips results folder
        """
        super(GenomeAnalysis, self)._run_tarzip()
        # augustus_output/predicted_genes
        Analysis.p_open(['tar', '-C', '%saugustus_output' % self.mainout, '-zcf',
                         '%saugustus_output/predicted_genes.tar.gz' % self.mainout, 'predicted_genes',
                         '--remove-files'],
                        'bash', shell=False)
        # augustus_output/extracted_proteins
        Analysis.p_open(['tar', '-C', '%saugustus_output' % self.mainout, '-zcf',
                         '%saugustus_output/extracted_proteins.tar.gz' % self.mainout, 'extracted_proteins',
                         '--remove-files'],
                        'bash', shell=False)
        # augustus_output/gb
        Analysis.p_open(['tar', '-C', '%saugustus_output' % self.mainout, '-zcf',
                         '%saugustus_output/gb.tar.gz' % self.mainout, 'gb',
                         '--remove-files'],
                        'bash', shell=False)
        # augustus_output/gffs
        Analysis.p_open(['tar', '-C', '%saugustus_output' % self.mainout, '-zcf',
                         '%saugustus_output/gffs.tar.gz' % self.mainout, 'gffs',
                         '--remove-files'],
                        'bash', shell=False)
        # single_copy_busco_sequences
        Analysis.p_open(['tar', '-C', '%s' % self.mainout, '-zcf',
                         '%ssingle_copy_busco_sequences.tar.gz' % self.mainout, 'single_copy_busco_sequences',
                         '--remove-files'],
                        'bash', shell=False)

    def _get_coordinates(self, missing_and_frag_only=False):
        """
        This function gets coordinates for candidate regions from tblastn result file
        :param missing_and_frag_only: tell whether to use the missing_and_frag_rerun tblastn file
        :type missing_and_frag_only: bool
        """

        if missing_and_frag_only:
            blast_file = open('%sblast_output/tblastn_%s_missing_and_frag_rerun.tsv' % (self.mainout, self._abrev))
        else:
            blast_file = open('%sblast_output/tblastn_%s.tsv' % (self.mainout, self._abrev))

        _logger.info('Getting coordinates for candidate regions...')

        dic = {}
        coords = {}
        for line in blast_file:
            if line.startswith('#'):
                pass
            else:
                try:
                    line = line.strip().split()
                    busco_name = line[0]
                    contig = line[1]  # busco_og and contig name, respectively
                    busco_start = int(line[6])
                    busco_end = int(line[7])  # busco hit positions
                    contig_start = int(line[8])
                    contig_end = int(line[9])  # contig postions
                    aln_len = int(line[3])  # e_value and alignment length
                    blast_eval = float(line[10])

                    if contig_end < contig_start:  # for minus-strand genes, invert coordinates for convenience
                        temp = contig_end
                        contig_end = contig_start
                        contig_start = temp
                    if busco_name not in dic.keys():  # create new entry in dictionary for current BUSCO
                        dic[busco_name] = [contig]
                        coords[busco_name] = {}
                        coords[busco_name][contig] = [contig_start, contig_end, deque([[busco_start, busco_end]]),
                                                      aln_len, blast_eval]
                    # get just the top scoring regions according to region limits
                    elif contig not in dic[busco_name] and len(dic[busco_name]) < self._region_limit:
                        # scoring regions
                        dic[busco_name].append(contig)
                        coords[busco_name][contig] = [contig_start, contig_end, deque([[busco_start, busco_end]]),
                                                      aln_len, blast_eval]

                    # replace the lowest scoring region if the current has a better score.
                    # needed because of multiple blast query having the same name when using ancestral_variants
                    # and not sorted by eval in the tblastn result file
                    elif contig not in dic[busco_name] and len(dic[busco_name]) >= self._region_limit:
                        to_replace = None
                        for entry in list(coords[busco_name].keys()):
                            if coords[busco_name][entry][4] > blast_eval:
                                if (to_replace and  # check if there is already a to_replace entry and compare the eval
                                            coords[busco_name][entry][4] > list(to_replace.values())[0]) \
                                        or not to_replace:
                                    # use a single entry dictionary to store the id to replace and its eval
                                    to_replace = {entry: coords[busco_name][entry][4]}
                        if to_replace:
                            dic[busco_name].remove(list(to_replace.keys())[0])
                            dic[busco_name].append(contig)
                            coords[busco_name][contig] = [contig_start, contig_end, deque([[busco_start, busco_end]]),
                                                          aln_len, blast_eval]

                    elif contig in dic[busco_name]:  # contigold already checked,
                        # now update coordinates
                        if contig_start < coords[busco_name][contig][0] and coords[busco_name][contig][0] - \
                                contig_start <= 50000:  # starts before, and withing 50kb of current position
                            coords[busco_name][contig][0] = contig_start
                            coords[busco_name][contig][2].append([busco_start, busco_end])
                        if contig_end > coords[busco_name][contig][1] \
                                and contig_end - coords[busco_name][contig][1] \
                                        <= 50000:  # ends after and within 50 kbs
                            coords[busco_name][contig][1] = contig_end
                            coords[busco_name][contig][3] = busco_end
                            coords[busco_name][contig][2].append([busco_start, busco_end])
                        elif coords[busco_name][contig][1] > contig_start > coords[busco_name][contig][0]:
                            # starts inside current coordinates
                            if contig_end < coords[busco_name][contig][1]:
                                # if ending inside, just add alignemnt positions to deque
                                coords[busco_name][contig][2].append([busco_start, busco_end])
                                # if ending after current coordinates, extend
                            elif contig_end > coords[busco_name][contig][1]:
                                coords[busco_name][contig][2][1] = contig_end
                                coords[busco_name][contig][2].append([busco_start, busco_end])

                except (IndexError, ValueError):
                    pass

        blast_file.close()
        final_locations = {}
        if missing_and_frag_only:
            out = open('%s/blast_output/coordinates_%s_missing_and_frag_rerun.tsv'
                       % (self.mainout, self._abrev), 'w')  # open Coordinates output file
        else:
            out = open('%s/blast_output/coordinates_%s.tsv'
                       % (self.mainout, self._abrev), 'w')  # open Coordinates output file

        for busco_group in coords:
            final_locations[busco_group] = []
            candidate_contigs = list(coords[busco_group].keys())  # list of candidate contigs
            size_lists = []

            for contig in candidate_contigs:
                potential_locations = coords[busco_group][contig][2]
                max_iterations = len(potential_locations)
                iter_count = 0

                final_regions = []  # nested list of regions
                used_pieces = []
                non_used = []

                while iter_count < max_iterations:
                    currently = potential_locations[iter_count]
                    if len(final_regions) == 0:
                        final_regions.append(currently)
                    else:
                        for region in final_regions:
                            if Analysis._check_overlap(currently, region) != 0:
                                gg = Analysis._define_boundary(currently, region)
                                region_index = final_regions.index(region)
                                final_regions[region_index] = gg
                                used_pieces.append(iter_count)
                            else:
                                non_used.append(iter_count)
                    iter_count += 1

                # done for this contig, now consolidate
                for entry_index in non_used:
                    entry = potential_locations[entry_index]
                    if entry in used_pieces:
                        pass  # already used
                    else:
                        ok = []
                        for region in final_regions:
                            checking = Analysis._check_overlap(entry, region)
                            if checking == 0:
                                # i.e. no overlap
                                pass
                            else:
                                ok.append([region, entry])
                        if len(ok) == 0:
                            # no overlaps at all (i.e. unique boundary)
                            final_regions.append(entry)
                        else:
                            region = ok[0][0]
                            currently = ok[0][1]
                            gg = Analysis._define_boundary(currently, region)
                            final_regions[final_regions.index(region)] = gg

                size_lists.append(Analysis._gargantua(final_regions))
            max_size = max(size_lists)
            size_cutoff = int(0.7 * max_size)
            index_passed_cutoffs = heapq.nlargest(self._region_limit, range(len(size_lists)), size_lists.__getitem__)

            for candidate in index_passed_cutoffs:
                if size_lists[candidate] >= size_cutoff:
                    seq_name = candidate_contigs[candidate]
                    seq_start = int(coords[busco_group][candidate_contigs[candidate]][0]) - self._flank
                    if seq_start < 0:
                        seq_start = 0
                    seq_end = int(coords[busco_group][candidate_contigs[candidate]][1]) + self._flank
                    final_locations[busco_group].append([seq_name, seq_start, seq_end])
                    out.write('%s\t%s\t%s\t%s\n' % (busco_group, seq_name, seq_start, seq_end))
        out.close()

    def _hmmer(self):
        """
        This function runs hmmsearch.
        :raises SystemExit: if the hmmsearch result folder is empty after the run
        """
        _logger.info('Running HMMER to confirm orthology of predicted proteins:')

        files = os.listdir('%saugustus_output/extracted_proteins' % self.mainout)
        files.sort()
        if not os.path.exists(self.mainout + 'hmmer_output'):
            Analysis.p_open(['mkdir', '%shmmer_output' % self.mainout], 'bash', shell=False)

        count = 0

        hmmer_run_strings = []

        for entry in files:
            if entry.split('.')[-2] == 'faa':
                count += 1
                group_name = entry.split('.')[0]
                group_index = entry.split('.')[-1]
                hmmer_call = [hmmer_cmd,
                              '--domtblout', '%shmmer_output/%s.out.%s' % (self.mainout, group_name, group_index),
                              '-o', '%stemp_%s%s' % (self._tmp, self._abrev, self._random),
                              '--cpu', '1',
                              '%shmms/%s.hmm' % (self._clade_path, group_name),
                              '%saugustus_output/extracted_proteins/%s' % (self.mainout, entry)]

                hmmer_run_strings.append(hmmer_call)

        self._run_threads(hmmer_run_strings, self._HmmerThreads)


class TranscriptomeAnalysis(Analysis):
    """
    This class runs a BUSCO analysis on a transcriptome. It extends Analysis.
    """

    @staticmethod
    def _reformats_seq_id(seq_id):
        """
        This function reformats the sequence id to its original values
        :param seq_id: the seq id to reformats
        :type seq_id: str
        :return: the reformatted seq_id
        :rtype: str
        """
        return "_".join(seq_id.split('_')[:-1])

    def check_dataset(self):
        """
        Check if the dataset integrity, if files and folder are present
        :raises SystemExit: if the dataset miss files or folders
        """
        # check whether the ancestral_variants file is present
        if os.path.exists('%sancestral_variants' % self._clade_path):
            self._has_variants_file = True
        else:
            self._has_variants_file = False
            _logger.warning("The dataset you provided does not contain the file ancestral_variants, "
                            "likely because it is an old version. "
                            "All blast steps will use the file ancestral")

    def check_dependencies(self):
        """
        This function checks that all dependencies are satisfied.
        """
        Analysis._check_blast()
        Analysis._check_hmmer()

    def __init__(self, params):
        """
        Initialize an instance.
        :param params: Values of all parameters that have to be defined
        :type params: dict
        """
        super(TranscriptomeAnalysis, self).__init__(params)
        self._mode = 'tran'

    def run_analysis(self):
        """
        This function calls all needed steps for running the analysis.
        """
        self.check_dataset()
        self._check_nucleotide()
        self._create_directory()
        if self._restart:
            checkpoint = self._get_checkpoint(reset_random_suffix=True)
            _logger.warning('Restarting an uncompleted run')
        else:
            checkpoint = 0  # all steps will be done
        if checkpoint < 1:
            _logger.info('****** Step 1/2, current time: %s ******' % time.strftime("%m/%d/%Y %H:%M:%S"))
            if self._has_variants_file:
                self._blast(ancestral_variants=True)
            else:
                self._blast(ancestral_variants=False)
            self._define_checkpoint(1)
        _logger.info('****** Step 2/2, current time: %s ******' % time.strftime("%m/%d/%Y %H:%M:%S"))
        self._load_score()
        self._load_length()
        self._get_coordinates()
        self._hmmer()
        self._produce_short_summary()
        self.cleanup()
        self._define_checkpoint()  # remove the checkpoint, run is done

    def cleanup(self):
        """
        This function cleans temporary files.
        """
        super(TranscriptomeAnalysis, self).cleanup()
        Analysis.p_open(['rm %s*%s%s_.temp' % (self._tmp, self._abrev, self._random)], 'bash', shell=True)
        Analysis.p_open(['rm %(tmp)s%(abrev)s.*ns? %(tmp)s%(abrev)s.*nin %(tmp)s%(abrev)s.*nhr'
                         % {'tmp': self._tmp, 'abrev': self._abrev + str(self._random)}], 'bash', shell=True)

    def _run_tarzip(self):
        """
        This function tarzips results folder
        """
        super(TranscriptomeAnalysis, self)._run_tarzip()
        # translated_proteins
        Analysis.p_open(['tar', '-C', '%s' % self.mainout, '-zcf',
                         '%stranslated_proteins.tar.gz' % self.mainout, 'translated_proteins',
                         '--remove-files'],
                        'bash', shell=False)

    def _get_coordinates(self):
        """
        This function gets coordinates for candidate regions from tblastn result file
        """
        _logger.info('Getting coordinates for candidate transcripts...')
        f = open('%sblast_output/tblastn_%s.tsv' % (self.mainout, self._abrev))  # open input file
        transcriptome_by_busco = {}
        self._transcriptome_by_scaff = {}
        maxi = 0
        for i in f:  # get a dictionary of BUSCO matches vs candidate scaffolds
            if i.startswith('#'):
                pass
            else:
                line = i.strip().split()
                if self._has_variants_file:
                    busco = '_'.join(line[0].split("_")[:-1])  # This pattern can support name like EOG00_1234_1
                else:
                    busco = line[0]
                scaff = line[1]
                leng = int(line[3])
                blast_eval = float(line[10])
                if busco not in transcriptome_by_busco.keys():
                    # Simply add it
                    # Use a single entry dict to keep scaffs id and their blast eval, for each busco
                    transcriptome_by_busco[busco] = [{scaff: blast_eval}]
                    # and keep a list of each busco by scaff
                    try:
                        self._transcriptome_by_scaff[scaff].append(busco)
                    except KeyError:
                        self._transcriptome_by_scaff[scaff] = [busco]
                    maxi = leng
                elif len(transcriptome_by_busco[busco]) < self._region_limit and leng >= 0.7 * maxi:
                    # check that this transcript is not already in, and update its eval if needed
                    add = True
                    for scaff_dict in transcriptome_by_busco[busco]:
                        if list(scaff_dict.keys())[0] == scaff:
                            add = False
                            if blast_eval < list(scaff_dict.values())[0]:
                                scaff_dict[scaff] = blast_eval  # update the eval for this scaff
                    if add:
                        transcriptome_by_busco[busco].append({scaff: blast_eval})
                        try:
                            self._transcriptome_by_scaff[scaff].append(busco)
                        except KeyError:
                            self._transcriptome_by_scaff[scaff] = [busco]
                        if leng > maxi:
                            maxi = leng
                elif len(transcriptome_by_busco[busco]) >= self._region_limit and leng >= 0.7 * maxi:
                    # replace the lowest scoring transcript if the current has a better score.
                    # needed because of multiple blast query having the same name when using ancestral_variants
                    # and not sorted by eval in the tblastn result file
                    to_replace = None
                    # Define if something has to be replaced
                    for entry in transcriptome_by_busco[busco]:
                        if list(entry.values())[0] > blast_eval:
                            if (to_replace and  # check if there is already a to_replace entry and compare the eval
                                        list(entry.values())[0] > list(to_replace.values())[0]) or not to_replace:
                                to_replace = {list(entry.keys())[0]: list(entry.values())[0]}

                    if to_replace:
                        # try to add the new one
                        # check that this scaffold is not already in, and update the eval if needed
                        # if the scaff was already in, do not replace the to_replace entry to keep the max number of
                        # candidate regions
                        add = True
                        for scaff_dict in transcriptome_by_busco[busco]:
                            if list(scaff_dict.keys())[0] == scaff:
                                add = False
                                if blast_eval < list(scaff_dict.values())[0]:
                                    scaff_dict[scaff] = blast_eval  # update the eval for this scaff
                        if add:
                            # add the new one
                            transcriptome_by_busco[busco].append({scaff: blast_eval})
                            try:
                                self._transcriptome_by_scaff[scaff].append(busco)
                            except KeyError:
                                self._transcriptome_by_scaff[scaff] = [busco]

                            # remove the old one
                            for entry in transcriptome_by_busco[busco]:
                                if list(entry.keys())[0] == list(to_replace.keys())[0]:
                                    scaff_to_remove = list(entry.keys())[0]
                                    break
                            transcriptome_by_busco[busco].remove(entry)

                            for entry in self._transcriptome_by_scaff[scaff_to_remove]:
                                if entry == busco:
                                    break
                            self._transcriptome_by_scaff[scaff_to_remove].remove(entry)

                            if leng > maxi:
                                maxi = leng

        _logger.info('Extracting candidate transcripts...')
        f = open(self._sequences)
        check = 0
        out = None
        for i in f:
            if i.startswith('>'):
                i = i.strip().split()
                i = i[0][1:]
                if i in list(self._transcriptome_by_scaff.keys()):
                    out = open('%s%s%s%s_.temp' % (self._tmp, i, self._abrev, self._random), 'w')
                    out.write('>%s\n' % i)
                    check = 1
                else:
                    check = 0
            elif check == 1:
                out.write(i)
        f.close()
        if out:
            out.close()
        if not os.path.exists('%stranslated_proteins' % self.mainout):
            Analysis.p_open(['mkdir', '%stranslated_proteins' % self.mainout], 'bash', shell=False)
        files = os.listdir(self._tmp)
        files.sort()
        lista = []
        for entry in files:
            if entry.endswith(self._abrev + str(self._random) + '_.temp'):
                lista.append(entry)

        _logger.info('Translating candidate transcripts...')
        for entry in lista:
            raw_seq = open(self._tmp + entry)
            name = self._abrev.join(entry.replace('_.temp', '')
                                    .split(self._abrev)[:-1])  # this works even if the runname is in the header
            trans_seq = open(self.mainout + 'translated_proteins/' + name + '.faa', 'w')
            nucl_seq = ''
            header = ''
            for line in raw_seq:
                if line.startswith('>'):
                    header = line.strip() + '_'
                else:
                    nucl_seq += line.strip()
            seq_count = 0
            for translation in Analysis._sixpack(nucl_seq):
                seq_count += 1
                trans_seq.write('%s%s\n%s\n' % (header, seq_count, translation))
            raw_seq.close()
            trans_seq.close()

        f2 = open('%sscores_cutoff' % self._clade_path)  # open target scores file
        # Load dictionary of HMM expected scores and full list of groups
        score_dic = {}
        for i in f2:
            i = i.strip().split()
            try:
                score_dic[i[0]] = float(i[1])  # float [1] = mean value; [2] = minimum value
            except IndexError:
                pass
        f2.close()
        self._totalbuscos = len(list(score_dic.keys()))

    def _hmmer(self):
        """
        This function runs hmmsearch.
        """
        _logger.info('Running HMMER to confirm transcript orthology:')
        files = os.listdir('%stranslated_proteins/' % self.mainout)
        files.sort()
        if not os.path.exists('%shmmer_output' % self.mainout):
            Analysis.p_open(['mkdir', '%shmmer_output' % self.mainout], 'bash', shell=False)

        count = 0

        hmmer_run_strings = []

        busco_index = {}

        for f in files:
            if f.endswith('.faa'):
                count += 1
                scaff = f[:-4]
                scaff_buscos = self._transcriptome_by_scaff[scaff]
                for busco in scaff_buscos:

                    try:
                        busco_index[busco] += 1
                    except KeyError:
                        busco_index[busco] = 1

                    hmmer_call = [hmmer_cmd,
                                  '--domtblout',
                                  '%shmmer_output/%s.out.%s' % (self.mainout, busco, busco_index[busco]),
                                  '-o', '%stemp_%s%s' % (self._tmp, self._abrev, str(self._random)),
                                  '--cpu', '1',
                                  '%shmms/%s.hmm' % (self._clade_path, busco),
                                  '%stranslated_proteins/%s' % (self.mainout, f)]

                    hmmer_run_strings.append(hmmer_call)

        # Run hmmer
        self._run_threads(hmmer_run_strings, self._HmmerThreads)


class GeneSetAnalysis(Analysis):
    """
    This class runs a BUSCO analysis on a gene set (proteins). It extends Analysis.
    """

    def check_dependencies(self):
        """
        This function checks that all dependencies are satisfied.
        """
        Analysis._check_hmmer()

    def __init__(self, params):
        """
        Initialize an instance.
        :param params: Values of all parameters that have to be defined
        :type params: dict
        """
        super(GeneSetAnalysis, self).__init__(params)
        if self._restart:
            _logger.error('There is no restart allowed for the protein mode')
            raise SystemExit
        self._mode = 'proteins'

    def run_analysis(self):
        """
        This function calls all needed steps for running the analysis.
        """
        self.check_dataset()
        self._check_protein()
        self._create_directory()
        self._load_score()
        self._load_length()
        self._hmmer()
        self._produce_short_summary()
        self.cleanup()

    def _hmmer(self):
        """
        This function runs hmmsearch.
        """

        # Run hmmer
        _logger.info('Running HMMER on the proteins:')

        if not os.path.exists(self.mainout + 'hmmer_output'):
            Analysis.p_open(['mkdir', '%shmmer_output' % self.mainout], 'bash', shell=False)
        files = os.listdir(self._clade_path + '/hmms')
        files.sort()
        f2 = open('%sscores_cutoff' % self._clade_path)  # open target scores file
        #   Load dictionary of HMM expected scores and full list of groups
        score_dic = {}
        for i in f2:
            i = i.strip().split()
            try:
                score_dic[i[0]] = float(i[1])  # [1] = mean value; [2] = minimum value
            except IndexError:
                pass
        self._totalbuscos = len(list(score_dic.keys()))
        f2.close()

        hmmer_run_strings = []
        for i in files:
            name = i[:-4]
            if name in score_dic:
                hmmer_run_strings.append([hmmer_cmd,
                                          '--domtblout', '%shmmer_output/%s.out.1' % (self.mainout, name),
                                          '-o', '%stemp_%s%s' % (self._tmp, self._abrev, str(self._random)),
                                          '--cpu', '1',
                                          '%s/hmms/%s.hmm' % (self._clade_path, name),
                                          '%s' % self._sequences])

        self._run_threads(hmmer_run_strings, self._HmmerThreads)


# end of classes definition, now module code

VERSION = '2.0.1'

CONTACT = 'mailto:support@orthodb.org'

ROOT_FOLDER = os.getcwd()

FORBIDDEN_HEADER_CHARS_BEFORE_SPLIT = ['/', '\'']

FORBIDDEN_HEADER_CHARS = ['ç', '¬', '¢', '´', 'ê', 'î', 'ô', 'ŵ', 'ẑ', 'û', 'â', 'ŝ', 'ĝ', 'ĥ', 'ĵ', 'ŷ',
                          'ĉ', 'é', 'ï', 'ẅ', 'ë', 'ẅ', 'ë', 'ẗ,', 'ü', 'í', 'ö', 'ḧ', 'é', 'ÿ', 'ẍ', 'è', 'é',
                          'à', 'ä', '¨', '€', '£', 'á']

#: Get an instance of _logger for keeping track of events
#logging.setLoggerClass(BUSCOLogger)
_log = BUSCOLogger(__file__.split("/")[-1])
_logger = _log._logger

_rerun_cmd = ''
busco_dirpath = join(qconfig.LIBS_LOCATION, 'busco')
hmmer_cmd = join(busco_dirpath, 'hmmsearch')
augustus_dirpath, augustus_cmd, augustus_config_path, etraining_cmd = None, None, None, None


def set_augustus_dir(dirpath):
    global augustus_dirpath, augustus_cmd, augustus_config_path, etraining_cmd
    augustus_dirpath = dirpath
    augustus_cmd = join(augustus_dirpath, 'bin', 'augustus')
    augustus_config_path = join(augustus_dirpath, 'config')
    etraining_cmd = join(augustus_dirpath, 'bin', 'etraining')


def get_augustus_script(fname):
    return join(augustus_dirpath, 'scripts', fname)


def _parse_args(args):
    """
    This function parses the arguments provided by the user
    :return: a dictionary having a key for each arguments
    :rtype: dict
    """

    # small hack to get sub-parameters with dash and pass it to Augustus
    for i, arg in enumerate(args):
        if (arg[0] == '-' or arg[0] == '--') and (args[i - 1] == '-a' or args[i - 1] == '--augustus'):
            args[i] = ' ' + arg

    parser = argparse.ArgumentParser(
        description='Welcome to BUSCO %s: the Benchmarking Universal Single-Copy Ortholog assessment tool.\n'
                    'For more detailed usage information, please review the README file provided with '
                    'this distribution and the BUSCO user guide.' % VERSION,
        usage='python BUSCO.py -i [SEQUENCE_FILE] -l [LINEAGE] -o [OUTPUT_NAME] -m [MODE] [OTHER OPTIONS]',
        formatter_class=RawTextHelpFormatter, add_help=False)

    required = parser.add_argument_group('required arguments')
    optional = parser.add_argument_group('optional arguments')

    required.add_argument(
        '-i', '--in', dest='in', required=True, metavar='FASTA FILE', help='Input sequence file in FASTA format. '
                                                                           'Can be an assembled genome or transcriptome (DNA), or protein sequences from an annotated gene set.')

    optional.add_argument(
        '-c', '--cpu', dest='cpu', required=False, metavar='N', help='Specify the number (N=integer) '
                                                                     'of threads/cores to use.')
    required.add_argument(
        '-o', '--out', dest='abrev', required=True, metavar='OUTPUT',
        help='Give your analysis run a recognisable short name. '
             'Output folders and files will be labelled with this name. WARNING: do not provide a path')

    optional.add_argument(
        '-e', '--evalue', dest='evalue', required=False, metavar='N', type=float, default=Analysis.EVALUE_DEFAULT,
        help='E-value cutoff for BLAST searches. '
             'Allowed formats, 0.001 or 1e-03 (Default: %.0e)' % Analysis.EVALUE_DEFAULT)

    required.add_argument(
        '-m', '--mode', dest='mode', required=True, metavar='MODE', help='Specify which BUSCO analysis mode to run.\n'
                                                                         'There are three valid modes:\n- geno or genome, for genome assemblies (DNA)\n- tran or transcriptome, for '
                                                                         'transcriptome assemblies (DNA)\n- prot or proteins, for annotated gene sets (protein)')
    required.add_argument(
        '-l', '--lineage', dest='clade', required=True, metavar='LINEAGE',
        help='Specify location of the BUSCO lineage data to be used.\n'
             'Visit http://busco.ezlab.org for available lineages.')

    optional.add_argument(
        '-f', '--force', action='store_true', required=False, default=False, dest='force',
        help='Force rewriting of existing files. Must be used when output files with the provided name already exist.')

    optional.add_argument(
        '-r', '--restart', action='store_true', required=False, default=False, dest='restart',
        help='Restart an uncompleted run. Not available for the protein mode')

    optional.add_argument(
        '-sp', '--species', required=False, dest='species', metavar='SPECIES',
        help='Name of existing Augustus species gene finding parameters. '
             'See Augustus documentation for available options.')

    optional.add_argument('--augustus_parameters', required=False, default='', dest='augustus_parameters',
                          help='Additional parameters for the fine-tuning of Augustus run. '
                               'For the species, do not use this option.\n'
                               'Use single quotes as follow: \'--param1=1 --param2=2\', '
                               'see Augustus documentation for available options.')

    optional.add_argument(
        '-t', '--tmp', metavar='PATH', required=False, dest='tmp', default='%s' % Analysis.TMP_DEFAULT,
        help='Where to store temporary files (Default: %s)' % Analysis.TMP_DEFAULT)

    optional.add_argument(
        '--limit', dest='limit', metavar='REGION_LIMIT', required=False, default=Analysis.REGION_LIMIT_DEFAULT,
        type=int, help='How many candidate regions to consider (default: %s)' % str(Analysis.REGION_LIMIT_DEFAULT))

    optional.add_argument(
        '--long', action='store_true', required=False, default=False, dest='long', help='Optimization mode Augustus '
                                                                                        'self-training (Default: Off) adds considerably to the run time, but can improve results for some non-model '
                                                                                        'organisms')

    optional.add_argument(
        '-q', '--quiet', dest='quiet', required=False, help='Disable the info logs, displays only errors',
        action="store_true")

    optional.add_argument(
        '-z', '--tarzip', dest='tarzip', required=False, help='Tarzip the output folders likely to '
                                                              'contain thousands of files',
        action="store_true")

    optional.add_argument('-v', '--version', action='version', help="Show this version and exit", version='BUSCO %s'
                                                                                                          % VERSION)

    optional.add_argument('-h', '--help', action="help", help="Show this help message and exit")

    args = vars(parser.parse_args(args))

    if args['quiet']:
        _logger.setLevel(logging.ERROR)

    _logger.debug('Args list is %s' % str(args))

    if len(args) == 1:
        parser.print_help()
        sys.exit(1)

    return args  # parse arguments


def _define_parameters(args):
    """
    This function defines the value of all parameters needed to start an analysis, \
    based on user provided and default values
    :param args: a dictionary having a key for each arguments, representing the user provided values
    :type args: dict
    :return: a dictionary having a key for each arguments, representing all values
    :rtype: dict
    :raises SystemExit: if the provided clade does not exist
    :raises SystemExit: if the provided mode does not exist
    :raises SystemExit: if the provided maximum number of regions is not an int between 1 and 20
    :raises SystemExit: if the provided input file does not exist
    """
    # Use an e-value cutoff of the default value, unless user has supplied a custom value using "-evalue float" option
    ev_cutoff = Analysis.EVALUE_DEFAULT  # default e-value cuttof
    if args['evalue'] and args['evalue'] != ev_cutoff:
        _logger.warning('You are using a custom e-value cutoff')
        ev_cutoff = args['evalue']
    maxflank = Analysis.MAX_FLANK
    region_limit = Analysis.REGION_LIMIT_DEFAULT
    if args['clade']:
        if args['clade'][-1] != '/':
            args['clade'] += '/'
        target_species = None
        clade_name = None
        domain = None
        dataset_creation_date = "N/A"
        dataset_nb_buscos = "N/A"
        dataset_nb_species = "N/A"
        # load the dataset config, or warn the user if not present
        try:
            target_species_file = open('%sdataset.cfg' % args['clade'])
            for l in target_species_file:
                if l.split("=")[0] == "name":
                    clade_name = l.strip().split("=")[1]
                elif l.split("=")[0] == "species":
                    target_species = l.strip().split("=")[1]
                elif l.split("=")[0] == "domain":
                    domain = l.strip().split("=")[1]
                elif l.split("=")[0] == "creation_date":
                    dataset_creation_date = l.strip().split("=")[1]
                elif l.split("=")[0] == "number_of_BUSCOs":
                    dataset_nb_buscos = l.strip().split("=")[1]
                elif l.split("=")[0] == "number_of_species":
                    dataset_nb_species = l.strip().split("=")[1]
            if domain != 'prokaryota' and domain != 'eukaryota':
                _logger.error('Corrupted dataset.cfg file: domain is %s, should be eukaryota or prokaryota' % domain)
                raise SystemExit

        except IOError:
            _logger.warning("The dataset you provided does not contain the file dataset.cfg, "
                            "likely because it is an old version. "
                            "Some parameters will be deduced from the dataset folder name")
    else:
        _logger.error('Please indicate the full path to a BUSCO clade dataset, example: -l /path/to/clade')
        raise SystemExit

    if not clade_name:  # 1.x datasets backward compatibility
        clade_name = args['clade'].strip('/').split('/')[-1].lower()
    if not domain:  # 1.x datasets backward compatibility
        if clade_name.startswith('bacteria'):
            domain = 'prokaryota'
        else:
            domain = 'eukaryota'

    # Use "generic" as the Augustus species unless user has specified the desired species metaparameters using the
    #  "-sp species" option
    if not args['species']:
        if not target_species:  # 1.x datasets backward compatibility
            if clade_name.startswith(('arthrop', 'examp')):
                target_species = 'fly'
            elif clade_name.startswith('vertebr'):
                target_species = 'human'
            elif clade_name.startswith('fung'):
                target_species = 'aspergillus_nidulans'
            elif clade_name.startswith('metazoa'):
                target_species = 'fly'
            elif clade_name.startswith('bacter'):
                target_species = 'E_coli_K12'
            elif clade_name.startswith('plant'):
                target_species = 'maize'
            elif clade_name.startswith('eukary'):
                target_species = 'fly'
            else:
                target_species = Analysis.SPECIES_DEFAULT
    else:
        target_species = args['species']

    _logger.info('The lineage dataset is: %s (%s)' % (clade_name, domain))

    # Set up the number of cores to be used
    # Augustus uses the python 'threading' library to be run in parallel, blast and HMMer allow this by default
    cpus = Analysis.CPUS_DEFAULT  # 1 core default
    if args['cpu']:
        cpus = args['cpu']

    # BUSCO mode (valid modes are genome, transcriptome and proteins)
    mode = args['mode']
    if mode == 'prot' or mode == 'proteins':
        mode = 'proteins'
    elif mode == 'geno' or mode == 'genome':
        mode = 'genome'
    elif mode == 'transcriptome' or mode == 'tran':
        mode = 'tran'
    else:
        _logger.error('Unknown mode specified * %s *, please check the documentation'
                      ' for valid modes.' % mode)
        raise SystemExit

    _logger.info('Mode is: %s' % mode)

    if mode == 'genome':
        if args['limit'] == 0 or args['limit'] > 20:
            _logger.error('Limit must be an integer between 1 and 20 (you have used: %s).' % args['limit'])
            raise SystemExit
        else:
            region_limit = args['limit']
            _logger.info('Maximum number of regions limited to: %s' % region_limit)

        # Fine tuning paramets for Augustus run
        # Example -a '--translation_table=6'
        if args['augustus_parameters']:
            augustus_parameters = args['augustus_parameters']
            _logger.info('The additional Augustus parameter(s) is/are: %s' % augustus_parameters)

    # Get the flank size
    # Minimum 5 Kbp and maximum 20 Kbp
    # Scaled as GenomeSize/50
    flank = None
    if mode == 'genome':  # scalled flanks
        try:
            f = open(args['in'])
            size = 0
            for line in f:
                if line.startswith('>'):
                    pass
                else:
                    size += len(line.strip())
            size /= 1000  # size in mb
            flank = int(size / 50)  # proportional flank size
            if flank < 5000:
                flank = 5000
            elif flank > maxflank:
                flank = maxflank
            f.close()
        except IOError:
            _logger.error('Impossible to read the fasta file %s ' % args['in'])
            raise SystemExit

    if '/' in args['abrev']:
        _logger.error('Please do not provide a full path as output (no slash), just a name. '
                      'Your entry for -o is %s\n\t\tThe results will be written in a subfolder of the current location '
                      'called run_name'
                      % args['abrev'])
        raise SystemExit

    return {"mode": mode, "target_species": target_species, "abrev": args['abrev'], "tmp": args['tmp'],
            "force": args['force'], "sequences": args['in'], "cpus": cpus, "clade_name": clade_name,
            "clade_path": args['clade'], "ev_cutoff": ev_cutoff, "domain": domain, "restart": args['restart'],
            "augustus_config_path": augustus_config_path, "tarzip": args['tarzip'],
            "region_limit": region_limit, "flank": flank, "long": args['long'],
            "dataset_creation_date": dataset_creation_date, "dataset_nb_species": dataset_nb_species,
            "dataset_nb_buscos": dataset_nb_buscos, "augustus_parameters": args['augustus_parameters']
            }


def _check_path_exist(path):
    """
    This function checks whether the provided path exists
    :param path: the path to be tested
    :type path: str
    :raises SystemExit: if the path cannot be reached
    """
    if not os.path.exists(path):
        _logger.error('Impossible to read %s' % path)
        raise SystemExit


def _set_rerun_busco_command(params):
    """
    This function sets the command line to call to reproduce this run
    :param params: the params provided by the user
    :type params: dict
    """
    global _rerun_cmd
    _rerun_cmd = 'python %s -i %s -o %s -l %s -m %s -c %s' % (__file__, params['sequences'], params['abrev'],
                                                              params['clade_path'], params['mode'],
                                                              str(params['cpus']))

    if params['long']:
        _rerun_cmd += ' --long'
    if params['region_limit'] != Analysis.REGION_LIMIT_DEFAULT:
        _rerun_cmd += ' --limit %s' % str(params['region_limit'])
    if params['tmp'] != './tmp':
        _rerun_cmd += ' -t %s' % params['tmp']
    if params['target_species']:
        _rerun_cmd += ' -sp %s' % params['target_species']
    if params['ev_cutoff'] != Analysis.EVALUE_DEFAULT:
        _rerun_cmd += ' -e %s' % str(params['ev_cutoff'])
    if params['tarzip']:
        _rerun_cmd += ' -z'
    if params['augustus_parameters']:
        _rerun_cmd += ' --augustus_parameters \'%s\'' % params['augustus_parameters']


def main(args, output_dir=None, show_thread=False):
    """
    This function runs a BUSCO analysis according to the provided parameters.
    See the help for more details:
    ``python BUSCO.py -h``

    :param show_thread:  a bool to append or not the thread name in the logs, e.g. INFO:BUSCO.py:thread_name
    :type show_thread: bool
    :raises SystemExit: if any errors occur
    """
    global ROOT_FOLDER
    ROOT_FOLDER = output_dir or ROOT_FOLDER

    start_time = time.time()

    # 1) Set-up the parameters
    args = _parse_args(args)
    output_dir = os.path.join(ROOT_FOLDER, 'run_%s' % args['abrev'])
    summary_path = os.path.join(output_dir, 'short_summary_%s.txt' % args['abrev'])
    if is_non_empty_file(summary_path):
        _logger.info('Using existing BUSCO files for ' + args['abrev'] + '...')
        return summary_path
    if not os.path.exists(output_dir):
        os.makedirs(output_dir)
    _log.set_up_file_handler(ROOT_FOLDER)

    if show_thread:
        _log.add_thread_info()

    try:

        _log.add_blank_line()
        _logger.info('****************** Start a BUSCO %s analysis, current time: %s ******************'
                     % (VERSION, time.strftime("%m/%d/%Y %H:%M:%S")))
        _check_path_exist(args['in'])
        _check_path_exist(args['clade'])
        params = _define_parameters(args)
        _set_rerun_busco_command(params)

        _logger.info('To reproduce this run: %s' % _rerun_cmd)

        # 2) init the appropriate analysis instance
        analysis = None
        if params['mode'] == 'proteins':
            analysis = GeneSetAnalysis(params)
        elif params['mode'] == 'genome':
            analysis = GenomeAnalysis(params)
        elif params['mode'] == 'tran':
            analysis = TranscriptomeAnalysis(params)

        # 3) Check dependencies
        _logger.info('Check dependencies...')
        analysis.check_dependencies()

        # 4) Check invalid header characters
        _logger.info('Check input file...')
        for line in open(params['sequences']):
            if line.startswith('>'):
                Analysis.check_fasta_header(line)

        # 5) Run the analysis
        analysis.run_analysis()

        _log.add_blank_line()
        if not _log.has_warning():
            _logger.info('BUSCO analysis done. Total running time: %s seconds' % str(time.time() - start_time))
        else:
            _logger.info('BUSCO analysis done with WARNING(s). Total running time: %s seconds'
                         % str(time.time() - start_time))
        _logger.info('Results written in %s\n' % analysis.mainout)
        return summary_path

    except SystemExit:
        _log.add_blank_line()
        _logger.error('BUSCO analysis failed !')
        _logger.info(
            'Check the logs, read the user guide, if you still need technical support, then please contact %s\n'
            % CONTACT)
        return None

    except KeyboardInterrupt:
        _log.add_blank_line()
        _logger.error('A signal was sent to kill the process')
        _logger.error('BUSCO analysis failed !')
        _logger.info(
            'Check the logs, read the user guide, if you still need technical support, then please contact %s\n'
            % CONTACT)
        return None

    except BaseException:
        _log.add_blank_line()
        exc_type, exc_value, exc_traceback = sys.exc_info()
        _logger.critical('Unhandled exception occurred: %s\n'
                         % repr(traceback.format_exception(exc_type, exc_value, exc_traceback)))
        _logger.error('BUSCO analysis failed !')
        _logger.info(
            'Check the logs, read the user guide, if you still need technical support, then please contact %s\n'
            % CONTACT)
        return None


# Entry point
if __name__ == "__main__":
    main(sys.argv)