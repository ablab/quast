#!/usr/bin/env python
# coding: utf-8
"""
.. module:: BuscoConfig
   :synopsis: Load and combine all parameters provided to BUSCO through config file, dataset and command line
.. versionadded:: 3.0.0
.. versionchanged:: 3.0.1

Copyright (c) 2016-2017, Evgeny Zdobnov (ez@ezlab.org)
Licensed under the MIT license. See LICENSE.md file.

"""
import os
from quast_libs.busco.pipebricks.PipeConfig import PipeConfig
from quast_libs.busco.pipebricks.PipeLogger import PipeLogger
from quast_libs import busco

try:
    from configparser import NoOptionError
    from configparser import NoSectionError
    from configparser import ParsingError
    from configparser import DuplicateSectionError
    from configparser import DuplicateOptionError
except ImportError:
    from ConfigParser import NoOptionError  # Python 2.7
    from ConfigParser import NoSectionError  # Python 2.7
    from ConfigParser import ParsingError  # Python 2.7


class BuscoConfig(PipeConfig):
    """
    This class extends pipebricks.PipeConfig to read the config.ini.default file. Furthermore, it uses extra args that can be
    provided through command line and information available in the dataset.cfg file to produce a single instance
    containing all correct parameters to be injected to a busco.BuscoAnalysis instance.
    """

    FORBIDDEN_HEADER_CHARS = ['ç', '¬', '¢', '´', 'ê', 'î', 'ô', 'ŵ', 'ẑ', 'û', 'â', 'ŝ', 'ĝ', 'ĥ', 'ĵ', 'ŷ',
                              'ĉ', 'é', 'ï', 'ẅ', 'ë', 'ẅ', 'ë', 'ẗ,', 'ü', 'í', 'ö', 'ḧ', 'é', 'ÿ', 'ẍ', 'è', 'é',
                              'à', 'ä', '¨', '€', '£', 'á']

    FORBIDDEN_HEADER_CHARS_BEFORE_SPLIT = ['/', '\'']

    HMMER_VERSION = 3.1

    MAX_FLANK = 20000

    VERSION = busco.__version__

    CONTACT = 'mailto:support@orthodb.org'

    DEFAULT_ARGS_VALUES = {'cpu': 1, 'evalue': 1e-3, 'species': 'fly', 'tmp_path': './tmp/', 'limit': 3,
                           'out_path': os.getcwd(), 'domain': 'eukaryota', 'clade_name': 'N/A',
                           'dataset_creation_date': 'N/A',
                           'dataset_nb_buscos': 'N/A', 'dataset_nb_species': 'N/A', 'augustus_parameters': '',
                           'long': False, 'restart': False, 'quiet': False, 'debug': False, 'force': False,
                           'tarzip': False, 'blast_single_core': False}

    MANDATORY_USER_PROVIDED_PARAMS = ['in', 'out', 'lineage_path', 'mode']

    _logger = PipeLogger.get_logger(__name__)

    def __init__(self, conf_file, args, checks=True):
        """
        :param conf_file: a path to a config.ini.default file
        :type conf_file: str
        :param args: key and values matching BUSCO parameters to override config.ini.default values
        :type args: dict
        :param checks: whether to proceed to the mandatory parameters + file dependencies checks,
         used in a main BUSCO analysis. Default True
        :type checks: bool
        """
        try:
            super(BuscoConfig, self).__init__(conf_file)
        except TypeError:
            try:
                PipeConfig.__init__(self, conf_file)  # Python 2.7
            except ParsingError as e:
                BuscoConfig._logger.error('Error in the config file: %s' % e)
                raise SystemExit
        except DuplicateOptionError as e:
            BuscoConfig._logger.error('Duplicated entry in the config.ini.default file: %s' % e)
            raise SystemExit
        except DuplicateSectionError as e:
            BuscoConfig._logger.error('Duplicated entry in the config.ini.default file: %s' % e)
            raise SystemExit
        except ParsingError as e:
            BuscoConfig._logger.error('Error in the config file: %s' % e)
            raise SystemExit

        try:

            # Update the config with args provided by the user, else keep config
            for key in args:
                if args[key] is not None and type(args[key]) is not bool:
                    self.set('busco', key, str(args[key]))
                elif args[key] is True:
                    self.set('busco', key, 'True')

            # Validate that all keys that are mandatory are there
            if checks:
                for param in BuscoConfig.MANDATORY_USER_PROVIDED_PARAMS:
                    try:
                        self.get('busco', param)
                    except NoOptionError:
                        BuscoConfig._logger.error('The parameter \'--%s\' was not provided. '
                                                  'Please add it in the config '
                                                  'file or provide it through the command line' % param)
                        raise SystemExit

            # Edit all path in the config to make them clean
            for item in self.items('busco'):
                if item[0].endswith('_path'):
                    self.set('busco', item[0], BuscoConfig.nice_path(item[1]))

            # load the dataset config, or warn the user if not present
            # Update the config with the info from dataset, when appropriate
            domain = None
            try:
                target_species_file = open('%sdataset.cfg' % self.get('busco', 'lineage_path'))
                for l in target_species_file:
                    if l.split("=")[0] == "name":
                        self.set('busco', 'clade_name', l.strip().split("=")[1])
                    elif l.split("=")[0] == "species":
                        try:
                            self.get('busco', 'species')
                            # if checks:
                            #    BuscoConfig._logger.warning('An augustus species is mentioned in the config file, '
                            #                                'dataset default species (%s) will be ignored'
                            #                                % l.strip().split("=")[1])
                        except NoOptionError:
                            self.set('busco', 'species', l.strip().split("=")[1])
                    elif l.split("=")[0] == "domain":
                        try:
                            self.get('busco', 'domain')
                            # if checks:
                            #    BuscoConfig._logger.warning('A domain for augustus training is mentioned in the config '
                            #                                'file, dataset default domain (%s) will be ignored'
                            #                                % l.strip().split("=")[1])
                        except NoOptionError:
                            self.set('busco', 'domain', l.strip().split("=")[1])
                        domain = l.strip().split("=")[1]
                    elif l.split("=")[0] == "creation_date":
                        self.set('busco', 'dataset_creation_date', l.strip().split("=")[1])
                    elif l.split("=")[0] == "number_of_BUSCOs":
                        self.set('busco', 'dataset_nb_buscos', l.strip().split("=")[1])
                    elif l.split("=")[0] == "number_of_species":
                        self.set('busco', 'dataset_nb_species', l.strip().split("=")[1])
                if checks and domain != 'prokaryota' and domain != 'eukaryota':
                    BuscoConfig._logger.error(
                        'Corrupted dataset.cfg file: domain is %s, should be eukaryota or prokaryota' % domain)
                    raise SystemExit
            except IOError:
                if checks:
                    BuscoConfig._logger.warning("The dataset you provided does not contain the file dataset.cfg, "
                                                "likely because it is an old version. Default species (%s, %s) will be "
                                                "used as augustus species"
                                                % (BuscoConfig.DEFAULT_ARGS_VALUES['species'],
                                                   BuscoConfig.DEFAULT_ARGS_VALUES['domain']))

            # Fill the other with default values if not present
            for param in list(BuscoConfig.DEFAULT_ARGS_VALUES.keys()):
                try:
                    self.get('busco', param)
                except NoOptionError:
                    self.set('busco', param, str(BuscoConfig.DEFAULT_ARGS_VALUES[param]))

            # Edit all path in the config to make them clean, again
            for item in self.items('busco'):
                if item[0].endswith('_path'):
                    self.set('busco', item[0], BuscoConfig.nice_path(item[1]))

            # Convert the ~ into full home path
            if checks:
                for key in self.sections():
                    for item in self.items(key):
                        if item[0].endswith('_path') or item[0] == 'path' or item[0] == 'in':
                            if item[1].startswith('~'):
                                self.set(key, item[0], os.path.expanduser(item[1]))

            # And check that in and lineage path and file actually exists
            if checks:
                for item in self.items('busco'):
                    if item[0] == 'lineage_path' or item[0] == 'in':
                        BuscoConfig.check_path_exist(item[1])
            # Prevent the user form using "/" in out name
            if checks:
                if '/' in self.get('busco', 'out'):
                    BuscoConfig._logger.error('Please do not provide a full path in --out parameter, no slash.'
                                              ' Use out_path in the config.ini.default file to specify the full path.')
                    raise SystemExit

            # Check the value of limit
            if checks:
                if self.getint('busco', 'limit') == 0 or self.getint('busco', 'limit') > 20:
                    BuscoConfig._logger.error('Limit must be an integer between 1 and 20 (you have used: %s). '
                                              'Note that this parameter is not needed by the protein mode.'
                                              % self.getint('busco', 'limit'))
                    raise SystemExit

            # Warn if custom evalue
            if checks:
                if self.getfloat('busco', 'evalue') != BuscoConfig.DEFAULT_ARGS_VALUES['evalue']:
                    BuscoConfig._logger.warning('You are using a custom e-value cutoff')

        except NoSectionError:
            BuscoConfig._logger.error('No section [busco] found in %s. Please make sure both the file and this section '
                                      'exist, see userguide.' % conf_file)
            raise SystemExit

        except NoOptionError:
            pass  # if mandatory options are not requiered because the BuscoConfig instance is not meant to be used
            # in a regular Busco Analysis but by an additional script.

        for item in self.items('busco'):
            BuscoConfig._logger.debug(item)

    @staticmethod
    def check_path_exist(path):
        """
        This function checks whether the provided path exists
        :param path: the path to be tested
        :type path: str
        :raises SystemExit: if the path cannot be reached
        """
        if not os.path.exists(path):
            BuscoConfig._logger.error('Impossible to read %s' % path)
            raise SystemExit

    @staticmethod
    def nice_path(path):
        """
        :param path: a path to check
        :type path: str
        :return: the same but cleaned path
        :rtype str:
        """
        try:
            if path[-1] != '/':
                path += '/'
            return path
        except TypeError:
            return None
