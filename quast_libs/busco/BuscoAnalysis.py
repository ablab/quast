#!/usr/bin/env python
# coding: utf-8
"""
.. module:: BuscoAnalysis
   :synopsis: BuscoAnalysis implements general BUSCO analysis specifics
.. versionadded:: 3.0.0
.. versionchanged:: 3.0.1

Copyright (c) 2016-2017, Evgeny Zdobnov (ez@ezlab.org)
Licensed under the MIT license. See LICENSE.md file.

""" 

from abc import ABCMeta, abstractmethod
from quast_libs.busco.BuscoConfig import BuscoConfig
import copy
import inspect
import os
from quast_libs.busco.pipebricks.PipeHelper import Analysis
from quast_libs.busco.pipebricks.PipeLogger import PipeLogger
from quast_libs.busco.pipebricks.Toolset import Tool
import random
import subprocess
from collections import deque
# from overrides import overrides  # useful fro dev, but don't want all user to install this

try:
    import queue as Queue
except ImportError:
    import Queue  # Python 2.7

try:
    from configparser import NoOptionError
    from configparser import NoSectionError
except ImportError:
    from ConfigParser import NoOptionError  # Python 2.7
    from ConfigParser import NoSectionError  # Python 2.7


class BuscoAnalysis(Analysis):
    """
    This abstract class defines methods required for most of BUSCO analyses and has to be extended
    by each specific analysis class
    It extends the pipebricks.PipeHelper Analysis
    """

    # declare a metaclass ABCMeta, which means that this class is abstract
    __metaclass__ = ABCMeta

    _logger = PipeLogger.get_logger(__name__)

    #
    # magic or public, meant to be accessed by external scripts [instance]
    #

    def __init__(self, config):
        """
        :param config: Values of all parameters to be used during the analysis
        :type config: BuscoConfig
        """

        # 1) load parameters
        # 2) load and validate tools
        # 3) check data and dataset integrity
        # 4) Ready for analysis
        # See also parent __init__
        super(BuscoAnalysis, self).__init__(config)  # _init tool called here
        try:
            # this both detect if a subclass did not properly declare the mode before calling parent __init__
            # and maintains PEP8 compliance checks
            if self._mode is None:
                self._mode = ''
        except AttributeError as e:
            BuscoAnalysis._logger.critical(e)
            raise SystemExit
        self._random = "_"+str(random.getrandbits(32))  # to have a unique component for temporary file names
        # runtime directory
        self.mainout = None
        self._root_folder = config.get('busco', 'out_path')
        self._out = config.get('busco', 'out')
        self._tmp = config.get('busco', 'tmp_path')
        self._force = config.getboolean('busco', 'force')
        self._long = config.getboolean('busco', 'long')
        self._restart = config.getboolean('busco', 'restart')
        self._cpus = config.getint('busco', 'cpu')
        self._blast_single_core = config.getboolean('busco', 'blast_single_core')
        self._sequences = config.get('busco', 'in')
        self._lineage_path = config.get('busco', 'lineage_path')
        self._lineage_name = config.get('busco', 'clade_name')
        self._domain = config.get('busco', 'domain')
        self._ev_cutoff = config.getfloat('busco', 'evalue')
        self._region_limit = config.getint('busco', 'limit')
        self._has_variants_file = False
        self._missing_busco_list = []
        self._fragmented_busco_list = []
        # -- end --
        self._tarzip = config.getboolean('busco', 'tarzip')
        self._dataset_creation_date = config.get('busco', 'dataset_creation_date')
        self._dataset_nb_species = config.get('busco', 'dataset_nb_species')
        self._dataset_nb_buscos = config.get('busco', 'dataset_nb_buscos')
        #
        self._totalbuscos = 0
        self._total = 0
        self._cutoff_dictionary = {}
        self._thread_list = None
        self._no_prediction = None
        self._exit_flag = None
        self._queue_lock = None
        self._work_queue = None
        self._location_dic = {}
        self._single_copy_files = None
        self._contig_length = {}

        # check data integrity
        self._check_dataset()
        BuscoAnalysis._logger.info('Check input file...')
        for line in open(self._sequences):
            if line.startswith('>'):
                self._check_fasta_header(line)

        self._set_rerun_busco_command()
        BuscoAnalysis._logger.info('To reproduce this run: %s' % self._rerun_cmd)

    @abstractmethod
    # @override
    def run_analysis(self):
        """
        Abstract method, override to call all needed steps for running the child analysis.
        """
        BuscoAnalysis._logger.info('Mode is: %s' % self._mode)
        BuscoAnalysis._logger.info('The lineage dataset is: %s (%s)' % (self._lineage_name, self._domain))
        # create the run(main) and the temporary directories
        self._create_main_and_tmp_dirs()

    def cleanup(self):
        """
        This function cleans temporary files. \
        It has to be overriden by subclasses when needed
        """
        self._p_open(['rm', '%stemp_%s%s' % (self._tmp, self._out, self._random)], 'bash', shell=False)

    def get_checkpoint(self, reset_random_suffix=False):
        """
        This function return the checkpoint if the checkpoint.tmp file exits or None if absent
        :param reset_random_suffix: to tell whether to reset the self._random
        value with the one found the checkpoint
        :type reset_random_suffix: bool
        :return: the checkpoint name
        :rtype: int
        """
        if os.path.exists('%scheckpoint.tmp' % self.mainout):
            line = open('%scheckpoint.tmp' % self.mainout, 'r').readline()
            if reset_random_suffix:
                BuscoAnalysis._logger.debug('Resetting random suffix to %s' % self._random)
                self._random = line.split('.')[-1]
            return int(int(line.split('.')[0]))
        else:
            return None

    #
    # public, meant to be accessed by external scripts [class]
    #

    @staticmethod
    def get_analysis(config):
        """
        This method returns the appropriate instance for the BUSCO mode specifed in parameters
        :param config:
        :type config: BuscoConfig
        :return:
        """
        # Need to import the sublcasses here, not sure this is a good design pattern then. (?)
        from quast_libs.busco.GenomeAnalysis import GenomeAnalysis
        from quast_libs.busco.TranscriptomeAnalysis import TranscriptomeAnalysis
        from quast_libs.busco.GeneSetAnalysis import GeneSetAnalysis
        mode = config.get('busco', 'mode')
        if mode == 'genome' or mode == 'geno':
            return GenomeAnalysis(config)
        elif mode == 'transcriptome' or mode == 'tran':
            return TranscriptomeAnalysis(config)
        elif mode == 'proteins' or mode == 'prot':
            return GeneSetAnalysis(config)
        else:
            BuscoAnalysis._logger.error('Unknown mode %s, use genome, transcriptome, or proteins', mode)
            raise SystemExit

    #
    # method that should be used as if protected, for internal use [instance]
    # to move to public and rename if meaningful
    #

    # @override
    def _init_tools(self):
        """
        Init the tools needed for the analysis
        """
        BuscoAnalysis._logger.info('Init tools...')
        self._hmmer = Tool('hmmsearch', self._params)
        self._mkblast = Tool('makeblastdb', self._params)
        self._tblastn = Tool('tblastn', self._params)
        BuscoAnalysis._logger.info('Check dependencies...')
        self._check_tool_dependencies()

    def _check_tool_dependencies(self):
        """
        check dependencies on tools
        :raises SystemExit: if a Tool is not available
        """
        # check 'tblastn' command availability
        if not Tool.check_tool_available('tblastn', self._params):
            BuscoAnalysis._logger.error(
                '\"tblastn\" is not accessible, please '
                'add or modify its path in the config file. Do not include the commmand in the path !')
            raise SystemExit

        # check 'makeblastdb' command availability
        if not Tool.check_tool_available('makeblastdb', self._params):
            BuscoAnalysis._logger.error(
                '\"makeblastdb\" is not accessible, please '
                'add or modify its path in the config file. Do not include the commmand in the path !')
            raise SystemExit

        # check 'hmmersearch' command availability
        if not Tool.check_tool_available('hmmsearch', self._params):
            BuscoAnalysis._logger.error(
                '\"hmmsearch\" is not accessible, '
                'add or modify its path in the config file. Do not include the commmand in the path !')
            raise SystemExit

        # check hmm version
        if self._get_hmmer_version(self._hmmer.cmd[0]) >= BuscoConfig.HMMER_VERSION:
            pass
        else:
            BuscoAnalysis._logger.error(
                'HMMer version detected is not supported, please use HMMer '
                ' v.%s +' % BuscoConfig.HMMER_VERSION)
            raise SystemExit

    def _check_fasta_header(self, header):
        """
        This function checks problematic characters in fasta headers,
        and warns the user and stops the execution
        :param header: a fasta header to check
        :type header: str
        :raises SystemExit: if a problematic character is found
        """
        for char in BuscoConfig.FORBIDDEN_HEADER_CHARS:
            if char in header:
                BuscoAnalysis._logger.error(
                    'The character \'%s\' is present in the fasta header %s, '
                    'which will crash BUSCO. Please clean the header of your '
                    'input file.' % (char, header.strip()))
                raise SystemExit

        for char in BuscoConfig.FORBIDDEN_HEADER_CHARS_BEFORE_SPLIT:
            if char in header.split()[0]:
                BuscoAnalysis._logger.error(
                    'The character \'%s\' is present in the fasta header %s, '
                    'which will crash Reader. Please clean the header of your'
                    ' input file.' % (char, header.split()[0].strip()))
                raise SystemExit

        if header.split()[0] == '>':
            BuscoAnalysis._logger.error(
                'A space is present in the fasta header %s, directly after '
                '\'>\' which will crash Reader. Please clean the header of '
                'your input file.' % (header.strip()))
            raise SystemExit

    def _check_dataset(self):
        """
        Check the input dataset integrity, both files and folder are available
        :raises SystemExit: if the dataset miss files or folders
        """
        # hmm folder
        flag = False
        for dirpath, dirnames, files in os.walk('%shmms' % self._lineage_path):
            if files:
                flag = True
        if not flag:
            BuscoAnalysis._logger.error(
                'The dataset you provided lacks hmm profiles in %shmms' %
                self._lineage_path)
            raise SystemExit
        # note: score and length cutoffs are checked when read,
        # see _load_scores and _load_lengths

    @abstractmethod
    def _run_hmmer(self):
        """
        This function runs hmmsearch.
        """
        pass

    def _write_output_header(self, out):
        """
        This function adds a header to the provided file
        :param out: a file to which the header will be added
        :type out: file
        """
        out.write(
            '# BUSCO version is: %s \n# The lineage dataset is: %s (Creation '
            'date: %s, number of species: %s, number of BUSCOs: %s)\n' %
            (BuscoConfig.VERSION, self._lineage_name, self._dataset_creation_date,
             self._dataset_nb_species, self._dataset_nb_buscos))
        out.write('# To reproduce this run: %s\n#\n' % self._rerun_cmd)

    def _extract_missing_and_frag_buscos_ancestral(self, ancestral_variants=False):
        """
        This function extracts from the file ancestral the sequences
        that match missing or fragmented buscos
        :param ancestral_variants: tell whether to use
        the ancestral_variants file
        :type ancestral_variants: bool
        """
        if self._has_variants_file:
            BuscoAnalysis._logger.info(
                'Extracting missing and fragmented buscos from '
                'the ancestral_variants file...')
        else:
            BuscoAnalysis._logger.info(
                'Extracting missing and fragmented buscos from '
                'the ancestral file...')

        if ancestral_variants:
            ancestral = open('%sancestral_variants' % self._lineage_path, 'r')
            output = open(
                '%sblast_output/missing_and_frag_ancestral_variants' %
                self.mainout, 'w')
        else:
            ancestral = open('%sancestral' % self._lineage_path, 'r')
            output = open(
                '%sblast_output/missing_and_frag_ancestral' % self.mainout,
                'w')

        result = ''

        buscos_to_retrieve = \
            self._missing_busco_list + self._fragmented_busco_list
        buscos_retrieved = []
        add = False
        for line in ancestral:
            if line.startswith('>'):
                if ancestral_variants:
                    line = '_'.join(line.split("_")[:-1])
                    # This pattern can support name like EOG00_1234_1
                busco_id = line.strip().strip('>')
                if busco_id in buscos_to_retrieve:
                    BuscoAnalysis._logger.debug('Found contig %s' % busco_id)
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
                BuscoAnalysis._logger.warning(
                    'The busco id(s) %s were not found in the '
                    'ancestral_variants file' %
                    list(set(buscos_to_retrieve) - set(buscos_retrieved)))
            else:
                BuscoAnalysis._logger.warning(
                    'The busco id(s) %s were not found in the ancestral file' %
                    list(set(buscos_to_retrieve) - set(buscos_retrieved)))

        output.write(result)
        output.close()
        ancestral.close()
        output.close()

    def _produce_short_summary(self):
        """
        This function reads the result files and
        produces the final short summary file
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
        # list of BUSCO ids
        self._fragmented_busco_list = results_from_hmmer[4]
        self._single_copy_files = results_from_hmmer[5]

        summary_file = open('%sshort_summary_%s.txt' % (self.mainout,
                                                        self._out), 'w')
        self._write_output_header(summary_file)
        summary_file.write(
            '# Summarized benchmarking in BUSCO notation for file %s\n# BUSCO '
            'was run in mode: %s\n\n' % (self._sequences, self._mode))
        s_percent = round((single_copy/float(self._totalbuscos))*100, 1)
        d_percent = round((multi_copy/float(self._totalbuscos))*100, 1)
        f_percent = round((only_fragments/float(self._totalbuscos))*100, 1)
        BuscoAnalysis._logger.info('Results:')
        out_line = ('\tC:%s%%[S:%s%%,D:%s%%],F:%s%%,M:%s%%,n:%s\n\n' %
                    (round(s_percent + d_percent, 1), s_percent, d_percent,
                     f_percent,
                     round(100 - s_percent - d_percent - f_percent, 1),
                     self._totalbuscos))
        summary_file.write(out_line)
        BuscoAnalysis._logger.info(out_line.replace('\t', '').strip())
        out_line = ('\t%s\tComplete BUSCOs (C)\n' % (single_copy + multi_copy))
        summary_file.write(out_line)
        BuscoAnalysis._logger.info(out_line.replace('\t', ' ').strip())
        out_line = ('\t%s\tComplete and single-copy BUSCOs (S)\n' %
                    single_copy)
        summary_file.write(out_line)
        BuscoAnalysis._logger.info(out_line.replace('\t', ' ').strip())
        out_line = ('\t%s\tComplete and duplicated BUSCOs (D)\n' % multi_copy)
        summary_file.write(out_line)
        BuscoAnalysis._logger.info(out_line.replace('\t', ' ').strip())
        out_line = ('\t%s\tFragmented BUSCOs (F)\n' % only_fragments)
        summary_file.write(out_line)
        BuscoAnalysis._logger.info(out_line.replace('\t', ' ').strip())
        out_line = ('\t%s\tMissing BUSCOs (M)\n' %
                    str(self._totalbuscos - single_copy - multi_copy -
                        only_fragments))
        summary_file.write(out_line)
        BuscoAnalysis._logger.info(out_line.replace('\t', ' ').strip())
        out_line = ('\t%s\tTotal BUSCO groups searched\n' % self._totalbuscos)
        summary_file.write(out_line)
        BuscoAnalysis._logger.info(out_line.replace('\t', ' ').strip())
        summary_file.close()

    def _load_score(self):
        """
        This function loads the score cutoffs file
        :raises SystemExit: if the scores_cutoff file cannot be read
        """
        try:
            # open target scores file
            score_file = open('%sscores_cutoff' % self._lineage_path)
        except IOError:
            BuscoAnalysis._logger.error(
                'Impossible to read the scores in %sscores_cutoff' %
                self._lineage_path)
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
            f = open('%slengths_cutoff' % self._lineage_path)
        except IOError:
            BuscoAnalysis._logger.error(
                'Impossible to read the lengths in %slengths_cutoff' %
                self._lineage_path)
            raise SystemExit

        for line in f:
            line = line.strip().split()

            leng_dic[line[0]] = float(line[3])  # legacy
            sd_dic[line[0]] = float(line[2])  # legacy

            self._cutoff_dictionary[line[0]]['sigma'] = float(line[2])
            # there is an arthropod profile with sigma 0
            # that causes a crash on divisions
            if float(line[2]) == 0.0:
                self._cutoff_dictionary[line[0]]['sigma'] = 1

            self._cutoff_dictionary[line[0]]['length'] = float(line[3])
        f.close()

    def _parse_hmmer(self, hmmer_results_files):
        """
        This function parses the hmmsearch output files and produces
        the full_table output file
        :param hmmer_results_files: the list of all output files
        :type hmmer_results_files: list
        """
        # TODO: replace self._mode ==... by a proper parent-child behavior
        # that overrides code when needed

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
                    if bit_score >= \
                            self._cutoff_dictionary[busco_query]['score']:
                        if prot_id not in hit_dic.keys():
                            hit_dic[prot_id] = [[hmm_start, hmm_end,
                                                 bit_score]]
                        else:
                            hit_dic[prot_id].append([hmm_start, hmm_end,
                                                     bit_score])
            f.close()

            length = self._measuring(hit_dic)

            length_count = 0

            if busco_query:
                if busco_query not in everything:
                    everything[busco_query] = hit_dic
                else:
                    for part in hit_dic:
                        everything[busco_query][part] = hit_dic[part]

                for hit in hit_dic:
                    everything[busco_query][hit][0].append(
                        length[length_count])
                    length_count += 1
                # classify genes using sigmas
                for entry in everything[busco_query]:
                    size = everything[busco_query][entry][0][3]
                    sigma = (self._cutoff_dictionary[busco_query]['length'] -
                             size) / \
                        self._cutoff_dictionary[busco_query]['sigma']
                    everything[busco_query][entry][0].append(sigma)
                    everything[busco_query][entry][0].append(file_name)

        # REFINE CLASSIFICATION

        # separate complete into multi and single-copy, and keep gene
        # over 2 sigma in a separate dict
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
        is_complete = self._filter_multi_match_genes(is_complete)
        is_very_large = self._filter_multi_match_genes(is_very_large)
        # filter duplicated gene that have a bad ratio compared to
        # the top scoring match
        is_complete = self._remove_bad_ratio_genes(is_complete)
        is_very_large = self._remove_bad_ratio_genes(is_very_large)

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

        # consider the very large genes as true findings only
        # if there is no BUSCO < 2 sigma already found.
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
                    # best score
                    if is_fragment[entity][fragment_key][2] > \
                            is_fragment[entity][best_fragment_key][2]:
                        best_fragment_key = fragment_key
                fg_count += 1
                the_fg[entity] = {best_fragment_key: is_fragment[entity]
                                  [best_fragment_key]}

        sc_count = len(the_sc)
        mc_count = len(the_mc)

        env.append(sc_count)
        env.append(mc_count)
        env.append(fg_count)

        out = open('%sfull_table_%s.tsv' % (self.mainout, self._out), 'w')
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

                if self._mode == 'proteins' or self._mode == 'transcriptome':
                    out_lines.append('%s\tComplete\t%s\t%s\t%s\n' %
                                     (entity, self._reformats_seq_id(seq_id),
                                      bit_score, seq_len))
                elif self._mode == 'genome':
                    scaff = self._split_seq_id(seq_id)
                    out_lines.append(
                        '%s\tComplete\t%s\t%s\t%s\t%s\t%s\n' %
                        (entity, self._reformats_seq_id(scaff['id']),
                         scaff['start'], scaff['end'], bit_score, seq_len))
                    csc[entity] = seq_id
                else:
                    BuscoAnalysis._logger.debug(self._mode)
                    raise SystemExit

        for entity in the_mc:
            for seq_id in the_mc[entity]:
                bit_score = the_mc[entity][seq_id][2]
                seq_len = the_mc[entity][seq_id][3]

                not_missing.append(entity)

                if self._mode == 'proteins' or self._mode == 'transcriptome':
                    out_lines.append('%s\tDuplicated\t%s\t%s\t%s\n' %
                                     (entity, self._reformats_seq_id(seq_id),
                                      bit_score, seq_len))
                elif self._mode == 'genome':
                    scaff = self._split_seq_id(seq_id)
                    out_lines.append(
                        '%s\tDuplicated\t%s\t%s\t%s\t%s\t%s\n' %
                        (entity, self._reformats_seq_id(scaff['id']),
                         scaff['start'], scaff['end'], bit_score, seq_len))

        for entity in the_fg:
            for seq_id in the_fg[entity]:
                bit_score = the_fg[entity][seq_id][2]
                seq_len = the_fg[entity][seq_id][3]

                not_missing.append(entity)
                fragmented.append(entity)

                if self._mode == 'proteins' or self._mode == 'transcriptome':
                    out_lines.append(
                        '%s\tFragmented\t%s\t%s\t%s\n' %
                        (entity, self._reformats_seq_id(seq_id), bit_score,
                         seq_len))
                elif self._mode == 'genome':
                    scaff = self._split_seq_id(seq_id)
                    out_lines.append(
                        '%s\tFragmented\t%s\t%s\t%s\t%s\t%s\n' %
                        (entity, self._reformats_seq_id(scaff['id']),
                         scaff['start'], scaff['end'], bit_score, seq_len))

        missing = []
        miss_file = open(
            '%smissing_busco_list_%s.tsv' % (self.mainout, self._out), 'w')
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

    def _create_main_and_tmp_dirs(self):
        """
        This function creates the run(main) and the temporary directories
        :raises SystemExit: if a run with the same name already exists and
        the force option is not set
        :raises SystemExit: if the user cannot write in the tmp directory
        """
        # final output directory
        self.mainout = self._root_folder+'run_%s/' % self._out
        # complain about the -r option if there is no checkpoint.tmp file
        if not self.get_checkpoint() and self._restart:
            BuscoAnalysis._logger.warning(
                'This is not an uncompleted run that can be restarted')
            self._restart = False

        if not os.path.exists(self.mainout) and self._out:
            try:
                os.makedirs('%s' % self.mainout)
            except OSError:
                BuscoAnalysis._logger.error(
                    'Cannot write to the output directory, please make sure '
                    'you have write permissions to %s' % self.mainout)
                raise SystemExit
        else:
            if not self._force and not self._restart:
                restart_msg1 = ''
                restart_msg2 = ''
                if self.get_checkpoint():
                    restart_msg1 = ' and seems uncompleted'
                    restart_msg2 = ', or use the -r option to continue '\
                                   'an uncompleted run'
                BuscoAnalysis._logger.error(
                    'A run with that name already exists%s...\n\tIf you '
                    'are sure you wish to overwrite existing files, please '
                    'use the -f option%s' % (restart_msg1, restart_msg2))

                raise SystemExit
            elif not self._restart:
                BuscoAnalysis._logger.info(
                    'Delete the current result folder and start a new run')
                self._p_open(['rm -rf %s*' % self.mainout], 'bash', shell=True)

        # create the tmp directory
        try:
            if self._tmp != './':
                if not os.path.exists(self._tmp):
                    os.makedirs(self._tmp)
                if self._tmp[-1] != '/':
                    self._tmp += '/'
            BuscoAnalysis._logger.info('Temp directory is %s' % self._tmp)
        except OSError:
            BuscoAnalysis._logger.error(
                'Cannot write to the temp directory, please make sure '
                'you have write permissions to %s' % self._tmp)
            raise SystemExit

        if not os.access(self._tmp, os.W_OK):
                BuscoAnalysis._logger.error(
                    'Cannot write to the temp directory, please make sure '
                    'you have write permissions to %s' % self._tmp)
                raise SystemExit

    def _set_checkpoint(self, nb=None):
        """
        This function update the checkpoint file with the provided id or delete
        it if none is provided
        :param nb: the id of the checkpoint
        :type nb: int
        """
        if nb:
            open('%scheckpoint.tmp' % self.mainout, 'w').write('%s.%s.%s' %
                                                               (nb, self._mode,
                                                                self._random))
        else:
            if os.path.exists('%scheckpoint.tmp' % self.mainout):
                os.remove('%scheckpoint.tmp' % self.mainout)

    def _run_tblastn(self, missing_and_frag_only=False, ancestral_variants=False):
        """
        This function runs tblastn
        :param missing_and_frag_only: to tell whether to blast only missing
        and fragmented buscos
        :type missing_and_frag_only: bool
        :param ancestral_variants: to tell whether to use the ancestral_variants file
        :type ancestral_variants: bool
        """
        if ancestral_variants:
            ancestral_sfx = '_variants'
        else:
            ancestral_sfx = ''

        if missing_and_frag_only:
            self._extract_missing_and_frag_buscos_ancestral(ancestral_variants)
            output_suffix = '_missing_and_frag_rerun'
            query_file = \
                '%sblast_output/missing_and_frag_ancestral%s' % (self.mainout,
                                                                 ancestral_sfx)
        else:
            output_suffix = ''
            query_file = '%sancestral%s' % (self._lineage_path, ancestral_sfx)

        if not missing_and_frag_only:

            BuscoAnalysis._logger.info('Create blast database...')
            blast_job = self._mkblast.create_job(BuscoAnalysis._logger)

            blast_job.add_parameter('-in')
            blast_job.add_parameter('%s' % self._sequences)
            blast_job.add_parameter('-dbtype')
            blast_job.add_parameter('nucl')
            blast_job.add_parameter('-out')
            blast_job.add_parameter('%s%s%s' % (self._tmp, self._out, self._random))

            self._mkblast.run_jobs(self._cpus, BuscoAnalysis._logger)

            if not os.path.exists('%sblast_output' % self.mainout):
                os.makedirs('%sblast_output' % self.mainout)

        BuscoAnalysis._logger.info(
            'Running tblastn, writing output to '
            '%sblast_output/tblastn_%s%s.tsv...' % (self.mainout, self._out,
                                                    output_suffix))

        tblastn_job = self._tblastn.create_job()

        tblastn_job.add_parameter('-evalue')
        tblastn_job.add_parameter(str(self._ev_cutoff))
        tblastn_job.add_parameter('-num_threads')
        if not self._blast_single_core:
            tblastn_job.add_parameter(str(self._cpus))
        else:
            tblastn_job.add_parameter('1')
        tblastn_job.add_parameter('-query')
        tblastn_job.add_parameter(query_file)
        tblastn_job.add_parameter('-db')
        tblastn_job.add_parameter('%s%s%s' % (self._tmp, self._out, self._random))
        tblastn_job.add_parameter('-out')
        tblastn_job.add_parameter('%sblast_output/tblastn_%s%s.tsv' % (self.mainout, self._out, output_suffix))
        tblastn_job.add_parameter('-outfmt')
        tblastn_job.add_parameter('7')

        self._tblastn.run_jobs(1, BuscoAnalysis._logger)  # tblastn manages available cpus by itself

        # check that blast worked
        if not os.path.exists('%sblast_output/tblastn_%s%s.tsv' % (self.mainout, self._out, output_suffix)):
            BuscoAnalysis._logger.error('tblastn failed !')
            raise SystemExit
        # check that the file is not truncated
        try:
            if "processed" not in open('%sblast_output/tblastn_%s%s.tsv' % (self.mainout, self._out,
                                                                            output_suffix), 'r').readlines()[-1]:
                BuscoAnalysis._logger.error('tblastn has ended prematurely '
                                            '(the result file lacks the expected final line), '
                                            'which will produce incomplete results in the next steps ! '
                                            'This problem likely appeared in blast+ 2.4 and '
                                            'seems not fully fixed in 2.6. '
                                            'It happens only when using multiple cores. '
                                            'You can use a single core (-c 1) or downgrade to blast+ 2.2.x, '
                                            'a safe choice regarding this issue. '
                                            'See blast+ documentation for more information.')
                raise SystemExit
        except IndexError:
            # if the tblastn result file is empty, for example in phase 2
            # if 100% was found in phase 1
            pass

    def _set_rerun_busco_command(self):
        """
        This function sets the command line to call to reproduce this run
        """

        frame = inspect.stack()[-1]
        if inspect.getmodule(frame[0]) is not None:
            entry_point = inspect.getmodule(frame[0]).__file__
        else:
            entry_point = '<undefined_entry_point>'

        self._rerun_cmd = 'python %s -i %s -o %s -l %s -m %s -c %s' % (entry_point, self._sequences, self._out,
                                                                       self._lineage_path, self._mode, self._cpus)

        if self._long:
            self._rerun_cmd += ' --long'
        if self._region_limit != BuscoConfig.DEFAULT_ARGS_VALUES['limit']:
            self._rerun_cmd += ' --limit %s' % self._region_limit
        if self._tmp != BuscoConfig.DEFAULT_ARGS_VALUES['tmp_path']:
            self._rerun_cmd += ' -t %s' % self._tmp
        if self._ev_cutoff != BuscoConfig.DEFAULT_ARGS_VALUES['evalue']:
            self._rerun_cmd += ' -e %s' % self._ev_cutoff
        if self._tarzip:
            self._rerun_cmd += ' -z'

    def _run_tarzip_hmmer_output(self):
        """
        This function tarzips 'hmmer_output' results folder
        """
        self._p_open(['tar', '-C', '%s' % self.mainout, '-zcf', '%shmmer_output.tar.gz' % self.mainout,
                      'hmmer_output', '--remove-files'], 'bash', shell=False)

    def _write_full_table_header(self, out):
        """
        This function adds a header line to the full table file
        :param out: a full table file
        :type out: file
        """
        out.write('# Busco id\tStatus\tSequence\tScore\tLength\n')

    def _reformats_seq_id(self, seq_id):
        """
        This function reformats the sequence id to its original values,
        if it was somehow modified during the process
        It has to be overriden by subclasses when needed
        :param seq_id: the seq id to reformats
        :type seq_id: str
        :return: the reformatted seq_id
        :rtype: str
        """
        return seq_id

    def _split_seq_id(self, seq_id):
        """
        This function split the provided seq id into id, start and stop
        :param seq_id: the seq id to split
        :type seq_id: str
        :return: a dict containing the id, the start, the end found in seq_id
        :rtype: dict
        """
        # -2,-1 instead of 0,1, if ':' in the fasta header, same for [,]
        name = seq_id.replace(']', '').split('[')[-1].split(':')[-2]
        start = \
            seq_id.replace(']', '').split('[')[-1].split(':')[-1].split('-')[0]
        end = \
            seq_id.replace(']', '').split('[')[-1].split(':')[-1].split('-')[1]
        return {'id': name, 'start': start, 'end': end}
    
    def _remove_bad_ratio_genes(self, original):
        """
        This function removes duplicate positive results if the score is above
        a 0.85 threshold compared to the top scoring match
        :param original: a dict with BUSCO as key, and a dict of matching genes
        as values
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
                if original[k][k2][2] < max_score*ratio:
                    del filtered[k][k2]
                    if len(filtered[k]) == 0:
                        del filtered[k]

        return filtered

    def _filter_multi_match_genes(self, original):
        """
        This function identifies genes that match the same BUSCO, and keeps
        the one with the best score only
        :param original: a dict with BUSCO as key, and a dict of matching genes
         as values
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

    def _measuring(self, nested):
        """
        :param nested:
        :type nested:
        :return:
        :rtype:
        """
        # TODO comment
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

    def _get_hmmer_version(self, hmmer_exec_path):
        """
        check the Tool has the correct version
        :raises SystemExit: if the version is not correct
        """
        hmmer_version = 0.0
        try:
            hmmer_version = subprocess.check_output('%s -h' % hmmer_exec_path,
                                                    shell=True)
            hmmer_version = hmmer_version.decode('utf-8')
            hmmer_version = hmmer_version.split('\n')[1].split()[2]
            hmmer_version = float(hmmer_version[:3])
        except ValueError:
            # to avoid a crash with super old version and notify the user,
            # will be useful
            hmmer_version = subprocess.check_output('%s -h' % hmmer_exec_path,
                                                    shell=True)
            hmmer_version = hmmer_version.decode('utf-8')
            hmmer_version = hmmer_version.split('\n')[1].split()[1]
            hmmer_version = float(hmmer_version[:3])
        finally:
            return hmmer_version

    def _check_overlap(self, a, b):
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

    def _define_boundary(self, a, b):
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
            # append left, IF entry is the first one; otherwise put into
            # the proper position
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

    def _gargantua(self, deck):
        """
        :param deck:
        :type deck: list
        :return:
        :rtype: int
        """
        # TODO comment
        total = 0
        for entry in deck:
            total += entry[1] - entry[0]
        return total

    def _p_open(self, cmd, name, shell=False):
        """
        This function call subprocess.Popen for the provided command and
        logs the results with the provided name
        NOTE: to replace by pipebricks.Tool when possible
        :param cmd: the command to execute
        :type cmd: list
        :param name: the name to use in the log
        :type name: str
        :param shell: whether to use the shell parameter to Popen.
        Needed if wildcard charcter used (*?). See on web
        :type shell: bool
        """
        process = subprocess.Popen(cmd, stderr=subprocess.PIPE,
                                   stdout=subprocess.PIPE, shell=shell)
        process_out = process.stderr.readlines() + process.stdout.readlines()
        for line in process_out:
            BuscoAnalysis._logger.info_external_tool(name, line.decode("utf-8").strip())
