#!/usr/bin/env python
# coding: utf-8
"""
.. module:: GenomeAnalysis
   :synopsis: GenomeAnalysis implements genome analysis specifics
.. versionadded:: 3.0.0
.. versionchanged:: 3.0.0

Copyright (c) 2016-2017, Evgeny Zdobnov (ez@ezlab.org)
Licensed under the MIT license. See LICENSE.md file.

"""
from quast_libs.busco.BuscoAnalysis import BuscoAnalysis
from quast_libs.busco.BuscoConfig import BuscoConfig
from collections import deque
import heapq
import os
from quast_libs.busco.pipebricks.PipeLogger import PipeLogger
from quast_libs.busco.pipebricks.Toolset import Tool
import time
# from overrides import overrides  # useful fro dev, but don't want all user to install this


class GenomeAnalysis(BuscoAnalysis):
    """
    This class runs a BUSCO analysis on a genome.
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
        # 1) load parameters
        # 2) load and validate tools
        # 3) check data and dataset integrity
        # 4) Ready for analysis
        # See parent __init__ where most of the calls and actions are made

        # Retrieve the augustus config path, mandatory for genome
        # Cannot be specified through conf because some augustus perl scripts use it as well
        # BUSCO could export it if absent, but do not want to mess up with the user env,
        # let's just tell the user to do it for now

        self._augustus_config_path = BuscoConfig.nice_path(os.environ.get('AUGUSTUS_CONFIG_PATH'))
        self._target_species = params.get('busco', 'species')
        self._augustus_parameters = params.get('busco', 'augustus_parameters')
        self._mode = 'genome'
        super(GenomeAnalysis, self).__init__(params)  # _init tool called here
        self._long = params.getboolean('busco', 'long')
        self._flank = self._define_flank()
        # data integrity checks not done by the parent class
        if self.check_nucleotide_file() is False:
            BuscoAnalysis._logger.error('Please provide a nucleotide file as input')
            raise SystemExit

    # @overrides
    def run_analysis(self):
        """
        This function calls all needed steps for running the analysis.
        """

        super(GenomeAnalysis, self).run_analysis()

        if self._restart:
            checkpoint = self.get_checkpoint(reset_random_suffix=True)
            BuscoAnalysis._logger.warning('Restarting an uncompleted run')
        else:
            checkpoint = 0  # all steps will be done

        BuscoAnalysis._logger.info(
            '****** Phase 1 of 2, initial predictions ******')
        if checkpoint < 1:
            BuscoAnalysis._logger.info(
                '****** Step 1/3, current time: %s ******' %
                time.strftime("%m/%d/%Y %H:%M:%S"))
            self._run_tblastn()
            self._set_checkpoint(1)

        if checkpoint < 2:
            BuscoAnalysis._logger.info(
                '****** Step 2/3, current time: %s ******' %
                time.strftime("%m/%d/%Y %H:%M:%S"))
            self._get_coordinates()
            self._run_augustus()
            BuscoAnalysis._logger.info(
                '****** Step 3/3, current time: %s ******' %
                time.strftime("%m/%d/%Y %H:%M:%S"))
            self._run_hmmer()
            self._set_checkpoint(2)
        self._load_score()
        self._load_length()
        if checkpoint == 2 or checkpoint == 3:
            BuscoAnalysis._logger.info('Phase 1 was already completed.')
        if checkpoint == 3:
            self._fix_restart_augustus_folder()
        self._produce_short_summary()
        BuscoAnalysis._logger.info(
            '****** Phase 2 of 2, predictions using species specific training '
            '******')
        if checkpoint < 3:
            BuscoAnalysis._logger.info(
                '****** Step 1/3, current time: %s ******' %
                time.strftime("%m/%d/%Y %H:%M:%S"))
            if self._has_variants_file:
                self._run_tblastn(missing_and_frag_only=True, ancestral_variants=True)
                self._get_coordinates(missing_and_frag_only=True)
            else:
                self._run_tblastn(missing_and_frag_only=True, ancestral_variants=False)
                self._get_coordinates(missing_and_frag_only=True)
            self._set_checkpoint(3)
        BuscoAnalysis._logger.info(
            '****** Step 2/3, current time: %s ******' %
            time.strftime("%m/%d/%Y %H:%M:%S"))
        self._rerun_augustus()
        self._move_retraining_parameters()
        self.cleanup()
        if self._tarzip:
            self._run_tarzip_augustus_output()
            self._run_tarzip_hmmer_output()
        # remove the checkpoint, run is done
        self._set_checkpoint()

    # @overrides
    def cleanup(self):
        """
        This function cleans temporary files
        """
        super(GenomeAnalysis, self).cleanup()
        self._p_open(['rm %s*%s%s_.temp' % (self._tmp, self._out, self._random)], 'bash', shell=True)
        self._p_open(
            ['rm %(tmp)s%(abrev)s.*ns? %(tmp)s%(abrev)s.*nin '
             '%(tmp)s%(abrev)s.*nhr' %
             {'tmp': self._tmp, 'abrev': self._out + str(self._random)}], 'bash', shell=True)

    # @overrides
    def _check_dataset(self):
        """
        Check if the dataset integrity, if files and folder are present
        :raises SystemExit: if the dataset miss files or folders
        """
        super(GenomeAnalysis, self)._check_dataset()
        # prfl folder
        flag = False
        for dirpath, dirnames, files in os.walk('%sprfl' % self._lineage_path):
            if files:
                flag = True
        if not flag:
            BuscoAnalysis._logger.error(
                'The dataset you provided lacks elements in %sprfl' %
                self._lineage_path)
            raise SystemExit
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

    # @overrides
    def _init_tools(self):
        """
        :return:
        """
        super(GenomeAnalysis, self)._init_tools()  # call to check_tool_dependencies made here
        self._check_file_dependencies()
        self._augustus = Tool('augustus', self._params)
        self._etraining = Tool('etraining', self._params)
        self._gff2gbSmallDNA_pl = Tool('gff2gbSmallDNA.pl', self._params)
        self._new_species_pl = Tool('new_species.pl', self._params)
        self._optimize_augustus_pl = Tool('optimize_augustus.pl', self._params)

    def _define_flank(self):
        """
        :return:
        """
        try:
            f = open(self._sequences, 'r')
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
            elif flank > BuscoConfig.MAX_FLANK:
                flank = BuscoConfig.MAX_FLANK
            f.close()
        except IOError:
            BuscoAnalysis._logger.error('Impossible to read the fasta file %s ' % self._sequences)
            raise SystemExit
        return flank

    def _fix_restart_augustus_folder(self):
        """
        This function resets and checks the augustus folder to make a restart
        possible in phase 2
        :raises SystemExit: if it is not possible to fix the folders
        """
        if os.path.exists('%saugustus_output/predicted_genes_run1' % self.mainout) \
                and os.path.exists('%shmmer_output_run1' % self.mainout):
            self._p_open(['rm', '-fr', '%saugustus_output/predicted_genes' %
                         self.mainout], 'bash', shell=False)
            self._p_open(['mv', '%saugustus_output/predicted_genes_run1' %
                         self.mainout, '%saugustus_output/predicted_genes' % self.mainout], 'bash',
                         shell=False)
            self._p_open(['rm', '-fr', '%shmmer_output' % self.mainout],
                         'bash', shell=False)
            self._p_open(['mv', '%shmmer_output_run1/' % self.mainout, '%shmmer_output/' % self.mainout],
                         'bash', shell=False)

        elif os.path.exists('%saugustus_output/predicted_genes' %
                            self.mainout) and os.path.exists('%shmmer_output' %
                                                             self.mainout):
            pass
        else:
            BuscoAnalysis._logger.error(
                'Impossible to restart the run, necessary folders are missing.'
                ' Use the -f option instead of -r')
            raise SystemExit

    def _get_coordinates(self, missing_and_frag_only=False):
        """
        This function gets coordinates for candidate regions
        from tblastn result file
        :param missing_and_frag_only: tell whether to use
        the missing_and_frag_rerun tblastn file
        :type missing_and_frag_only: bool
        """

        if missing_and_frag_only:
            blast_file = open(
                '%sblast_output/tblastn_%s_missing_and_frag_rerun.tsv' %
                (self.mainout, self._out))
        else:
            blast_file = open('%sblast_output/tblastn_%s.tsv' %
                              (self.mainout, self._out))

        BuscoAnalysis._logger.info('Maximum number of candidate contig per BUSCO limited to: %s' % self._region_limit)

        BuscoAnalysis._logger.info(
            'Getting coordinates for candidate regions...')

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
                    # for minus-strand genes,invert coordinates for convenience
                    if contig_end < contig_start:
                        temp = contig_end
                        contig_end = contig_start
                        contig_start = temp
                    # create new entry in dictionary for current BUSCO
                    if busco_name not in dic.keys():
                        dic[busco_name] = [contig]
                        coords[busco_name] = {}
                        coords[busco_name][contig] = [contig_start, contig_end,
                                                      deque([[busco_start,
                                                              busco_end]]),
                                                      aln_len, blast_eval]
                    # get just the top scoring regions
                    # according to region limits
                    elif contig not in dic[busco_name] and len(dic[busco_name]) < self._region_limit:
                        # scoring regions
                        dic[busco_name].append(contig)
                        coords[busco_name][contig] = [contig_start, contig_end,
                                                      deque([[busco_start,
                                                              busco_end]]),
                                                      aln_len, blast_eval]

                    # replace the lowest scoring region if the current has
                    # a better score.
                    # needed because of multiple blast query having
                    # the same name when using ancestral_variants
                    # and not sorted by eval in the tblastn result file
                    elif contig not in dic[busco_name] and len(dic[busco_name]) >= self._region_limit:
                        to_replace = None
                        for entry in list(coords[busco_name].keys()):
                            if coords[busco_name][entry][4] > blast_eval:
                                # check if there is already a to_replace entry
                                # and compare the eval
                                if (to_replace and coords[busco_name][entry][4] > list(to_replace.values())[0]) \
                                        or not to_replace:
                                    # use a single entry dictionary to store
                                    # the id to replace and its eval
                                    to_replace = {entry: coords[busco_name][entry][4]}
                        if to_replace:
                            dic[busco_name].remove(list(to_replace.keys())[0])
                            dic[busco_name].append(contig)
                            coords[busco_name][contig] = [contig_start,
                                                          contig_end,
                                                          deque([[busco_start,
                                                                  busco_end]]),
                                                          aln_len, blast_eval]
                    # contigold already checked
                    elif contig in dic[busco_name]:
                        # now update coordinates
                        if contig_start < coords[busco_name][contig][0] and \
                                                coords[busco_name][contig][0] - \
                                                contig_start <= 50000:
                            # starts before and withing 50kb of current pos
                            coords[busco_name][contig][0] = contig_start
                            coords[busco_name][contig][2].append([busco_start,
                                                                  busco_end])
                        if contig_end > coords[busco_name][contig][1] \
                                and contig_end - coords[busco_name][contig][1] <= 50000:
                            # ends after and within 50 kbs
                            coords[busco_name][contig][1] = contig_end
                            coords[busco_name][contig][3] = busco_end
                            coords[busco_name][contig][2].append([busco_start,
                                                                  busco_end])
                        elif coords[busco_name][contig][1] > contig_start > \
                                coords[busco_name][contig][0]:
                            # starts inside current coordinates
                            if contig_end < coords[busco_name][contig][1]:
                                # if ending inside, just add
                                # alignemnt positions to deque
                                coords[busco_name][contig][2].append(
                                    [busco_start, busco_end])
                                # if ending after current coordinates, extend
                            elif contig_end > coords[busco_name][contig][1]:
                                coords[busco_name][contig][2][1] = contig_end
                                coords[busco_name][contig][2].append(
                                    [busco_start, busco_end])

                except (IndexError, ValueError):
                    pass

        blast_file.close()
        final_locations = {}
        if missing_and_frag_only:
            out = open(
                '%s/blast_output/coordinates_%s_missing_and_frag_rerun.tsv' %
                (self.mainout, self._out), 'w')  # Coordinates output file
        else:
            out = open(
                '%s/blast_output/coordinates_%s.tsv' %
                (self.mainout, self._out), 'w')  # Coordinates output file

        for busco_group in coords:
            final_locations[busco_group] = []
            # list of candidate contigs
            candidate_contigs = list(coords[busco_group].keys())
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
                            if self._check_overlap(currently, region) != 0:
                                gg = self._define_boundary(currently,
                                                           region)
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
                            checking = self._check_overlap(entry, region)
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
                            gg = self._define_boundary(currently, region)
                            final_regions[final_regions.index(region)] = gg

                size_lists.append(self._gargantua(final_regions))
            max_size = max(size_lists)
            size_cutoff = int(0.7 * max_size)
            index_passed_cutoffs = heapq.nlargest(int(self._region_limit),
                                                  range(len(size_lists)),
                                                  size_lists.__getitem__)

            for candidate in index_passed_cutoffs:
                if size_lists[candidate] >= size_cutoff:
                    seq_name = candidate_contigs[candidate]
                    seq_start = int(coords[busco_group]
                                    [candidate_contigs[candidate]][0]) - self._flank
                    if seq_start < 0:
                        seq_start = 0
                    seq_end = int(coords[busco_group]
                                  [candidate_contigs[candidate]][1]) + self._flank
                    final_locations[busco_group].append([seq_name, seq_start,
                                                         seq_end])
                    out.write('%s\t%s\t%s\t%s\n' % (busco_group, seq_name,
                                                    seq_start, seq_end))
        out.close()

    def _extract_scaffolds(self, missing_and_frag_only=False):
        """
        This function extract the scaffold having blast results
        :param missing_and_frag_only: to tell which coordinate file
        to look for, complete or just missing and fragment
        :type missing_and_frag_only: bool
        """
        BuscoAnalysis._logger.info('Pre-Augustus scaffold extraction...')
        if missing_and_frag_only:
            coord = open(
                '%s/blast_output/coordinates_%s_missing_and_frag_rerun.tsv' %
                (self.mainout, self._out))
        else:
            coord = open(
                '%s/blast_output/coordinates_%s.tsv' % (self.mainout,
                                                        self._out))
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
                    out = open('%s%s%s%s_.temp' % (self._tmp, i, self._out,
                                                   self._random), 'w')
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

    def _run_augustus(self):
        """
        This function runs Augustus
        """
        # Run Augustus on all candidate regions
        # 1- Get the temporary sequence files (no multi-fasta support in
        # Augustus)
        # 2- Build a list with the running commands (for threading)
        # 3- Launch Augustus in paralell using Threading
        # 4- Prepare the sequence files to be analysed with HMMer 3.1

        # Extract candidate contigs/scaffolds from genome assembly
        # Augustus can't handle multi-fasta files, each sequence has
        # to be present in its own file
        # Write the temporary sequence files

        # First, delete the augustus result folder, in case we are in
        #  the restart mode at this point
        self._p_open(['rm -rf %saugustus_output/*' % self.mainout], 'bash', shell=True)

        # get all scaffolds with blast results
        self._extract_scaffolds()

        # Now run Augustus on each candidate region with its respective
        # Block-profile

        BuscoAnalysis._logger.info(
            'Running Augustus prediction using %s as species:' %
            self._target_species)

        if self._augustus_parameters:
            BuscoAnalysis._logger.info(
                'Additional parameters for Augustus are %s: ' %
                self._augustus_parameters)

        augustus_log = '%saugustus_output/augustus.log' % self.mainout
        BuscoAnalysis._logger.info(
            '[augustus]\tPlease find all logs related to Augustus errors here: %s' %
            augustus_log)
        if not os.path.exists('%saugustus_output/predicted_genes' % self.mainout):
            os.makedirs('%saugustus_output/predicted_genes' % self.mainout)

        # coordinates of hits by BUSCO
        self._location_dic = {}
        f = open('%s/blast_output/coordinates_%s.tsv' % (self.mainout,
                                                         self._out))
        for line in f:
            line = line.strip().split('\t')
            scaff_id = line[1]
            scaff_start = line[2]
            scaff_end = line[3]
            group_name = line[0]

            if group_name not in self._location_dic:
                # scaffold,start and end
                self._location_dic[group_name] = [[scaff_id, scaff_start,
                                                   scaff_end]]
            elif group_name in self._location_dic:
                self._location_dic[group_name].append([scaff_id, scaff_start,
                                                       scaff_end])
        f.close()
        # Make a list containing the commands to be executed in parallel
        # with threading.
        for entry in self._location_dic:

            for location in self._location_dic[entry]:
                scaff = location[0] + self._out + str(self._random) + \
                        '_.temp'
                scaff_start = location[1]
                scaff_end = location[2]
                output_index = self._location_dic[entry].index(location) + 1

                out_name = '%saugustus_output/predicted_genes/%s.out.%s' % \
                           (self.mainout, entry, output_index)

                augustus_job = self._augustus.create_job()
                augustus_job.add_parameter('--codingseq=1')
                augustus_job.add_parameter('--proteinprofile=%sprfl/%s.prfl' % (self._lineage_path, entry))
                augustus_job.add_parameter('--predictionStart=%s ' % scaff_start)
                augustus_job.add_parameter('--predictionEnd=%s ' % scaff_end)
                augustus_job.add_parameter('--species=%s' % self._target_species)
                for p in self._augustus_parameters.split(' '):
                    if len(p) > 2:
                        augustus_job.add_parameter(p)
                augustus_job.add_parameter('%s%s' % (self._tmp, scaff))
                augustus_job.stdout_file = [out_name, 'w']
                augustus_job.stderr_file = [augustus_log, 'a']

        self._augustus.run_jobs(self._cpus, BuscoAnalysis._logger)

        # Preparation of sequences for use with HMMer

        # Parse Augustus output files
        # ('run_XXXX/augustus') and extract protein
        # sequences to a FASTA file
        # ('run_XXXX/augustus_output/extracted_proteins').
        BuscoAnalysis._logger.info('Extracting predicted proteins...')
        files = os.listdir('%saugustus_output/predicted_genes' % self.mainout)
        files.sort()
        for entry in files:
            self._p_open(
                ['sed -i.bak \'1,3d\' %saugustus_output/predicted_genes/%s;'
                 'rm %saugustus_output/predicted_genes/%s.bak' %
                 (self.mainout, entry, self.mainout, entry)], 'bash',
                shell=True)
        if not os.path.exists(self.mainout + 'augustus_output/extracted_proteins'):
            os.makedirs('%saugustus_output/extracted_proteins' % self.mainout)

        self._no_predictions = []
        for entry in files:
            self._extract(self.mainout, entry)
            self._extract(self.mainout, entry, aa=False)

        self._p_open(['find %saugustus_output/extracted_proteins -size 0 -delete' % self.mainout],
                     'bash', shell=True)

    def _set_rerun_busco_command(self):
        """
        This function sets the command line to call to reproduce this run
        """
        super(GenomeAnalysis, self)._set_rerun_busco_command()
        self._rerun_cmd += ' -sp %s' % self._target_species
        if self._augustus_parameters:
            self._rerun_cmd += ' --augustus_parameters \'%s\'' % self._augustus_parameters

    def _rerun_augustus(self):
        """
        This function runs Augustus and hmmersearch for the second time
        """
        #  TODO: augustus and augustus rerun should reuse the shared code
        #  when possible, and hmmer should not be here

        augustus_log = '%saugustus_output/augustus.log' % self.mainout

        if not os.path.exists('%saugustus_output/gffs' % self.mainout):
            os.makedirs('%saugustus_output/gffs' % self.mainout)
        if not os.path.exists('%saugustus_output/gb' % self.mainout):
            os.makedirs('%saugustus_output/gb' % self.mainout)

        BuscoAnalysis._logger.info(
            'Training Augustus using Single-Copy Complete BUSCOs:')
        BuscoAnalysis._logger.info(
            'Converting predicted genes to short genbank files at %s...' %
            time.strftime("%m/%d/%Y %H:%M:%S"))
        for entry in self._single_copy_files:
            check = 0
            file_name = self._single_copy_files[entry].split('-')[-1]
            target_seq_name = self._single_copy_files[entry].split('[')[0]
            group_name = file_name.split('.')[0]

            # create GFFs with only the "single-copy" BUSCO sequences
            pred_file = open('%saugustus_output/predicted_genes/%s' %
                             (self.mainout, file_name))
            gff_file = open('%saugustus_output/gffs/%s.gff' %
                            (self.mainout, group_name), 'w')
            BuscoAnalysis._logger.debug('creating %s' % ('%saugustus_output/gffs/%s.gff' % (self.mainout, group_name)))
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

            gff2_gb_small_dna_pl_job = self._gff2gbSmallDNA_pl.create_job()

            gff2_gb_small_dna_pl_job.add_parameter('%saugustus_output/gffs/%s.gff' % (self.mainout, entry))

            gff2_gb_small_dna_pl_job.add_parameter('%s' % self._sequences)

            gff2_gb_small_dna_pl_job.add_parameter('1000')

            gff2_gb_small_dna_pl_job.add_parameter('%saugustus_output/gb/%s.raw.gb' % (self.mainout, entry))

            gff2_gb_small_dna_pl_job.stderr_file = [augustus_log, 'a']

            gff2_gb_small_dna_pl_job.stdout_file = gff2_gb_small_dna_pl_job.stderr_file

        self._gff2gbSmallDNA_pl.run_jobs(self._cpus, BuscoAnalysis._logger, log_it=False)

        BuscoAnalysis._logger.info(
            'All files converted to short genbank files, now running '
            'the training scripts at %s...' % time.strftime("%m/%d/%Y %H:%M:%S"))

        # create new species config file from template
        new_species_pl_job = self._new_species_pl.create_job()
        # bacteria clade needs to be flagged as "prokaryotic"
        if self._domain == 'prokaryota':
            new_species_pl_job.add_parameter('--prokaryotic')
        new_species_pl_job.add_parameter('--species=BUSCO_%s%s' % (self._out, self._random))
        new_species_pl_job.stderr_file = [augustus_log, 'a']
        new_species_pl_job.stdout_file = new_species_pl_job.stderr_file
        self._new_species_pl.run_jobs(self._cpus, BuscoAnalysis._logger, False)

        BuscoAnalysis._logger.debug('concat all gb files...')

        self._p_open(['find %saugustus_output/gb/ -type f -name "*.gb" -exec cat {} \; '
                     '> %saugustus_output/training_set_%s.txt' % (self.mainout, self.mainout, self._out)],
                     'bash', shell=True)

        BuscoAnalysis._logger.debug('run etraining...')
        # train on new training set (complete single copy buscos)
        etraining_job = self._etraining.create_job()
        etraining_job.add_parameter('--species=BUSCO_%s%s' % (self._out, self._random))
        etraining_job.add_parameter('%saugustus_output/training_set_%s.txt' % (self.mainout, self._out))
        etraining_job.stderr_file = [augustus_log, 'a']
        etraining_job.stdout_file = new_species_pl_job.stderr_file
        self._etraining.run_jobs(self._cpus, BuscoAnalysis._logger, False)

        # long mode (--long) option - runs all the Augustus optimization
        # scripts (adds ~1 day of runtime)
        if self._long:
            BuscoAnalysis._logger.warning(
                'Optimizing augustus metaparameters, this may take a '
                'very long time, started at %s' % time.strftime("%m/%d/%Y %H:%M:%S"))
            optimize_augustus_pl_job = self._optimize_augustus_pl.create_job()
            optimize_augustus_pl_job.add_parameter('--cpus=%s' % self._cpus)
            optimize_augustus_pl_job.add_parameter('--species=BUSCO_%s%s' % (self._out, self._random))
            optimize_augustus_pl_job.add_parameter('%saugustus_output/training_set_%s.txt' % (self.mainout, self._out))
            optimize_augustus_pl_job.stderr_file = [augustus_log, 'a']
            optimize_augustus_pl_job.stdout_file = optimize_augustus_pl_job.stderr_file
            self._optimize_augustus_pl.run_jobs(self._cpus, BuscoAnalysis._logger, False)
            # train on new training set (complete single copy buscos)
            etraining_job = self._etraining.create_job()
            etraining_job.add_parameter('--species=BUSCO_%s%s' % (self._out, self._random))
            etraining_job.add_parameter('%saugustus_output/training_set_%s.txt' % (self.mainout, self._out))
            etraining_job.stderr_file = [augustus_log, 'a']
            etraining_job.stdout_file = new_species_pl_job.stderr_file
            self._etraining.run_jobs(self._cpus, BuscoAnalysis._logger, False)

        #######################################################

        # get all scaffolds with blast results
        self._extract_scaffolds(missing_and_frag_only=True)

        BuscoAnalysis._logger.info(
            'Re-running Augustus with the new metaparameters, number of '
            'target BUSCOs: %s' % len(self._missing_busco_list +
                                      self._fragmented_busco_list))

        augustus_rerun_seds = []

        # Move first run and create a folder for the rerun
        self._p_open(['mv', '%saugustus_output/predicted_genes' % self.mainout,
                     '%saugustus_output/predicted_genes_run1' % self.mainout], 'bash', shell=False)
        os.makedirs('%saugustus_output/predicted_genes' % self.mainout)
        self._p_open(['mv', '%shmmer_output/' % self.mainout, '%shmmer_output_run1/'
                     % self.mainout], 'bash', shell=False)
        os.makedirs('%shmmer_output/' % self.mainout)

        # Update the location dict with the new blast search
        # coordinates of hits by BUSCO
        self._location_dic = {}
        f = open('%s/blast_output/coordinates_%s_missing_and_frag_rerun.tsv' %
                 (self.mainout, self._out))
        for line in f:
            line = line.strip().split('\t')
            scaff_id = line[1]
            scaff_start = line[2]
            scaff_end = line[3]
            group_name = line[0]

            if group_name not in self._location_dic:
                # scaffold,start and end
                self._location_dic[group_name] = [[scaff_id, scaff_start,
                                                   scaff_end]]
            elif group_name in self._location_dic:
                self._location_dic[group_name].append([scaff_id, scaff_start,
                                                       scaff_end])
        f.close()

        for entry in self._missing_busco_list + self._fragmented_busco_list:
            if entry in self._location_dic:
                for location in self._location_dic[entry]:
                    scaff = location[0] + self._out + str(self._random) + \
                            '_.temp'
                    scaff_start = location[1]
                    scaff_end = location[2]
                    output_index = self._location_dic[entry].index(location) + 1

                    out_name = '%saugustus_output/predicted_genes/%s.out.%s' \
                               % (self.mainout, entry, output_index)

                    augustus_job = self._augustus.create_job()
                    augustus_job.add_parameter('--codingseq=1')
                    augustus_job.add_parameter('--proteinprofile=%sprfl/%s.prfl' % (self._lineage_path, entry))
                    augustus_job.add_parameter('--predictionStart=%s ' % scaff_start)
                    augustus_job.add_parameter('--predictionEnd=%s ' % scaff_end)
                    augustus_job.add_parameter('--species=BUSCO_%s' % self._out + str(self._random))
                    for p in self._augustus_parameters.split(' '):
                        if len(p) > 2:
                            augustus_job.add_parameter(p)
                    augustus_job.add_parameter('%s%s' % (self._tmp, scaff))
                    augustus_job.stdout_file = [out_name, 'w']
                    augustus_job.stderr_file = [augustus_log, 'a']

                    sed_call = 'sed -i.bak \'1,3d\' %s;rm %s.bak' % (out_name,
                                                                     out_name)
                    augustus_rerun_seds.append(sed_call)

                    out_name = '%shmmer_output/%s.out.%s' % (self.mainout,
                                                             entry,
                                                             output_index)
                    augustus_fasta = \
                        '%saugustus_output/extracted_proteins/%s.faa.%s' \
                        % (self.mainout, entry, output_index)

                    hmmer_job = self._hmmer.create_job()

                    hmmer_job.add_parameter('--domtblout')
                    hmmer_job.add_parameter('%s' % out_name)
                    hmmer_job.add_parameter('-o')
                    hmmer_job.add_parameter('%stemp_%s%s' % (self._tmp, self._out, self._random))
                    hmmer_job.add_parameter('--cpu')
                    hmmer_job.add_parameter('1')
                    hmmer_job.add_parameter('%shmms/%s.hmm' % (self._lineage_path, entry))
                    hmmer_job.add_parameter('%s' % augustus_fasta)

            else:
                pass

        self._augustus.run_jobs(self._cpus, BuscoAnalysis._logger)

        for sed_string in augustus_rerun_seds:
            self._p_open(['%s' % sed_string], 'bash', shell=True)
        # Extract fasta files from augustus output
        BuscoAnalysis._logger.info('Extracting predicted proteins...')
        self._no_predictions = []
        for entry in self._missing_busco_list + self._fragmented_busco_list:
            if entry in self._location_dic:
                for location in self._location_dic[entry]:
                    output_index = self._location_dic[entry].index(location) + 1
                    # when extract gets reworked to not need MAINOUT,
                    # change to OUT_NAME
                    plain_name = '%s.out.%s' % (entry, output_index)
                    self._extract(self.mainout, plain_name)
                    self._extract(self.mainout, plain_name, aa=False)
            else:
                pass

        # filter out the line that have no augustus prediction
        for job in self._hmmer.jobs_to_run:
            word = job.cmd_line
            target_seq = word[-1].split('/')[-1]
            if target_seq in self._no_predictions:
                self._hmmer.remove_job(job)

        BuscoAnalysis._logger.info('****** Step 3/3, current time: %s ******'
                                    % time.strftime("%m/%d/%Y %H:%M:%S"))
        BuscoAnalysis._logger.info(
            'Running HMMER to confirm orthology of predicted proteins:')

        self._hmmer.run_jobs(self._cpus, BuscoAnalysis._logger)

        # Fuse the run1 and rerun folders
        self._p_open(['mv %saugustus_output/predicted_genes/*.* '
                     '%saugustus_output/predicted_genes_run1/ 2> /dev/null'
                      % (self.mainout, self.mainout)], 'bash', shell=True)
        self._p_open(['mv %shmmer_output/*.* %shmmer_output_run1/ 2> /dev/null'
                     % (self.mainout, self.mainout)], 'bash', shell=True)
        self._p_open(['rm', '-r', '%saugustus_output/predicted_genes' % self.mainout], 'bash',
                     shell=False)
        self._p_open(['rm', '-r', '%shmmer_output' % self.mainout], 'bash', shell=False)
        self._p_open(['mv', '%saugustus_output/predicted_genes_run1'
                     % self.mainout, '%saugustus_output/predicted_genes' %
                     self.mainout], 'bash', shell=False)
        self._p_open(['mv', '%shmmer_output_run1' % self.mainout, '%shmmer_output' % self.mainout], 'bash', shell=False)

        # Compute the final results
        self._produce_short_summary()

        if len(self._missing_busco_list) == self._totalbuscos:
            BuscoAnalysis._logger.warning(
                'BUSCO did not find any match. Do not forget to check '
                'the file %s to exclude a problem regarding Augustus' %
                augustus_log)
        # get single-copy files as fasta
        if not os.path.exists('%ssingle_copy_busco_sequences' % self.mainout):
            os.makedirs('%ssingle_copy_busco_sequences' % self.mainout)

        BuscoAnalysis._logger.debug('Getting single-copy files...')
        for entry in self._single_copy_files:
            check = 0

            file_name = \
                self._single_copy_files[entry].split('-')[-1].replace('out',
                                                                      'faa')
            file_name_nucl = \
                self._single_copy_files[entry].split('-')[-1].replace('out',
                                                                      'fna')
            target_seq_name = self._single_copy_files[entry].split('[')[0]
            group_name = file_name.split('.')[0]
            seq_coord_start = \
                self._single_copy_files[entry].split(']-')[0].split('[')[1]

            pred_fasta_file = open('%saugustus_output/extracted_proteins/%s' %
                                   (self.mainout, file_name))
            single_copy_outfile = open('%ssingle_copy_busco_sequences/%s.faa' %
                                       (self.mainout, group_name), 'w')

            pred_fasta_file_nucl = \
                open('%saugustus_output/extracted_proteins/%s' %
                     (self.mainout, file_name_nucl))
            single_copy_outfile_nucl = \
                open('%ssingle_copy_busco_sequences/%s.fna' %
                     (self.mainout, group_name), 'w')

            for line in pred_fasta_file:
                if line.startswith('>%s' % target_seq_name):
                    single_copy_outfile.write(
                        '>%s:%s:%s\n' % (group_name, self._sequences,
                                         seq_coord_start))
                    check = 1
                elif line.startswith('>'):
                    check = 0
                elif check == 1:
                    single_copy_outfile.write(line)

            for line in pred_fasta_file_nucl:
                if line.startswith('>%s' % target_seq_name):
                    single_copy_outfile_nucl.write(
                        '>%s:%s:%s\n' % (group_name, self._sequences,
                                         seq_coord_start))
                    check = 1
                elif line.startswith('>'):
                    check = 0
                elif check == 1:
                    single_copy_outfile_nucl.write(line)

            pred_fasta_file.close()
            single_copy_outfile.close()
            pred_fasta_file_nucl.close()
            single_copy_outfile_nucl.close()

    def _move_retraining_parameters(self):
        """
        This function moves retraining parameters from augustus species folder
        to the run folder
        """
        if os.path.exists(self._augustus_config_path + ('species/BUSCO_%s%s' % (self._out, self._random))):
            self._p_open(['cp', '-r', '%sspecies/BUSCO_%s%s'
                         % (self._augustus_config_path, self._out, self._random),
                         '%saugustus_output/retraining_parameters' % self.mainout], 'bash', shell=False)
            self._p_open(['rm', '-rf', '%sspecies/BUSCO_%s%s'
                         % (self._augustus_config_path, self._out, self._random)], 'bash', shell=False)
        else:
            BuscoAnalysis._logger.warning(
                'Augustus did not produce a retrained species folder, '
                'please check the augustus log file in the run folder '
                'to ensure that nothing went wrong '
                '(%saugustus_output/augustus.log)' % self.mainout)

    # @overrides
    def _run_hmmer(self):
        """
        This function runs hmmsearch.
        :raises SystemExit: if the hmmsearch result folder is empty
        after the run
        """
        BuscoAnalysis._logger.info(
            'Running HMMER to confirm orthology of predicted proteins:')

        files = os.listdir('%saugustus_output/extracted_proteins' %
                           self.mainout)
        files.sort()
        if not os.path.exists(self.mainout + 'hmmer_output'):
            os.makedirs('%shmmer_output' % self.mainout)

        count = 0
        for entry in files:
            if entry.split('.')[-2] == 'faa':
                count += 1
                group_name = entry.split('.')[0]
                group_index = entry.split('.')[-1]
                hmmer_job = self._hmmer.create_job()
                hmmer_job.add_parameter('--domtblout')
                hmmer_job.add_parameter('%shmmer_output/%s.out.%s' % (self.mainout, group_name, group_index))
                hmmer_job.add_parameter('-o')
                hmmer_job.add_parameter('%stemp_%s%s' % (self._tmp, self._out, self._random))
                hmmer_job.add_parameter('--cpu')
                hmmer_job.add_parameter('1')
                hmmer_job.add_parameter('%shmms/%s.hmm' % (self._lineage_path, group_name))
                hmmer_job.add_parameter('%saugustus_output/extracted_proteins/%s' %
                                        (self.mainout, entry))

        self._hmmer.run_jobs(self._cpus, BuscoAnalysis._logger)

    def _run_tarzip_augustus_output(self):
        """
        This function tarzips results folder
        """
        # augustus_output/predicted_genes
        self._p_open(['tar', '-C', '%saugustus_output' % self.mainout,
                      '-zcf', '%saugustus_output/predicted_genes.tar.gz' %
                      self.mainout, 'predicted_genes', '--remove-files'],
                     'bash', shell=False)
        # augustus_output/extracted_proteins
        self._p_open(['tar', '-C', '%saugustus_output' % self.mainout,
                      '-zcf', '%saugustus_output/extracted_proteins.tar.gz' %
                      self.mainout, 'extracted_proteins', '--remove-files'],
                     'bash', shell=False)
        # augustus_output/gb
        self._p_open(['tar', '-C', '%saugustus_output' % self.mainout,
                      '-zcf', '%saugustus_output/gb.tar.gz' % self.mainout, 'gb', '--remove-files'],
                     'bash', shell=False)
        # augustus_output/gffs
        self._p_open(['tar', '-C', '%saugustus_output' % self.mainout,
                      '-zcf', '%saugustus_output/gffs.tar.gz' %
                      self.mainout, 'gffs', '--remove-files'], 'bash', shell=False)
        # single_copy_busco_sequences
        self._p_open(['tar', '-C', '%s' % self.mainout, '-zcf',
                      '%ssingle_copy_busco_sequences.tar.gz' % self.mainout,
                      'single_copy_busco_sequences', '--remove-files'], 'bash', shell=False)

    def _extract(self, path, group, aa=True):
        """
        This function extracts fasta files from augustus output
        :param path: the path to the BUSCO run folder
        :type path: str
        :param group: the BUSCO group id
        :type group: str
        :param aa: to tell whether to extract amino acid instead of nucleotide
        sequence
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
                out.write('>g%s[%s:%s-%s]\n' % (count, places[0], places[1],
                                                places[2]))
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
            self._no_predictions.append('%s.faa.%s' %
                                        (group_name, group_index))

    # @overrides
    def _check_tool_dependencies(self):
        """
        check dependencies on tools
        :raises SystemExit: if a Tool is not available
        """
        super(GenomeAnalysis, self)._check_tool_dependencies()
        # check 'augustus' command availability
        if not Tool.check_tool_available('augustus', self._params):
            BuscoAnalysis._logger.error(
                '\"augustus\" is not accessible, check its path in the config.ini.default file (do not include the commmand '
                'in the path !), and '
                'add it to your $PATH environmental variable (the entry in config.ini.default is not sufficient '
                'for retraining to work properly)')
            raise SystemExit

        # check availability augustus - related commands
        if not Tool.check_tool_available('etraining', self._params):
            BuscoAnalysis._logger.error(
                '\"etraining\" is not accessible, check its path in the config.ini.default file(do not include the commmand '
                'in the path !), and '
                'add it to your $PATH environmental variable (the entry in config.ini.default is not sufficient '
                'for retraining to work properly)')
            raise SystemExit

        if not Tool.check_tool_available('gff2gbSmallDNA.pl', self._params):
            BuscoAnalysis._logger.error(
                'Impossible to locate the required script gff2gbSmallDNA.pl. '
                'Check that you properly declared the path to augustus scripts folder in your '
                'config.ini.default file (do not include the script name '
                'in the path !)')
            raise SystemExit

        if not Tool.check_tool_available('new_species.pl', self._params):
            BuscoAnalysis._logger.error(
                'Impossible to locate the required script new_species.pl. '
                'Check that you properly declared the path to augustus scripts folder in your '
                'config.ini.default file (do not include the script name '
                'in the path !)')
            raise SystemExit

        if not Tool.check_tool_available('optimize_augustus.pl', self._params):
            BuscoAnalysis._logger.error(
                'Impossible to locate the required script optimize_augustus.pl. '
                'Check that you properly declared the path to augustus scripts folder in your '
                'config.ini.default file (do not include the script name '
                'in the path !)')
            raise SystemExit

    def _check_file_dependencies(self):
        """
        check dependencies on files and folders
        properly configured.
        :raises SystemExit: if Augustus config path is not writable or
        not set at all
        :raises SystemExit: if Augustus config path does not contain
        the needed species
        present
        """
        try:
            if not os.access(self._augustus_config_path, os.W_OK):
                BuscoAnalysis._logger.error(
                    'Cannot write to Augustus config path, '
                    'please make sure you have write permissions to %s' %
                    self._augustus_config_path)
                raise SystemExit
        except TypeError:
            BuscoAnalysis._logger.error(
                'The environment variable AUGUSTUS_CONFIG_PATH is not set')
            raise SystemExit

        if not os.path.exists(self._augustus_config_path + '/species/%s' % self._target_species):
            BuscoAnalysis._logger.error(
                'Impossible to locate the species "%s" in Augustus config path'
                ' (%sspecies), check that AUGUSTUS_CONFIG_PATH is properly set'
                ' and contains this species. \n\t\tSee the help if you want '
                'to provide an alternative species' %
                (self._target_species, self._augustus_config_path))
            raise SystemExit

    def _write_full_table_header(self, out):
        """
        This function adds a header line to the full table file
        :param out: a full table file
        :type out: file
        """
        out.write('# Busco id\tStatus\tContig\tStart\tEnd\tScore\tLength\n')

    #
    # method that should be considered as if protected, for internal use [class]
    # to move to public and rename if meaningful
    #

    # Nothing
