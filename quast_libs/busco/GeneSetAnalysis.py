#!/usr/bin/env python3
# coding: utf-8
"""
.. module:: GeneSetAnalysis
   :synopsis: GeneSetAnalysis implements genome analysis specifics
.. versionadded:: 3.0.0
.. versionchanged:: 3.0.0

Copyright (c) 2016-2017, Evgeny Zdobnov (ez@ezlab.org)
Licensed under the MIT license. See LICENSE.md file.

"""
import os
from quast_libs.busco.BuscoAnalysis import BuscoAnalysis
from quast_libs.busco.BuscoConfig import BuscoConfig
from quast_libs.busco.pipebricks.Toolset import Tool
from quast_libs.busco.pipebricks.PipeLogger import PipeLogger
# from overrides import overrides  # useful fro dev, but don't want all user to install this


class GeneSetAnalysis(BuscoAnalysis):
    """
    This class runs a BUSCO analysis on a gene set.
    """

    _logger = PipeLogger.get_logger(__name__)

    #
    # magic or public, meant to be accessed by external scripts [instance]
    #

    def __init__(self, params):
        """
        Initialize an instance.
        :param params: Values of all parameters that have to be defined
        :type params: PipeConfig
        """
        self._mode = 'proteins'
        super(GeneSetAnalysis, self).__init__(params)
        if self._params.getboolean('busco', 'restart'):
            GeneSetAnalysis._logger.error(
                'There is no restart allowed for the protein mode')
            raise SystemExit
        # data integrity checks not done by the parent class
        if self.check_protein_file() is False:
            GeneSetAnalysis._logger.error('Please provide a protein file as input')
            raise SystemExit

    # @overrides
    def run_analysis(self):
        """
        This function calls all needed steps for running the analysis.
        """
        super(GeneSetAnalysis, self).run_analysis()
        # validate sequence file
        if super(GeneSetAnalysis, self).check_protein_file() is False:
            GeneSetAnalysis._logger.error('Please provide a protein file as input')
            raise SystemExit
        self._load_score()
        self._load_length()
        self._run_hmmer()
        self._produce_short_summary()
        self.cleanup()
        if self._tarzip:
            self._run_tarzip_hmmer_output()

    #
    # public, meant to be accessed by external scripts [class]
    #

    # Nothing

    #
    # method that should be used as if protected, for internal use [instance]
    # to move to public and rename if meaningful
    #

    # @overrides
    def _init_tools(self):
        """
        Init the tools needed for the analysis
        """
        GeneSetAnalysis._logger.info('Init tools...')
        self._hmmer = Tool('hmmsearch', self._params)
        GeneSetAnalysis._logger.info('Check dependencies...')
        self._check_tool_dependencies()

    # @override
    def _run_hmmer(self):
        """
        This function runs hmmsearch.
        """

        # Run hmmer
        GeneSetAnalysis._logger.info('Running HMMER on the proteins:')

        if not os.path.exists(self.mainout + 'hmmer_output'):
            os.makedirs('%shmmer_output' % self.mainout)
            
        files = os.listdir(self._lineage_path + '/hmms')
        files.sort()
        # open target scores file
        f2 = open('%sscores_cutoff' % self._lineage_path)
        #   Load dictionary of HMM expected scores and full list of groups
        score_dic = {}
        for i in f2:
            i = i.strip().split()
            try:
                score_dic[i[0]] = float(i[1])  # values; [1] = mean; [2] = min
            except IndexError:
                pass
        self._totalbuscos = len(list(score_dic.keys()))
        f2.close()

        hmmer_tool = Tool('hmmsearch', self._params)
        for entry in files:
            name = entry[:-4]
            if name in score_dic:
                hmmer_job = hmmer_tool.create_job()
                hmmer_job.add_parameter('--domtblout')
                hmmer_job.add_parameter('%shmmer_output/%s.out.1' % (self.mainout, name))
                hmmer_job.add_parameter('-o')
                hmmer_job.add_parameter('%stemp_%s%s' % (self._tmp, self._out, self._random))
                hmmer_job.add_parameter('--cpu')
                hmmer_job.add_parameter('1')
                hmmer_job.add_parameter('%shmms/%s.hmm' % (self._lineage_path, name))
                hmmer_job.add_parameter('%s' % self._sequences)
        
        hmmer_tool.run_jobs(self._cpus)

    # @override
    def _check_tool_dependencies(self):
        """
        check dependencies on tools
        :raises SystemExit: if a Tool is not available
        """

        # check 'hmmersearch' command availability
        if not Tool.check_tool_available('hmmsearch', self._params):
            BuscoAnalysis._logger.error(
                '\"hmmsearch\" is not accessible, '
                'add or modify its path in the config file. Do not include the command '
                'in the path !')
            raise SystemExit

        # check version
        if self._get_hmmer_version(self._hmmer.cmd[0]) >= BuscoConfig.HMMER_VERSION:
            pass
        else:
            BuscoAnalysis._logger.error(
                'HMMer version detected is not supported, please use HMMer '
                ' v.%s +' % BuscoConfig.HMMER_VERSION)
            raise SystemExit
