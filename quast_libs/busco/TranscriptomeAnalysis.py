#!/usr/bin/env python3
# coding: utf-8
"""
.. module:: TranscriptomeAnalysis
   :synopsis:TranscriptomeAnalysis implements genome analysis specifics
.. versionadded:: 3.0.0
.. versionchanged:: 3.0.0

Copyright (c) 2016-2017, Evgeny Zdobnov (ez@ezlab.org)
Licensed under the MIT license. See LICENSE.md file.

"""
import os
import time

from quast_libs.busco.BuscoAnalysis import BuscoAnalysis
from quast_libs.busco.pipebricks.PipeLogger import PipeLogger
# from overrides import overrides  # useful fro dev, but don't want all user to install this


class TranscriptomeAnalysis(BuscoAnalysis):
    """
    Analysis on a transcriptome.
    """

    _logger = PipeLogger.get_logger(__name__)

    #
    # magic or public, meant to be accessed by external scripts [instance]
    #

    def __init__(self, params):
        """
        Initialize an instance.
        :param params: Values of all parameters that have to be defined
        :type params: BuscoConfig
        """
        self._mode = 'transcriptome'
        super(TranscriptomeAnalysis, self).__init__(params)
        self._transcriptome_by_scaff = {}
        # data integrity checks not done by the parent class
        if self.check_nucleotide_file() is False:
            TranscriptomeAnalysis._logger.error('Please provide a nucleotide file as input')
            raise SystemExit

    # @overrides
    def run_analysis(self):
        """
        This function calls all needed steps for running the analysis.
        """

        super(TranscriptomeAnalysis, self).run_analysis()

        if self._restart:
            checkpoint = self.get_checkpoint(
                reset_random_suffix=True)
            TranscriptomeAnalysis._logger.warning(
                'Restarting an uncompleted run')
        else:
            checkpoint = 0  # all steps will be done
        if checkpoint < 1:
            TranscriptomeAnalysis._logger.info(
                '****** Step 1/2, current time: %s ******' %
                time.strftime("%m/%d/%Y %H:%M:%S"))
            if self._has_variants_file:
                self._run_tblastn(
                    ancestral_variants=True)
            else:
                self._run_tblastn(
                    ancestral_variants=False)
            self._set_checkpoint(1)
        TranscriptomeAnalysis._logger.info(
            '****** Step 2/2, current time: %s ******' %
            time.strftime("%m/%d/%Y %H:%M:%S"))
        self._load_score()
        self._load_length()
        self._get_coordinates()
        self._run_hmmer()
        self._produce_short_summary()
        self.cleanup()
        if self._tarzip:
            self._run_tarzip_hmmer_output()
            self._run_tarzip_translated_proteins()
        # remove the checkpoint, run is done
        self._set_checkpoint()

    # @overrides
    def cleanup(self):
        """
        This function cleans temporary files.
        """
        super(TranscriptomeAnalysis, self).cleanup()
        self._p_open(['rm %s*%s%s_.temp' % (self._tmp, self._out, self._random)], 'bash', shell=True)
        self._p_open(['rm %(tmp)s%(abrev)s.*ns? %(tmp)s%(abrev)s.*nin %(tmp)s%(abrev)s.*nhr' %
                     {'tmp': self._tmp, 'abrev': self._out + str(self._random)}], 'bash', shell=True)

    # @overrides
    def _check_dataset(self):
        """
        Check if the dataset integrity, if files and folder are present
        :raises SystemExit: if the dataset miss files or folders
        """
        super(TranscriptomeAnalysis, self)._check_dataset()
        # note: score and length cutoffs are checked when read,
        # see _load_scores and _load_lengths
        # ancestral would cause blast to fail, and be detected, see _blast()
        # dataset.cfg is not mandatory

        # check whether the ancestral_variants file is present
        if os.path.exists('%sancestral_variants' % self._lineage_path):
            self._has_variants_file = True
        else:
            self._has_variants_file = False
            BuscoAnalysis._logger.warning(
                'The dataset you provided does not contain the file '
                'ancestral_variants, likely because it is an old version. '
                'All blast steps will use the file ancestral instead')

    #
    # public, meant to be accessed by external scripts [class]
    #

    # Nothing

    #
    # method that should be used as if protected, for internal use [instance]
    # to move to public and rename if meaningful
    #

    def _sixpack(self, seq):
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
                rev += BuscoAnalysis.COMP[letter]
            except KeyError:
                rev += BuscoAnalysis.COMP['N']
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
                        new += BuscoAnalysis.CODONS[part]
                    except KeyError:
                        new += 'X'
                    part = ''
                    part += letter
                else:
                    part += letter
            if len(part) == 3:
                try:
                    new += BuscoAnalysis.CODONS[part]
                except KeyError:
                    new += 'X'
            transc.append(new)
        return transc

    def _reformats_seq_id(self, seq_id):
        """
        This function reformats the sequence id to its original values
        :param seq_id: the seq id to reformats
        :type seq_id: str
        :return: the reformatted seq_id
        :rtype: str
        """
        return "_".join(seq_id.split('_')[:-1])

    def _get_coordinates(self):
        """
        This function gets coordinates for candidate regions from
        tblastn result file
        """

        TranscriptomeAnalysis._logger.info('Maximum number of candidate transcript per BUSCO limited to: %s'
                                           % self._region_limit)

        TranscriptomeAnalysis._logger.info(
            'Getting coordinates for candidate transcripts...')
        # open input file
        f = open('%sblast_output/tblastn_%s.tsv' % (self.mainout, self._out))
        transcriptome_by_busco = {}
        self._transcriptome_by_scaff = {}
        maxi = 0
        for i in f:  # get a dictionary of BUSCO matches vs candidate scaffolds
            if i.startswith('#'):
                pass
            else:
                line = i.strip().split()
                if self._has_variants_file:
                    busco = '_'.join(line[0].split("_")[:-1])  # This pattern
                else:                                          # can support
                    busco = line[0]                            # name like
                scaff = line[1]                                # EOG00_1234_1
                leng = int(line[3])
                blast_eval = float(line[10])
                if busco not in transcriptome_by_busco.keys():
                    # Simply add it
                    # Use a single entry dict to keep scaffs id and their
                    # blast eval, for each busco
                    transcriptome_by_busco[busco] = [{scaff: blast_eval}]
                    # and keep a list of each busco by scaff
                    try:
                        self._transcriptome_by_scaff[scaff].append(busco)
                    except KeyError:
                        self._transcriptome_by_scaff[scaff] = [busco]
                    maxi = leng
                elif len(transcriptome_by_busco[busco]) < self._region_limit \
                        and leng >= 0.7 * maxi:
                    # check that this transcript is not already in, and update
                    # its eval if needed
                    add = True
                    for scaff_dict in transcriptome_by_busco[busco]:
                        if list(scaff_dict.keys())[0] == scaff:
                            add = False
                            # update the eval for this scaff
                            if blast_eval < list(scaff_dict.values())[0]:
                                scaff_dict[scaff] = blast_eval
                    if add:
                        transcriptome_by_busco[busco].append(
                            {scaff: blast_eval})
                        try:
                            self._transcriptome_by_scaff[scaff].append(busco)
                        except KeyError:
                            self._transcriptome_by_scaff[scaff] = [busco]
                        if leng > maxi:
                            maxi = leng
                elif len(transcriptome_by_busco[busco]) >= self._region_limit \
                        and leng >= 0.7 * maxi:
                    # replace the lowest scoring transcript if the current has
                    # a better score. needed because of multiple blast query
                    # having the same name when using ancestral_variants and
                    # not sorted by eval in the tblastn result file
                    to_replace = None
                    # Define if something has to be replaced
                    for entry in transcriptome_by_busco[busco]:
                        if list(entry.values())[0] > blast_eval:
                            # check if there is already a to_replace entry and
                            # compare the eval
                            if (to_replace and
                                    list(entry.values())[0] >
                                    list(to_replace.values())[0]) or \
                                        not to_replace:
                                to_replace = {list(entry.keys())[0]:
                                              list(entry.values())[0]}

                    if to_replace:
                        # try to add the new one
                        # check that this scaffold is not already in,
                        # and update the eval if needed
                        # if the scaff was already in, do not replace
                        # the to_replace entry to keep the max number of
                        # candidate regions
                        add = True
                        for scaff_dict in transcriptome_by_busco[busco]:
                            if list(scaff_dict.keys())[0] == scaff:
                                add = False
                                if blast_eval < list(scaff_dict.values())[0]:
                                    # update the eval for this scaff
                                    scaff_dict[scaff] = blast_eval
                        if add:
                            # add the new one
                            transcriptome_by_busco[busco].append({
                                scaff: blast_eval})
                            try:
                                self._transcriptome_by_scaff[scaff].append(
                                    busco)
                            except KeyError:
                                self._transcriptome_by_scaff[scaff] = [busco]

                            # remove the old one
                            for entry in transcriptome_by_busco[busco]:
                                if list(entry.keys())[0] == \
                                        list(to_replace.keys())[0]:
                                    scaff_to_remove = list(entry.keys())[0]
                                    break
                            transcriptome_by_busco[busco].remove(entry)

                            for entry in self._transcriptome_by_scaff[scaff_to_remove]:
                                if entry == busco:
                                    break
                            self._transcriptome_by_scaff[scaff_to_remove].remove(entry)

                            if leng > maxi:
                                maxi = leng

        TranscriptomeAnalysis._logger.info(
            'Extracting candidate transcripts...')
        f = open(self._sequences)
        check = 0
        out = None
        for i in f:
            if i.startswith('>'):
                i = i.strip().split()
                i = i[0][1:]
                if i in list(self._transcriptome_by_scaff.keys()):
                    out = open('%s%s%s%s_.temp' % (self._tmp, i, self._out,
                                                   self._random), 'w')
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
            os.makedirs('%stranslated_proteins' % self.mainout)
        files = os.listdir(self._tmp)
        files.sort()
        lista = []
        for entry in files:
            if entry.endswith(self._out + str(self._random) + '_.temp'):
                lista.append(entry)

        TranscriptomeAnalysis._logger.info(
            'Translating candidate transcripts...')
        for entry in lista:
            raw_seq = open(self._tmp + entry)
            # this works even if the runname is in the header
            name = self._out.join(entry.replace('_.temp', '').split(self._out)[:-1])
            trans_seq = open(self.mainout + 'translated_proteins/' + name +
                             '.faa', 'w')
            nucl_seq = ''
            header = ''
            for line in raw_seq:
                if line.startswith('>'):
                    header = line.strip() + '_'
                else:
                    nucl_seq += line.strip()
            seq_count = 0
            for translation in self._sixpack(nucl_seq):
                seq_count += 1
                trans_seq.write(
                    '%s%s\n%s\n' % (header, seq_count, translation))
            raw_seq.close()
            trans_seq.close()

        # open target scores file
        f2 = open('%sscores_cutoff' % self._lineage_path)
        # Load dictionary of HMM expected scores and full list of groups
        score_dic = {}
        for i in f2:
            i = i.strip().split()
            try:
                # float values: [1]=mean; [2]=minimum
                score_dic[i[0]] = float(i[1])
            except IndexError:
                pass
        f2.close()
        self._totalbuscos = len(list(score_dic.keys()))

    #
    # method that should be considered as if protected, for internal use [class]
    # to move to public and rename if meaningful
    #

    # Nothing

    def _run_tarzip_translated_proteins(self):
        """
        This function tarzips results folder
        """
        # translated_proteins
        self._p_open(['tar', '-C', '%s' % self.mainout, '-zcf',
                     '%stranslated_proteins.tar.gz' % self.mainout, 'translated_proteins', '--remove-files'], 'bash',
                     shell=False)

    # @overrides
    def _run_hmmer(self):
        """
        This function runs hmmsearch.
        """
        TranscriptomeAnalysis._logger.info(
            'Running HMMER to confirm transcript orthology:')
        files = os.listdir('%stranslated_proteins/' % self.mainout)
        files.sort()
        if not os.path.exists('%shmmer_output' % self.mainout):
            os.makedirs('%shmmer_output' % self.mainout)

        count = 0

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

                    hmmer_job = self._hmmer.create_job()
                    hmmer_job.add_parameter('--domtblout')
                    hmmer_job.add_parameter('%shmmer_output/%s.out.%s' % (self.mainout, busco, busco_index[busco]))
                    hmmer_job.add_parameter('-o')
                    hmmer_job.add_parameter('%stemp_%s%s' % (self._tmp, self._out, str(self._random)))
                    hmmer_job.add_parameter('--cpu')
                    hmmer_job.add_parameter('1')
                    hmmer_job.add_parameter('%shmms/%s.hmm' % (self._lineage_path, busco))
                    hmmer_job.add_parameter('%stranslated_proteins/%s' % (self.mainout, f))

        # Run hmmer
        self._hmmer.run_jobs(self._cpus)
